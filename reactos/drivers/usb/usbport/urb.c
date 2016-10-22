#include "usbport.h"

#define NDEBUG
#include <debug.h>

#define NDEBUG_USBPORT_URB
#include "usbdebug.h"

NTSTATUS
NTAPI
USBPORT_ResetPipe(IN PDEVICE_OBJECT FdoDevice,
                  IN PIRP Irp,
                  IN PURB Urb)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    PUSBPORT_PIPE_HANDLE PipeHandle;
    PUSBPORT_ENDPOINT Endpoint;
    KIRQL OldIrql;
    NTSTATUS Status;

    DPRINT_URB("USBPORT_ResetPipe: ... \n");

    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;
    PipeHandle = (PUSBPORT_PIPE_HANDLE)Urb->UrbPipeRequest.PipeHandle;

    if (!USBPORT_ValidatePipeHandle((PUSBPORT_DEVICE_HANDLE)Urb->UrbHeader.UsbdDeviceHandle, PipeHandle))
        return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_INVALID_PIPE_HANDLE);

    Endpoint = PipeHandle->Endpoint;

    KeAcquireSpinLock(&Endpoint->EndpointSpinLock, &Endpoint->EndpointOldIrql);

    if (IsListEmpty(&Endpoint->TransferList))
    {
        if (Urb->UrbHeader.UsbdFlags & 0x00000010)
        {
            KeAcquireSpinLock(&FdoExtension->MiniportSpinLock, &OldIrql);

            FdoExtension->MiniPortInterface->Packet.SetEndpointDataToggle(FdoExtension->MiniPortExt,
                                                                          (PVOID)((ULONG_PTR)Endpoint + sizeof(USBPORT_ENDPOINT)),
                                                                          0);

            KeReleaseSpinLock(&FdoExtension->MiniportSpinLock, OldIrql);
        }

        Status = USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_SUCCESS);
    }
    else
    {
        Status = USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_ERROR_BUSY);
    }

    Endpoint->Flags |= ENDPOINT_FLAG_QUEUENE_EMPTY;

    KeAcquireSpinLock(&FdoExtension->MiniportSpinLock, &OldIrql);

    FdoExtension->MiniPortInterface->Packet.SetEndpointStatus(FdoExtension->MiniPortExt,
                                                              (PVOID)((ULONG_PTR)Endpoint + sizeof(USBPORT_ENDPOINT)),
                                                              0);

    KeReleaseSpinLock(&FdoExtension->MiniportSpinLock, OldIrql);
    KeReleaseSpinLock(&Endpoint->EndpointSpinLock, Endpoint->EndpointOldIrql);

    return Status;
}

NTSTATUS
NTAPI
USBPORT_ClearStall(IN PDEVICE_OBJECT FdoDevice,
                   IN PIRP Irp,
                   IN PURB Urb)
{
    PUSBPORT_DEVICE_HANDLE DeviceHandle;
    PUSBPORT_PIPE_HANDLE PipeHandle;
    USBD_STATUS USBDStatus;
    PUSBPORT_ENDPOINT Endpoint;
    NTSTATUS Status;
    USB_DEFAULT_PIPE_SETUP_PACKET SetupPacket;

    DPRINT_URB("USBPORT_ClearStall: ... \n");

    PipeHandle = (PUSBPORT_PIPE_HANDLE)Urb->UrbPipeRequest.PipeHandle;
    DeviceHandle = (PUSBPORT_DEVICE_HANDLE)Urb->UrbHeader.UsbdDeviceHandle;

    if (!USBPORT_ValidatePipeHandle(DeviceHandle, PipeHandle))
        return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_INVALID_PIPE_HANDLE);

    Endpoint = PipeHandle->Endpoint;

    RtlZeroMemory(&SetupPacket, sizeof(USB_DEFAULT_PIPE_SETUP_PACKET));

    SetupPacket.bmRequestType._BM.Recipient = BMREQUEST_TO_ENDPOINT;
    SetupPacket.bRequest = USB_REQUEST_CLEAR_FEATURE;
    SetupPacket.wValue.W = 0;
    SetupPacket.wIndex.W = Endpoint->EndpointProperties.EndpointAddress;
    SetupPacket.wLength = 0;

    USBPORT_SendSetupPacket(DeviceHandle,
                            FdoDevice,
                            &SetupPacket,
                            NULL,
                            0,
                            NULL,
                            &USBDStatus);

    Status = USBPORT_USBDStatusToNtStatus(Urb, USBDStatus);

    return Status;
}

