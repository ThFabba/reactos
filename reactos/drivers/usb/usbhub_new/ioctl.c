#include "usbhub.h"

//#define NDEBUG
#include <debug.h>

NTSTATUS
NTAPI
USBH_SelectConfigOrInterfaceComplete(IN PDEVICE_OBJECT DeviceObject,
                                     IN PIRP Irp,
                                     IN PVOID Context)
{
    PUSBHUB_PORT_PDO_EXTENSION PortExtension;
    PUSBHUB_FDO_EXTENSION HubExtension;
    PVOID TimeoutContext; // PUSBHUB_BANDWIDTH_TIMEOUT_CONTEXT
    PUSBHUB_PORT_DATA PortData = NULL;
    NTSTATUS Status;
    KIRQL OldIrql;

    DPRINT("USBH_SelectConfigOrInterfaceComplete ... \n");

    PortData;

    if (Irp->PendingReturned)
    {
         IoMarkIrpPending(Irp);
    }

    PortExtension = (PUSBHUB_PORT_PDO_EXTENSION)Context;
    HubExtension = PortExtension->HubExtension;

    if (HubExtension)
    {
        PortData = &HubExtension->PortData[PortExtension->PortNumber - 1];
    }

    Status = Irp->IoStatus.Status;

    if (Irp->IoStatus.Status == STATUS_SUCCESS)
    {
        KeAcquireSpinLock(&PortExtension->PortTimeoutSpinLock, &OldIrql);

        TimeoutContext = PortExtension->BndwTimeoutContext;

        if (TimeoutContext)
        {
            DPRINT1("USBH_SelectConfigOrInterfaceComplete: TimeoutContext != NULL. FIXME. \n");
            DbgBreakPoint();
        }

        KeReleaseSpinLock(&PortExtension->PortTimeoutSpinLock, OldIrql);

        PortExtension->PortPdoFlags &= ~(USBHUB_PDO_FLAG_PORT_RESTORE_FAIL |
                                         USBHUB_PDO_FLAG_ALLOC_BNDW_FAILED);

        if (PortData && PortData->ConnectionStatus != DeviceHubNestedTooDeeply)
        {
            PortData->ConnectionStatus = DeviceConnected;
        }
    }
    else
    {
        DPRINT1("USBH_SelectConfigOrInterfaceComplete: Status != STATUS_SUCCESS. FIXME. \n");
        DbgBreakPoint();
    }

    return Status;
}

