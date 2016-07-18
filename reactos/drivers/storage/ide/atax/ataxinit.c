#include "atax.h"               

//#define NDEBUG
#include <debug.h>


NTSTATUS
AtaXSendInquiry(
    PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN ULONG DeviceNumber)
{
  PPDO_DEVICE_EXTENSION  AtaXDevicePdoExtension;
  PINQUIRYDATA           InquiryBuffer;
  PSENSE_DATA            SenseBuffer;
  KEVENT                 Event;
  KIRQL                  Irql;
  BOOLEAN                KeepTrying = TRUE;
  IO_STATUS_BLOCK        IoStatusBlock;
  PIRP                   Irp;
  PIO_STACK_LOCATION     IrpStack;
  SCSI_REQUEST_BLOCK     Srb;
  PCDB                   Cdb;
  ULONG                  RetryCount = 0;
  NTSTATUS               Status;


  DPRINT("AtaXSendInquiry: AtaXChannelFdoExtension - %p\n", AtaXChannelFdoExtension);

  AtaXDevicePdoExtension = AtaXChannelFdoExtension->AtaXDevicePdo[DeviceNumber]->DeviceExtension;

  InquiryBuffer = ExAllocatePool(NonPagedPool, INQUIRYDATABUFFERSIZE);
  if ( InquiryBuffer == NULL )
    return STATUS_INSUFFICIENT_RESOURCES;

  SenseBuffer = ExAllocatePool(NonPagedPool, SENSE_BUFFER_SIZE);
  if ( SenseBuffer == NULL )
  {
    ExFreePool(InquiryBuffer);
    return STATUS_INSUFFICIENT_RESOURCES;
  }

  while ( KeepTrying )
  {
    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(
                 IOCTL_SCSI_EXECUTE_IN,
                 AtaXChannelFdoExtension->CommonExtension.SelfDevice,
                 NULL,
                 0,
                 InquiryBuffer,
                 INQUIRYDATABUFFERSIZE,
                 TRUE,
                 &Event,
                 &IoStatusBlock);

    if ( Irp == NULL )
    {
      DPRINT("AtaXSendInquiry: IoBuildDeviceIoControlRequest() failed\n");
      return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(&Srb, sizeof(SCSI_REQUEST_BLOCK));

    Srb.Length          = sizeof(SCSI_REQUEST_BLOCK);
    Srb.OriginalRequest = Irp;
    Srb.PathId          = AtaXDevicePdoExtension->PathId;
    Srb.TargetId        = AtaXDevicePdoExtension->TargetId;
    Srb.Lun             = AtaXDevicePdoExtension->Lun;
    Srb.Function        = SRB_FUNCTION_EXECUTE_SCSI;
    Srb.SrbFlags        = SRB_FLAGS_DATA_IN | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
    Srb.TimeOutValue    = 4;
    Srb.CdbLength       = 6;

    Srb.SenseInfoBuffer       = SenseBuffer;
    Srb.SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    Srb.DataBuffer            = InquiryBuffer;
    Srb.DataTransferLength    = INQUIRYDATABUFFERSIZE;

    IrpStack = IoGetNextIrpStackLocation (Irp);
    IrpStack->Parameters.Scsi.Srb = &Srb;

    Cdb = (PCDB)Srb.Cdb;
    Cdb->CDB6INQUIRY.OperationCode     = SCSIOP_INQUIRY;
    Cdb->CDB6INQUIRY.LogicalUnitNumber = AtaXDevicePdoExtension->Lun;
    Cdb->CDB6INQUIRY.AllocationLength  = INQUIRYDATABUFFERSIZE;

    Status = IoCallDriver(AtaXDevicePdoExtension->CommonExtension.SelfDevice, Irp);

    if ( Status == STATUS_PENDING )
    {
      DPRINT("AtaXSendInquiry: Waiting for the driver to process request...\n");
      KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
      Status = IoStatusBlock.Status;
    }

    DPRINT("AtaXSendInquiry: Request processed by driver, status = 0x%08X\n", Status);

    if ( SRB_STATUS(Srb.SrbStatus) == SRB_STATUS_SUCCESS )
    {
      RtlCopyMemory(&AtaXChannelFdoExtension->InquiryData[DeviceNumber],
                    InquiryBuffer,
                    INQUIRYDATABUFFERSIZE);

      Status = STATUS_SUCCESS;
      KeepTrying = FALSE;
    }
    else
    {
      DPRINT("AtaXSendInquiry: Inquiry SRB failed with SrbStatus 0x%08X\n", Srb.SrbStatus);
      DPRINT("AtaXSendInquiry: SenseBuffer->SenseKey - %x\n", SenseBuffer->SenseKey);

      if ( Srb.SrbStatus & SRB_STATUS_QUEUE_FROZEN )
      {
        KeepTrying = FALSE;

        DPRINT("AtaXSendInquiry: the queue is frozen at TargetId %d\n", Srb.TargetId);

        AtaXDevicePdoExtension->Flags &= ~LUNEX_FROZEN_QUEUE;

        KeAcquireSpinLock(&AtaXChannelFdoExtension->SpinLock, &Irql);
        AtaXGetNextRequest(AtaXChannelFdoExtension, AtaXDevicePdoExtension);
        KeLowerIrql(Irql);
      }

      if ( SRB_STATUS(Srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN )
      {
        DPRINT("AtaXSendInquiry: Data overrun at TargetId %d\n", Srb.TargetId);

        RtlCopyMemory(&AtaXChannelFdoExtension->InquiryData,
                      InquiryBuffer,
                      (Srb.DataTransferLength > INQUIRYDATABUFFERSIZE) ?
                      INQUIRYDATABUFFERSIZE : Srb.DataTransferLength);

        Status = STATUS_SUCCESS;
        KeepTrying = FALSE;
      }
      else if ( (Srb.SrbStatus & SRB_STATUS_AUTOSENSE_VALID) &&
                 SenseBuffer->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST )
      {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        KeepTrying = FALSE;
      }
      else
      {
        if ( (RetryCount < 2) &&
             (SRB_STATUS(Srb.SrbStatus) != SRB_STATUS_NO_DEVICE) &&
             (SRB_STATUS(Srb.SrbStatus) != SRB_STATUS_SELECTION_TIMEOUT) )
        {
          RetryCount++;
          KeepTrying = TRUE;
        }
        else
        {
          KeepTrying = FALSE;

          if ( SRB_STATUS(Srb.SrbStatus) == SRB_STATUS_BAD_FUNCTION ||
               SRB_STATUS(Srb.SrbStatus) == SRB_STATUS_BAD_SRB_BLOCK_LENGTH )
          {
            Status = STATUS_INVALID_DEVICE_REQUEST;
          }
          else
          {
            Status = STATUS_IO_DEVICE_ERROR;
          }
        }
      }
    }
  }

  ExFreePool(InquiryBuffer);
  ExFreePool(SenseBuffer);

  DPRINT("AtaXSendInquiry: done with Status 0x%08X\n", Status);
  return Status;
}

BOOLEAN
AtaXIssueIdentify(
    IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension,
    IN PIDENTIFY_DATA Identify,
    IN ULONG DeviceNumber,
    IN UCHAR Command)
{
  PATAX_REGISTERS_1  AtaXRegisters1;
  PATAX_REGISTERS_2  AtaXRegisters2;
  UCHAR              StatusByte;
  UCHAR              DiagnosticCode;
  UCHAR              Signature1, Signature2;
  UCHAR              Signature3, Signature4;
  UCHAR              SignatureDeviceNumber;
  ULONG              WaitCount = 20000;
  ULONG              ix, jx;

  DPRINT("AtaXIssueIdentify: AtaXChannelFdoExtension - %p, DeviceNumber - %x, Command - %x\n", AtaXChannelFdoExtension, DeviceNumber, Command);

  AtaXRegisters1 = &AtaXChannelFdoExtension->BaseIoAddress1;
  AtaXRegisters2 = &AtaXChannelFdoExtension->BaseIoAddress2;

  // �������� ���������� �� ������ DeviceNumber
  WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect, (UCHAR)(((DeviceNumber & 1) << 4) | IDE_DRIVE_SELECT));

  // ��������� ������� ��������� � ���������� ����������
  StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);

  // ���� ������� IDE_COMMAND_IDENTIFY - ATA, ����� (IDE_COMMAND_ATAPI_IDENTIFY) - ATAPI
  if ( Command == IDE_COMMAND_IDENTIFY )
  {
    // ��������� ���� ������ �� �������� �������
    StatusByte &= ~(IDE_STATUS_ERROR | IDE_STATUS_INDEX);
    DPRINT("AtaXIssueIdentify: Checking for ATA. Status (%x)\n", StatusByte);

    if ( StatusByte != IDE_STATUS_IDLE )
    {
      // ������ ����������� ����� ����������
      AtaXSoftReset(AtaXRegisters1, AtaXRegisters2, DeviceNumber);
      // �������� ���������� �� ������ DeviceNumber
      WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect, (UCHAR)(((DeviceNumber & 1) << 4) | IDE_DRIVE_SELECT));
      AtaXWaitOnBusy(AtaXRegisters2);

      // ��������� �������������� ������� ���������
      StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);

      // ����� ������ ������ ��� ���������� ������� ����������� ���������� 
      // � ����� ��������� ��������� �������� ���������, ������������ ��� ���
      // (���������� ��������� ��������: ATA, SCSI � ������. ������ ���.)
      DiagnosticCode        = READ_PORT_UCHAR(AtaXRegisters1->Error);
      Signature1            = READ_PORT_UCHAR(AtaXRegisters1->SectorCount);
      Signature2            = READ_PORT_UCHAR(AtaXRegisters1->LowLBA);
      Signature3            = READ_PORT_UCHAR(AtaXRegisters1->MidLBA);
      Signature4            = READ_PORT_UCHAR(AtaXRegisters1->HighLBA);
      SignatureDeviceNumber = READ_PORT_UCHAR(AtaXRegisters1->DriveSelect);
  	
      DPRINT("AtaXIssueIdentify: Signature1  - %x, Signature2 - %x,  Signature3  - %x, Signature4 - %x\n", Signature1, Signature2, Signature3, Signature4);
      DPRINT("AtaXIssueIdentify: SignatureDeviceNumber  - %x, DiagnosticCode - %x\n", SignatureDeviceNumber, DiagnosticCode);

      if ( Signature3 == 0x14 && Signature4 == 0xEB ) // ��������� Atapi-���������� 
        return FALSE;

      DPRINT("AtaXIssueIdentify: Resetting channel\n");
      WRITE_PORT_UCHAR(AtaXRegisters2->AlternateStatus, IDE_DC_RESET_CONTROLLER );
      KeStallExecutionProcessor(50 * 1000);
      WRITE_PORT_UCHAR(AtaXRegisters2->AlternateStatus, IDE_DC_REENABLE_CONTROLLER);

      // ������������ ATA ��������� Master-���������� �������� �� 31 �������! (30 ������ ��� Slave-����������)
      do
      {
        KeStallExecutionProcessor(100);
        StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
      } while ( (StatusByte & IDE_STATUS_BUSY) && WaitCount-- );

      // �������� ���������� �� ������ DeviceNumber
      WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect, (UCHAR)(((DeviceNumber & 1) << 4) | IDE_DRIVE_SELECT));

      // ��� ��������� ��������� (��������� ������ Atapi �� ���������� ��������� ����� ������������ ������)
      Signature3 = READ_PORT_UCHAR(AtaXRegisters1->MidLBA);
      Signature4 = READ_PORT_UCHAR(AtaXRegisters1->HighLBA);

      if ( Signature3 == 0x14 && Signature4 == 0xEB ) // ��������� Atapi-���������� 
        return FALSE;
      
      StatusByte &= ~IDE_STATUS_INDEX;  // ��������� ��������� ��� �� �������� �������
      DPRINT("AtaXIssueIdentify: StatusByte - %p\n", StatusByte);
      if ( StatusByte != IDE_STATUS_IDLE )
        return FALSE;
    }
  }

  DPRINT("AtaXIssueIdentify: Checking for ATAPI. Status (%x)\n", StatusByte);

  // ������������� � HighLBA � � MidLBA �������� 
  // ������ ��������� IDENTIFY_DATA (������� ���������� - 512 ���� (��� 256 ����))
  WRITE_PORT_UCHAR(AtaXRegisters1->MidLBA,  (sizeof(IDENTIFY_DATA) & 0xFF));
  WRITE_PORT_UCHAR(AtaXRegisters1->HighLBA, (sizeof(IDENTIFY_DATA) >> 8));

  for ( jx = 0; jx < 2; jx++ )
  {
    AtaXWaitOnBusy(AtaXRegisters2);

    //StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
    if ( Command == IDE_COMMAND_IDENTIFY )        DPRINT("AtaXIssueIdentify: Send ATA IDENTIFY command.\n");
    if ( Command == IDE_COMMAND_ATAPI_IDENTIFY )  DPRINT("AtaXIssueIdentify: Send ATAPI IDENTIFY command.\n");

    // ���������� ATA/ATAPI ���������� IDENTIFY command
    WRITE_PORT_UCHAR(AtaXRegisters1->Command, Command);

    // �������� ���������� ����������
    for ( ix = 0; ix < 4; ix++ )
    {
      AtaXWaitForDrq(AtaXRegisters2);
      // ��������� �������������� ������� ���������
      StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
      DPRINT("AtaXIssueIdentify: StatusByte (%x)\n", StatusByte);

      if ( StatusByte & IDE_STATUS_DRQ )  // ���� ���������� ������
      {
        // ��������� ������� ��������� � ���������� ����������
        StatusByte = READ_PORT_UCHAR(AtaXRegisters1->Status);
        DPRINT("AtaXIssueIdentify: Checking for DRQ. Status (%x)\n", StatusByte);

        // ��������� �������� ���������
        Signature3 = READ_PORT_UCHAR(AtaXRegisters1->MidLBA);
        Signature4 = READ_PORT_UCHAR(AtaXRegisters1->HighLBA);
        if ( Signature3 == 0x14 && Signature4 == 0xEB )  // ���� ��������� Atapi-���������� 
          return FALSE; 

        break;  //Ok
      }

      if ( Command == IDE_COMMAND_IDENTIFY )  // ���� ATA-����������
      {
        // ��������� �������� ��������� (���� ���������� �� ������)
        Signature3 = READ_PORT_UCHAR(AtaXRegisters1->MidLBA);
        Signature4 = READ_PORT_UCHAR(AtaXRegisters1->HighLBA);
        if ( Signature3 == 0x14 && Signature4 == 0xEB )  // ���� ��������� Atapi-���������� 
          return FALSE; 
      }

      AtaXWaitOnBusy(AtaXRegisters2);
      // ��������� �������������� ������� ���������
      StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
    }

    if ( ix == 4 && jx == 0 )
    {
      // ��� ����������� ������ �� ���������� ����� ������ �������, �� ����� ��� ���� ����
      DPRINT("AtaXIssueIdentify: DRQ never asserted (%x). Error reg (%x)\n", StatusByte, READ_PORT_UCHAR(AtaXRegisters1->Error));


      // ������ ����������� ����� ����������
      AtaXSoftReset(AtaXRegisters1, AtaXRegisters2, DeviceNumber);
      // ��������� �������������� ������� ���������
      StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);

      DPRINT("AtaXIssueIdentify: StatusByte after soft reset - %p\n", StatusByte);
    }
    else
    {
      break;
    }
  }

  if ( (Command == IDE_COMMAND_IDENTIFY) && (StatusByte & IDE_STATUS_ERROR) )
    return FALSE;
 

  AtaXWaitOnBusy(AtaXRegisters2);
  // ��������� �������������� ������� ���������
  StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
  DPRINT("AtaXIssueIdentify: Status before read words - %x\n", StatusByte);

  if ( !(StatusByte & IDE_STATUS_DRQ) ) // ���� ���������� �� ������
    return FALSE;

  // ��������� ������� ���������� � ����� Identify
  READ_PORT_BUFFER_USHORT(AtaXRegisters1->Data, (PUSHORT)Identify, sizeof(IDENTIFY_DATA) / 2); // 0x200/2 = 0x100 (512/2 = 256)

  // ��������� �������������� ������� ���������
  StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
  DPRINT("AtaXIssueIdentify: Status before read words - %x\n", StatusByte);


  if ( StatusByte & IDE_STATUS_ERROR ) // ���� ������
    return FALSE;

  ///*
  DPRINT("Identify->GeneralConfiguration;       /* 000 */ - %x\n", Identify->GeneralConfiguration);     //useful if the device is not a hard disk
  DPRINT("Identify->NumCylinders                /* 002 */ - %x\n", Identify->NumCylinders);
  DPRINT("Identify->Reserved1                   /* 004 */ - %x\n", Identify->Reserved1);
  DPRINT("Identify->NumHeads                    /* 006 */ - %x\n", Identify->NumHeads);
  DPRINT("Identify->UnformattedBytesPerTrack    /* 008 */ - %x\n", Identify->UnformattedBytesPerTrack);
  DPRINT("Identify->UnformattedBytesPerSector   /* 010 */ - %x\n", Identify->UnformattedBytesPerSector);
  DPRINT("Identify->NumSectorsPerTrack          /* 012 */ - %x\n", Identify->NumSectorsPerTrack);

  DPRINT("Identify->BufferType                  /* 040 */ - %x\n", Identify->BufferType);
  DPRINT("Identify->BufferSectorSize            /* 042 */ - %x\n", Identify->BufferSectorSize);
  DPRINT("Identify->NumberOfEccBytes            /* 044 */ - %x\n", Identify->NumberOfEccBytes);

  DPRINT("Identify->MaximumBlockTransfer        /* 094 */ - %x\n", Identify->MaximumBlockTransfer);
  DPRINT("Identify->VendorUnique2               /* 095 */ - %x\n", Identify->VendorUnique2);
  DPRINT("Identify->DoubleWordIo                /* 096 */ - %x\n", Identify->DoubleWordIo);
  DPRINT("Identify->Capabilities                /* 098 */ - %x\n", Identify->Capabilities);
  DPRINT("Identify->Reserved2                   /* 100 */ - %x\n", Identify->Reserved2);
  DPRINT("Identify->VendorUnique3               /* 102 */ - %x\n", Identify->VendorUnique3);
  DPRINT("Identify->PioCycleTimingMode          /* 103 */ - %x\n", Identify->PioCycleTimingMode);
  DPRINT("Identify->VendorUnique4               /* 104 */ - %x\n", Identify->VendorUnique4);
  DPRINT("Identify->DmaCycleTimingMode          /* 105 */ - %x\n", Identify->DmaCycleTimingMode );
  DPRINT("Identify->TranslationFieldsValid      /* 106 */ - %x\n", Identify->TranslationFieldsValid);
  DPRINT("Identify->NumberOfCurrentCylinders    /* 108 */ - %x\n", Identify->NumberOfCurrentCylinders);
  DPRINT("Identify->NumberOfCurrentHeads        /* 110 */ - %x\n", Identify->NumberOfCurrentHeads);
  DPRINT("Identify->CurrentSectorsPerTrack      /* 112 */ - %x\n", Identify->NumberOfCurrentHeads);
  DPRINT("Identify->CurrentSectorCapacity       /* 114 */ - %x\n", Identify->CurrentSectorCapacity);
  DPRINT("Identify->CurrentMultiSectorSetting   /* 118 */ - %x\n", Identify->CurrentMultiSectorSetting);
  DPRINT("Identify->UserAddressableSectors      /* 120 */ - %x\n", Identify->UserAddressableSectors);  // total number of 28-bit LBA addressable sectors on the drive (if non-zero, the drive supports LBA28) 
  DPRINT("Identify->SingleWordDMASupport        /* 124 */ - %x\n", Identify->SingleWordDMASupport);
  DPRINT("Identify->SingleWordDMAActive         /* 125 */ - %x\n", Identify->SingleWordDMAActive);
  DPRINT("Identify->MultiWordDMASupport         /* 126 */ - %x\n", Identify->MultiWordDMASupport);
  DPRINT("Identify->MultiWordDMAActive          /* 127 */ - %x\n", Identify->MultiWordDMAActive);
  DPRINT("Identify->AdvancedPIOModes            /* 128 */ - %x\n", Identify->AdvancedPIOModes);
  DPRINT("Identify->Reserved4                   /* 129 */ - %x\n", Identify->Reserved4);
  DPRINT("Identify->MinimumMWXferCycleTime      /* 130 */ - %x\n", Identify->MinimumMWXferCycleTime);
  DPRINT("Identify->RecommendedMWXferCycleTime  /* 132 */ - %x\n", Identify->RecommendedMWXferCycleTime);
  DPRINT("Identify->MinimumPIOCycleTime         /* 134 */ - %x\n", Identify->MinimumPIOCycleTime);
  DPRINT("Identify->MinimumPIOCycleTimeIORDY    /* 136 */ - %x\n", Identify->MinimumPIOCycleTimeIORDY);

  DPRINT("Identify->MajorRevision               /* 160 */ - %x\n", Identify->MajorRevision);
  DPRINT("Identify->MinorRevision               /* 162 */ - %x\n", Identify->MinorRevision);
  DPRINT("Identify->Reserved6                   /* 164 */ - %x\n", Identify->Reserved6);
  DPRINT("Identify->CommandSetSupport           /* 166 */ - %x\n", Identify->CommandSetSupport); // bit 10 set if supports LBA48
  DPRINT("Identify->Reserved6a[0]               /* 168 */ - %x\n", Identify->Reserved6a[0]);
  DPRINT("Identify->Reserved6a[1]               /* 170 */ - %x\n", Identify->Reserved6a[1]);
  DPRINT("Identify->CommandSetActive            /* 172 */ - %x\n", Identify->CommandSetActive);
  DPRINT("Identify->Reserved6b                  /* 174 */ - %x\n", Identify->Reserved6b);
  DPRINT("Identify->UltraDMASupport             /* 176 */ - %x\n", Identify->UltraDMASupport);  // supported UDMA modes
  DPRINT("Identify->UltraDMAActive              /* 177 */ - %x\n", Identify->UltraDMAActive);   // which UDMA mode is active

  DPRINT("Identify->Reserved7[0]                /* 178 */ - %x\n", Identify->Reserved7[0]);
  DPRINT("Identify->Reserved7[1]                /* 180 */ - %x\n", Identify->Reserved7[1]);
  DPRINT("Identify->Reserved7[2]                /* 182 */ - %x\n", Identify->Reserved7[2]);
  DPRINT("Identify->Reserved7[3]                /* 184 */ - %x\n", Identify->Reserved7[3]);
  DPRINT("Identify->Reserved7[4]                /* 186 */ - %x\n", Identify->Reserved7[4]);  // bit 12 set if detects 80-pin cable (from a master drive)
  DPRINT("Identify->Reserved7[5]                /* 188 */ - %x\n", Identify->Reserved7[5]);
  DPRINT("Identify->Reserved7[6]                /* 190 */ - %x\n", Identify->Reserved7[6]);
  DPRINT("Identify->Reserved7[7]                /* 192 */ - %x\n", Identify->Reserved7[7]);
  DPRINT("Identify->Reserved7[8]                /* 194 */ - %x\n", Identify->Reserved7[8]);
  DPRINT("Identify->Reserved7[9]                /* 196 */ - %x\n", Identify->Reserved7[9]);
  DPRINT("Identify->Reserved7[10]               /* 198 */ - %x\n", Identify->Reserved7[10]);

  DPRINT("Identify->Max48BitLBA[0]              /* 200 */ - %x\n", Identify->Max48BitLBA[0]); // contain LowPart of 48-bit LBA on the drive (probably also proof that LBA48 is supported)
  DPRINT("Identify->Max48BitLBA[1]              /* 204 */ - %x\n", Identify->Max48BitLBA[1]); // contain HightPart of 48-bit LBA 

  DPRINT("Identify->LastLun                     /* 252 */ - %x\n", Identify->LastLun);
  DPRINT("Identify->Reserved8                   /*  -  */ - %x\n", Identify->Reserved8);
  DPRINT("Identify->MediaStatusNotification     /* 254 */ - %x\n", Identify->MediaStatusNotification);
  DPRINT("Identify->Reserved9                   /* 254 */ - %x\n", Identify->MediaStatusNotification);
  DPRINT("Identify->DeviceWriteProtect          /* 255 */ - %x\n", Identify->DeviceWriteProtect);
  DPRINT("Identify->Reserved10                  /* 255 */ - %x\n", Identify->Reserved10);
  //*/


  DPRINT(" AtaXIssueIdentify return - TRUE\n");
  return TRUE;
}

