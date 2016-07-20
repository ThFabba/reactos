#ifndef _PCIIDEX_PCH_
#define _PCIIDEX_PCH_

#include <ntifs.h>
#include <ide.h>
#include <..\bmaster.h>


#define PRIMARY_CHANNEL		0x00
#define SECONDARY_CHANNEL	0x01

typedef NTSTATUS
(NTAPI *PALLOCATE_ADAPTER)(IN ULONG AllocateAdapterContext);

typedef struct _PCIIDEX_DRIVER_EXTENSION
{
	PCONTROLLER_PROPERTIES HwGetControllerProperties;
	ULONG MiniControllerExtensionSize;
	PCIIDE_UDMA_MODES_SUPPORTED HwUdmaModesSupported;
} PCIIDEX_DRIVER_EXTENSION, *PPCIIDEX_DRIVER_EXTENSION;

typedef struct _COMMON_DEVICE_EXTENSION
{
	BOOLEAN IsFDO;
} COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;

typedef struct _FDO_DEVICE_EXTENSION
{
	COMMON_DEVICE_EXTENSION Common;

	PBUS_INTERFACE_STANDARD BusInterface;
	IDE_CONTROLLER_PROPERTIES Properties;

	PHYSICAL_ADDRESS BusMasterPortBase;
	ULONG BusMasterBase;
	PDEVICE_OBJECT LowerDevice;
	PDEVICE_OBJECT Pdo[MAX_IDE_CHANNEL];
	USHORT VendorId;
	USHORT DeviceId;
        BOOLEAN ControllerMode[MAX_IDE_CHANNEL]; //TRUE - Native, FALSE - Compatible

	PUCHAR MiniControllerExtension[0];

} FDO_DEVICE_EXTENSION, *PFDO_DEVICE_EXTENSION;

typedef struct _PDO_DEVICE_EXTENSION
{
	COMMON_DEVICE_EXTENSION Common;

	ULONG Channel;
	PDEVICE_OBJECT ControllerFdo;
	PDEVICE_OBJECT SelfDevice;

	PDMA_ADAPTER DmaAdapter;
	ULONG NumberOfMapRegisters;
	PVOID CommonBuffer;
	PHYSICAL_ADDRESS LogicalAddress;

	PSCATTER_GATHER_LIST SGList;
	PVOID AllocateAdapter;
	ULONG AllocateAdapterContext;
	BOOLEAN WriteToDevice;

} PDO_DEVICE_EXTENSION, *PPDO_DEVICE_EXTENSION;

/* fdo.c */

DRIVER_ADD_DEVICE PciIdeXAddDevice;
NTSTATUS NTAPI
PciIdeXAddDevice(
	IN PDRIVER_OBJECT DriverObject,
	IN PDEVICE_OBJECT Pdo);

NTSTATUS NTAPI
PciIdeXFdoPnpDispatch(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp);

/* misc.c */

IO_COMPLETION_ROUTINE PciIdeXGenericCompletion;
NTSTATUS NTAPI
PciIdeXGenericCompletion(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context);

NTSTATUS
ForwardIrpAndWait(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp);

NTSTATUS NTAPI
ForwardIrpAndForget(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp);

NTSTATUS
DuplicateUnicodeString(
	IN ULONG Flags,
	IN PCUNICODE_STRING SourceString,
	OUT PUNICODE_STRING DestinationString);

/* pdo.c */

NTSTATUS NTAPI
PciIdeXPdoPnpDispatch(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp);

/* dma.c */

NTSTATUS
QueryBusMasterInterface(
    IN PPDO_DEVICE_EXTENSION DeviceExtension,
    IN PIO_STACK_LOCATION Stack);

#endif /* _PCIIDEX_PCH_ */