NTSTATUS
NTAPI
USBH_PdoUrbFilter(IN PUSBHUB_PORT_PDO_EXTENSION PortExtension, 
                  IN PIRP Irp)
{
    PUSB_CONFIGURATION_DESCRIPTOR ConfigDescriptor;
    PUSBHUB_FDO_EXTENSION HubExtension;
    PDEVICE_OBJECT DeviceObject;
    PIO_STACK_LOCATION IoStack;
    PURB Urb;
    USHORT Function;
    ULONG MaxPower;
    USBD_STATUS UrbStatus;
    BOOLEAN IsValidConfig;

    HubExtension = PortExtension->HubExtension;
    DeviceObject = PortExtension->Common.SelfDevice;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    Urb = (PURB)IoStack->Parameters.Others.Argument1;

    DPRINT("USBH_PdoUrbFilter: Device - %p, Irp - %p, Urb - %p\n",
           DeviceObject,
           Irp,
           Urb);

    if (PortExtension->PortPdoFlags & (USBHUB_PDO_FLAG_PORT_RESTORE_FAIL |
                                       USBHUB_PDO_FLAG_PORT_RESSETING))
    {
        Urb->UrbHeader.Status = USBD_STATUS_INVALID_PARAMETER;
        USBH_CompleteIrp(Irp, STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    Function = Urb->UrbHeader.Function;

    if (Function == URB_FUNCTION_SELECT_CONFIGURATION)
    {
        ConfigDescriptor = Urb->UrbSelectConfiguration.ConfigurationDescriptor;

        if (ConfigDescriptor)
        {
            IsValidConfig = TRUE;

            if (ConfigDescriptor->bDescriptorType != USB_CONFIGURATION_DESCRIPTOR_TYPE)
            {
                IsValidConfig = FALSE;
                UrbStatus = USBD_STATUS_INVALID_CONFIGURATION_DESCRIPTOR;
            }

            if (ConfigDescriptor->bLength < sizeof(USB_CONFIGURATION_DESCRIPTOR))
            {
                IsValidConfig = FALSE;
                UrbStatus = USBD_STATUS_INVALID_CONFIGURATION_DESCRIPTOR;
            }

            if (!IsValidConfig)
            {
                Urb->UrbHeader.Status = UrbStatus;
                USBH_CompleteIrp(Irp, STATUS_INVALID_PARAMETER);
                return STATUS_INVALID_PARAMETER;
            }

            MaxPower = 2 * ConfigDescriptor->MaxPower;
            PortExtension->MaxPower = MaxPower;

            if (HubExtension->MaxPower < MaxPower)
            {
                DPRINT1("USBH_PdoUrbFilter: USBH_InvalidatePortDeviceState() UNIMPLEMENTED. FIXME. \n");

                DbgBreakPoint();
                PortExtension->PortPdoFlags |= USBHUB_PDO_FLAG_INSUFFICIENT_PWR;
                //USBH_InvalidatePortDeviceState()
                USBH_CompleteIrp(Irp, STATUS_INVALID_PARAMETER);
                return STATUS_INVALID_PARAMETER;
            }
        }
    }

    if (Function == URB_FUNCTION_SELECT_CONFIGURATION ||
        Function == URB_FUNCTION_SELECT_INTERFACE)
    {
        IoCopyCurrentIrpStackLocationToNext(Irp);

        IoSetCompletionRoutine(Irp,
                               USBH_SelectConfigOrInterfaceComplete,
                               PortExtension,
                               TRUE,
                               TRUE,
                               TRUE);

        return IoCallDriver(HubExtension->RootHubPdo2, Irp);
    }

    if (Function == URB_FUNCTION_ABORT_PIPE ||
        Function == URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL ||
        Function == URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL ||
        Function == URB_FUNCTION_GET_FRAME_LENGTH ||
        Function == URB_FUNCTION_SET_FRAME_LENGTH ||
        Function == URB_FUNCTION_GET_CURRENT_FRAME_NUMBER)
    {
        return USBH_PassIrp(HubExtension->RootHubPdo2, Irp);
    }

    if (Function == URB_FUNCTION_CONTROL_TRANSFER ||
        Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER ||
        Function == URB_FUNCTION_ISOCH_TRANSFER)
    {
        if (PortExtension->PortPdoFlags & USBHUB_PDO_FLAG_DELETE_PENDING)
        {
            Urb->UrbHeader.Status = USBD_STATUS_INVALID_PARAMETER;
            USBH_CompleteIrp(Irp, STATUS_DELETE_PENDING);
            return STATUS_DELETE_PENDING;
        }

        return USBH_PassIrp(HubExtension->RootHubPdo2, Irp);
    }

    if (Function != URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR)
    {
        return USBH_PassIrp(HubExtension->RootHubPdo2, Irp);
    }
    else
    {
        DPRINT1("USBH_PdoUrbFilter: URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR UNIMPLEMENTED. FIXME. \n");
        DbgBreakPoint();
        USBH_CompleteIrp(Irp, STATUS_NOT_IMPLEMENTED);
        return STATUS_NOT_IMPLEMENTED;
    }
}

NTSTATUS
NTAPI
USBH_PdoIoctlSubmitUrb(IN PUSBHUB_PORT_PDO_EXTENSION PortExtension,
                       IN PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;
    PUSBHUB_FDO_EXTENSION HubExtension;
    PURB Urb;
    NTSTATUS Status;

    DPRINT("USBH_PdoIoctlSubmitUrb ... \n");

    HubExtension = (PUSBHUB_FDO_EXTENSION)PortExtension->HubExtension;
    IoStack = Irp->Tail.Overlay.CurrentStackLocation;

    Urb = (PURB)IoStack->Parameters.Others.Argument1;

    if (PortExtension->DeviceHandle == NULL)
    {
        Urb->UrbHeader.UsbdDeviceHandle = (PVOID)-1;
        Status = USBH_PassIrp(HubExtension->RootHubPdo2, Irp);
    }
    else
    {
        Urb->UrbHeader.UsbdDeviceHandle = PortExtension->DeviceHandle;
        Status = USBH_PdoUrbFilter(PortExtension, Irp);
    }

    return Status;
}

NTSTATUS
NTAPI
USBH_IoctlGetNodeInformation(IN PUSBHUB_FDO_EXTENSION HubExtension,
                             IN PIRP Irp)
{
    PUSB_NODE_INFORMATION NodeInfo;
    PIO_STACK_LOCATION IoStack;
    ULONG BufferLength;
    NTSTATUS Status;
    BOOLEAN HubIsBusPowered;

    DPRINT("USBH_IoctlGetNodeInformation ... \n");

    Status = STATUS_SUCCESS;

    NodeInfo = (PUSB_NODE_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    BufferLength = IoStack->Parameters.DeviceIoControl.OutputBufferLength;

    RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer, BufferLength);

    if (BufferLength < sizeof(USB_NODE_INFORMATION))
    {
        USBH_CompleteIrp(Irp, STATUS_BUFFER_TOO_SMALL);
        return STATUS_BUFFER_TOO_SMALL;
    }

    NodeInfo->NodeType = UsbHub;

    RtlCopyMemory(&NodeInfo->u.HubInformation.HubDescriptor,
                  HubExtension->HubDescriptor,
                  sizeof(USB_HUB_DESCRIPTOR));

    HubIsBusPowered = USBH_HubIsBusPowered(HubExtension->Common.SelfDevice,
                                           HubExtension->HubConfigDescriptor);

    NodeInfo->u.HubInformation.HubIsBusPowered = HubIsBusPowered;

    Irp->IoStatus.Information = sizeof(USB_NODE_INFORMATION);

    USBH_CompleteIrp(Irp, Status);

    return Status;
}

NTSTATUS
NTAPI
USBH_IoctlGetHubCapabilities(IN PUSBHUB_FDO_EXTENSION HubExtension,
                             IN PIRP Irp)
{
    PUSB_HUB_CAPABILITIES Capabilities;
    PIO_STACK_LOCATION IoStack;
    ULONG BufferLength;
    ULONG Length;
    ULONG HubIs2xCapable = 0;

    DPRINT("USBH_IoctlGetHubCapabilities ... \n");

    Capabilities = (PUSB_HUB_CAPABILITIES)Irp->AssociatedIrp.SystemBuffer;

    HubIs2xCapable = HubExtension->HubFlags & USBHUB_FDO_FLAG_USB20_HUB;

    IoStack = IoGetCurrentIrpStackLocation(Irp);

    BufferLength = IoStack->Parameters.DeviceIoControl.OutputBufferLength;

    if (BufferLength <= sizeof(HubIs2xCapable))
    {
        Length = IoStack->Parameters.DeviceIoControl.OutputBufferLength;
    }
    else
    {
        Length = sizeof(HubIs2xCapable);
    }

    RtlZeroMemory(Capabilities, BufferLength);
    RtlCopyMemory(Capabilities, &HubIs2xCapable, Length);

    Irp->IoStatus.Information = Length;

    USBH_CompleteIrp(Irp, STATUS_SUCCESS);

    return 0;
}

NTSTATUS
NTAPI
USBH_IoctlGetNodeConnectionAttributes(IN PUSBHUB_FDO_EXTENSION HubExtension,
                                      IN PIRP Irp)
{
    PUSB_NODE_CONNECTION_ATTRIBUTES Attributes;
    ULONG ConnectionIndex;
    ULONG NumPorts;
    ULONG Port;
    NTSTATUS Status;
    PUSBHUB_PORT_DATA PortData;
    PIO_STACK_LOCATION IoStack;
    ULONG BufferLength;

    DPRINT("USBH_IoctlGetNodeConnectionAttributes ... \n");

    PortData = HubExtension->PortData;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    BufferLength = IoStack->Parameters.DeviceIoControl.OutputBufferLength;
    Attributes = (PUSB_NODE_CONNECTION_ATTRIBUTES)Irp->AssociatedIrp.SystemBuffer;

    if (BufferLength < sizeof(USB_NODE_CONNECTION_ATTRIBUTES))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        USBH_CompleteIrp(Irp, Status);
        return Status;
    }

    ConnectionIndex = Attributes->ConnectionIndex;
    RtlZeroMemory(Attributes, BufferLength);
    Attributes->ConnectionIndex = ConnectionIndex;

    Status = STATUS_INVALID_PARAMETER;

    NumPorts = HubExtension->HubDescriptor->bNumberOfPorts;
    Port = 1;

    if (NumPorts > 0)
    {
        while (Port != ConnectionIndex)
        {
            ++PortData;
            ++Port;

            if (Port > NumPorts)
            {
                goto Exit;
            }
        }

        Attributes->ConnectionStatus = PortData->ConnectionStatus;
        Attributes->PortAttributes = PortData->PortAttributes;

        Irp->IoStatus.Information = sizeof(USB_NODE_CONNECTION_ATTRIBUTES);
        Status = STATUS_SUCCESS;
    }

Exit:

    USBH_CompleteIrp(Irp, Status);
    return Status;
}

NTSTATUS
NTAPI
USBH_IoctlGetNodeConnectionInformation(IN PUSBHUB_FDO_EXTENSION HubExtension,
                                       IN PIRP Irp,
                                       IN BOOLEAN IsExt)
{
    PUSBHUB_PORT_DATA PortData;
    ULONG BufferLength;
    PUSB_NODE_CONNECTION_INFORMATION_EX Info;
    ULONG ConnectionIndex;
    ULONG NumPorts;
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObject;
    PUSBHUB_PORT_PDO_EXTENSION PortExtension;
    ULONG Port;
    PIO_STACK_LOCATION IoStack;

    DPRINT("USBH_IoctlGetNodeConnectionInformation ... \n");

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    Info = (PUSB_NODE_CONNECTION_INFORMATION_EX)Irp->AssociatedIrp.SystemBuffer;
    BufferLength = IoStack->Parameters.DeviceIoControl.OutputBufferLength;

    PortData = HubExtension->PortData;

    if (BufferLength < (sizeof(USB_NODE_CONNECTION_INFORMATION_EX) - sizeof(USB_PIPE_INFO)))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto Exit;
    }

    ConnectionIndex = Info->ConnectionIndex;
    RtlZeroMemory(Info, BufferLength);
    Info->ConnectionIndex = ConnectionIndex;

    NumPorts = HubExtension->HubDescriptor->bNumberOfPorts;

    Status = STATUS_INVALID_PARAMETER;

    Port = 1;

    if (NumPorts >= 1)
    {
        while (Port != Info->ConnectionIndex)
        {
            ++PortData;
            ++Port;

            if (Port > NumPorts)
            {
                goto Exit;
            }
        }

        DeviceObject = PortData->DeviceObject;

        if (DeviceObject)
        {
            PortExtension = (PUSBHUB_PORT_PDO_EXTENSION)DeviceObject->DeviceExtension;

            Info->ConnectionStatus = PortData->ConnectionStatus;
            Info->DeviceIsHub = PortExtension->PortPdoFlags & USBHUB_PDO_FLAG_HUB_DEVICE;

            RtlCopyMemory(&Info->DeviceDescriptor,
                          &PortExtension->DeviceDescriptor,
                          sizeof(USB_DEVICE_DESCRIPTOR));

            if (PortExtension->DeviceHandle)
            {
                Status = USBD_GetDeviceInformationEx(PortExtension,
                                                     HubExtension,
                                                     Info,
                                                     BufferLength,
                                                     PortExtension->DeviceHandle);
            }
            else
            {
                Status = STATUS_SUCCESS;
            }

            if (NT_SUCCESS(Status))
            {
                if (!IsExt)
                {
                    Info->Speed = Info->Speed == FALSE;
                }

                Irp->IoStatus.Information = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) +
                                            (Info->NumberOfOpenPipes - 1) * sizeof(USB_PIPE_INFO);
                goto Exit;
            }

            if (Status != STATUS_BUFFER_TOO_SMALL)
            {
                goto Exit;
            }
        }
        else
        {
            Info->ConnectionStatus = PortData->ConnectionStatus;
        }

        Irp->IoStatus.Information = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) -
                                    sizeof(USB_PIPE_INFO);

        Status = STATUS_SUCCESS;
    }