BOOLEAN
AtaXDetectDevices(IN PFDO_CHANNEL_EXTENSION AtaXChannelFdoExtension)
{
  PATAX_REGISTERS_1  AtaXRegisters1;
  PATAX_REGISTERS_2  AtaXRegisters2;
  ULONG              DeviceNumber;
  UCHAR              StatusByte;
  UCHAR              DiagnosticCode;
  UCHAR              Signature1 = 0;
  UCHAR              Signature2 = 0;
  UCHAR              Signature3 = 0;
  UCHAR              Signature4 = 0;
  UCHAR              SignatureDeviceNumber;
  BOOLEAN            DeviceResponded = FALSE;
  ULONG              ix;

  DPRINT("AtaXDetectDevices (%p)\n", AtaXChannelFdoExtension);

  ASSERT(AtaXChannelFdoExtension);

  AtaXRegisters1 = &AtaXChannelFdoExtension->BaseIoAddress1;
  AtaXRegisters2 = &AtaXChannelFdoExtension->BaseIoAddress2;

  AtaXChannelFdoExtension->ExpectingInterrupt = FALSE;  // ���������� "��������� ����������"
  AtaXChannelFdoExtension->CurrentSrb         = NULL;   // ��� �������� SRB

  ix = MAX_IDE_DEVICE;

  // ����� ��������� �� ������
  for ( DeviceNumber = 0; DeviceNumber < ix; DeviceNumber++ )
  {
    // �������� ���������� �� ������ DeviceNumber
    WRITE_PORT_UCHAR(AtaXRegisters1->DriveSelect,
                    (UCHAR)(((DeviceNumber & 1) << 4) | IDE_DRIVE_SELECT));

    StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus); // ��������� �������������� ������� ���������
    DPRINT("AtaXDetectDevices: StatusByte - %x\n", StatusByte);

    if ( StatusByte == 0xFF )
      continue;

    AtaXSoftReset(AtaXRegisters1, AtaXRegisters2, DeviceNumber); // ������ ����������� �����
    AtaXWaitOnBusy(AtaXRegisters2);

    // ����� ������ ������ ��� ���������� ������� ����������� ���������� 
    // � ����� ��������� ��������� �������� ���������, ������������ ��� ���
    // (��������: "���������� ��������� ��������: ATA, SCSI � ������." ������ ���.)
    DiagnosticCode        = READ_PORT_UCHAR(AtaXRegisters1->Error);
    Signature1            = READ_PORT_UCHAR(AtaXRegisters1->SectorCount);
    Signature2            = READ_PORT_UCHAR(AtaXRegisters1->LowLBA);
    Signature3            = READ_PORT_UCHAR(AtaXRegisters1->MidLBA);     //ByteCountLow
    Signature4            = READ_PORT_UCHAR(AtaXRegisters1->HighLBA);    //ByteCountHigh
    SignatureDeviceNumber = READ_PORT_UCHAR(AtaXRegisters1->DriveSelect);
	
    DPRINT("AtaXDetectDevices: Signature1  - %x, Signature2 - %x\n", Signature1, Signature2);
    DPRINT("AtaXDetectDevices: Signature3  - %x, Signature4 - %x\n", Signature3, Signature4);
    DPRINT("AtaXDetectDevices: SignatureDeviceNumber  - %x\n", SignatureDeviceNumber);
    DPRINT("AtaXDetectDevices: DiagnosticCode         - %x\n", DiagnosticCode);

    if ( !DiagnosticCode )
      continue;

    if ( Signature3 == 0x14 && Signature4 == 0xEB ) // ��������� PATA ATAPI
    {
      /*
       ������� IDENTIFY PACKET DEVICE ���������� ���� �� ��������� ��������������� ���������� �� 
       ����������, ������� ������������ �������� �������. (ata-6_russian 8.17.8) */

      // ��������� ������� ���������� � ���������� ���������� ������
      if ( AtaXIssueIdentify(AtaXChannelFdoExtension,
                             &AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber],
                             DeviceNumber,
                             IDE_COMMAND_ATAPI_IDENTIFY) )
      {
        DPRINT("AtaXDetectDevices: device %x is PATA ATAPI\n", DeviceNumber);
        AtaXChannelFdoExtension->DeviceFlags[DeviceNumber] |= DFLAGS_DEVICE_PRESENT;
        AtaXChannelFdoExtension->DeviceFlags[DeviceNumber] |= DFLAGS_ATAPI_DEVICE;
        DPRINT("AtaXDetectDevices: AtaXChannelFdoExtension->DeviceFlags[%x] - %p\n", DeviceNumber, AtaXChannelFdoExtension->DeviceFlags[DeviceNumber]);

        DeviceResponded = TRUE;

        StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
        if ( StatusByte == IDE_STATUS_ERROR )
        {
          AtaXSoftReset(AtaXRegisters1, AtaXRegisters2, DeviceNumber);    // ������ ����������� �����
        }

        AtaXSendInquiry(AtaXChannelFdoExtension, DeviceNumber);

        goto Responded;
      }
      else
      {
        // ��� ����������
        DPRINT("AtaXDetectDevices: device %x not detected at PATA ATAPI device\n", DeviceNumber);
        //ObDereferenceObject(AtaXChannelFdoExtension->AtaXDevicePdo[DeviceNumber]);
        //IoDeleteDevice(AtaXChannelFdoExtension->AtaXDevicePdo[DeviceNumber]);
        AtaXChannelFdoExtension->DeviceFlags[DeviceNumber] &= ~DFLAGS_DEVICE_PRESENT;
        continue;
      }
    }
    else if ( Signature3 == 0x3C && Signature4 == 0xC3 ) // ��������� SATA ATA
    {
      DPRINT("AtaXDetectDevices: device %x is SATA - FIXME\n", DeviceNumber);
      continue;
    }
    else if ( Signature3 == 0x69 && Signature4 == 0x96 ) // ��������� SATA ATAPI
    {
      DPRINT("AtaXDetectDevices: device %x is SATA ATAPI - FIXME\n", DeviceNumber);
      continue;
    }
    else //if (Signature3 == 0 && Signature4 == 0)
    {
      // ��������� ATA
      /*
       ������� IDENTIFY DEVICE ���������� ���� ��� ��������� ��������������� ���������� �� ����������.
       ����� ������� ������, ���������� ������������� ��� BSY � �������, �������������� � �������� 256 ���� 
       ����������������� ���������� ����������, ������������� ��� DRQ � �������, ������� ��� BSY � ����,
       � ���������� ������ INTRQ ���� nIEN ������ � ����. (ata-6_russian 8.16.8)*/

      // ��������� ������� ���������� � ���������� ���������� ������
      if ( AtaXIssueIdentify(AtaXChannelFdoExtension,
                             &AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber],
                             DeviceNumber,
                             IDE_COMMAND_IDENTIFY) )
      {
        DPRINT("AtaXDetectDevices: device %x is PATA\n", DeviceNumber);
        AtaXChannelFdoExtension->DeviceFlags[DeviceNumber] |= DFLAGS_DEVICE_PRESENT;
        DeviceResponded = TRUE;

        StatusByte = READ_PORT_UCHAR(AtaXRegisters2->AlternateStatus);
        if ( StatusByte == IDE_STATUS_ERROR )
        {
          AtaXSoftReset(AtaXRegisters1, AtaXRegisters2, DeviceNumber); // ������ ����������� �����
        }
      }
      else
      {
        // ��� ����������
        DPRINT("AtaXDetectDevices: device %x not detected at PATA device\n", DeviceNumber);
        //ObDereferenceObject(AtaXChannelFdoExtension->AtaXDevicePdo[DeviceNumber]);
        //IoDeleteDevice(AtaXChannelFdoExtension->AtaXDevicePdo[DeviceNumber]);
        AtaXChannelFdoExtension->DeviceFlags[DeviceNumber] &= ~DFLAGS_DEVICE_PRESENT;
        continue;
      }
    }

