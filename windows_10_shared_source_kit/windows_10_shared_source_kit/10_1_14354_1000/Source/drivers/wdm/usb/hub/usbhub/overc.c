/*++

Copyright (c) Microsoft Corporation. All rights reserved

Module Name:

    overc.c

     3-10-2004

Abstract:

    This file contains code and documentation for overcurrent handling.

    These routines provide the handling of overcurrent events reported by a hub.  It
    is important to note that not all hubs will report an overcurrent event even when
    one of the ports is shorted out. These hubs usually implement some time of current
    limitation in hardware so they are stil compliant.  When overcurrent occurrs
    on these hubs they are usually recovered thru 'ESD' or hardware reset recovery.
    Other hubs will disconnect from the bus when they experience overcurrent.

    Because of this shorting out a port will not necessarily cause the an overcurrent
    event and UI to be generated by us.

Author:

    jdunn

Environment:

    Kernel mode

Revision History:

--*/


#include "hubdef.h"
#include "pch.h"

#ifdef HUB_WPP
#include "overc.tmh"
#endif

CCHAR usbfile_overc_c[] = "overc.c";

#define USBFILE usbfile_overc_c

VOID
UsbhQueueOvercurrentReset(
    PDEVICE_OBJECT HubFdo,
    PHUB_EXCEPTION_RECORD Exr
    )
 /*++

Routine Description:

    Attept 'auto' overcurrent recovery for a port.

Arguments:

    Exr - optional pointer to the exception record
    ( we could use this to tweak reset behavior if we needed to )

Return Value:

    ntStatus
--*/
{
    PDEVICE_EXTENSION_HUB hubExt;
    PHUB_PORT_DATA pd;
    NTSTATUS nts;

    hubExt = FdoExt(HubFdo);

    UsbhAssertPassive();
    LOG(HubFdo, LOG_OVERC, 'ovr1', 0, Exr->PortNumber);
    
    DbgTrace((UsbhDebugTrace,"%!FUNC! >\n"));

    if (hubExt->OvercurrentDetected == FALSE) {
        
        hubExt->OvercurrentDetected = TRUE;

        UsbhQueueWorkItem(HubFdo,
                          DelayedWorkQueue,
                          UsbhSetHubOvercurrentDetectedKey,
                          0,
                          0,
                          SIG_OVERC_WORKITEM);
    }
    
    pd = UsbhGetPortData(HubFdo, Exr->PortNumber);
    UsbhAssert(HubFdo, pd);

    if (pd) {
        PSTATE_CONTEXT sc;

        sc = PDOC(pd);
        
        // disable the port change queue until we can reset
        UsbhPCE_Disable(HubFdo, Exr->PortNumber, sc);

        nts = UsbhQueueWorkItem(HubFdo,
                          DelayedWorkQueue,
                          UsbhAutoOvercurrentResetWorker,
                          PDOC(pd),
                          pd->PortNumber,
                          SIG_OVERC_WORKITEM);

        if (NT_ERROR(nts)) {

            Usbh_OvercurrentDerefHubBusy(HubFdo, pd, TRUE);
            
            // on failure we initiate a hardware reset
            UsbhDispatch_HardResetEvent(HubFdo, PNP_CONTEXT(HubFdo), HRE_RequestReset);
        }
    }

    return;

}


VOID
UsbhAutoOvercurrentResetWorker(
    PDEVICE_OBJECT HubFdo,
    ULONG PortNumber,
    PVOID Context
    )
 /*++

Routine Description:

    Attempt 'auto' overcurrent recovery for a port.

    This routine handles the case where there is no driver

Arguments:

    Context - state context that requested the reset.
    
Return Value:

    none -- we reset if we cannot clear it.

--*/
{
    PDEVICE_EXTENSION_HUB hubExt;
    NTSTATUS nts = STATUS_SUCCESS;
    PHUB_PORT_DATA pd;

    hubExt = FdoExt(HubFdo);

    UsbhAssertPassive();

    pd = UsbhGetPortData(HubFdo, LOWORD(PortNumber));

    nts = Usbh__TestPoint__Ulong(HubFdo, TstPt_OvercurrentResetWorker,
        nts, PortNumber);

    if (NT_SUCCESS(nts)) {
        nts = REF_HUB(HubFdo, Context, AutoOverCurrentWorker_ocaW);

        if (NT_SUCCESS(nts)) {
            UsbhOvercurrentResetWorker(HubFdo, PortNumber, Context, TRUE);

            DEREF_HUB(HubFdo, Context); 
        }
    } 
    
    if (!NT_SUCCESS(nts)) {
        Usbh_OvercurrentDerefHubBusy(HubFdo, pd, TRUE);
    }
}


