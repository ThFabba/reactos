#ifndef _ATAX_PCH_
#define _ATAX_PCH_

#include <ntddk.h>
#include <stdio.h>
#include <wdm.h>
#include <ide.h>
#include <initguid.h>
#include <wdmguid.h>


//
// ����������e ���������� ����������
//

extern ULONG AtaXDeviceCounter;  // ��������� ���������
extern ULONG AtaXChannelCounter; // ��������� �������

//
// ����������e ��������
//

typedef struct _ATAX_REGISTERS_1 {        //// ������ ����� ��������� ��������� IDE ����������

  PUSHORT   Data;             //0    Read/Write data register is 16 bits PIO data transfer (if DWordIO - 32 bits - ?)

  union  {                    //1
    PUCHAR  Error;                // When read
    PUCHAR  Features;             // When written 
  };

  union  {                    //2
    PUCHAR  SectorCount;          // When read (non PACKET command). When written
    PUCHAR  InterruptReason;      // When read. PACKET command 
  };

  PUCHAR    LowLBA;           //3    Read/Write

  union  {                    //4
    PUCHAR  MidLBA;               // Read/Write non PACKET command
    PUCHAR  ByteCountLow;         // Read/Write PACKET command
  };

  union  {                    //5
    PUCHAR  HighLBA;              // Read/Write non PACKET command
    PUCHAR  ByteCountHigh;        // Read/Write PACKET command
  };

  PUCHAR    DriveSelect;      //6    Read/Write

  union  {                    //7
    PUCHAR  Status;               // When read
    PUCHAR  Command;              // When write 
  };

} ATAX_REGISTERS_1, *PATAX_REGISTERS_1;

typedef struct _ATAX_REGISTERS_2 {        //// ������ ����� ��������� ���������� IDE ����������

  union  {                    //0
    PUCHAR  AlternateStatus;      // When read
    PUCHAR  DeviceControl;        // When write
  };

  PUCHAR    DriveAddress;     //1 (not used)

} ATAX_REGISTERS_2, *PATAX_REGISTERS_2;

typedef struct _COMMON_ATAX_DEVICE_EXTENSION { 

  PDEVICE_OBJECT  LowerDevice; // ���� ���� � ����� ���� Filter DO, �� ��������� �� Filter DO, ���� ���, �� ��������� �� ������ PDO. (������ ��� FDO. ��� PDO ����� NULL)
  PDEVICE_OBJECT  LowerPdo;    // ��������� �� ������ (� �����) ������ PDO
  PDRIVER_OBJECT  SelfDriver;  // ��������� �� ����������� ������ �������� (�� AtaX)
  PDEVICE_OBJECT  SelfDevice;  // ��������� �� ����������� ������ ���������� (PDO ���� FDO)
  BOOLEAN         IsFDO;       // TRUE - ���� ��� ���������� FDO, FALSE - PDO

} COMMON_ATAX_DEVICE_EXTENSION, *PCOMMON_ATAX_DEVICE_EXTENSION;

typedef struct _FDO_CHANNEL_EXTENSION {                   //// FDO ���������� AtaXChannel

  COMMON_ATAX_DEVICE_EXTENSION   CommonExtension;          // ����� � ��� PDO � ��� FDO ����������

  ULONG                    Channel;                           // ����� ������
  ATAX_REGISTERS_1         BaseIoAddress1;                    // ������ ������� (��� ������) ��� ����� ��������� ���������
  ATAX_REGISTERS_2         BaseIoAddress2;                    // ������ ������� (��� ������) ��� ����� ��������� ���������� (������������ ������ ������)

  // Interfaces
  PBUS_INTERFACE_STANDARD  BusInterface;

  // IoConnectInterrupt() 
  PKINTERRUPT              InterruptObject;
  ULONG                    InterruptLevel;
  ULONG                    InterruptVector;
  USHORT                   InterruptFlags;
  UCHAR                    InterruptShareDisposition;
  UCHAR                    Padded1;
  KAFFINITY                InterruptAffinity;


} FDO_CHANNEL_EXTENSION, *PFDO_CHANNEL_EXTENSION;

//
// ����������� �������
//

// atax.c

// ataxfdo.c
NTSTATUS
AtaXChannelFdoDispatchPnp(
    IN PDEVICE_OBJECT AtaXChannelFdo,
    IN PIRP Irp);

NTSTATUS NTAPI
AddChannelFdo(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT ChannelPdo);

#endif /* _ATAX_PCH_ */
