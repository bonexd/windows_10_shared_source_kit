/*++

Copyright (c) 2011  Microsoft Corporation

Module Name:

    syminfo.c

--*/

#include "hubdef.h"
#include "pch.h"
#include "logdef.h"

#define DECLARE_TYPE(Name) Name _DECL_##Name

//
// Keep the list sorted
//

DECLARE_TYPE (DEBUG_LOG);
DECLARE_TYPE (DEVICE_EXTENSION_HUB);
DECLARE_TYPE (DEVICE_EXTENSION_PDO);
DECLARE_TYPE (DEVICE_OBJECT);
DECLARE_TYPE (G_STATE_LOG_ENTRY);
DECLARE_TYPE (HUB_EXCEPTION_RECORD);
DECLARE_TYPE (HUB_GLOBALS);
DECLARE_TYPE (HUB_PORT_DATA);
DECLARE_TYPE (HUB_POWER_CONTEXT);
DECLARE_TYPE (HUB_REFERENCE_LIST_ENTRY);
DECLARE_TYPE (HUB_TIMER_OBJECT);
DECLARE_TYPE (HUB_WORKITEM);
DECLARE_TYPE (IO_LIST_ENTRY);
DECLARE_TYPE (IRP);
DECLARE_TYPE (LATCH_LIST_ENTRY);
DECLARE_TYPE (PORT_CHANGE_CONTEXT);
DECLARE_TYPE (PORT_CHANGE_LOG_ENTRY);
DECLARE_TYPE (SSH_BUSY_HANDLE);
DECLARE_TYPE (STATE_CONTEXT);
DECLARE_TYPE (USB_CD_ERROR_INFORMATION);
DECLARE_TYPE (USB_BOS_DESCRIPTOR);
DECLARE_TYPE (USB_COMMON_DESCRIPTOR);
DECLARE_TYPE (USB_CONFIGURATION_DESCRIPTOR);
DECLARE_TYPE (USB_DEVICE_CAPABILITY_CONTAINER_ID_DESCRIPTOR);
DECLARE_TYPE (USB_DEVICE_CAPABILITY_DESCRIPTOR);
DECLARE_TYPE (USB_DEVICE_CAPABILITY_SUPERSPEED_USB_DESCRIPTOR);
DECLARE_TYPE (USB_DEVICE_CAPABILITY_USB20_EXTENSION_DESCRIPTOR);
DECLARE_TYPE (USB_DEVICE_DESCRIPTOR);
DECLARE_TYPE (USB_ENDPOINT_DESCRIPTOR);
DECLARE_TYPE (USB_HUB_DESCRIPTOR);
DECLARE_TYPE (USB_ID_STRING);
DECLARE_TYPE (USB_INTERFACE_ASSOCIATION_DESCRIPTOR);
DECLARE_TYPE (USB_INTERFACE_DESCRIPTOR);
DECLARE_TYPE (USB_START_FAILDATA);
DECLARE_TYPE (USBHUB_FDO_FLAGS);
DECLARE_TYPE (USBHUB_FDO_FLAGS);
DECLARE_TYPE (USBHUB_PDO_FLAGS);

//
// Make it build
//

int __cdecl main() {
    return 0;
}