VOID
UsbhOvercurrentResetWorker(
    PDEVICE_OBJECT HubFdo,
    ULONG PortNumber,
    PSTATE_CONTEXT RequestSc,
    BOOLEAN Auto
    )
 /*++

Routine Description:

    Attempt overcurrent recovery for a port.  This routine resets
    port power and port state.

Arguments:

    Auto - Indicates that the hub is trying to automatically recover the port, as opposed to 
           requiring the user to use the popup UI

    RequestSc - This is the oevrc context for the port that generated the overcurrent event. 

Return Value:

    none -- we reset if we cannot clear it.

--*/
{
    PDEVICE_EXTENSION_HUB hubExt;
    PHUB_PORT_DATA pd;
    NTSTATUS nts;
    USHORT pn = (USHORT)PortNumber;
    USB_HUB_PORT_STATE ps;
    USBD_STATUS usbdStatus;
    PSTATE_CONTEXT sc;
    
    hubExt = FdoExt(HubFdo);

    pd = UsbhGetPortData(HubFdo, pn);

    UsbhAssertPassive();

    // the worker runs in its own context attched to the portdata structure
    sc = RequestSc;
    LOG(HubFdo, LOG_OVERC, 'ovrW', sc, PortNumber);

    // we have no device so all we need to do is re-power the port
    // we will see a fresh connect if the device is still there.

    do {

        // allow 500 ms for overcurrent condition to dissipate
        UsbhWait(HubFdo, 500);

        LOG(HubFdo, LOG_OVERC, 'ov1P', sc, pn);

        // reset the indicator now BEFORE we re-enumerate so
        // that the indicator is set properly on the connect.
        UsbhEvPIND_SetAuto(HubFdo, pn, sc);

        // drop the old device
        UsbhPCE_BusDisconnect(HubFdo, sc, pn);

        // re-power the port
        nts = UsbhSetPortPower(HubFdo, pn);

        LOG(HubFdo, LOG_OVERC, 'ov2P', nts, pn);
        if (NT_SUCCESS(nts)) {
            // wait power up time before doing any enumeration

            UsbhWait(HubFdo, HubG.GlobalDefaultHubPowerUpTime);

            UsbhPCE_Resume(HubFdo, sc, pn);
            LOG(HubFdo, LOG_OVERC, 'ovRS', nts, pn);

            nts = UsbhQueryPortState(HubFdo,
                                     pn,
                                     &ps,
                                     &usbdStatus);

            if (Usb_Disconnected(nts)) {
                // no longer connected bail
                break;
            }

            LOG(HubFdo, LOG_POWER, 'ov4P', ps.Status.us, ps.Change.us);
            LOG(HubFdo, LOG_POWER, 'ov4.', nts, pn);

            if (NT_SUCCESS(nts)) {

                if (ps.Status.Power == 0) {
                    // could not re- apply power?
                    TEST_TRAP();
                }

                if (ps.Status.Connect == 1 &&
                    ps.Change.ConnectChange == 0) {
                    // device still connected, queue a change so that we reenumerate it.

                    LOG(HubFdo, LOG_POWER, 'ov5P', pn, nts);

                    UsbhQueueSoftConnectChange(HubFdo, pn, sc, FALSE);
                }
            }

            // reset OvercurrentCounter
            pd->OvercurrentCounter = 0;

            // re-enable the port change queue

            LOG(HubFdo, LOG_OVERC, 'ov3e', sc, pn);

            UsbhPCE_Enable(HubFdo, sc, pn);

            // recovery complete
            break;

        }

        if (Usb_Disconnected(nts)) {
            // no longer connected bail
            break;
        }

        //** The failsafe here is to initaite a full reset of the hub -- this will cause
        //** the hub to reinitialize.

        LOG(HubFdo, LOG_OVERC, 'ov4!', sc, PortNumber);
        TEST_TRAP();
        // could not re-power, queue a hardware reset
        UsbhDispatch_HardResetEvent(HubFdo, sc, HRE_RequestReset);

    } WHILE (0);

    Usbh_OvercurrentDerefHubBusy(HubFdo, pd, Auto);
}


