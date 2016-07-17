#ifndef _ATAX_PCH_
#define _ATAX_PCH_

#include <ntddk.h>
#include <stdio.h>
#include <wdm.h>
#include <ide.h>
#include <wdmguid.h>


//
// ����������e ���������� ����������
//

extern ULONG AtaXDeviceCounter;  // ��������� ���������
extern ULONG AtaXChannelCounter; // ��������� �������

//
// ����������e ��������
//

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

  // Interfaces
  PBUS_INTERFACE_STANDARD  BusInterface;

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
