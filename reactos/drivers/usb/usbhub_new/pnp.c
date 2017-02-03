#include "usbhub.h"

//#define NDEBUG
#include <debug.h>

NTSTATUS
NTAPI
USBH_IrpCompletion(IN PDEVICE_OBJECT DeviceObject,
                   IN PIRP Irp,
                   IN PVOID Context)
{
    PRKEVENT Event;

    DPRINT("USBH_IrpCompletion: Irp - %p\n", Irp);

    Event = (PRKEVENT)Context;
    KeSetEvent(Event, EVENT_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
NTAPI
USBH_HubPnPIrpComplete(IN PDEVICE_OBJECT DeviceObject,
                       IN PIRP Irp,
                       IN PVOID Context)
{
    PUSBHUB_FDO_EXTENSION HubExtension;

    DPRINT("USBH_HubPnPIrpComplete: Irp - %p\n", Irp);

     HubExtension = (PUSBHUB_FDO_EXTENSION)Context;

    if (!NT_SUCCESS(Irp->IoStatus.Status))
    {
        DPRINT1("USBH_HubPnPIrpComplete: Irp failed - %p\n", Irp->IoStatus.Status);
        HubExtension->HubFlags |= USBHUB_FDO_FLAG_DEVICE_FAILED;
    }

    Irp->IoStatus.Status = STATUS_MORE_PROCESSING_REQUIRED;

    KeSetEvent(&HubExtension->LowerDeviceEvent, EVENT_INCREMENT, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
NTAPI
USBHUB_GetBusInterface(IN PDEVICE_OBJECT DeviceObject,
                       IN PUSB_BUS_INTERFACE_HUB_V5 BusInterface)
{
    PIRP Irp;
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    KEVENT Event;

    DPRINT("USBHUB_GetBusInterface: ... \n");

    Irp = IoAllocateIrp(DeviceObject->StackSize, 0);

    if (!Irp)
    {
        DPRINT1("USBHUB_GetBusInterface: IoAllocateIrp() failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    IoSetCompletionRoutine(Irp,
                           USBH_IrpCompletion,
                           &Event,
                           TRUE,
                           TRUE,
                           TRUE);

    IoStack = IoGetNextIrpStackLocation(Irp);

    IoStack->MajorFunction = IRP_MJ_PNP;
    IoStack->MinorFunction = IRP_MN_QUERY_INTERFACE;

    IoStack->Parameters.QueryInterface.InterfaceType = &USB_BUS_INTERFACE_HUB_GUID;
    IoStack->Parameters.QueryInterface.Size = sizeof(USB_BUS_INTERFACE_HUB_V5);
    IoStack->Parameters.QueryInterface.Version = USB_BUSIF_HUB_VERSION_5;
    IoStack->Parameters.QueryInterface.Interface = (PINTERFACE)BusInterface;
    IoStack->Parameters.QueryInterface.InterfaceSpecificData = DeviceObject;

    Status = IoCallDriver(DeviceObject, Irp);

    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event,
                              Suspended,
                              KernelMode,
                              FALSE,
                              NULL);

        Status = Irp->IoStatus.Status;
    }

    IoFreeIrp(Irp);

    return Status;
}

NTSTATUS
NTAPI
USBHUB_GetBusInterfaceUSBDI(IN PDEVICE_OBJECT DeviceObject,
                            IN PUSB_BUS_INTERFACE_USBDI_V2 BusInterfaceUSBDI)
{
    PIRP Irp;
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    KEVENT Event;

    DPRINT("USBHUB_GetBusInterfaceUSBDI: ... \n");

    Irp = IoAllocateIrp(DeviceObject->StackSize, 0);

    if (!Irp)
    {
        DPRINT1("USBHUB_GetBusInterfaceUSBDI: IoAllocateIrp() failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    IoSetCompletionRoutine(Irp,
                           USBH_IrpCompletion,
                           &Event,
                           TRUE,
                           TRUE,
                           TRUE);

    IoStack = IoGetNextIrpStackLocation(Irp);

    IoStack->MajorFunction = IRP_MJ_PNP;
    IoStack->MinorFunction = IRP_MN_QUERY_INTERFACE;

    IoStack->Parameters.QueryInterface.InterfaceType = &USB_BUS_INTERFACE_USBDI_GUID;
    IoStack->Parameters.QueryInterface.Size = sizeof(USB_BUS_INTERFACE_USBDI_V2);
    IoStack->Parameters.QueryInterface.Version = USB_BUSIF_USBDI_VERSION_2;
    IoStack->Parameters.QueryInterface.Interface = (PINTERFACE)BusInterfaceUSBDI;
    IoStack->Parameters.QueryInterface.InterfaceSpecificData = NULL;

    Status = IoCallDriver(DeviceObject, Irp);

    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event,
                              Suspended,
                              KernelMode,
                              FALSE,
                              NULL);

        Status = Irp->IoStatus.Status;
    }

    IoFreeIrp(Irp);

    return Status;
}

VOID
NTAPI
USBH_QueryCapabilities(IN PDEVICE_OBJECT DeviceObject,
                       IN PDEVICE_CAPABILITIES DeviceCapabilities)
{
    PIRP Irp;
    PIO_STACK_LOCATION IoStack;
    KEVENT Event;

    DPRINT("USBH_QueryCapabilities: ... \n");

    Irp = IoAllocateIrp(DeviceObject->StackSize, 0);

    if (!Irp)
    {
        DPRINT1("USBH_QueryCapabilities: IoAllocateIrp() failed\n");
        return;
    }

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    RtlZeroMemory(DeviceCapabilities, sizeof(DEVICE_CAPABILITIES));

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    IoSetCompletionRoutine(Irp,
                           USBH_IrpCompletion,
                           &Event,
                           TRUE,
                           TRUE,
                           TRUE);

    IoStack = IoGetNextIrpStackLocation(Irp);
 
    IoStack->MajorFunction = IRP_MJ_PNP;
    IoStack->MinorFunction = IRP_MN_QUERY_CAPABILITIES;

    IoStack->Parameters.DeviceCapabilities.Capabilities = DeviceCapabilities;
    IoStack->Parameters.DeviceCapabilities.Capabilities->Size = sizeof(DEVICE_CAPABILITIES);
    IoStack->Parameters.DeviceCapabilities.Capabilities->Version = 1;
    IoStack->Parameters.DeviceCapabilities.Capabilities->Address = -1;
    IoStack->Parameters.DeviceCapabilities.Capabilities->UINumber = -1;

    if (IoCallDriver(DeviceObject, Irp) == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event,
                              Suspended,
                              KernelMode,
                              FALSE,
                              NULL);
    }

    IoFreeIrp(Irp);
}

NTSTATUS
NTAPI
USBH_OpenConfiguration(IN PUSBHUB_FDO_EXTENSION HubExtension)
{
    PUSB_INTERFACE_DESCRIPTOR Pid;
    PURB Urb;
    NTSTATUS Status;
    USBD_INTERFACE_LIST_ENTRY InterfaceList[2] = {{NULL, NULL}, {NULL, NULL}};

    DPRINT("USBH_OpenConfiguration ... \n");

    if (HubExtension->HubFlags & USBHUB_FDO_FLAG_USB20_HUB && 
        HubExtension->LowerPDO != HubExtension->RootHubPdo)
    {
        Pid = USBD_ParseConfigurationDescriptorEx(HubExtension->HubConfigDescriptor,
                                                  HubExtension->HubConfigDescriptor,
                                                  -1,
                                                  -1,
                                                  USB_DEVICE_CLASS_HUB,
                                                  -1,
                                                  2);

        if (Pid)
        {
            HubExtension->HubFlags |= USBHUB_FDO_FLAG_MULTIPLE_TTS;
        }
        else
        {
            Pid = USBD_ParseConfigurationDescriptorEx(HubExtension->HubConfigDescriptor,
                                                      HubExtension->HubConfigDescriptor,
                                                      -1,
                                                      -1,
                                                      USB_DEVICE_CLASS_HUB,
                                                      -1,
                                                      1);

            if (Pid)
            {
                goto Next;
            }

            Pid = USBD_ParseConfigurationDescriptorEx(HubExtension->HubConfigDescriptor,
                                                      HubExtension->HubConfigDescriptor,
                                                      -1,
                                                      -1,
                                                      USB_DEVICE_CLASS_HUB,
                                                      -1,
                                                      0);
        }
    }
    else
    {
        Pid = USBD_ParseConfigurationDescriptorEx(HubExtension->HubConfigDescriptor,
                                                  HubExtension->HubConfigDescriptor,
                                                  -1,
                                                  -1,
                                                  USB_DEVICE_CLASS_HUB,
                                                  -1,
                                                  -1);
    }

    if (!Pid)
    {
        return STATUS_UNSUCCESSFUL;
    }

  Next:

    if (Pid->bInterfaceClass != USB_DEVICE_CLASS_HUB)
    {
        return STATUS_UNSUCCESSFUL;
    }

    InterfaceList[0].InterfaceDescriptor = Pid;

    Urb = USBD_CreateConfigurationRequestEx(HubExtension->HubConfigDescriptor,
                                            InterfaceList);

    if (!Urb)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = USBH_FdoSyncSubmitUrb(HubExtension->Common.SelfDevice, Urb);

    if (NT_SUCCESS(Status))
    {
        RtlCopyMemory(&HubExtension->PipeInfo,
                      InterfaceList[0].Interface->Pipes,
                      sizeof(USBD_PIPE_INFORMATION));

        HubExtension->ConfigHandle = Urb->UrbSelectConfiguration.ConfigurationHandle;
    }

    ExFreePool(Urb);

    return Status;
}

NTSTATUS
NTAPI
USBD_Initialize20Hub(IN PUSBHUB_FDO_EXTENSION HubExtension)
{
    PUSB_BUSIFFN_INITIALIZE_20HUB Initialize20Hub;
    ULONG TtCount;
    PUSB_DEVICE_HANDLE DeviceHandle;

    DPRINT("USBD_InitUsb2Hub ... \n");

    Initialize20Hub = HubExtension->BusInterface.Initialize20Hub;

    if (!Initialize20Hub)
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    TtCount = 1;

    if (HubExtension->HubFlags & USBHUB_FDO_FLAG_MULTIPLE_TTS)
    {
        TtCount = HubExtension->HubDescriptor->bNumberOfPorts;
    }

    DeviceHandle = USBH_SyncGetDeviceHandle(HubExtension->LowerDevice);

    return Initialize20Hub(HubExtension->BusInterface.BusContext,
                           DeviceHandle,
                           TtCount);
}

NTSTATUS
NTAPI
USBH_StartHubFdoDevice(IN PUSBHUB_FDO_EXTENSION HubExtension,
                       IN PIRP Irp)
{
    NTSTATUS Status;
    WCHAR KeyName[64];
    PVOID DisableRemoteWakeup = NULL;
    ULONG HubCount = 0;
    PUSB_DEVICE_HANDLE DeviceHandle;
    USB_DEVICE_TYPE DeviceType;
    DEVICE_CAPABILITIES  DeviceCapabilities;
    BOOLEAN IsBusPowered;

    DPRINT("USBH_StartHubFdoDevice: ... \n");

    KeInitializeEvent(&HubExtension->LowerDeviceEvent, NotificationEvent, FALSE);
    KeInitializeEvent(&HubExtension->RootHubNotificationEvent,
                      NotificationEvent,
                      TRUE);

    HubExtension->HubFlags = 0;
    HubExtension->HubConfigDescriptor = NULL;
    HubExtension->HubDescriptor = NULL;
    HubExtension->SCEIrp = NULL;
    HubExtension->HubBuffer = NULL;

    IoCopyCurrentIrpStackLocationToNext(Irp);

    IoSetCompletionRoutine(Irp,
                           USBH_HubPnPIrpComplete,
                           HubExtension,
                           TRUE,
                           TRUE,
                           TRUE);

    if (IoCallDriver(HubExtension->LowerDevice, Irp) == STATUS_PENDING)
    {
        KeWaitForSingleObject(&HubExtension->LowerDeviceEvent,
                              Suspended,
                              KernelMode,
                              FALSE,
                              NULL);
    }

    HubExtension->RootHubPdo = NULL;

    Status = USBH_SyncGetRootHubPdo(HubExtension->LowerDevice,
                                    &HubExtension->RootHubPdo,
                                    &HubExtension->RootHubPdo2);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBH_SyncGetRootHubPdo() failed - %p\n", Status);
        goto ErrorExit;
    }

    USBH_WriteFailReasonID(HubExtension->LowerPDO, 5);

    if (HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_FAILED)
    {
        DPRINT1("USBH_StartHubFdoDevice: USBHUB_FDO_FLAG_DEVICE_FAILED - TRUE\n");
        Status = STATUS_UNSUCCESSFUL;
        goto ErrorExit;
    }

    HubExtension->HubFlags |= USBHUB_FDO_FLAG_REMOTE_WAKEUP;

    swprintf(KeyName, L"DisableRemoteWakeup");

    Status = USBD_GetPdoRegistryParameter(HubExtension->LowerPDO,
                                          &DisableRemoteWakeup,
                                          sizeof(DisableRemoteWakeup),
                                          KeyName,
                                          (wcslen(KeyName) + 1) * sizeof(WCHAR));

    if (NT_SUCCESS(Status) && DisableRemoteWakeup)
    {
        DPRINT("USBH_StartHubFdoDevice: DisableRemoteWakeup - TRUE\n");
        HubExtension->HubFlags &= ~USBHUB_FDO_FLAG_REMOTE_WAKEUP;
    }

    HubExtension->CurrentPowerState.DeviceState = PowerDeviceD0;

    USBH_SyncGetHubCount(HubExtension->LowerDevice,
                         &HubCount);

    Status = USBHUB_GetBusInterface(HubExtension->RootHubPdo,
                                    &HubExtension->BusInterface);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBH_StartHubFdoDevice: USBHUB_GetBusInterface() failed - %p\n",
                Status);
        goto ErrorExit;
    }

    Status = USBHUB_GetBusInterfaceUSBDI(HubExtension->LowerDevice,
                                         &HubExtension->BusInterfaceUSBDI);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBH_StartHubFdoDevice: USBHUB_GetBusInterfaceUSBDI() failed - %p\n",
                Status);
        goto ErrorExit;
    }

    DeviceHandle = USBH_SyncGetDeviceHandle(HubExtension->LowerDevice);

    if (DeviceHandle)
    {
        Status = USBH_GetDeviceType(HubExtension, DeviceHandle, &DeviceType);

        if (!NT_SUCCESS(Status))
        {
            DPRINT1("USBH_StartHubFdoDevice: USBH_GetDeviceType() failed - %p\n", Status);
            goto ErrorExit;
        }

        if (DeviceType == Usb20Device)
        {
            HubExtension->HubFlags |= USBHUB_FDO_FLAG_USB20_HUB;
        }
    }

    if (HubCount > 6)
    {
        DPRINT1("USBH_StartHubFdoDevice: HubCount > 6 - %x\n", HubCount);
        USBH_WriteFailReasonID(HubExtension->LowerPDO, 6);
        HubExtension->HubFlags |= USBHUB_FDO_FLAG_DEVICE_FAILED;
        DbgBreakPoint();
    }

    RtlZeroMemory(&DeviceCapabilities, sizeof(DEVICE_CAPABILITIES));

    USBH_QueryCapabilities(HubExtension->LowerDevice, &DeviceCapabilities);

    //RtlFillMemory(HubExtension->DeviceState, POWER_SYSTEM_MAXIMUM, PowerDeviceD3);

    HubExtension->SystemWake = DeviceCapabilities.SystemWake;
    HubExtension->DeviceWake = DeviceCapabilities.DeviceWake;

    RtlCopyMemory(HubExtension->DeviceState,
                  &DeviceCapabilities.DeviceState,
                  POWER_SYSTEM_MAXIMUM * sizeof(DEVICE_POWER_STATE));

    Status = USBH_GetDeviceDescriptor(HubExtension->Common.SelfDevice,
                                      &HubExtension->HubDeviceDescriptor);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBH_StartHubFdoDevice: USBH_GetDeviceDescriptor() failed - %p\n",
                Status);
        goto ErrorExit;
    }

    Status = USBH_GetConfigurationDescriptor(HubExtension->Common.SelfDevice,
                                             &HubExtension->HubConfigDescriptor);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBH_StartHubFdoDevice: USBH_GetConfigurationDescriptor() failed - %p\n",
                Status);
        goto ErrorExit;
    }

    Status = USBH_SyncGetHubDescriptor(HubExtension);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBH_StartHubFdoDevice: USBH_SyncGetHubDescriptor() failed - %p\n",
                Status);
        goto ErrorExit;
    }

    IsBusPowered = USBH_HubIsBusPowered(HubExtension->Common.SelfDevice,
                                        HubExtension->HubConfigDescriptor);

    if (IsBusPowered)
    {
        HubExtension->MaxPower = 100;
        HubExtension->HubConfigDescriptor->MaxPower = 250; // 500 mA
    }
    else
    {
        HubExtension->MaxPower = 500;
    }

    Status = USBH_OpenConfiguration(HubExtension);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBH_StartHubFdoDevice: USBH_OpenConfiguration() failed - %p\n",
                Status);
        goto ErrorExit;
    }

    if (HubExtension->HubFlags & USBHUB_FDO_FLAG_USB20_HUB)
    {
        Status = USBD_Initialize20Hub(HubExtension);
    }

    if (!NT_SUCCESS(Status))
    {
        goto ErrorExit;
    }

    HubExtension->SCEIrp = IoAllocateIrp(HubExtension->Common.SelfDevice->StackSize,
                                         FALSE);

    HubExtension->ResetPortIrp = IoAllocateIrp(HubExtension->Common.SelfDevice->StackSize,
                                               FALSE);

    if (!HubExtension->SCEIrp || !HubExtension->ResetPortIrp)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorExit;
    }

    HubExtension->HubBufferLength = HubExtension->PipeInfo.MaximumPacketSize;

    HubExtension->HubBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                                    HubExtension->HubBufferLength,
                                                    USB_HUB_TAG);

    if (!HubExtension->HubBuffer)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorExit;
    }

    Status = USBH_SyncPowerOnPorts(HubExtension);

    if (!NT_SUCCESS(Status))
    {
        goto ErrorExit;
    }
    else
    {
        USHORT Port;

        HubExtension->HubFlags |= USBHUB_FDO_FLAG_DEVICE_STARTED;

        Port = 1;

        if (HubExtension->HubDescriptor->bNumberOfPorts >= 1)
        {
            do
            {
                USBH_SyncClearPortStatus(HubExtension,
                                         Port++,
                                         USBHUB_FEATURE_C_PORT_CONNECTION);
            }
            while (Port <= HubExtension->HubDescriptor->bNumberOfPorts);
        }
    }

    if (HubExtension->LowerPDO == HubExtension->RootHubPdo)
    {
        USBD_RegisterRootHubCallBack(HubExtension);
    }
    else
    {
        HubExtension->HubFlags |= USBHUB_FDO_FLAG_DO_ENUMERATION;
        DbgBreakPoint();
    }

    goto Exit;

  ErrorExit:

    if (HubExtension->HubDescriptor)
    {
        ExFreePool(HubExtension->HubDescriptor);
        HubExtension->HubDescriptor = NULL;
    }

    if (HubExtension->SCEIrp)
    {
        IoFreeIrp(HubExtension->SCEIrp);
        HubExtension->SCEIrp = NULL;
    }

    if (HubExtension->ResetPortIrp)
    {
        IoFreeIrp(HubExtension->ResetPortIrp);
        HubExtension->ResetPortIrp = NULL;
    }

    if (HubExtension->HubBuffer)
    {
        ExFreePool(HubExtension->HubBuffer);
        HubExtension->HubBuffer = NULL;
    }

    if (HubExtension->HubConfigDescriptor)
    {
        ExFreePool(HubExtension->HubConfigDescriptor);
        HubExtension->HubConfigDescriptor = NULL;
    }

  Exit:

    USBH_CompleteIrp(Irp, Status);

    return Status;
}