Exit:
    USBH_CompleteIrp(Irp, Status);
    return Status;
}

NTSTATUS
NTAPI
USBH_IoctlGetNodeConnectionDriverKeyName(IN PUSBHUB_FDO_EXTENSION HubExtension,
                                         IN PIRP Irp)
{
    PUSBHUB_PORT_DATA PortData;
    PDEVICE_OBJECT PortDevice;
    ULONG Length;
    ULONG ResultLength;
    ULONG Port;
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    ULONG BufferLength;
    PUSB_NODE_CONNECTION_DRIVERKEY_NAME KeyName;
    PUSBHUB_PORT_PDO_EXTENSION PortExtension;

    DPRINT("USBH_IoctlGetNodeConnectionDriverKeyName ... \n");

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    KeyName = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)Irp->AssociatedIrp.SystemBuffer;
    BufferLength = IoStack->Parameters.DeviceIoControl.OutputBufferLength;

    if (BufferLength < sizeof(USB_NODE_CONNECTION_DRIVERKEY_NAME))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        USBH_CompleteIrp(Irp, Status);
        return Status;
    }

    Status = STATUS_INVALID_PARAMETER;

    if (!HubExtension->HubDescriptor->bNumberOfPorts)
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        USBH_CompleteIrp(Irp, Status);
        return Status;
    }

    Port = 1;

    do
    {
        if (Port == KeyName->ConnectionIndex)
        {
            PortData = &HubExtension->PortData[KeyName->ConnectionIndex - 1];

            PortDevice = PortData->DeviceObject;

            if (PortDevice)
            {
                PortExtension = (PUSBHUB_PORT_PDO_EXTENSION)PortDevice->DeviceExtension;

                if (PortExtension->PortPdoFlags & 0x04000000)
                {
                    ResultLength = BufferLength - sizeof(USB_NODE_CONNECTION_DRIVERKEY_NAME);

                    Status = IoGetDeviceProperty(PortDevice,
                                                 DevicePropertyDriverKeyName,
                                                 BufferLength - sizeof(USB_NODE_CONNECTION_DRIVERKEY_NAME),
                                                 &KeyName->DriverKeyName,
                                                 &ResultLength);

                    if (Status == STATUS_BUFFER_TOO_SMALL)
                    {
                        Status = STATUS_SUCCESS;
                    }

                    Length = ResultLength + sizeof(USB_NODE_CONNECTION_DRIVERKEY_NAME);

                    KeyName->ActualLength = Length;

                    if (BufferLength < Length)
                    {
                        KeyName->DriverKeyName[0] = 0;
                        Irp->IoStatus.Information = sizeof(USB_NODE_CONNECTION_DRIVERKEY_NAME);
                    }
                    else
                    {
                        Irp->IoStatus.Information = Length;
                    }
                }
                else
                {
                    Status = STATUS_INVALID_DEVICE_STATE;
                }
            }
        }

        ++Port;
    }
    while (Port <= HubExtension->HubDescriptor->bNumberOfPorts);

    USBH_CompleteIrp(Irp, Status);

    return Status;
}

