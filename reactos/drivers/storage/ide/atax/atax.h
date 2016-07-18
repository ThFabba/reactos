#ifndef _ATAX_PCH_
#define _ATAX_PCH_

#include <ntddk.h>
#include <stdio.h>
#include <wdm.h>
#include <ide.h>
#include <srb.h>
#include <scsi.h>
#include <initguid.h>
#include <wdmguid.h>
#include <..\bmaster.h>


//
// ����������e ���������� ����������
//

extern ULONG AtaXDeviceCounter;  // ��������� ���������
extern ULONG AtaXChannelCounter; // ��������� �������

//
// ����������� ������ IDE ����������
//
#define IDE_DRIVE_SELECT             0xA0

//
// Flags
//
#define ATAX_DISCONNECT_ALLOWED      0x00001000

//
// ����������� ������ ��� ���������� IDE ���������
//
#define DFLAGS_DEVICE_PRESENT        0x0001    // Indicates that some device is present.
#define DFLAGS_ATAPI_DEVICE          0x0002    // Indicates whether Atapi commands can be used.
#define DFLAGS_TAPE_DEVICE           0x0004    // Indicates whether this is a tape device.
#define DFLAGS_INT_DRQ               0x0008    // Indicates whether device interrupts as DRQ is set after receiving Atapi Packet Command
#define DFLAGS_REMOVABLE_DRIVE       0x0010    // Indicates that the drive has the 'removable' bit set in identify data (offset 128)
#define DFLAGS_MEDIA_STATUS_ENABLED  0x0020    // Media status notification enabled
#define DFLAGS_ATAPI_CHANGER         0x0040    // Indicates atapi 2.5 changer present.
#define DFLAGS_SANYO_ATAPI_CHANGER   0x0080    // Indicates multi-platter device, not conforming to the 2.5 spec.
#define DFLAGS_CHANGER_INITED        0x0100    // Indicates that the init path for changers has already been done.
#define DFLAGS_USE_DMA               0x0200    // Indicates that device use DMA mode (if FALSE - PIO mode)

//
// ����������� ������ ��� ATA/ATAPI ��������� 
//
#define IDE_COMMAND_READ             0x20
#define IDE_COMMAND_WRITE            0x30
#define IDE_COMMAND_READ_MULTIPLE    0xC4
#define IDE_COMMAND_WRITE_MULTIPLE   0xC5
#define IDE_COMMAND_SET_MULTIPLE     0xC6
#define IDE_COMMAND_READ_DMA         0xC8
#define IDE_COMMAND_WRITE_DMA        0xCA

#define IDE_COMMAND_GET_MEDIA_STATUS 0xDA

#define IDE_COMMAND_ATAPI_RESET      0x08
#define IDE_COMMAND_ATAPI_PACKET     0xA0
#define IDE_COMMAND_ATAPI_IDENTIFY   0xA1
#define IDE_COMMAND_IDENTIFY         0xEC

#define IDE_COMMAND_SET_FEATURES     0xEF

//
// ����������� ������ ��� ATAPI ���������
//
#define ATAPI_MODE_SENSE             0x5A
#define ATAPI_MODE_SELECT            0x55
#define ATAPI_FORMAT_UNIT            0x24

//
// ����������� ��� ���������� ATA/ATAPI ������������
//
#define IDE_DC_REENABLE_CONTROLLER   0x00  // ��������� ���������� ��� ������
#define IDE_DC_DISABLE_INTERRUPTS    0x02  // ��������� ���������� ��� ������
#define IDE_DC_RESET_CONTROLLER      0x04  // �������� �����

//
// ����������� ������� ��� ATA/ATAPI ���������
//
#define IDE_STATUS_BUSY              0x80
#define IDE_STATUS_IDLE              0x50
#define IDE_STATUS_DRDY              0x40
#define IDE_STATUS_DSC               0x10
#define IDE_STATUS_DRQ               0x08
#define IDE_STATUS_CORRECTED_ERROR   0x04
#define IDE_STATUS_INDEX             0x02
#define IDE_STATUS_ERROR             0x01

//
// LUN Extension flags
//
#define LUNEX_FROZEN_QUEUE           0x0001
#define LUNEX_NEED_REQUEST_SENSE     0x0004
#define LUNEX_BUSY                   0x0008
#define LUNEX_FULL_QUEUE             0x0010
#define LUNEX_REQUEST_PENDING        0x0020

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