NTSTATUS
NTAPI
USBH_FdoStartDevice(IN PUSBHUB_FDO_EXTENSION HubExtension,
                    IN PIRP Irp)
{
    NTSTATUS Status;

    DPRINT("USBH_FdoStartDevice: ... \n");

    HubExtension->RootHubPdo = NULL;

    Status = USBH_SyncGetRootHubPdo(HubExtension->LowerDevice,
                                    &HubExtension->RootHubPdo,
                                    &HubExtension->RootHubPdo2);

    if (NT_SUCCESS(Status))
    {
        if (HubExtension->RootHubPdo)
        {
            Status = USBH_StartHubFdoDevice(HubExtension, Irp);
        }
        else
        {
            DPRINT1("USBH_FdoStartDevice: FIXME. start ParentDevice\n");
            DbgBreakPoint();
        }
    }
    else
    {
        DPRINT1("USBH_FdoStartDevice: FIXME. USBH_SyncGetRootHubPdo return - %p\n",
                Status);

        DbgBreakPoint();
        USBH_CompleteIrp(Irp, Status);
    }

    return Status;
}

NTSTATUS
NTAPI
USBH_FdoPnP(IN PUSBHUB_FDO_EXTENSION HubExtension,
            IN PIRP Irp,
            IN UCHAR Minor)
{
    NTSTATUS Status=0;

    DPRINT("USBH_FdoPnP: HubExtension - %p, Irp - %p, Minor - %x\n",
           HubExtension,
           Irp,
           Minor);

    switch (Minor)
    {
        case IRP_MN_START_DEVICE: // 0
            DPRINT("IRP_MN_START_DEVICE\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Status = USBH_FdoStartDevice(HubExtension, Irp);
            break;

        case IRP_MN_QUERY_REMOVE_DEVICE: // 1
            DPRINT("IRP_MN_QUERY_REMOVE_DEVICE\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_REMOVE_DEVICE: // 2
            DPRINT("IRP_MN_REMOVE_DEVICE\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_CANCEL_REMOVE_DEVICE: // 3
            DPRINT("IRP_MN_CANCEL_REMOVE_DEVICE\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_STOP_DEVICE: // 4
            DPRINT("IRP_MN_STOP_DEVICE\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_STOP_DEVICE: // 5
            DPRINT("IRP_MN_QUERY_STOP_DEVICE\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_CANCEL_STOP_DEVICE: // 6
            DPRINT("IRP_MN_CANCEL_STOP_DEVICE\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_DEVICE_RELATIONS: // 7
            DPRINT("IRP_MN_QUERY_DEVICE_RELATIONS\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_INTERFACE: // 8
            DPRINT("IRP_MN_QUERY_INTERFACE\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_CAPABILITIES:           // 9
            DPRINT("IRP_MN_QUERY_CAPABILITIES\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_RESOURCES:              // 10
            DPRINT("IRP_MN_QUERY_RESOURCES\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:  // 11
            DPRINT("IRP_MN_QUERY_RESOURCE_REQUIREMENTS\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_DEVICE_TEXT:            // 12
            DPRINT("IRP_MN_QUERY_DEVICE_TEXT\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: // 13
            DPRINT("IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_READ_CONFIG: // 15
            DPRINT("IRP_MN_READ_CONFIG\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_WRITE_CONFIG: // 16
            DPRINT("IRP_MN_WRITE_CONFIG\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_EJECT: // 17
            DPRINT("IRP_MN_EJECT\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_SET_LOCK: // 18
            DPRINT("IRP_MN_SET_LOCK\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_ID: // 19
            DPRINT("IRP_MN_QUERY_ID\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_PNP_DEVICE_STATE: // 20
            DPRINT("IRP_MN_QUERY_PNP_DEVICE_STATE\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_QUERY_BUS_INFORMATION: // 21
            DPRINT("IRP_MN_QUERY_BUS_INFORMATION\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_DEVICE_USAGE_NOTIFICATION: // 22
            DPRINT("IRP_MN_DEVICE_USAGE_NOTIFICATION\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        case IRP_MN_SURPRISE_REMOVAL: // 23
            DPRINT("IRP_MN_SURPRISE_REMOVAL\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;

        default:
            DPRINT("unknown IRP_MN_???\n");
            Status = USBH_PassIrp(HubExtension->LowerDevice, Irp);
            break;
    }

    return Status;
}