NTSTATUS
NTAPI
USBH_IoctlGetDescriptor(IN PUSBHUB_FDO_EXTENSION HubExtension,
                        IN PIRP Irp)
{
    ULONG BufferLength;
    PUSBHUB_PORT_DATA PortData;
    PUSB_DESCRIPTOR_REQUEST UsbRequest;
    ULONG Port;
    PDEVICE_OBJECT PortDevice;
    PUSBHUB_PORT_PDO_EXTENSION PortExtension;
    struct _URB_CONTROL_TRANSFER * Urb;
    NTSTATUS Status;
    ULONG RequestBufferLength;
    PIO_STACK_LOCATION IoStack;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    BufferLength = IoStack->Parameters.DeviceIoControl.OutputBufferLength;
    PortData = HubExtension->PortData;

    DPRINT("USBH_IoctlGetDescriptor: BufferLength - %x\n", BufferLength);

    if (BufferLength < sizeof(USB_DESCRIPTOR_REQUEST))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
        USBH_CompleteIrp(Irp, Status);
        return Status;
    }

    UsbRequest = (PUSB_DESCRIPTOR_REQUEST)Irp->AssociatedIrp.SystemBuffer;

    RequestBufferLength = UsbRequest->SetupPacket.wLength;

    if (RequestBufferLength > BufferLength - 
                              (sizeof(USB_DESCRIPTOR_REQUEST) - sizeof(UCHAR)))
    {
        DPRINT("USBH_IoctlGetDescriptor: RequestBufferLength - %x\n",
               RequestBufferLength);

        Status = STATUS_BUFFER_TOO_SMALL;
        USBH_CompleteIrp(Irp, Status);
        return Status;
    }

    Port = 1;

    Status = STATUS_INVALID_PARAMETER;

    if (HubExtension->HubDescriptor->bNumberOfPorts >= 1)
    {
        while (Port != UsbRequest->ConnectionIndex)
        {
            ++PortData;
            ++Port;

            if (Port > HubExtension->HubDescriptor->bNumberOfPorts)
            {
                goto Exit;
            }
        }

        PortDevice = PortData->DeviceObject;

        if (PortDevice)
        {
            PortExtension = (PUSBHUB_PORT_PDO_EXTENSION)PortDevice->DeviceExtension;

            if (UsbRequest->SetupPacket.bmRequest == USB_CONFIGURATION_DESCRIPTOR_TYPE &&
                RequestBufferLength == sizeof(USB_CONFIGURATION_DESCRIPTOR))
            {
                Status = STATUS_SUCCESS;

                RtlCopyMemory(&UsbRequest->Data[0],
                              &PortExtension->ConfigDescriptor,
                              sizeof(USB_CONFIGURATION_DESCRIPTOR));

                Irp->IoStatus.Information = sizeof(USB_DESCRIPTOR_REQUEST) - sizeof(UCHAR) +
                                            sizeof(USB_CONFIGURATION_DESCRIPTOR);
            }
            else
            {
                Urb = ExAllocatePoolWithTag(NonPagedPool,
                                            sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                                            USB_HUB_TAG);

                if (Urb)
                {
                    Urb->Hdr.Function = URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE;
                    Urb->Hdr.Length = sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);

                    Urb->TransferBuffer = &UsbRequest->Data[0];
                    Urb->TransferBufferLength = RequestBufferLength;
                    Urb->TransferBufferMDL = 0;
                    Urb->UrbLink = 0;

                    RtlCopyMemory(Urb->SetupPacket,
                                  &UsbRequest->SetupPacket,
                                  sizeof(USB_DEFAULT_PIPE_SETUP_PACKET));

                    Status = USBH_SyncSubmitUrb(PortExtension->Common.SelfDevice,
                                                (PURB)Urb);

                    Irp->IoStatus.Information = (sizeof(USB_DESCRIPTOR_REQUEST) - sizeof(UCHAR)) +
                                                Urb->TransferBufferLength;

                    ExFreePool(Urb);
                }
                else
                {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                }
            }
        }
    }

