#include "atax.h"               

//#define NDEBUG
#include <debug.h>


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
