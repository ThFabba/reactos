#include "usbhub.h"

#define NDEBUG
#include <debug.h>

NTSTATUS
NTAPI
USBH_AddDevice(
  IN PDRIVER_OBJECT DriverObject,
  IN PDEVICE_OBJECT LowerPDO)
{
    PDEVICE_OBJECT DeviceObject;
    NTSTATUS Status;
    PUSBHUB_FDO_EXTENSION HubExtension;
    PDEVICE_OBJECT LowerDevice;

    DPRINT("USBH_AddDevice: DriverObject - %p, LowerPDO - %p\n",
           DriverObject,
           LowerPDO);

    DeviceObject = NULL;

    Status = IoCreateDevice(DriverObject,
                            sizeof(USBHUB_FDO_EXTENSION),
                            NULL,
                            0x8600,
                            FILE_AUTOGENERATED_DEVICE_NAME,
                            FALSE,
                            &DeviceObject);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBH_AddDevice: IoCreateDevice() fail\n");

        if (DeviceObject)
        {
            IoDeleteDevice(DeviceObject);
        }

        return Status;
    }

    DPRINT("USBH_AddDevice: DeviceObject - %p\n", DeviceObject);

    HubExtension = (PUSBHUB_FDO_EXTENSION)DeviceObject->DeviceExtension;
    RtlZeroMemory(HubExtension, sizeof(USBHUB_FDO_EXTENSION));

    HubExtension->Common.ExtensionType = USBH_EXTENSION_TYPE_HUB;

    LowerDevice = IoAttachDeviceToDeviceStack(DeviceObject, LowerPDO);

    if (!LowerDevice)
    {
        DPRINT1("USBH_AddDevice: IoAttachDeviceToDeviceStack() fail\n");

        if (DeviceObject)
        {
            IoDeleteDevice(DeviceObject);
        }

        return STATUS_UNSUCCESSFUL;
    }

    DPRINT("USBH_AddDevice: LowerDevice  - %p\n", LowerDevice);

    HubExtension->Common.SelfDevice = DeviceObject;

    HubExtension->LowerPDO = LowerPDO;
    HubExtension->LowerDevice = LowerDevice;

    DeviceObject->Flags |= DO_POWER_PAGABLE;
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DPRINT("USBH_AddDevice: call IoWMIRegistrationControl() UNIMPLEMENTED. FIXME. \n");

    return Status;
}

VOID
NTAPI
USBH_DriverUnload(
  IN PDRIVER_OBJECT DriverObject)
{
    DPRINT("USBH_DriverUnload: UNIMPLEMENTED\n");
}

NTSTATUS
NTAPI
USBH_HubDispatch(IN PDEVICE_OBJECT DeviceObject,
                 IN PIRP Irp)
{
    PCOMMON_DEVICE_EXTENSION DeviceExtension;
    ULONG ExtensionType;
    NTSTATUS Status;

    DPRINT("USBH_HubDispatch: DeviceObject - %p, Irp - %p\n",
           DeviceObject,
           Irp);

    DeviceExtension = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    ExtensionType = DeviceExtension->ExtensionType;

    if (ExtensionType == USBH_EXTENSION_TYPE_HUB)
    {
        Status = USBH_FdoDispatch((PUSBHUB_FDO_EXTENSION)DeviceExtension, Irp);
    }
    else if (ExtensionType == USBH_EXTENSION_TYPE_PORT)
    {
        Status = USBH_PdoDispatch((PUSBHUB_PORT_PDO_EXTENSION)DeviceExtension, Irp);
    }
    else
    {
        DPRINT1("USBH_HubDispatch: Unknown ExtensionType - %x\n", ExtensionType);
        DbgBreakPoint();
    }

    return Status;
}

NTSTATUS
NTAPI
DriverEntry(IN PDRIVER_OBJECT DriverObject,
            IN PUNICODE_STRING RegistryPath)
{
    DPRINT("USBHUB: DriverEntry - %wZ\n", RegistryPath);

    DriverObject->DriverExtension->AddDevice = USBH_AddDevice;
    DriverObject->DriverUnload = USBH_DriverUnload;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = USBH_HubDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = USBH_HubDispatch;

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = USBH_HubDispatch;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = USBH_HubDispatch;

    DriverObject->MajorFunction[IRP_MJ_PNP] = USBH_HubDispatch;
    DriverObject->MajorFunction[IRP_MJ_POWER] = USBH_HubDispatch;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = USBH_HubDispatch;

    return STATUS_SUCCESS;
}