Exit:

    USBH_CompleteIrp(Irp, Status);
    return Status;
}

NTSTATUS
NTAPI
USBH_DeviceControl(IN PUSBHUB_FDO_EXTENSION HubExtension,
                   IN PIRP Irp)
{
    NTSTATUS Status = STATUS_DEVICE_BUSY;
    PIO_STACK_LOCATION IoStack;
    ULONG ControlCode;
    BOOLEAN IsCheckHubIdle = FALSE; 

    DPRINT("USBH_DeviceControl: HubExtension - %p, Irp - %p\n",
           HubExtension,
           Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ControlCode = IoStack->Parameters.DeviceIoControl.IoControlCode;
    DPRINT("USBH_DeviceControl: ControlCode - %p\n", ControlCode);

    if ((HubExtension->CurrentPowerState.DeviceState != PowerDeviceD0) &&
        (HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STARTED))
    {
        IsCheckHubIdle = TRUE;
        USBH_HubSetD0(HubExtension);
    }

    switch (ControlCode)
    {
        case IOCTL_USB_GET_HUB_CAPABILITIES:
            DPRINT("USBH_DeviceControl: IOCTL_USB_GET_HUB_CAPABILITIES. \n");
            if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STOPPED))
            {
                Status = USBH_IoctlGetHubCapabilities(HubExtension, Irp);
                break;
            }

            USBH_CompleteIrp(Irp, Status);
            break;

        case IOCTL_USB_HUB_CYCLE_PORT:
            DPRINT("USBH_DeviceControl: IOCTL_USB_HUB_CYCLE_PORT. \n");
            DPRINT1("USBH_DeviceControl: UNIMPLEMENTED. FIXME. \n");
            DbgBreakPoint();
            break;

        case IOCTL_USB_GET_NODE_INFORMATION:
            DPRINT("USBH_DeviceControl: IOCTL_USB_GET_NODE_INFORMATION. \n");
            if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STOPPED))
            {
                Status = USBH_IoctlGetNodeInformation(HubExtension, Irp);
                break;
            }

            USBH_CompleteIrp(Irp, Status);
            break;

        case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION:
            DPRINT("USBH_DeviceControl: IOCTL_USB_GET_NODE_CONNECTION_INFORMATION. \n");
            if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STOPPED))
            {
                Status = USBH_IoctlGetNodeConnectionInformation(HubExtension,
                                                                Irp,
                                                                FALSE);
                break;
            }

            USBH_CompleteIrp(Irp, Status);
            break;

        case IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX:
            DPRINT("USBH_DeviceControl: IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX. \n");
            if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STOPPED))
            {
                Status = USBH_IoctlGetNodeConnectionInformation(HubExtension,
                                                                Irp,
                                                                TRUE);
                break;
            }

            USBH_CompleteIrp(Irp, Status);
            break;

        case IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES:
            DPRINT("USBH_DeviceControl: IOCTL_USB_GET_NODE_CONNECTION_ATTRIBUTES. \n");
            if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STOPPED))
            {
                Status = USBH_IoctlGetNodeConnectionAttributes(HubExtension, Irp);
                break;
            }

            USBH_CompleteIrp(Irp, Status);
            break;

        case IOCTL_USB_GET_NODE_CONNECTION_NAME:
            DPRINT("USBH_DeviceControl: IOCTL_USB_GET_NODE_CONNECTION_NAME. \n");
            DPRINT1("USBH_DeviceControl: UNIMPLEMENTED. FIXME. \n");
            DbgBreakPoint();
            break;

        case IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME:
            DPRINT("USBH_DeviceControl: IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME. \n");
            if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STOPPED))
            {
                Status = USBH_IoctlGetNodeConnectionDriverKeyName(HubExtension, Irp);
                break;
            }

            USBH_CompleteIrp(Irp, Status);
            break;

        case IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION:
            DPRINT("USBH_DeviceControl: IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION. \n");
            if (!(HubExtension->HubFlags & USBHUB_FDO_FLAG_DEVICE_STOPPED))
            {
                Status = USBH_IoctlGetDescriptor(HubExtension, Irp);
                break;
            }

            USBH_CompleteIrp(Irp, Status);
            break;

        case 0x2F0003:
            DPRINT("USBH_DeviceControl: IOCTL_KS_PROPERTY. \n");
            Status = STATUS_INVALID_DEVICE_REQUEST;
            USBH_CompleteIrp(Irp, Status);
            break;

        default:
            DPRINT("USBH_DeviceControl: IOCTL_ ???\n");
            Status = USBH_PassIrp(HubExtension->RootHubPdo, Irp);
            break;
    }

    if (IsCheckHubIdle)
    {
        USBH_CheckHubIdle(HubExtension);
    }

    return Status;
}