Responded:
    if ( DeviceResponded )
    {
      ///*
      DPRINT("FullIdentifyData.GeneralConfiguration       - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].GeneralConfiguration);
      DPRINT("FullIdentifyData.NumCylinders               - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].NumCylinders);
      DPRINT("FullIdentifyData.NumHeads                   - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].NumHeads);
      DPRINT("FullIdentifyData.NumSectorsPerTrack         - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].NumSectorsPerTrack);
      DPRINT("FullIdentifyData.RecommendedMWXferCycleTime - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].RecommendedMWXferCycleTime);
      DPRINT("FullIdentifyData.PioCycleTimingMode         - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].PioCycleTimingMode);
      DPRINT("FullIdentifyData.UltraDMASupport            - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].UltraDMASupport);
      DPRINT("FullIdentifyData.UltraDMAActive             - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].UltraDMAActive);
      DPRINT("FullIdentifyData.DmaCycleTimingMode         - %x\n", AtaXChannelFdoExtension->FullIdentifyData[DeviceNumber].DmaCycleTimingMode);
      //*/

      AtaXDeviceSetup(AtaXChannelFdoExtension, DeviceNumber);
    }
    DPRINT("\n");
  }

  DPRINT("AtaXDetectDevices return   - %x \n", DeviceResponded);
  return DeviceResponded;
}
