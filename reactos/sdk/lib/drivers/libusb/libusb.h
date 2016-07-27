#ifndef LIBUSB_H__
#define LIBUSB_H__

#include <ntddk.h>
#include <hubbusif.h>
#include <usbbusif.h>

extern "C"
{
    #include <usbdlib.h>
}

//
// FIXME: 
// #include <usbprotocoldefs.h>
//
#include <stdio.h>
#include <wdmguid.h>

//
// FIXME:
// the following includes are required to get kcom to compile
//
#include <portcls.h>
#include <kcom.h>

#include "common_interfaces.h"

//
// flags for handling USB_REQUEST_SET_FEATURE / USB_REQUEST_GET_FEATURE
//
#define PORT_ENABLE         1
#define PORT_SUSPEND        2
#define PORT_OVER_CURRENT   3
#define PORT_RESET          4
#define PORT_POWER          8
#define C_PORT_CONNECTION   16
#define C_PORT_ENABLE       17
#define C_PORT_SUSPEND      18
#define C_PORT_OVER_CURRENT 19
#define C_PORT_RESET        20

//
// flags for Urb->UrbHeader.UsbdFlags
//
#define USBD_FLAG_ALLOCATED_MDL         0x00000001
#define USBD_FLAG_ALLOCATED_TRANSFER    0x00000002

typedef struct
{
    BOOLEAN IsFDO;                                               // is device a FDO or PDO
    BOOLEAN IsHub;                                               // is device a hub / child - not yet used
    PDISPATCHIRP Dispatcher;                                     // dispatches the code
}COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;


typedef struct _WORK_ITEM_DATA
{
    WORK_QUEUE_ITEM WorkItem;                                   // work item
    PVOID CallbackContext;                                      // callback context
    PRH_INIT_CALLBACK CallbackRoutine;                          // callback routine
} INIT_ROOT_HUB_CONTEXT, *PINIT_ROOT_HUB_CONTEXT;

//-----------------------------------------------------------------------------
typedef struct _LIBUSB_RH_DESCRIPTORS {

  USB_DEVICE_DESCRIPTOR           DeviceDescriptor;             // 18
  USB_CONFIGURATION_DESCRIPTOR    ConfigDescriptor;             // 9
  USB_INTERFACE_DESCRIPTOR        InterfaceDescriptor;          // 9
  USB_ENDPOINT_DESCRIPTOR         EndPointDescriptor;           // 7
  USB_HUB_DESCRIPTOR              Descriptor;                   // 7 + 2[1..32] (7 + 2..64)

} LIBUSB_RH_DESCRIPTORS, *PLIBUSB_RH_DESCRIPTORS;

//-----------------------------------------------------------------------------
typedef struct _LIBUSB_SG_ELEMENT {

  PHYSICAL_ADDRESS                SgPhysicalAddress;            //
  ULONG                           SgTransferLength;             //

} LIBUSB_SG_ELEMENT, *PLIBUSB_SG_ELEMENT;

//-----------------------------------------------------------------------------
typedef struct _LIBUSB_TRANSFER {

  PIRP                            Irp;                          // 
  PURB                            Urb;                          // 
  SIZE_T                          HcdTransferLength;            // 
  PLIBUSB_PIPE_HANDLE             PipeHandle;                   // 


} LIBUSB_TRANSFER, *PLIBUSB_TRANSFER;

//
// tag for allocations
//
#define TAG_USBLIB 'LBSU'

//
// assert for c++ - taken from portcls
//
#define PC_ASSERT(exp) \
  (VOID)((!(exp)) ? \
    RtlAssert((PVOID) #exp, (PVOID)__FILE__, __LINE__, NULL ), FALSE : TRUE)

// hcd_controller.cpp
extern "C"
{
NTSTATUS NTAPI CreateHCDController(PHCDCONTROLLER *HcdController);

// hardware.cpp
NTSTATUS NTAPI CreateUSBHardware(PUSBHARDWAREDEVICE *OutHardware);

// misc.cpp
NTSTATUS NTAPI SyncForwardIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS NTAPI GetBusInterface(PDEVICE_OBJECT DeviceObject, PBUS_INTERFACE_STANDARD busInterface);
NTSTATUS NTAPI ParseResources(IN PCM_RESOURCE_LIST AllocatedResourcesTranslated, IN PLIBUSB_RESOURCES Resources);

// root_hub_controller.cpp
NTSTATUS NTAPI CreateHubController(PHUBCONTROLLER * OutHubController);

// memory_manager.cpp
NTSTATUS NTAPI CreateDMAMemoryManager(PDMAMEMORYMANAGER *OutMemoryManager);

// usb_device.cpp
NTSTATUS NTAPI CreateUSBDevice(PUSBDEVICE *OutDevice);

// libusb.cpp
NTSTATUS NTAPI USBLIB_AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);
NTSTATUS NTAPI USBLIB_Dispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);

}

#endif /* LIBUSB_H__ */
