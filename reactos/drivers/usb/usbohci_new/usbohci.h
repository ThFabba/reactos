#ifndef USBOHCI_H__
#define USBOHCI_H__

#include <ntddk.h>
#include <windef.h>
#include <stdio.h>
#include <wdm.h>
#include <hubbusif.h>
#include <usbbusif.h>
#include <usbdlib.h>
#include "..\usbmport.h"


extern USBPORT_REGISTRATION_PACKET RegPacket;

NTSTATUS NTAPI USBPORT_RegisterUSBPortDriver(PDRIVER_OBJECT DriverObject, ULONG Version, PUSBPORT_REGISTRATION_PACKET RegistrationPacket);

ULONG NTAPI OHCI_StartController(PVOID Context, PUSBPORT_RESOURCES Resources);

#endif /* USBOHCI_H__ */