VOID
UsbhDriverOvercurrentResetWorker(
    PDEVICE_OBJECT HubFdo,
    ULONG PortNumber,
    PVOID Context
    )
 /*++

Routine Description:

    Attempt popup UI overcurrent recovery for a port.

    This routine handles the case where there is no driver

Arguments:

Return Value:

    none -- we reset if we cannot clear it.

--*/
{
    PDEVICE_EXTENSION_HUB hubExt;
    NTSTATUS nts = STATUS_SUCCESS;
    PHUB_PORT_DATA pd;

    hubExt = FdoExt(HubFdo);

    UsbhAssertPassive();

    // popup overcurrent UI

    nts = Usbh__TestPoint__Ulong(HubFdo, TstPt_OvercurrentResetWorker,
        nts, PortNumber);

    if (NT_SUCCESS(nts)) {
        
        nts = REF_HUB(HubFdo, Context, DevOverCurrentWorker_ocdW);
        if (NT_SUCCESS(nts)) {
            UsbhDeviceOvercurrentPopup(HubFdo, (USHORT)PortNumber);

            DEREF_HUB(HubFdo, Context);
        }
    } 
    
    if (!NT_SUCCESS(nts)) {
        pd = UsbhGetPortData(HubFdo, LOWORD(PortNumber));
        Usbh_OvercurrentDerefHubBusy(HubFdo, pd, FALSE);
    }
}


VOID
UsbhQueueDriverOvercurrent(
    PDEVICE_OBJECT HubFdo,
    PHUB_EXCEPTION_RECORD Exr
    )
 /*++

Routine Description:

    Overcurrent occured on a device which has a driver loaded.  Give the device a chnace
    to recover on its own.

Arguments:

    Exr - optional pointer to the exception record
    ( we could use this to tweak reset behavior if we needed to )

Return Value:

    ntStatus
--*/
{
    PDEVICE_EXTENSION_HUB hubExt;
    PHUB_PORT_DATA pd;
    NTSTATUS nts;

    hubExt = FdoExt(HubFdo);

    UsbhAssertPassive();
    LOG(HubFdo, LOG_OVERC, 'Dov1', 0, Exr->PortNumber);

    DbgTrace((UsbhDebugTrace,"%!FUNC! >\n"));

    if (hubExt->OvercurrentDetected == FALSE) {
        
        hubExt->OvercurrentDetected = TRUE;

        UsbhQueueWorkItem(HubFdo,
                          DelayedWorkQueue,
                          UsbhSetHubOvercurrentDetectedKey,
                          0,
                          0,
                          SIG_OVERC_WORKITEM);
    }

    pd = UsbhGetPortData(HubFdo, Exr->PortNumber);
    UsbhAssert(HubFdo, pd);

    if (pd) {
        PSTATE_CONTEXT sc;

        sc = PDOC(pd);

        // disable the port change queue until we can reset
        UsbhPCE_Disable(HubFdo, Exr->PortNumber, sc);

        nts = UsbhQueueWorkItem(HubFdo,
                          DelayedWorkQueue,
                          UsbhDriverOvercurrentResetWorker,
                          sc,
                          pd->PortNumber,
                          SIG_OVERC_WORKITEM);

        if (NT_ERROR(nts)) {

            Usbh_OvercurrentDerefHubBusy(HubFdo, pd, FALSE);

            // on failure we initiate a hardware reset
            UsbhDispatch_HardResetEvent(HubFdo, PNP_CONTEXT(HubFdo), HRE_RequestReset);
        }
    } else {
        Usbh_OvercurrentDerefHubBusy(HubFdo, pd, FALSE);
    }


    return;
}


VOID
UsbhDeviceOvercurrentPopup(
    PDEVICE_OBJECT HubFdo,
    USHORT PortNumber
    )
/*++

Description:

    Gnerates the WMI event that pops up the overcurrent UI

Arguments:

Irql: PASSIVE

Return:

    none

--*/
{
    PDEVICE_EXTENSION_HUB hubExt;
    PUSB_CONNECTION_NOTIFICATION notification;
    NTSTATUS nts;
    PHUB_PORT_DATA pd;

    UsbhAssertPassive();

    hubExt = FdoExt(HubFdo);

    UsbhEvPIND_SetBlink(HubFdo, PortNumber, PNP_CONTEXT(HubFdo), PortLED_Amber, 500);

    notification = UsbhBuildWmiConnectionNotification(HubFdo,
                                                      PortNumber);

    if (notification) {
        // set information
        notification->NotificationType = OverCurrent;

        // attempt to trigger the event
        nts = WmiFireEvent(HubFdo,
                           (LPGUID)&GUID_USB_WMI_STD_NOTIFICATION,
                           0,
                           sizeof(struct _USB_CONNECTION_NOTIFICATION),
                           notification);
    } else {
        pd = UsbhGetPortData(HubFdo, LOWORD(PortNumber));
        Usbh_OvercurrentDerefHubBusy(HubFdo, pd, FALSE);
    }
}