typedef struct _HW_DEVICE_EXTENSION {                     //// ���������� ���������� ��������� ������

  PCIIDE_TRANSFER_MODE_SELECT  TransferInfo;               // ���������� � ������� ��������� ������

  // Controller properties
  PIDE_CONTROLLER_PROPERTIES   ControllerProperties;       // !!! 1.��������� �� ���������������� ���������� IDE ����������� 
  PUCHAR                       MiniControllerExtension;    // !!! 2.��������� �� ���������� ���������� ������������ IDE ����������� (������ ������ ����� ����� ControllerProperties)

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

typedef struct _COMMON_ATAX_DEVICE_EXTENSION { 

  PDEVICE_OBJECT  LowerDevice; // ���� ���� � ����� ���� Filter DO, �� ��������� �� Filter DO, ���� ���, �� ��������� �� ������ PDO. (������ ��� FDO. ��� PDO ����� NULL)
  PDEVICE_OBJECT  LowerPdo;    // ��������� �� ������ (� �����) ������ PDO
  PDRIVER_OBJECT  SelfDriver;  // ��������� �� ����������� ������ �������� (�� AtaX)
  PDEVICE_OBJECT  SelfDevice;  // ��������� �� ����������� ������ ���������� (PDO ���� FDO)
  BOOLEAN         IsFDO;       // TRUE - ���� ��� ���������� FDO, FALSE - PDO

} COMMON_ATAX_DEVICE_EXTENSION, *PCOMMON_ATAX_DEVICE_EXTENSION;

typedef struct _FDO_CHANNEL_EXTENSION {                   //// FDO ���������� AtaXChannel

  COMMON_ATAX_DEVICE_EXTENSION   CommonExtension;          // ����� � ��� PDO � ��� FDO ����������

  ULONG                    Flags;                             // ����� FDO ���������� 
  ULONG                    Channel;                           // ����� ������
  IDE_CHANNEL_STATE        ChannelState;                      // ��������� ������
  ATAX_REGISTERS_1         BaseIoAddress1;                    // ������ ������� (��� ������) ��� ����� ��������� ���������
  ATAX_REGISTERS_2         BaseIoAddress2;                    // ������ ������� (��� ������) ��� ����� ��������� ���������� (������������ ������ ������)
  KDPC                     Dpc;
  KSPIN_LOCK               SpinLock;

  // Interfaces
  PBUS_INTERFACE_STANDARD  BusInterface;
  BUS_MASTER_INTERFACE     BusMasterInterface;

  // IoConnectInterrupt() 
  PKINTERRUPT              InterruptObject;
  ULONG                    InterruptLevel;
  ULONG                    InterruptVector;
  USHORT                   InterruptFlags;
  UCHAR                    InterruptShareDisposition;
  UCHAR                    Padded1;
  KAFFINITY                InterruptAffinity;

  PSCSI_REQUEST_BLOCK      CurrentSrb;                        // ������� SCSI_REQUEST_BLOCK
  BOOLEAN                  ExpectingInterrupt;                // ��������� ����������

  PDEVICE_OBJECT           AtaXDevicePdo[MAX_IDE_DEVICE];     // ��������� �� �������� PDO
  IDENTIFY_DATA            FullIdentifyData[MAX_IDE_DEVICE];  // Identify ������ ��� ������� ���������� ("�������" ����������)
  USHORT                   DeviceFlags[MAX_IDE_DEVICE];       // ����� ����������
  INQUIRYDATA              InquiryData[MAX_IDE_DEVICE];       // Inquiry ������ ��� ������� ����������

  HW_DEVICE_EXTENSION      HwDeviceExtension;                 // ��������� �����������

} FDO_CHANNEL_EXTENSION, *PFDO_CHANNEL_EXTENSION;

typedef struct _PDO_DEVICE_EXTENSION {                    //// PDO ���������� AtaXDevice

  COMMON_ATAX_DEVICE_EXTENSION  CommonExtension;                // ����� � ��� PDO � ��� FDO ����������

  ULONG                    Flags;                          // ����� PDO ���������� 
  PFDO_CHANNEL_EXTENSION   AtaXChannelFdoExtension;        // ��������� �� ���������� ������������� FDO
  UCHAR                    PathId;                         // ����� ������ (Primari ��� Secondary)
  UCHAR                    TargetId;                       // ����� ������� (Master ��� Slave)
  UCHAR                    Lun;                            // �� ������������ � AtaX (0)

  // ������� IRPs ��� ����������
  KDEVICE_QUEUE            DeviceQueue;                    // ��������� ��� ��������� ������� �� IRPs
  ULONG                    MaxQueueCount;                  // ������������ ���������� IRPs � �������
  ULONG                    QueueCount;                     // ������� IRPs � �������
  ULONG                    SortKey;                        // ������ ����������

} PDO_DEVICE_EXTENSION, *PPDO_DEVICE_EXTENSION; 

//
// ����������� �������
//

// atax.c
VOID 
AtaXWaitOnBusy(IN PATAX_REGISTERS_2 AtaXRegisters2);

VOID
AtaXWaitForDrq(IN PATAX_REGISTERS_2 AtaXRegisters2);

VOID 
AtaXSoftReset(
    IN PATAX_REGISTERS_1 AtaXRegisters1,
    IN PATAX_REGISTERS_2 AtaXRegisters2,
    IN ULONG DeviceNumber);

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