NTSTATUS
NTAPI
USBH_PdoInternalControl(IN PUSBHUB_PORT_PDO_EXTENSION PortExtension,
                        IN PIRP Irp)
{
    PUSBHUB_FDO_EXTENSION HubExtension;
    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    ULONG ControlCode;
    PIO_STACK_LOCATION IoStack;

    DPRINT("USBH_PdoInternalControl: PortExtension - %p, Irp - %p\n",
           PortExtension,
           Irp);

    HubExtension = PortExtension->HubExtension;

    if (PortExtension->PortPdoFlags & USBHUB_PDO_FLAG_NOT_CONNECTED)
    {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto Exit;
    }

    if (PortExtension->CurrentPowerState.DeviceState != PowerDeviceD0)
    {
        Status = STATUS_DEVICE_POWERED_OFF;
        goto Exit;
    }

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ControlCode = IoStack->Parameters.DeviceIoControl.IoControlCode;
    DPRINT("USBH_PdoInternalControl: ControlCode - %p\n", ControlCode);

    if (ControlCode == IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO)
    {
        HubExtension = PortExtension->RootHubExtension;
        DPRINT("USBH_PdoInternalControl: HubExtension - %p\n", HubExtension);
    }

    if (!HubExtension)
    {
        Status = STATUS_DEVICE_BUSY;
        goto Exit;
    }

    switch (ControlCode)
    {
        case IOCTL_INTERNAL_USB_SUBMIT_URB:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_SUBMIT_URB. \n");
            return USBH_PdoIoctlSubmitUrb(PortExtension, Irp);

        case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION. \n");
            break;

        case IOCTL_INTERNAL_USB_GET_PORT_STATUS:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_GET_PORT_STATUS. \n");
            break;

        case IOCTL_INTERNAL_USB_RESET_PORT:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_RESET_PORT. \n");
            break;

        case IOCTL_INTERNAL_USB_ENABLE_PORT:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_ENABLE_PORT. \n");
            break;

        case IOCTL_INTERNAL_USB_CYCLE_PORT:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_CYCLE_PORT. \n");
            break;

        case IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_GET_DEVICE_HANDLE. \n");
            break;

        case IOCTL_INTERNAL_USB_GET_HUB_COUNT:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_GET_HUB_COUNT. \n");
            break;

        case IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_GET_ROOTHUB_PDO. \n");
            break;

        case IOCTL_INTERNAL_USB_GET_HUB_NAME:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_GET_HUB_NAME. \n");
            break;

        case IOCTL_GET_HCD_DRIVERKEY_NAME:
            DPRINT("USBH_PdoInternalControl: IOCTL_GET_HCD_DRIVERKEY_NAME. \n");
            break;

        case IOCTL_INTERNAL_USB_GET_BUS_INFO:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_GET_BUS_INFO. \n");
            break;

        case IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO:
            DPRINT("USBH_PdoInternalControl: IOCTL_INTERNAL_USB_GET_PARENT_HUB_INFO. \n");
            break;

        default:
            DPRINT("USBH_PdoInternalControl: IOCTL_ ???\n");
            break;
    }

Exit:
    USBH_CompleteIrp(Irp, Status);
    return Status;
}