NTSTATUS
NTAPI
USBPORT_SyncResetPipeAndClearStall(IN PDEVICE_OBJECT FdoDevice,
                                   IN PIRP Irp,
                                   IN PURB Urb)
{
    PUSBPORT_DEVICE_HANDLE DeviceHandle;
    PUSBPORT_PIPE_HANDLE PipeHandle;
    PUSBPORT_ENDPOINT Endpoint;
    ULONG EndpointState;
    NTSTATUS Status;

    DPRINT_URB("USBPORT_SyncResetPipeAndClearStall: ... \n");

    ASSERT(Urb->UrbHeader.UsbdDeviceHandle);
    ASSERT(Urb->UrbHeader.Length == sizeof(struct _URB_PIPE_REQUEST));
    ASSERT(Urb->UrbPipeRequest.PipeHandle);

    DeviceHandle = (PUSBPORT_DEVICE_HANDLE)Urb->UrbHeader.UsbdDeviceHandle;
    PipeHandle = (PUSBPORT_PIPE_HANDLE)Urb->UrbPipeRequest.PipeHandle;

    if (!USBPORT_ValidatePipeHandle(DeviceHandle, PipeHandle))
        return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_INVALID_PIPE_HANDLE);

    if (PipeHandle->Flags & 2)
        return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_SUCCESS);

    Endpoint = PipeHandle->Endpoint;
    InterlockedIncrement(&DeviceHandle->DeviceHandleLock);

    if (Endpoint->EndpointProperties.TransferType != USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
    {
        Urb->UrbHeader.UsbdFlags |= 0x00000010;
        Status = USBPORT_ClearStall(FdoDevice, Irp, Urb);
    }
    else
    {
        Status = USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_SUCCESS);
    }

    if (NT_SUCCESS(Status))
    {
        Status = USBPORT_ResetPipe(FdoDevice, Irp, Urb);

        if (Endpoint->EndpointProperties.TransferType == USBPORT_TRANSFER_TYPE_ISOCHRONOUS)
        {
            while (TRUE)
            {
                KeAcquireSpinLock(&Endpoint->EndpointSpinLock, &Endpoint->EndpointOldIrql);

                EndpointState = Endpoint->StateLast;

                if (EndpointState == USBPORT_ENDPOINT_PAUSED &&
                    IsListEmpty(&Endpoint->TransferList))
                {
                    USBPORT_SetEndpointState(Endpoint, USBPORT_ENDPOINT_ACTIVE);
                }

                KeReleaseSpinLock(&Endpoint->EndpointSpinLock, Endpoint->EndpointOldIrql);

                if (EndpointState == USBPORT_ENDPOINT_ACTIVE)
                    break;

                USBPORT_Wait(FdoDevice, 1);
            }
        }
    }

    InterlockedDecrement(&DeviceHandle->DeviceHandleLock);

    return Status;
}

