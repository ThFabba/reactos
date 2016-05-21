#include "usbohci.h"

//#define NDEBUG
#include <debug.h>


USBPORT_REGISTRATION_PACKET RegPacket;


NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;

    DPRINT("DriverEntry: DriverObject - %p, RegistryPath - %p\n", DriverObject, RegistryPath);
    DPRINT("DriverEntry: sizeof(USBPORT_REGISTRATION_PACKET) - %x\n", sizeof(USBPORT_REGISTRATION_PACKET));

    RtlZeroMemory(&RegPacket, sizeof(USBPORT_REGISTRATION_PACKET));

    RegPacket.Version                               = 1;                                          // Type: 1 - OHCI, 2 - UHCI, 3 - EHCI
    RegPacket.MiniPortHwResourcesSize               = 0;                                          // Size OHCI MiniPort Extension

    Status = USBPORT_RegisterUSBPortDriver(DriverObject, 1, &RegPacket);
    DPRINT("DriverEntry: USBPORT_RegisterUSBPortDriver return Status - %x\n", Status);

    return Status;
}

