/*
 * PROJECT:     ReactOS Universal Serial Bus Bulk Storage Driver
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        drivers/usb/usbstor/pdo.c
 * PURPOSE:     USB block storage device driver.
 * PROGRAMMERS:
 *              James Tabor
 *              Michael Martin (michael.martin@reactos.org)
 *              Johannes Anderwald (johannes.anderwald@reactos.org)
 */

#include "usbstor.h"

#define NDEBUG
#include <debug.h>

NTSTATUS
USBSTOR_BuildCBW(
    IN ULONG Tag,
    IN ULONG DataTransferLength,
    IN UCHAR LUN,
    IN UCHAR CommandBlockLength,
    IN PUCHAR CommandBlock,
    IN OUT PCBW Control)
{
    //
    // sanity check
    //
    ASSERT(CommandBlockLength <= 16);

    //
    // now initialize CBW
    //
    Control->Signature = CBW_SIGNATURE;
    Control->Tag = Tag;
    Control->DataTransferLength = DataTransferLength;
    Control->Flags = (CommandBlock[0] != SCSIOP_WRITE) ? 0x80 : 0x00;
    Control->LUN = (LUN & MAX_LUN);
    Control->CommandBlockLength = CommandBlockLength;

    //
    // copy command block
    //
    RtlCopyMemory(Control->CommandBlock, CommandBlock, CommandBlockLength);

    //
    // done
    //
    return STATUS_SUCCESS;
}

PIRP_CONTEXT
USBSTOR_AllocateIrpContext()
{
    PIRP_CONTEXT Context;

    //
    // allocate irp context
    //
    Context = (PIRP_CONTEXT)AllocateItem(NonPagedPool, sizeof(IRP_CONTEXT));
    if (!Context)
    {
        //
        // no memory
        //
        return NULL;
    }

    //
    // allocate cbw block
    //
    Context->cbw = (PCBW)AllocateItem(NonPagedPool, 512);
    if (!Context->cbw)
    {
        //
        // no memory
        //
        FreeItem(Context);
        return NULL;
    }

    //
    // done
    //
    return Context;

}

BOOLEAN
USBSTOR_IsCSWValid(
    PIRP_CONTEXT Context)
{
    //
    // sanity checks
    //
    if (sizeof(CSW) != 0x0d)
    {
        DPRINT1("[USBSTOR] Expected sizeof(CSW) == 0x0d but got %x\n", sizeof(CSW));
        return FALSE;
    }

    if (Context->csw->Signature != CSW_SIGNATURE)
    {
        DPRINT1("[USBSTOR] Expected Signature %x but got %x\n", CSW_SIGNATURE, Context->csw->Signature);
        return FALSE;
    }

    if (Context->csw->Tag != (ULONG)Context->csw)
    {
        DPRINT1("[USBSTOR] Expected Tag %x but got %x\n", (ULONG)Context->csw, Context->csw->Tag);
        return FALSE;
    }

    //
    // CSW is valid
    //
    return TRUE;

}