NTSTATUS
NTAPI
USBPORT_HandleSetOrClearFeature(IN PDEVICE_OBJECT FdoDevice,
                                IN PIRP Irp,
                                IN PURB Urb)
{
    DPRINT1("USBPORT_HandleSetOrClearFeature: UNIMPLEMENTED. FIXME. \n");
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
USBPORT_HandleDataTransfers(IN PURB Urb)
{
    PUSBPORT_ENDPOINT Endpoint;

    DPRINT_URB("USBPORT_HandleDataTransfers: Urb - %p\n", Urb);

    Endpoint = ((PUSBPORT_PIPE_HANDLE)(Urb->UrbBulkOrInterruptTransfer.PipeHandle))->Endpoint;

    if (Endpoint->EndpointProperties.TransferType != USBPORT_TRANSFER_TYPE_CONTROL)
    {
        if (Endpoint->EndpointProperties.Direction)
            Urb->UrbBulkOrInterruptTransfer.TransferFlags &= ~USBD_TRANSFER_DIRECTION_IN; // ~1
        else
            Urb->UrbBulkOrInterruptTransfer.TransferFlags |= USBD_TRANSFER_DIRECTION_IN; // 1
    }

    USBPORT_QueueTransferUrb(Urb);

    return STATUS_PENDING;
}

NTSTATUS
NTAPI
USBPORT_HandleGetStatus(IN PIRP Irp,
                        IN PURB Urb)
{
    PUSB_DEFAULT_PIPE_SETUP_PACKET SetupPacket = (PUSB_DEFAULT_PIPE_SETUP_PACKET)&Urb->UrbControlDescriptorRequest.Reserved1;
    NTSTATUS Status;

    SetupPacket->bmRequestType.B = 0;
    SetupPacket->bmRequestType._BM.Dir = BMREQUEST_DEVICE_TO_HOST;
    SetupPacket->bRequest = USB_REQUEST_GET_STATUS;
    SetupPacket->wLength = Urb->UrbControlDescriptorRequest.TransferBufferLength;
    SetupPacket->wValue.W = 0;

    switch (Urb->UrbHeader.Function)
    {
        case URB_FUNCTION_GET_STATUS_FROM_DEVICE: // 0x13
            DPRINT_URB("USBPORT_HandleGetStatus: URB_FUNCTION_GET_STATUS_FROM_DEVICE\n");
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_DEVICE;
            break;

        case URB_FUNCTION_GET_STATUS_FROM_INTERFACE: // 0x14
            DPRINT_URB("USBPORT_HandleGetStatus: URB_FUNCTION_GET_STATUS_FROM_INTERFACE\n");
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_INTERFACE;
            break;

        case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT: // 0x15
            DPRINT_URB("USBPORT_HandleGetStatus: URB_FUNCTION_GET_STATUS_FROM_ENDPOINT\n");
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_ENDPOINT;
            break;

        case URB_FUNCTION_GET_STATUS_FROM_OTHER: // 0x21
            DPRINT_URB("USBPORT_HandleGetStatus: URB_FUNCTION_GET_STATUS_FROM_OTHER\n");
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_OTHER;
            break;
    }

    if (SetupPacket->wLength == 2)
    {
        Urb->UrbControlTransfer.TransferFlags |= USBD_SHORT_TRANSFER_OK; // 2

        if (SetupPacket->bmRequestType._BM.Dir)
            Urb->UrbControlTransfer.TransferFlags |= USBD_TRANSFER_DIRECTION_IN; // 1;
        else
            Urb->UrbControlTransfer.TransferFlags &= ~USBD_TRANSFER_DIRECTION_IN; // ~1;

        DPRINT_URB("GetStatus: SetupPacket->bmRequestType.B - %x\n",
               SetupPacket->bmRequestType.B);

        DPRINT_URB("GetStatus: SetupPacket->bRequest        - %x\n",
               SetupPacket->bRequest);

        DPRINT_URB("GetStatus: SetupPacket->wIndex.W        - %x\n",
               SetupPacket->wIndex.W);

        DPRINT_URB("GetStatus: SetupPacket->wLength         - %x\n",
               SetupPacket->wLength);

        USBPORT_QueueTransferUrb(Urb);

        Status = STATUS_PENDING;
    }
    else
    {
        Status = USBPORT_USBDStatusToNtStatus(Urb,
                                              USBD_STATUS_INVALID_PARAMETER);

        DPRINT1("USBPORT_HandleGetStatus: Bad wLength\n");
    }

    return Status;
}

NTSTATUS
NTAPI
USBPORT_HandleVendorOrClass(IN PIRP Irp,
                            IN PURB Urb)
{
    PUSB_DEFAULT_PIPE_SETUP_PACKET SetupPacket = (PUSB_DEFAULT_PIPE_SETUP_PACKET)&Urb->UrbControlDescriptorRequest.Reserved1;

    // Specifies a value, from 4 to 31 inclusive, that becomes part of the request type code in the USB-defined setup packet.
    // This value is defined by USB for a class request or the vendor for a vendor request.

    SetupPacket->bmRequestType._BM.Dir = Urb->UrbControlTransfer.TransferFlags & 1;
    SetupPacket->wLength = Urb->UrbControlDescriptorRequest.TransferBufferLength;

    Urb->UrbControlTransfer.TransferFlags |= USBD_SHORT_TRANSFER_OK; // 2

    switch (Urb->UrbHeader.Function)
    {
        case URB_FUNCTION_VENDOR_DEVICE: // 0x17
            DPRINT_URB("USBPORT_HandleVendorOrClass: URB_FUNCTION_VENDOR_DEVICE\n");
            SetupPacket->bmRequestType._BM.Type = BMREQUEST_VENDOR;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_DEVICE;
            break;

        case URB_FUNCTION_VENDOR_INTERFACE: // 0x18
            DPRINT_URB("USBPORT_HandleVendorOrClass: URB_FUNCTION_VENDOR_INTERFACE\n");
            SetupPacket->bmRequestType._BM.Type = BMREQUEST_VENDOR;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_INTERFACE;
            break;

        case URB_FUNCTION_VENDOR_ENDPOINT: // 0x19
            DPRINT_URB("USBPORT_HandleVendorOrClass: URB_FUNCTION_VENDOR_ENDPOINT\n");
            SetupPacket->bmRequestType._BM.Type = BMREQUEST_VENDOR;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_ENDPOINT;
            break;

        case URB_FUNCTION_CLASS_DEVICE: // 0x1A
            DPRINT_URB("USBPORT_HandleVendorOrClass: URB_FUNCTION_CLASS_DEVICE\n");
            SetupPacket->bmRequestType._BM.Type = BMREQUEST_CLASS;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_DEVICE;
            break;

        case URB_FUNCTION_CLASS_INTERFACE: // 0x1B
            DPRINT_URB("USBPORT_HandleVendorOrClass: URB_FUNCTION_CLASS_INTERFACE\n");
            SetupPacket->bmRequestType._BM.Type = BMREQUEST_CLASS;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_INTERFACE;
            break;

        case URB_FUNCTION_CLASS_ENDPOINT: // 0x1C
            DPRINT_URB("USBPORT_HandleVendorOrClass: URB_FUNCTION_CLASS_ENDPOINT\n");
            SetupPacket->bmRequestType._BM.Type = BMREQUEST_CLASS;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_ENDPOINT;
            break;

        case URB_FUNCTION_CLASS_OTHER: // 0x1F
            DPRINT_URB("USBPORT_HandleVendorOrClass: URB_FUNCTION_CLASS_OTHER\n");
            SetupPacket->bmRequestType._BM.Type = BMREQUEST_CLASS;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_OTHER;
            break;

        case URB_FUNCTION_VENDOR_OTHER: // 0x20
            DPRINT_URB("USBPORT_HandleVendorOrClass: URB_FUNCTION_VENDOR_OTHER\n");
            SetupPacket->bmRequestType._BM.Type = BMREQUEST_VENDOR;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_OTHER;
            break;
    }

    DPRINT_URB("VendorOrClass: bmRequestType.B - %x\n", SetupPacket->bmRequestType.B);
    DPRINT_URB("VendorOrClass: bRequest        - %x\n", SetupPacket->bRequest);
    DPRINT_URB("VendorOrClass: wValue.LowByte  - %x\n", SetupPacket->wValue.LowByte);
    DPRINT_URB("VendorOrClass: wValue.HiByte   - %x\n", SetupPacket->wValue.HiByte);
    DPRINT_URB("VendorOrClass: wIndex.W        - %x\n", SetupPacket->wIndex.W);
    DPRINT_URB("VendorOrClass: wLength         - %x\n", SetupPacket->wLength);

    USBPORT_QueueTransferUrb(Urb);

    return STATUS_PENDING;
}

NTSTATUS
NTAPI
USBPORT_HandleGetSetDescriptor(IN PIRP Irp,
                               IN PURB Urb)
{
    PUSB_DEFAULT_PIPE_SETUP_PACKET SetupPacket = (PUSB_DEFAULT_PIPE_SETUP_PACKET)&Urb->UrbControlDescriptorRequest.Reserved1;

    SetupPacket->wLength = Urb->UrbControlDescriptorRequest.TransferBufferLength;
    SetupPacket->bmRequestType.B = 0; // Clear bmRequestType
    SetupPacket->bmRequestType._BM.Type = BMREQUEST_STANDARD;

    switch (Urb->UrbHeader.Function)
    {
        case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE: // 0x0B
            DPRINT_URB("USBPORT_HandleGetSetDescriptor: URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE\n");
            SetupPacket->bRequest = USB_REQUEST_GET_DESCRIPTOR;
            SetupPacket->bmRequestType._BM.Dir = BMREQUEST_DEVICE_TO_HOST;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_DEVICE;
            break;

        case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE: // 0x0C
            DPRINT_URB("USBPORT_HandleGetSetDescriptor: URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE\n");
            SetupPacket->bRequest = USB_REQUEST_SET_DESCRIPTOR;
            SetupPacket->bmRequestType._BM.Dir = BMREQUEST_HOST_TO_DEVICE;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_DEVICE;
            break;

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT: // 0x24
            DPRINT_URB("USBPORT_HandleGetSetDescriptor: URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT\n");
            SetupPacket->bRequest = USB_REQUEST_GET_DESCRIPTOR;
            SetupPacket->bmRequestType._BM.Dir = BMREQUEST_DEVICE_TO_HOST;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_ENDPOINT;
            break;

        case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT: // 0x25
            DPRINT_URB("USBPORT_HandleGetSetDescriptor: URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT\n");
            SetupPacket->bRequest = USB_REQUEST_SET_DESCRIPTOR;
            SetupPacket->bmRequestType._BM.Dir = BMREQUEST_HOST_TO_DEVICE;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_ENDPOINT;
            break;

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE: // 0x28
            DPRINT_URB("USBPORT_HandleGetSetDescriptor: URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE\n");
            SetupPacket->bRequest = USB_REQUEST_GET_DESCRIPTOR;
            SetupPacket->bmRequestType._BM.Dir = BMREQUEST_DEVICE_TO_HOST;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_INTERFACE;
            break;

        case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE: // 0x29
            DPRINT_URB("USBPORT_HandleGetSetDescriptor: URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE\n");
            SetupPacket->bRequest = USB_REQUEST_SET_DESCRIPTOR;
            SetupPacket->bmRequestType._BM.Dir = BMREQUEST_HOST_TO_DEVICE;
            SetupPacket->bmRequestType._BM.Recipient = BMREQUEST_TO_INTERFACE;
            break;
    }

    Urb->UrbControlTransfer.TransferFlags |= USBD_SHORT_TRANSFER_OK; // 2

    if (SetupPacket->bmRequestType._BM.Dir)
        Urb->UrbControlTransfer.TransferFlags |= USBD_TRANSFER_DIRECTION_IN; // 1;
    else
        Urb->UrbControlTransfer.TransferFlags &= ~USBD_TRANSFER_DIRECTION_IN; // ~1;

    DPRINT_URB("GetSetDescriptor: SetupPacket->bmRequestType.B - %x\n",
           SetupPacket->bmRequestType.B); // 0x80

    DPRINT_URB("GetSetDescriptor: SetupPacket->bRequest        - %x\n",
           SetupPacket->bRequest); // USB_REQUEST_GET_DESCRIPTOR

    DPRINT_URB("GetSetDescriptor: SetupPacket->wValue.LowByte  - %x\n",
           SetupPacket->wValue.LowByte); // Urb->UrbControlDescriptorRequest.Index;

    DPRINT_URB("GetSetDescriptor: SetupPacket->wValue.HiByte   - %x\n",
           SetupPacket->wValue.HiByte); // Urb->UrbControlDescriptorRequest.DescriptorType;

    DPRINT_URB("GetSetDescriptor: SetupPacket->wIndex.W        - %x\n",
           SetupPacket->wIndex.W); // Urb->UrbControlDescriptorRequest.LanguageId;

    DPRINT_URB("GetSetDescriptor: SetupPacket->wLength         - %x\n",
           SetupPacket->wLength); // Urb->UrbControlDescriptorRequest.TransferBufferLength;

    USBPORT_QueueTransferUrb(Urb);

    return STATUS_PENDING;
}

NTSTATUS
NTAPI
USBPORT_ValidateTransferParametersURB(IN PURB Urb)
{
    struct _URB_CONTROL_TRANSFER *UrbRequest;
    PMDL Mdl;

    DPRINT_URB("USBPORT_ValidateTransferParametersURB: Urb - %p\n", Urb);

    UrbRequest = &Urb->UrbControlTransfer;

    if (UrbRequest->TransferBuffer == NULL &&
        UrbRequest->TransferBufferMDL == NULL &&
        UrbRequest->TransferBufferLength > 0)
    {
        DPRINT1("USBPORT_ValidateTransferParametersURB: Not valid parameter\n");
        return STATUS_INVALID_PARAMETER;
    }

    if ((UrbRequest->TransferBuffer > 0 || UrbRequest->TransferBufferMDL > 0) &&
        UrbRequest->TransferBufferLength == 0)
    {
        DPRINT1("USBPORT_ValidateTransferParametersURB: Not valid parameter\n");
        return STATUS_INVALID_PARAMETER;
    }

    if (UrbRequest->TransferBuffer != NULL &&
        UrbRequest->TransferBufferMDL == NULL &&
        UrbRequest->TransferBufferLength != 0)
    {
        DPRINT_URB("USBPORT_ValidateTransferParametersURB: TransferBuffer - %p, TransferBufferLength - %x\n",
                   UrbRequest->TransferBuffer,
                   UrbRequest->TransferBufferLength);

        Mdl = IoAllocateMdl(UrbRequest->TransferBuffer,
                            UrbRequest->TransferBufferLength,
                            FALSE,
                            FALSE,
                            NULL);

        if (!Mdl)
        {
            DPRINT1("USBPORT_ValidateTransferParametersURB: Not allocated Mdl\n");
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        MmBuildMdlForNonPagedPool(Mdl);

        UrbRequest->TransferBufferMDL = Mdl;
        Urb->UrbHeader.UsbdFlags |= USBD_FLAG_ALLOCATED_MDL;

        DPRINT_URB("USBPORT_ValidateTransferParametersURB: Mdl - %p\n", Mdl);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
USBPORT_ValidateURB(IN PDEVICE_OBJECT FdoDevice,
                    IN PIRP Irp,
                    IN PURB Urb,
                    IN BOOLEAN IsControlTransfer,
                    IN BOOLEAN IsNullTransfer)
{
    struct _URB_CONTROL_TRANSFER *UrbRequest;
    PUSBPORT_DEVICE_HANDLE DeviceHandle;
    NTSTATUS Status;
    USBD_STATUS USBDStatus;

    UrbRequest = &Urb->UrbControlTransfer;

    if (UrbRequest->UrbLink)
    {
        Status = USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_INVALID_PARAMETER);
        DPRINT1("USBPORT_ValidateURB: Not valid parameter\n");
        return Status;
    }

    DeviceHandle = (PUSBPORT_DEVICE_HANDLE)Urb->UrbHeader.UsbdDeviceHandle;

    if (IsControlTransfer)
    {
        UrbRequest->TransferFlags |= USBD_DEFAULT_PIPE_TRANSFER;
        UrbRequest->PipeHandle = &DeviceHandle->PipeHandle;
    }

    if (UrbRequest->TransferFlags & USBD_DEFAULT_PIPE_TRANSFER)
    {
        if (UrbRequest->TransferBufferLength > 0x1000)
        {
            Status = USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_INVALID_PARAMETER);
            DPRINT1("USBPORT_ValidateURB: Not valid parameter\n");
            return Status;
        }

        if (Urb->UrbHeader.Function == URB_FUNCTION_CONTROL_TRANSFER)
        {
            UrbRequest->PipeHandle = &DeviceHandle->PipeHandle;
        }
    }

    if (!USBPORT_ValidatePipeHandle(DeviceHandle, UrbRequest->PipeHandle))
    {
        Status = USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_INVALID_PIPE_HANDLE);
        DPRINT1("USBPORT_ValidateURB: Not valid pipe handle\n");
        return Status;
    }

    UrbRequest->hca.Reserved8[0] = NULL; // Transfer

    if (IsNullTransfer)
    {
        UrbRequest->TransferBuffer = 0;
        UrbRequest->TransferBufferMDL = NULL;
        UrbRequest->TransferBufferLength = 0;
    }
    else
    {
        Status = USBPORT_ValidateTransferParametersURB(Urb);

        if (!NT_SUCCESS(Status))
        {
            return Status;
        }
    }

    USBDStatus = USBPORT_AllocateTransfer(FdoDevice,
                                          Urb,
                                          DeviceHandle,
                                          Irp,
                                          NULL);

    Status = USBPORT_USBDStatusToNtStatus(Urb, USBDStatus);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBPORT_ValidateURB: Not allocated transfer\n");
    }

    return Status;
}

