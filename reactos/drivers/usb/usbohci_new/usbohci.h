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


typedef struct _OHCI_OPERATIONAL_REGISTERS {

  ULONG HcRevision;          // +00 +0x00
  ULONG HcControl;           // +04 +0x04
  ULONG HcCommandStatus;     // +08 +0x08
  ULONG HcInterruptStatus;   // +12 +0x0C
  ULONG HcInterruptEnable;   // +16 +0x10
  ULONG HcInterruptDisable;  // +20 +0x14
  ULONG HcHCCA;              // +24 +0x18
  ULONG HcPeriodCurrentED;   // +28 +0x1C
  ULONG HcControlHeadED;     // +32 +0x20
  ULONG HcControlCurrentED;  // +36 +0x24
  ULONG HcBulkHeadED;        // +40 +0x28
  ULONG HcBulkCurrentED;     // +44 +0x2C
  ULONG HcDoneHead;          // +48 +0x30
  ULONG HcFmInterval;        // +52 +0x34
  ULONG HcFmRemaining;       // +56 +0x38
  ULONG HcFmNumber;          // +60 +0x3C
  ULONG HcPeriodicStart;     // +64 +0x40
  ULONG HcLSThreshold;       // +68 +0x44
  ULONG HcRhDescriptorA;     // +72 +0x48
  ULONG HcRhDescriptorB;     // +76 +0x4C
  ULONG HcRhStatus;          // +80 +0x50
  ULONG HcRhPortStatus[1];   // +84 +0x54

} OHCI_OPERATIONAL_REGISTERS, *POHCI_OPERATIONAL_REGISTERS;


extern USBPORT_REGISTRATION_PACKET RegPacket;

NTSTATUS NTAPI USBPORT_RegisterUSBPortDriver(PDRIVER_OBJECT DriverObject, ULONG Version, PUSBPORT_REGISTRATION_PACKET RegistrationPacket);

ULONG NTAPI OHCI_StartController(PVOID Context, PUSBPORT_RESOURCES Resources);

#endif /* USBOHCI_H__ */