NTSTATUS
USBSTOR_QueueWorkItem(
    PIRP_CONTEXT Context,
    PIRP Irp)
{
    PERRORHANDLER_WORKITEM_DATA ErrorHandlerWorkItemData;

    //
    // Allocate Work Item Data
    //
    ErrorHandlerWorkItemData = ExAllocatePoolWithTag(NonPagedPool, sizeof(ERRORHANDLER_WORKITEM_DATA), USB_STOR_TAG);
    if (!ErrorHandlerWorkItemData)
    {
        //
        // no memory
        //
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // error handling started
    //
    Context->FDODeviceExtension->SrbErrorHandlingActive = TRUE;

    //
    // srb error handling finished
    //
    Context->FDODeviceExtension->TimerWorkQueueEnabled = FALSE;

    //
    // Initialize and queue the work item to handle the error
    //
    ExInitializeWorkItem(&ErrorHandlerWorkItemData->WorkQueueItem,
                         ErrorHandlerWorkItemRoutine,
                         ErrorHandlerWorkItemData);

    ErrorHandlerWorkItemData->DeviceObject = Context->FDODeviceExtension->FunctionalDeviceObject;
    ErrorHandlerWorkItemData->Context = Context;
    ErrorHandlerWorkItemData->Irp = Irp;
    ErrorHandlerWorkItemData->DeviceObject = Context->FDODeviceExtension->FunctionalDeviceObject;

    DPRINT1("Queuing WorkItemROutine\n");
    ExQueueWorkItem(&ErrorHandlerWorkItemData->WorkQueueItem, DelayedWorkQueue);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

// driver verifier
IO_COMPLETION_ROUTINE USBSTOR_SenseCSWCompletionRoutine;

NTSTATUS
NTAPI
USBSTOR_SenseCSWCompletionRoutine(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Ctx)
{
    PIRP_CONTEXT           Context = (PIRP_CONTEXT)Ctx;
    PIRP_CONTEXT           OriginalContext = Context->OriginalContext;
    PIO_STACK_LOCATION     IoStack;
    PSCSI_REQUEST_BLOCK    Srb;
    PDEVICE_OBJECT         FdoDevice = Context->PDODeviceExtension->LowerDeviceObject;
    PFDO_DEVICE_EXTENSION  FdoDeviceExtension = Context->PDODeviceExtension->LowerDeviceObject->DeviceExtension;


    DPRINT("USBSTOR_SenseCSWCompletionRoutine: Irp %p Ctx %p Status %x\n", Irp, Ctx, Irp->IoStatus.Status);

    // is there a MDL
    if (Context->TransferBufferMDL)
    {
        // is there an IRP associated
        if (Context->Irp)
        {
            // did we allocate the MDL
            if (Context->TransferBufferMDL != Context->Irp->MdlAddress)
            {
                // free MDL
                IoFreeMdl(Context->TransferBufferMDL);
            }
        }
        else
        {
            // free MDL
            IoFreeMdl(Context->TransferBufferMDL);
        }
    }

    if (!NT_SUCCESS(Irp->IoStatus.Status))
    {
        // if STALL
        DPRINT1("USBSTOR_SenseCSWCompletionRoutine Status %x\n", Irp->IoStatus.Status);
    }

    if (!USBSTOR_IsCSWValid(Context))
    {
        // 6.3.1  Valid CSW
        DPRINT1("CSW not Valid. Index %x Status %x\n", Context->ErrorIndex, Irp->IoStatus.Status);
    }

    // 6.3.2  Meaningful CSW
    if (Context->csw->Status <= 0x02)
    {
        if (Context->csw->Status == 0x01)
        {
            DPRINT1("CSWStatus 0x01. Command failed\n");
        }

        if (Context->csw->Status == 0x02)
        {
            DPRINT1("CSWStatus 0x02. Reset recovery\n");
        }
    }
    else
    {
        DPRINT1("FIXME: CSW not Meaningful. CSWStatus %x\n", Irp->IoStatus.Status);
        ASSERT(FALSE);
    }

    // FIXME: check status
    //Context->Irp->IoStatus.Status = Irp->IoStatus.Status;
    //Context->Irp->IoStatus.Information = Context->TransferDataLength;

    // free our allocated Sense IRP
    IoFreeIrp(Irp);

    // free Sense CBW
    FreeItem(Context->cbw);

    // free our allocated CDB
    ExFreePool(Context->SenseCDB);

    // free our allocated IRP
    IoFreeIrp(Context->Irp);

    // free Sense Context
    FreeItem(Context);

    // get SRB pointer
    IoStack = IoGetCurrentIrpStackLocation(OriginalContext->Irp);
    Srb = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;
    ASSERT(Srb);

    // Sense data is OK
    Srb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;

    // free CBW
    FreeItem(OriginalContext->cbw);

    // FIXME: check status -> if (SRB_STATUS(Srb->SrbStatus) != SRB_STATUS_SUCCESS )  Irp->IoStatus.Status = ConvertSrbStatus(Srb->SrbStatus);
    OriginalContext->Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
    OriginalContext->Irp->IoStatus.Information = 0;  // OriginalContext->TransferDataLength;

    if (FdoDeviceExtension->IrpListFreeze)
    {
        // UnFreezing Queue (?spinlock)
        FdoDeviceExtension->IrpListFreeze = FALSE;
    }

    // terminate current IRP
    USBSTOR_QueueTerminateRequest(FdoDevice, OriginalContext->Irp);

    // complete IRP
    IoCompleteRequest(OriginalContext->Irp, IO_NO_INCREMENT);

    // start next IRP
    USBSTOR_QueueNextRequest(FdoDevice);

    // free context
    FreeItem(OriginalContext);

    // done
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
USBSTOR_SendRequestSense(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP OriginalIrp,
    IN PIRP_CONTEXT OriginalContext)
{
    PPDO_DEVICE_EXTENSION  PDODeviceExtension = OriginalContext->PDODeviceExtension;
    PFDO_DEVICE_EXTENSION  FDODeviceExtension = OriginalContext->FDODeviceExtension;
    PIO_STACK_LOCATION     IoStack = IoGetCurrentIrpStackLocation(OriginalContext->Irp);
    PSCSI_REQUEST_BLOCK    InitialSrb;
    PCDB                   Cdb;
    PIRP_CONTEXT           Context;
    PIRP                   Irp;


    // allocate CDB for REQUEST SENSE
    Cdb = ExAllocatePool(NonPagedPool, sizeof(Cdb->CDB6INQUIRY));

    if ( Cdb == NULL )
    {
        DPRINT1("USBSTOR_SendRequestSense Allocate CDB failed\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Cdb, sizeof(Cdb->CDB6INQUIRY));

    // get original SRB
    InitialSrb = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;
    ASSERT(InitialSrb);

    // Build CDB for REQUEST SENSE
    Cdb->CDB6INQUIRY.OperationCode     = SCSIOP_REQUEST_SENSE;
    Cdb->CDB6INQUIRY.LogicalUnitNumber = (PDODeviceExtension->LUN & 15);  //USB_MAX_LUN
    Cdb->CDB6INQUIRY.Reserved1         = 0;
    Cdb->CDB6INQUIRY.PageCode          = 0;
    Cdb->CDB6INQUIRY.IReserved         = 0;
    Cdb->CDB6INQUIRY.AllocationLength  = (UCHAR)InitialSrb->SenseInfoBufferLength;
    Cdb->CDB6INQUIRY.Control           = 0;

    // allocate IRP-context for REQUEST SENSE
    Context = USBSTOR_AllocateIrpContext();
    if (!Context)
    {
        // no memory
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // save old IRP-context
    Context->OriginalContext = OriginalContext;

    // now build the CBW
    USBSTOR_BuildCBW((ULONG)Context->cbw,
                     InitialSrb->SenseInfoBufferLength,
                     PDODeviceExtension->LUN,
                     sizeof(Cdb->CDB6INQUIRY), //6
                     (PUCHAR)Cdb,
                     Context->cbw);

    DPRINT("USBSTOR_SendRequestSense: CBW %p\n", Context->cbw);
    //DumpCBW((PUCHAR)Context->cbw);

    // now initialize the URB
    UsbBuildInterruptOrBulkTransferRequest(&Context->Urb,
                                           sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                           FDODeviceExtension->InterfaceInformation->Pipes[FDODeviceExtension->BulkOutPipeIndex].PipeHandle,
                                           Context->cbw,
                                           NULL,
                                           sizeof(CBW),
                                           USBD_TRANSFER_DIRECTION_OUT,
                                           NULL);

    // initialize context
    Context->Irp                = OriginalIrp;
    Context->TransferData       = InitialSrb->SenseInfoBuffer;
    Context->TransferDataLength = InitialSrb->SenseInfoBufferLength;
    Context->FDODeviceExtension = OriginalContext->FDODeviceExtension;
    Context->PDODeviceExtension = OriginalContext->PDODeviceExtension;
    Context->RetryCount         = OriginalContext->RetryCount;
    Context->ErrorIndex         = 0;

    // save CDB 
    Context->SenseCDB = Cdb;

    // is there transfer data
    if (Context->TransferDataLength)
    {
        // allocate MDL for buffer, buffer must be allocated from NonPagedPool
        Context->TransferBufferMDL = IoAllocateMdl(Context->TransferData, Context->TransferDataLength, FALSE, FALSE, NULL);
        if (!Context->TransferBufferMDL)
        {
            // failed to allocate MDL
            FreeItem(Context->cbw);
            FreeItem(Context);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        // build MDL for nonpaged pool
        MmBuildMdlForNonPagedPool(Context->TransferBufferMDL);
    }

    // now allocate the IRP
    Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    if (!Irp)
    {
        FreeItem(Context->cbw);
        FreeItem(Context);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (OriginalIrp)
    {
        // mark orignal IRP as pending
        IoMarkIrpPending(OriginalIrp);
    }

    // send IRP
    DPRINT("USBSTOR_SendRequestSense: USBSTOR_SendCBW(%p, %p), Irql %x\n", Context, Irp, KeGetCurrentIrql());
    USBSTOR_SendCBW(Context, Irp);

    // done
    return STATUS_PENDING;
}

//
// driver verifier
//
IO_COMPLETION_ROUTINE USBSTOR_CSWCompletionRoutine;

NTSTATUS
NTAPI
USBSTOR_CSWCompletionRoutine(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Ctx)
{
    PIRP_CONTEXT Context;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;
    PCDB pCDB;
    PREAD_CAPACITY_DATA_EX CapacityDataEx;
    PREAD_CAPACITY_DATA CapacityData;
    PUFI_CAPACITY_RESPONSE Response;
    NTSTATUS Status;
    PDEVICE_OBJECT FdoDevice;
    PFDO_DEVICE_EXTENSION FdoDeviceExtension;

    //
    // access context
    //
    Context = (PIRP_CONTEXT)Ctx;
    FdoDevice = Context->PDODeviceExtension->LowerDeviceObject;
    FdoDeviceExtension = FdoDevice->DeviceExtension;

    //
    // is there a mdl
    //
    if (Context->TransferBufferMDL)
    {
        //
        // is there an irp associated
        //
        if (Context->Irp)
        {
            //
            // did we allocate the mdl
            //
            if (Context->TransferBufferMDL != Context->Irp->MdlAddress)
            {
                //
                // free mdl
                //
                IoFreeMdl(Context->TransferBufferMDL);
            }
        }
        else
        {
            //
            // free mdl
            //
            IoFreeMdl(Context->TransferBufferMDL);
        }
    }

    DPRINT("USBSTOR_CSWCompletionRoutine Status %x\n", Irp->IoStatus.Status);

    if (!NT_SUCCESS(Irp->IoStatus.Status))
    {
        if (Context->ErrorIndex == 0)
        {
            //
            // increment error index
            //
            Context->ErrorIndex = 1;

            //
            // clear STALL and resend CSW
            //
            Status = USBSTOR_QueueWorkItem(Context, Irp);
            ASSERT(Status == STATUS_MORE_PROCESSING_REQUIRED);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // perform reset recovery
        //
        Context->ErrorIndex = 3;
        IoFreeIrp(Irp);
        Status = USBSTOR_QueueWorkItem(Context, NULL);
        ASSERT(Status == STATUS_MORE_PROCESSING_REQUIRED);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    if (!USBSTOR_IsCSWValid(Context))
    {
        //
        // perform reset recovery
        //
        Context->ErrorIndex = 3;
        IoFreeIrp(Irp);
        Status = USBSTOR_QueueWorkItem(Context, NULL);
        ASSERT(Status == STATUS_MORE_PROCESSING_REQUIRED);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    // 6.3.2  Meaningful CSW
    if (Context->csw->Status <= 0x02)
    {
        if (Context->csw->Status == 0x01)  // 6.6.4  Command Failure
        {
            DPRINT1("CSWStatus 0x01. Command failed\n");

            // Freezing Queue (?spinlock)
            FdoDeviceExtension->IrpListFreeze = TRUE;

            // Send a SCSI REQUEST SENSE command
            Status = USBSTOR_SendRequestSense(Context->PDODeviceExtension->Self, Irp, Context);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        if (Context->csw->Status == 0x02)  //6.6.2  Internal Device Error
        {
            DPRINT1("CSWStatus 0x02. Reset recovery\n");

            // increment error index
            Context->ErrorIndex = 3;

            // free our allocated irp
            IoFreeIrp(Irp);

            // perform Reset Recovery and handle error
            Status = USBSTOR_QueueWorkItem(Context, NULL);
            ASSERT(Status == STATUS_MORE_PROCESSING_REQUIRED);
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }
    else
    {
        DPRINT1("FIXME: CSW not Meaningful. CSWStatus %x\n", Irp->IoStatus.Status);
        ASSERT(FALSE);
    }

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Context->Irp);

    //
    // get request block
    //
    Request = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;
    ASSERT(Request);

    //
    // get SCSI command data block
    //
    pCDB = (PCDB)Request->Cdb;
    Request->SrbStatus = SRB_STATUS_SUCCESS;

    //
    // read capacity needs special work
    //
    if (pCDB->AsByte[0] == SCSIOP_READ_CAPACITY)
    {
        //
        // get output buffer
        //
        Response = (PUFI_CAPACITY_RESPONSE)Context->TransferData;

        //
        // store in pdo
        //
        Context->PDODeviceExtension->BlockLength = NTOHL(Response->BlockLength);
        Context->PDODeviceExtension->LastLogicBlockAddress = NTOHL(Response->LastLogicalBlockAddress);

        if (Request->DataTransferLength == sizeof(READ_CAPACITY_DATA_EX))
        {
            //
            // get input buffer
            //
            CapacityDataEx = (PREAD_CAPACITY_DATA_EX)Request->DataBuffer;

            //
            // set result
            //
            CapacityDataEx->BytesPerBlock = Response->BlockLength;
            CapacityDataEx->LogicalBlockAddress.QuadPart = Response->LastLogicalBlockAddress;
            Irp->IoStatus.Information = sizeof(READ_CAPACITY_DATA_EX);
       }
       else
       {
            //
            // get input buffer
            //
            CapacityData = (PREAD_CAPACITY_DATA)Request->DataBuffer;

            //
            // set result
            //
            CapacityData->BytesPerBlock = Response->BlockLength;
            CapacityData->LogicalBlockAddress = Response->LastLogicalBlockAddress;
            Irp->IoStatus.Information = sizeof(READ_CAPACITY_DATA);
       }

       //
       // free response
       //
       FreeItem(Context->TransferData);
    }

    //
    // free cbw
    //
    FreeItem(Context->cbw);

    //
    // FIXME: check status
    //
    Context->Irp->IoStatus.Status = Irp->IoStatus.Status;
    Context->Irp->IoStatus.Information = Context->TransferDataLength;

    //
    // terminate current request
    //
    USBSTOR_QueueTerminateRequest(FdoDevice, Context->Irp);

    //
    // complete request
    //
    IoCompleteRequest(Context->Irp, IO_NO_INCREMENT);

    //
    // start next request
    //
    USBSTOR_QueueNextRequest(FdoDevice);

    //
    // free our allocated irp
    //
    IoFreeIrp(Irp);

    //
    // free context
    //
    FreeItem(Context);

    //
    // done
    //
    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
USBSTOR_SendCSW(
    PIRP_CONTEXT Context,
    PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;

    //
    // get next irp stack location
    //
    IoStack = IoGetNextIrpStackLocation(Irp);

    //
    // now initialize the urb for sending the csw
    //
    UsbBuildInterruptOrBulkTransferRequest(&Context->Urb,
                                           sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                           Context->FDODeviceExtension->InterfaceInformation->Pipes[Context->FDODeviceExtension->BulkInPipeIndex].PipeHandle,
                                           Context->csw,
                                           NULL,
                                           sizeof(CSW),
                                           USBD_TRANSFER_DIRECTION_IN | USBD_SHORT_TRANSFER_OK,
                                           NULL);

    //
    // initialize stack location
    //
    IoStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    IoStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
    IoStack->Parameters.Others.Argument1 = (PVOID)&Context->Urb;
    IoStack->Parameters.DeviceIoControl.InputBufferLength = Context->Urb.UrbHeader.Length;
    Irp->IoStatus.Status = STATUS_SUCCESS;


    //
    // if error - IRP List (queue) Frozen 
    //
    if (Context->FDODeviceExtension->IrpListFreeze)
    {
         //
         // setup completion routine for Request Sense 
         //
         IoSetCompletionRoutine(Irp, USBSTOR_SenseCSWCompletionRoutine, Context, TRUE, TRUE, TRUE);
    }
    else
    {
         //
         // setup completion routine
         //
         IoSetCompletionRoutine(Irp, USBSTOR_CSWCompletionRoutine, Context, TRUE, TRUE, TRUE);
    }

    //
    // call driver
    //
    IoCallDriver(Context->FDODeviceExtension->LowerDeviceObject, Irp);
}


//
// driver verifier
//
IO_COMPLETION_ROUTINE USBSTOR_DataCompletionRoutine;

NTSTATUS
NTAPI
USBSTOR_DataCompletionRoutine(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Ctx)
{
    PIRP_CONTEXT Context;
    NTSTATUS Status;


    DPRINT("USBSTOR_DataCompletionRoutine Irp %p Ctx %p Status %x\n", Irp, Ctx, Irp->IoStatus.Status);

    //
    // access context
    //
    Context = (PIRP_CONTEXT)Ctx;

    if (!NT_SUCCESS(Irp->IoStatus.Status))
    {
        //
        // clear STALL and send CSW
        //
        Context->ErrorIndex = 1;
        Status = USBSTOR_QueueWorkItem(Context, Irp);
        ASSERT(Status == STATUS_MORE_PROCESSING_REQUIRED);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // send csw
    //
    USBSTOR_SendCSW(Context, Irp);

    //
    // cancel completion
    //
    return STATUS_MORE_PROCESSING_REQUIRED;
}

//
// driver verifier
//
IO_COMPLETION_ROUTINE USBSTOR_CBWCompletionRoutine;

NTSTATUS
NTAPI
USBSTOR_CBWCompletionRoutine(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Ctx)
{
    PIRP_CONTEXT Context;
    PIO_STACK_LOCATION IoStack;
    UCHAR Code;
    USBD_PIPE_HANDLE PipeHandle;
    NTSTATUS Status;

    DPRINT("USBSTOR_CBWCompletionRoutine Irp %p Ctx %p Status %x\n", Irp, Ctx, Irp->IoStatus.Status);

    //
    // access context
    //
    Context = (PIRP_CONTEXT)Ctx;

    //
    // get next stack location
    //
    IoStack = IoGetNextIrpStackLocation(Irp);

    if (!NT_SUCCESS(Irp->IoStatus.Status))
    {
        // perform Reset Recovery and send the CSW
        Context->ErrorIndex = 2;
        Status = USBSTOR_QueueWorkItem(Context, Irp);
        ASSERT(Status == STATUS_MORE_PROCESSING_REQUIRED);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // is there data to be submitted
    //
    if (Context->TransferDataLength)
    {
        //
        // get command code
        //
        Code = Context->cbw->CommandBlock[0];

        if (Code == SCSIOP_WRITE)
        {
            //
            // write request use bulk out pipe
            //
            PipeHandle = Context->FDODeviceExtension->InterfaceInformation->Pipes[Context->FDODeviceExtension->BulkOutPipeIndex].PipeHandle;
        }
        else
        {
            //
            // default bulk in pipe
            //
            PipeHandle = Context->FDODeviceExtension->InterfaceInformation->Pipes[Context->FDODeviceExtension->BulkInPipeIndex].PipeHandle;
        }

        //
        // now initialize the urb for sending data
        //
        UsbBuildInterruptOrBulkTransferRequest(&Context->Urb,
                                               sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                               PipeHandle,
                                               NULL,
                                               Context->TransferBufferMDL,
                                               Context->TransferDataLength,
                                               ((Code == SCSIOP_WRITE) ? USBD_TRANSFER_DIRECTION_OUT : (USBD_TRANSFER_DIRECTION_IN | USBD_SHORT_TRANSFER_OK)),
                                               NULL);

        //
        // setup completion routine
        //
        IoSetCompletionRoutine(Irp, USBSTOR_DataCompletionRoutine, Context, TRUE, TRUE, TRUE);
    }
    else
    {
        //
        // now initialize the urb for sending the csw
        //

        UsbBuildInterruptOrBulkTransferRequest(&Context->Urb,
                                               sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                               Context->FDODeviceExtension->InterfaceInformation->Pipes[Context->FDODeviceExtension->BulkInPipeIndex].PipeHandle,
                                               Context->csw,
                                               NULL,
                                               sizeof(CSW),
                                               USBD_TRANSFER_DIRECTION_IN | USBD_SHORT_TRANSFER_OK,
                                               NULL);

        //
        // setup completion routine
        //
        IoSetCompletionRoutine(Irp, USBSTOR_CSWCompletionRoutine, Context, TRUE, TRUE, TRUE);
    }

    //
    // initialize stack location
    //
    IoStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    IoStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
    IoStack->Parameters.Others.Argument1 = (PVOID)&Context->Urb;
    IoStack->Parameters.DeviceIoControl.InputBufferLength = Context->Urb.UrbHeader.Length;
    Irp->IoStatus.Status = STATUS_SUCCESS;

    //
    // call driver
    //
    IoCallDriver(Context->FDODeviceExtension->LowerDeviceObject, Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
DumpCBW(
    PUCHAR Block)
{
    DPRINT("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
        Block[0] & 0xFF, Block[1] & 0xFF, Block[2] & 0xFF, Block[3] & 0xFF, Block[4] & 0xFF, Block[5] & 0xFF, Block[6] & 0xFF, Block[7] & 0xFF, Block[8] & 0xFF, Block[9] & 0xFF,
        Block[10] & 0xFF, Block[11] & 0xFF, Block[12] & 0xFF, Block[13] & 0xFF, Block[14] & 0xFF, Block[15] & 0xFF, Block[16] & 0xFF, Block[17] & 0xFF, Block[18] & 0xFF, Block[19] & 0xFF,
        Block[20] & 0xFF, Block[21] & 0xFF, Block[22] & 0xFF, Block[23] & 0xFF, Block[24] & 0xFF, Block[25] & 0xFF, Block[26] & 0xFF, Block[27] & 0xFF, Block[28] & 0xFF, Block[29] & 0xFF,
        Block[30] & 0xFF);

}

NTSTATUS
USBSTOR_SendCBW(
    PIRP_CONTEXT Context,
    PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;

    //
    // get next stack location
    //
    IoStack = IoGetNextIrpStackLocation(Irp);

    //
    // initialize stack location
    //
    IoStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    IoStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
    IoStack->Parameters.Others.Argument1 = (PVOID)&Context->Urb;
    IoStack->Parameters.DeviceIoControl.InputBufferLength = Context->Urb.UrbHeader.Length;
    Irp->IoStatus.Status = STATUS_SUCCESS;

    //
    // setup completion routine
    //
    IoSetCompletionRoutine(Irp, USBSTOR_CBWCompletionRoutine, Context, TRUE, TRUE, TRUE);

    //
    // call driver
    //
    return IoCallDriver(Context->FDODeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS
USBSTOR_SendRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP OriginalRequest,
    IN UCHAR CommandLength,
    IN PUCHAR Command,
    IN ULONG TransferDataLength,
    IN PUCHAR TransferData,
    IN ULONG RetryCount)
{
    PIRP_CONTEXT Context;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PFDO_DEVICE_EXTENSION FDODeviceExtension;
    PIRP Irp;
    PUCHAR MdlVirtualAddress;

    //
    // first allocate irp context
    //
    Context = USBSTOR_AllocateIrpContext();
    if (!Context)
    {
        //
        // no memory
        //
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // get FDO device extension
    //
    FDODeviceExtension = (PFDO_DEVICE_EXTENSION)PDODeviceExtension->LowerDeviceObject->DeviceExtension;

    //
    // now build the cbw
    //
    USBSTOR_BuildCBW((ULONG)Context->cbw,
                     TransferDataLength,
                     PDODeviceExtension->LUN,
                     CommandLength,
                     Command,
                     Context->cbw);

    DPRINT("CBW %p\n", Context->cbw);
    DumpCBW((PUCHAR)Context->cbw);

    //
    // now initialize the urb
    //
    UsbBuildInterruptOrBulkTransferRequest(&Context->Urb,
                                           sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                           FDODeviceExtension->InterfaceInformation->Pipes[FDODeviceExtension->BulkOutPipeIndex].PipeHandle,
                                           Context->cbw,
                                           NULL,
                                           sizeof(CBW),
                                           USBD_TRANSFER_DIRECTION_OUT,
                                           NULL);

    //
    // initialize rest of context
    //
    Context->Irp = OriginalRequest;
    Context->TransferData = TransferData;
    Context->TransferDataLength = TransferDataLength;
    Context->FDODeviceExtension = FDODeviceExtension;
    Context->PDODeviceExtension = PDODeviceExtension;
    Context->RetryCount = RetryCount;

    //
    // is there transfer data
    //
    if (Context->TransferDataLength)
    {
        //
        // check if the original request already does have an mdl associated
        //
        if (OriginalRequest)
        {
            if ((OriginalRequest->MdlAddress != NULL) &&
                (Context->TransferData == NULL || Command[0] == SCSIOP_READ || Command[0] == SCSIOP_WRITE))
            {
                //
                // Sanity check that the Mdl does describe the TransferData for read/write
                //
                if (CommandLength == UFI_READ_WRITE_CMD_LEN)
                {
                    MdlVirtualAddress = MmGetMdlVirtualAddress(OriginalRequest->MdlAddress);

                    //
                    // is there an offset
                    //
                    if (MdlVirtualAddress != Context->TransferData)
                    {
                        //
                        // lets build an mdl
                        //
                        Context->TransferBufferMDL = IoAllocateMdl(Context->TransferData, MmGetMdlByteCount(OriginalRequest->MdlAddress), FALSE, FALSE, NULL);
                        if (!Context->TransferBufferMDL)
                        {
                            //
                            // failed to allocate MDL
                            //
                            FreeItem(Context->cbw);
                            FreeItem(Context);
                            return STATUS_INSUFFICIENT_RESOURCES;
                        }

                        //
                        // now build the partial mdl
                        //
                        IoBuildPartialMdl(OriginalRequest->MdlAddress, Context->TransferBufferMDL, Context->TransferData, Context->TransferDataLength);
                    }
                }

                if (!Context->TransferBufferMDL)
                {
                    //
                    // I/O paging request
                    //
                    Context->TransferBufferMDL = OriginalRequest->MdlAddress;
                }
            }
            else
            {
                //
                // allocate mdl for buffer, buffer must be allocated from NonPagedPool
                //
                Context->TransferBufferMDL = IoAllocateMdl(Context->TransferData, Context->TransferDataLength, FALSE, FALSE, NULL);
                if (!Context->TransferBufferMDL)
                {
                    //
                    // failed to allocate MDL
                    //
                    FreeItem(Context->cbw);
                    FreeItem(Context);
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                //
                // build mdl for nonpaged pool
                //
                MmBuildMdlForNonPagedPool(Context->TransferBufferMDL);
            }
        }
        else
        {
            //
            // allocate mdl for buffer, buffer must be allocated from NonPagedPool
            //
            Context->TransferBufferMDL = IoAllocateMdl(Context->TransferData, Context->TransferDataLength, FALSE, FALSE, NULL);
            if (!Context->TransferBufferMDL)
            {
                //
                // failed to allocate MDL
                //
                FreeItem(Context->cbw);
                FreeItem(Context);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            //
            // build mdl for nonpaged pool
            //
            MmBuildMdlForNonPagedPool(Context->TransferBufferMDL);
        }
    }

    //
    // now allocate the request
    //
    Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    if (!Irp)
    {
        FreeItem(Context->cbw);
        FreeItem(Context);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (OriginalRequest)
    {
        //
        // mark orignal irp as pending
        //
        IoMarkIrpPending(OriginalRequest);
    }

    //
    // send request
    //
    USBSTOR_SendCBW(Context, Irp);

    //
    // done
    //
    return STATUS_PENDING;
}

NTSTATUS
USBSTOR_SendFormatCapacity(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG RetryCount)
{
    UFI_READ_FORMAT_CAPACITY Cmd;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // get request block
    //
    Request = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;

    //
    // initialize inquiry cmd
    //
    RtlZeroMemory(&Cmd, sizeof(UFI_READ_FORMAT_CAPACITY));
    Cmd.Code = SCSIOP_READ_FORMATTED_CAPACITY;
    Cmd.LUN = (PDODeviceExtension->LUN & MAX_LUN);
    Cmd.AllocationLengthMsb = HTONS(Request->DataTransferLength & 0xFFFF) >> 8;
    Cmd.AllocationLengthLsb = HTONS(Request->DataTransferLength & 0xFFFF) & 0xFF;

    //
    // now send the request
    //
    return USBSTOR_SendRequest(DeviceObject, Irp, UFI_READ_FORMAT_CAPACITY_CMD_LEN, (PUCHAR)&Cmd, Request->DataTransferLength, (PUCHAR)Request->DataBuffer, RetryCount);
}

NTSTATUS
USBSTOR_SendInquiry(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG RetryCount)
{
    UFI_INQUIRY_CMD Cmd;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get request block
    //
    Request = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // initialize inquiry cmd
    //
    RtlZeroMemory(&Cmd, sizeof(UFI_INQUIRY_CMD));
    Cmd.Code = SCSIOP_INQUIRY;
    Cmd.LUN = (PDODeviceExtension->LUN & MAX_LUN);
    Cmd.AllocationLength = sizeof(UFI_INQUIRY_RESPONSE);

    //
    // sanity check
    //
    ASSERT(Request->DataTransferLength >= sizeof(UFI_INQUIRY_RESPONSE));

    //
    // now send the request
    //
    return USBSTOR_SendRequest(DeviceObject, Irp, UFI_INQUIRY_CMD_LEN, (PUCHAR)&Cmd, Request->DataTransferLength, (PUCHAR)Request->DataBuffer, RetryCount);
}

NTSTATUS
USBSTOR_SendCapacity(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG RetryCount)
{
    UFI_CAPACITY_CMD Cmd;
    PUFI_CAPACITY_RESPONSE Response;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // allocate capacity response
    //
    Response = (PUFI_CAPACITY_RESPONSE)AllocateItem(NonPagedPool, PAGE_SIZE);
    if (!Response)
    {
        //
        // no memory
        //
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // initialize capacity cmd
    //
    RtlZeroMemory(&Cmd, sizeof(UFI_INQUIRY_CMD));
    Cmd.Code = SCSIOP_READ_CAPACITY;
    Cmd.LUN = (PDODeviceExtension->LUN & MAX_LUN);

    //
    // send request, response will be freed in completion routine
    //
    return USBSTOR_SendRequest(DeviceObject, Irp, UFI_READ_CAPACITY_CMD_LEN, (PUCHAR)&Cmd, sizeof(UFI_CAPACITY_RESPONSE), (PUCHAR)Response, RetryCount);
}

NTSTATUS
USBSTOR_SendModeSense(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG RetryCount)
{
#if 0
    UFI_SENSE_CMD Cmd;
    NTSTATUS Status;
    PVOID Response;
    PCBW OutControl;
    PCDB pCDB;
    PUFI_MODE_PARAMETER_HEADER Header;
#endif
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // sanity check
    //
    ASSERT(PDODeviceExtension->Common.IsFDO == FALSE);

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get request block
    //
    Request = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;

    RtlZeroMemory(Request->DataBuffer, Request->DataTransferLength);
    Request->SrbStatus = SRB_STATUS_SUCCESS;
    Irp->IoStatus.Information = Request->DataTransferLength;
    Irp->IoStatus.Status = STATUS_SUCCESS;
    USBSTOR_QueueTerminateRequest(PDODeviceExtension->LowerDeviceObject, Irp);
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    //
    // start next request
    //
    USBSTOR_QueueNextRequest(PDODeviceExtension->LowerDeviceObject);

    return STATUS_SUCCESS;

#if 0
    //
    // get SCSI command data block
    //
    pCDB = (PCDB)Request->Cdb;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // allocate sense response from non paged pool
    //
    Response = (PUFI_CAPACITY_RESPONSE)AllocateItem(NonPagedPool, Request->DataTransferLength);
    if (!Response)
    {
        //
        // no memory
        //
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // sanity check
    //


    // Supported pages
    // MODE_PAGE_ERROR_RECOVERY
    // MODE_PAGE_FLEXIBILE
    // MODE_PAGE_LUN_MAPPING
    // MODE_PAGE_FAULT_REPORTING
    // MODE_SENSE_RETURN_ALL

    //
    // initialize mode sense cmd
    //
    RtlZeroMemory(&Cmd, sizeof(UFI_INQUIRY_CMD));
    Cmd.Code = SCSIOP_MODE_SENSE;
    Cmd.LUN = (PDODeviceExtension->LUN & MAX_LUN);
    Cmd.PageCode = pCDB->MODE_SENSE.PageCode;
    Cmd.PC = pCDB->MODE_SENSE.Pc;
    Cmd.AllocationLength = HTONS(pCDB->MODE_SENSE.AllocationLength);

    DPRINT1("PageCode %x\n", pCDB->MODE_SENSE.PageCode);
    DPRINT1("PC %x\n", pCDB->MODE_SENSE.Pc);

    //
    // now send mode sense cmd
    //
    Status = USBSTOR_SendCBW(DeviceObject, UFI_SENSE_CMD_LEN, (PUCHAR)&Cmd, Request->DataTransferLength, &OutControl);
    if (!NT_SUCCESS(Status))
    {
        //
        // failed to send CBW
        //
        DPRINT1("USBSTOR_SendCapacityCmd> USBSTOR_SendCBW failed with %x\n", Status);
        FreeItem(Response);
        ASSERT(FALSE);
        return Status;
    }

    //
    // now send data block response
    //
    Status = USBSTOR_SendData(DeviceObject, Request->DataTransferLength, Response);
    if (!NT_SUCCESS(Status))
    {
        //
        // failed to send CBW
        //
        DPRINT1("USBSTOR_SendCapacityCmd> USBSTOR_SendData failed with %x\n", Status);
        FreeItem(Response);
        ASSERT(FALSE);
        return Status;
    }

    Header = (PUFI_MODE_PARAMETER_HEADER)Response;

    //
    // TODO: build layout
    //
    // first struct is the header
    // MODE_PARAMETER_HEADER / _MODE_PARAMETER_HEADER10
    //
    // followed by
    // MODE_PARAMETER_BLOCK
    //
    //
    UNIMPLEMENTED

    //
    // send csw
    //
    Status = USBSTOR_SendCSW(DeviceObject, OutControl, 512, &CSW);

    DPRINT1("------------------------\n");
    DPRINT1("CSW %p\n", &CSW);
    DPRINT1("Signature %x\n", CSW.Signature);
    DPRINT1("Tag %x\n", CSW.Tag);
    DPRINT1("DataResidue %x\n", CSW.DataResidue);
    DPRINT1("Status %x\n", CSW.Status);

    //
    // FIXME: handle error
    //
    ASSERT(CSW.Status == 0);
    ASSERT(CSW.DataResidue == 0);

    //
    // calculate transfer length
    //
    *TransferBufferLength = Request->DataTransferLength - CSW.DataResidue;

    //
    // copy buffer
    //
    RtlCopyMemory(Request->DataBuffer, Response, *TransferBufferLength);

    //
    // free item
    //
    FreeItem(OutControl);

    //
    // free response
    //
    FreeItem(Response);

    //
    // done
    //
    return Status;
#endif
}

NTSTATUS
USBSTOR_SendReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG RetryCount)
{
    UFI_READ_WRITE_CMD Cmd;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PCDB pCDB;
    ULONG BlockCount, Temp;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get request block
    //
    Request = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;

    //
    // get SCSI command data block
    //
    pCDB = (PCDB)Request->Cdb;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // informal debug print
    //
    DPRINT("USBSTOR_SendReadWrite DataTransferLength %lu, BlockLength %lu\n", Request->DataTransferLength, PDODeviceExtension->BlockLength);

    //
    // sanity check
    //
    ASSERT(PDODeviceExtension->BlockLength);

    //
    // block count
    //
    BlockCount = Request->DataTransferLength / PDODeviceExtension->BlockLength;

    //
    // initialize read cmd
    //
    RtlZeroMemory(&Cmd, sizeof(UFI_READ_WRITE_CMD));
    Cmd.Code = pCDB->AsByte[0];
    Cmd.LUN = (PDODeviceExtension->LUN & MAX_LUN);
    Cmd.ContiguousLogicBlocksByte0 = pCDB->CDB10.TransferBlocksMsb;
    Cmd.ContiguousLogicBlocksByte1 = pCDB->CDB10.TransferBlocksLsb;
    Cmd.LogicalBlockByte0 = pCDB->CDB10.LogicalBlockByte0;
    Cmd.LogicalBlockByte1 = pCDB->CDB10.LogicalBlockByte1;
    Cmd.LogicalBlockByte2 = pCDB->CDB10.LogicalBlockByte2;
    Cmd.LogicalBlockByte3 = pCDB->CDB10.LogicalBlockByte3;

    //
    // sanity check
    //
    Temp = (Cmd.ContiguousLogicBlocksByte0 << 8 | Cmd.ContiguousLogicBlocksByte1);
    ASSERT(NTOHL(Temp == BlockCount));

    DPRINT("USBSTOR_SendReadWrite BlockAddress %x%x%x%x BlockCount %lu BlockLength %lu\n", Cmd.LogicalBlockByte0, Cmd.LogicalBlockByte1, Cmd.LogicalBlockByte2, Cmd.LogicalBlockByte3, BlockCount, PDODeviceExtension->BlockLength);

    //
    // send request
    //
    return USBSTOR_SendRequest(DeviceObject, Irp, UFI_READ_WRITE_CMD_LEN, (PUCHAR)&Cmd, Request->DataTransferLength, (PUCHAR)Request->DataBuffer, RetryCount);
}

NTSTATUS
USBSTOR_SendTestUnit(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP Irp,
    IN ULONG RetryCount)
{
    UFI_TEST_UNIT_CMD Cmd;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get request block
    //
    Request = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;

    //
    // no transfer length
    //
    ASSERT(Request->DataTransferLength == 0);

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // initialize test unit cmd
    //
    RtlZeroMemory(&Cmd, sizeof(UFI_TEST_UNIT_CMD));
    Cmd.Code = SCSIOP_TEST_UNIT_READY;
    Cmd.LUN = (PDODeviceExtension->LUN & MAX_LUN);

    //
    // send the request
    //
    return USBSTOR_SendRequest(DeviceObject, Irp, UFI_TEST_UNIT_CMD_LEN, (PUCHAR)&Cmd, 0, NULL, RetryCount);
}

NTSTATUS
USBSTOR_SendUnknownRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP Irp,
    IN ULONG RetryCount)
{
    PPDO_DEVICE_EXTENSION PDODeviceExtension;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;
    UFI_UNKNOWN_CMD Cmd;

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get request block
    //
    Request = IoStack->Parameters.Others.Argument1;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // check that we're sending to the right LUN
    //
    ASSERT(Request->Cdb[1] == (PDODeviceExtension->LUN & MAX_LUN));

    //
    // sanity check
    //
    ASSERT(Request->CdbLength <= sizeof(UFI_UNKNOWN_CMD));

    //
    // initialize test unit cmd
    //
    RtlCopyMemory(&Cmd, Request->Cdb, Request->CdbLength);

    //
    // send the request
    //
    return USBSTOR_SendRequest(DeviceObject, Irp, Request->CdbLength, (PUCHAR)&Cmd, Request->DataTransferLength, Request->DataBuffer, RetryCount);
}

NTSTATUS
USBSTOR_HandleExecuteSCSI(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG RetryCount)
{
    PCDB pCDB;
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    PSCSI_REQUEST_BLOCK Request;
    PPDO_DEVICE_EXTENSION PDODeviceExtension;

    //
    // get PDO device extension
    //
    PDODeviceExtension = (PPDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // sanity check
    //
    ASSERT(PDODeviceExtension->Common.IsFDO == FALSE);

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get request block
    //
    Request = (PSCSI_REQUEST_BLOCK)IoStack->Parameters.Others.Argument1;

    //
    // get SCSI command data block
    //
    pCDB = (PCDB)Request->Cdb;

    DPRINT("USBSTOR_HandleExecuteSCSI Operation Code %x\n", pCDB->AsByte[0]);

    if (pCDB->AsByte[0] == SCSIOP_READ_CAPACITY)
    {
        //
        // sanity checks
        //
        ASSERT(Request->DataBuffer);

        DPRINT("SCSIOP_READ_CAPACITY Length %lu\n", Request->DataTransferLength);
        Status = USBSTOR_SendCapacity(DeviceObject, Irp, RetryCount);
    }
    else if (pCDB->MODE_SENSE.OperationCode == SCSIOP_MODE_SENSE)
    {
        DPRINT("SCSIOP_MODE_SENSE DataTransferLength %lu\n", Request->DataTransferLength);
        ASSERT(pCDB->MODE_SENSE.AllocationLength == Request->DataTransferLength);
        ASSERT(Request->DataBuffer);

        //
        // send mode sense command
        //
        Status = USBSTOR_SendModeSense(DeviceObject, Irp, RetryCount);
    }
    else if (pCDB->AsByte[0] == SCSIOP_READ_FORMATTED_CAPACITY)
    {
        DPRINT("SCSIOP_READ_FORMATTED_CAPACITY DataTransferLength %lu\n", Request->DataTransferLength);

        //
        // send read format capacity
        //
        Status = USBSTOR_SendFormatCapacity(DeviceObject, Irp, RetryCount);
    }
    else if (pCDB->AsByte[0] == SCSIOP_INQUIRY)
    {
        DPRINT("SCSIOP_INQUIRY DataTransferLength %lu\n", Request->DataTransferLength);

        //
        // send read format capacity
        //
        Status = USBSTOR_SendInquiry(DeviceObject, Irp, RetryCount);
    }
    else if (pCDB->MODE_SENSE.OperationCode == SCSIOP_READ ||  pCDB->MODE_SENSE.OperationCode == SCSIOP_WRITE)
    {
        DPRINT("SCSIOP_READ / SCSIOP_WRITE DataTransferLength %lu\n", Request->DataTransferLength);

        //
        // send read / write command
        //
        Status = USBSTOR_SendReadWrite(DeviceObject, Irp, RetryCount);
    }
    else if (pCDB->AsByte[0] == SCSIOP_MEDIUM_REMOVAL)
    {
        DPRINT("SCSIOP_MEDIUM_REMOVAL\n");

        //
        // just complete the request
        //
        Request->SrbStatus = SRB_STATUS_SUCCESS;
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = Request->DataTransferLength;
        USBSTOR_QueueTerminateRequest(PDODeviceExtension->LowerDeviceObject, Irp);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        //
        // start next request
        //
        USBSTOR_QueueNextRequest(PDODeviceExtension->LowerDeviceObject);

        return STATUS_SUCCESS;
    }
    else if (pCDB->MODE_SENSE.OperationCode == SCSIOP_TEST_UNIT_READY)
    {
        DPRINT("SCSIOP_TEST_UNIT_READY\n");

        //
        // send test unit command
        //
        Status = USBSTOR_SendTestUnit(DeviceObject, Irp, RetryCount);
    }
    else
    {
        // Unknown request. Simply forward
        DPRINT1("Forwarding unknown Operation Code %x\n", pCDB->AsByte[0]);
        Status = USBSTOR_SendUnknownRequest(DeviceObject, Irp, RetryCount);
    }

    return Status;
}