NTSTATUS
NTAPI
USBPORT_HandleSubmitURB(IN PDEVICE_OBJECT PdoDevice,
                        IN PIRP Irp,
                        IN PURB Urb)
{
    PUSBPORT_RHDEVICE_EXTENSION PdoExtension;
    PDEVICE_OBJECT FdoDevice;
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    USHORT Function;
    PUSBPORT_DEVICE_HANDLE DeviceHandle;
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;

    ASSERT(Urb);

    PdoExtension = (PUSBPORT_RHDEVICE_EXTENSION)PdoDevice->DeviceExtension;
    FdoDevice = PdoExtension->FdoDevice;
    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;

    Urb->UrbHeader.Status = USBD_STATUS_SUCCESS;
    Urb->UrbHeader.UsbdFlags = 0;

    Function = Urb->UrbHeader.Function;

    if (Function > URB_FUNCTION_MAX)
    {
        Status = USBPORT_USBDStatusToNtStatus(Urb,
                                              USBD_STATUS_INVALID_URB_FUNCTION);

        DPRINT1("USBPORT_HandleSubmitURB: Unknown URB function - %x !!!\n",
               Function);

        return Status;
    }

    if (FdoExtension->TimerFlags & USBPORT_TMFLAG_RH_SUSPENDED)
    {
        DPRINT1("USBPORT_HandleSubmitURB: Bad Request\n");

        USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_DEVICE_GONE);

        Irp->IoStatus.Status = STATUS_PENDING;
        IoMarkIrpPending(Irp);
        IoCsqInsertIrp(&FdoExtension->BadRequestIoCsq, Irp, NULL);

        return STATUS_PENDING;
    }

    DeviceHandle = (PUSBPORT_DEVICE_HANDLE)Urb->UrbHeader.UsbdDeviceHandle;

    if (!DeviceHandle)
    {
        DeviceHandle = &PdoExtension->DeviceHandle;
        Urb->UrbHeader.UsbdDeviceHandle = DeviceHandle;
    }

    if (!USBPORT_ValidateDeviceHandle(PdoExtension->FdoDevice,
                                      DeviceHandle))
    {
        DPRINT1("USBPORT_HandleSubmitURB: Not valid device handle\n");

        Irp->IoStatus.Status = STATUS_PENDING;
        IoMarkIrpPending(Irp);
        IoCsqInsertIrp(&FdoExtension->BadRequestIoCsq, Irp, NULL);

        return STATUS_PENDING;
    }

    InterlockedIncrement(&DeviceHandle->DeviceHandleLock);

    DPRINT_URB("USBPORT_HandleSubmitURB: Function - 0x%02X, DeviceHandle - %p\n",
               Function,
               Urb->UrbHeader.UsbdDeviceHandle);

    switch (Function)
    {
        case URB_FUNCTION_ISOCH_TRANSFER: // 0x10
            DPRINT1("USBPORT_HandleSubmitURB: URB_FUNCTION_ISOCH_TRANSFER UNIMPLEMENTED. FIXME. \n");
            break;

        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER: // 0x09
        case URB_FUNCTION_CONTROL_TRANSFER: // 0x08
            Status = USBPORT_ValidateURB(FdoDevice, Irp, Urb, FALSE, FALSE);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("USBPORT_HandleSubmitURB: Not valid URB\n");
                return Status;
            }

            Status = USBPORT_HandleDataTransfers(Urb);
            break;

        case URB_FUNCTION_VENDOR_DEVICE: // 0x17
        case URB_FUNCTION_VENDOR_INTERFACE: // 0x18
        case URB_FUNCTION_VENDOR_ENDPOINT: // 0x19
        case URB_FUNCTION_CLASS_DEVICE: // 0x1A
        case URB_FUNCTION_CLASS_INTERFACE: // 0x1B
        case URB_FUNCTION_CLASS_ENDPOINT: // 0x1C
        case URB_FUNCTION_CLASS_OTHER: // 0x1F
        case URB_FUNCTION_VENDOR_OTHER: // 0x20
            Status = USBPORT_ValidateURB(FdoDevice, Irp, Urb, TRUE, FALSE);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("USBPORT_HandleSubmitURB: Not valid URB\n");
                return Status;
            }

            Status = USBPORT_HandleVendorOrClass(Irp, Urb);
            break;

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE: // 0x0B
        case URB_FUNCTION_SET_DESCRIPTOR_TO_DEVICE: // 0x0C
        case URB_FUNCTION_GET_DESCRIPTOR_FROM_ENDPOINT: // 0x24
        case URB_FUNCTION_SET_DESCRIPTOR_TO_ENDPOINT: // 0x25
        case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE: // 0x28
        case URB_FUNCTION_SET_DESCRIPTOR_TO_INTERFACE: // 0x29
            Status = USBPORT_ValidateURB(FdoDevice, Irp, Urb, TRUE, FALSE);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("USBPORT_HandleSubmitURB: Not valid URB\n");
                return Status;
            }

            Status = USBPORT_HandleGetSetDescriptor(Irp, Urb);
            break;

        case URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR: // 0x2A
            DPRINT1("USBPORT_HandleSubmitURB: URB_FUNCTION_GET_MS_FEATURE_DESCRIPTOR (0x2A) NOT_SUPPORTED\n");
            return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_INVALID_URB_FUNCTION);

        case URB_FUNCTION_GET_STATUS_FROM_DEVICE: // 0x13
        case URB_FUNCTION_GET_STATUS_FROM_INTERFACE: // 0x14
        case URB_FUNCTION_GET_STATUS_FROM_ENDPOINT: // 0x15
        case URB_FUNCTION_GET_STATUS_FROM_OTHER: // 0x21
            Status = USBPORT_ValidateURB(FdoDevice, Irp, Urb, TRUE, FALSE);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("USBPORT_HandleSubmitURB: Not valid URB\n");
                return Status;
            }

            Status = USBPORT_HandleGetStatus(Irp, Urb);
            break;

        case URB_FUNCTION_SELECT_CONFIGURATION: // 0x00
            Status = USBPORT_HandleSelectConfiguration(PdoExtension->FdoDevice,
                                                       Irp,
                                                       Urb);
            break;

        case URB_FUNCTION_SELECT_INTERFACE: // 0x01
            Status = USBPORT_HandleSelectInterface(PdoExtension->FdoDevice,
                                                   Irp,
                                                   Urb);
            break;

        case URB_FUNCTION_GET_CONFIGURATION: // 0x26
            DPRINT1("USBPORT_HandleSubmitURB: URB_FUNCTION_GET_CONFIGURATION (0x26) UNIMPLEMENTED. FIXME\n");
            break;

        case URB_FUNCTION_GET_INTERFACE: // 0x27
            DPRINT1("USBPORT_HandleSubmitURB: URB_FUNCTION_GET_INTERFACE (0x27) NOT_SUPPORTED\n");
            return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_INVALID_URB_FUNCTION);

        case URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL: // 0x1E
            Status = USBPORT_SyncResetPipeAndClearStall(PdoExtension->FdoDevice,
                                                        Irp,
                                                        Urb);
            break;

        case URB_FUNCTION_SYNC_RESET_PIPE: // 0x30
            Status = USBPORT_ResetPipe(PdoExtension->FdoDevice,
                                       Irp,
                                       Urb);
            break;

        case URB_FUNCTION_SYNC_CLEAR_STALL: // 0x31
            Status = USBPORT_ClearStall(PdoExtension->FdoDevice,
                                        Irp,
                                        Urb);
            break;

        case URB_FUNCTION_ABORT_PIPE: // 0x02
            Status = USBPORT_AbortPipe(PdoExtension->FdoDevice,
                                       Irp,
                                       Urb);
            break;

        case URB_FUNCTION_SET_FEATURE_TO_DEVICE: // 0x0D
        case URB_FUNCTION_SET_FEATURE_TO_INTERFACE: // 0x0E
        case URB_FUNCTION_SET_FEATURE_TO_ENDPOINT: // 0x0F
        case URB_FUNCTION_CLEAR_FEATURE_TO_INTERFACE: // 0x11
        case URB_FUNCTION_CLEAR_FEATURE_TO_ENDPOINT: // 0x12
        case URB_FUNCTION_CLEAR_FEATURE_TO_OTHER: // 0x22
        case URB_FUNCTION_SET_FEATURE_TO_OTHER: // 0x23
            Status = USBPORT_ValidateURB(FdoDevice, Irp, Urb, TRUE, TRUE);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("USBPORT_HandleSubmitURB: Not valid URB\n");
                return Status;
            }

            Status = USBPORT_HandleSetOrClearFeature(PdoExtension->FdoDevice,
                                                     Irp,
                                                     Urb);
            break;

        case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER: // 0x07
            Status = USBPORT_HandleGetCurrentFrame(PdoExtension->FdoDevice,
                                                   Irp,
                                                   Urb);
            break;

        case URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL: // 0x03
            DPRINT1("USBPORT_HandleSubmitURB: URB_FUNCTION_TAKE_FRAME_LENGTH_CONTROL (0x03) NOT_SUPPORTED\n");
            return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_NOT_SUPPORTED);

        case URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL: // 0x04
            DPRINT1("USBPORT_HandleSubmitURB: URB_FUNCTION_RELEASE_FRAME_LENGTH_CONTROL (0x04) NOT_SUPPORTED\n");
            return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_NOT_SUPPORTED);

        case URB_FUNCTION_GET_FRAME_LENGTH: // 0x05
            DPRINT1("USBPORT_HandleSubmitURB: URB_FUNCTION_GET_FRAME_LENGTH (0x05) NOT_SUPPORTED\n");
            return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_NOT_SUPPORTED);

        case URB_FUNCTION_SET_FRAME_LENGTH: // 0x06
            DPRINT1("USBPORT_HandleSubmitURB: URB_FUNCTION_SET_FRAME_LENGTH (0x06) NOT_SUPPORTED\n");
            return USBPORT_USBDStatusToNtStatus(Urb, USBD_STATUS_NOT_SUPPORTED);

        default:
            DPRINT1("USBPORT_HandleSubmitURB: Unknown URB Function - %x\n", Function);
            //0x16 22 URB_FUNCTION_RESERVED_0X0016
            //0x1D 29 URB_FUNCTION_RESERVE_0X001D
            //0x2B 43 URB_FUNCTION_RESERVE_0X002B
            //0x2C 44 URB_FUNCTION_RESERVE_0X002C
            //0x2D 45 URB_FUNCTION_RESERVE_0X002D
            //0x2E 46 URB_FUNCTION_RESERVE_0X002E
            //0x2F 47 URB_FUNCTION_RESERVE_0X002F
            break;
    }

    if (Status == STATUS_PENDING)
    {
        return Status;
    }

    if (Urb->UrbHeader.UsbdFlags & USBD_FLAG_ALLOCATED_TRANSFER)
    {
        PUSBPORT_TRANSFER Transfer;

        Transfer = (PUSBPORT_TRANSFER)Urb->UrbControlTransfer.hca.Reserved8[0];
        Urb->UrbControlTransfer.hca.Reserved8[0] = NULL;
        Urb->UrbHeader.UsbdFlags |= ~USBD_FLAG_ALLOCATED_TRANSFER;
        ExFreePool(Transfer);
    }

    if (DeviceHandle)
    {
        InterlockedDecrement(&DeviceHandle->DeviceHandleLock);
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}
