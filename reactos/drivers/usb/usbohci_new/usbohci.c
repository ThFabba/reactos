#include "usbohci.h"

//#define NDEBUG
#include <debug.h>


USBPORT_REGISTRATION_PACKET RegPacket;


ULONG NTAPI OHCI_StartController(PVOID Context, PUSBPORT_RESOURCES Resources)
{
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs;
  POHCI_EXTENSION              OhciExtension = (POHCI_EXTENSION)Context;
  ULONG                        MiniPortStatus = 0;
  ULONG                        ix;
  UCHAR                        HeadIndex;
  POHCI_ENDPOINT_DESCRIPTOR    StaticHwED;

  DPRINT("OHCI_StartController: Context - %p, Resources - %p\n", Context, Resources);

  MiniPortStatus = 0;
  OperationalRegs = (POHCI_OPERATIONAL_REGISTERS)Resources->ResourceBase; //HC on-chip operational registers
  OhciExtension->OperationalRegs = OperationalRegs;

  MiniPortStatus = 0;//OHCI_TakeControlHC(OhciExtension);
  DPRINT("OHCI_StartController: FIXME OHCI_TakeControlHC \n");

  if ( MiniPortStatus != 0 )
  {
    DPRINT1("OHCI_StartController: OHCI_TakeControlHC return MiniPortStatus - %x\n", MiniPortStatus);
    return MiniPortStatus;
  }

  OhciExtension->HcResourcesVA = Resources->StartVA;
  OhciExtension->HcResourcesPA = Resources->StartPA;
  DPRINT("OHCI_StartController: HcResourcesVA - %p, HcResourcesPA - %p\n", OhciExtension->HcResourcesVA, OhciExtension->HcResourcesPA);

  for (ix = 0; ix < 63; ix++)  // FIXME 63 == 32+16+8+4+2+1 (Endpoint Poll Interval (ms))
  {
    StaticHwED = &OhciExtension->HcResourcesVA->InterrruptHeadED[ix];

    if ( ix == 0 )
    {
      HeadIndex = 0;
      StaticHwED->NextED = 0;
    }
    else
    {
      HeadIndex = ((ix - 1) >> 1);
      ASSERT(HeadIndex >= 0 && HeadIndex < 31);
      StaticHwED->NextED = OhciExtension->IntStaticED[HeadIndex].PhysicalAddress;
    }

    StaticHwED->EndpointControl.sKip = 1;           // 
    StaticHwED->TailPointer          = 0;           // 
    StaticHwED->HeadPointer          = 0;           // 

    OhciExtension->IntStaticED[ix].HwED             = StaticHwED;
    OhciExtension->IntStaticED[ix].PhysicalAddress  = (ULONG)OhciExtension->HcResourcesPA + sizeof(OHCI_HCCA) + ix * sizeof(OHCI_ENDPOINT_DESCRIPTOR); 
    OhciExtension->IntStaticED[ix].HeadIndex        = HeadIndex;

    DPRINT("OHCI_StartController: ix - 0x%02X, HeadIndex - 0x%02X, StaticHwED - %p, StaticHwED->NextED - %p\n",
                                  ix, HeadIndex, StaticHwED, StaticHwED->NextED);
  }


  return MiniPortStatus;
}

VOID NTAPI OHCI_EnableInterrupts(PVOID Context)
{
  DPRINT("OHCI_EnableInterrupts: Context - %p\n", Context);
}

VOID NTAPI OHCI_DisableInterrupts(PVOID Context)
{
  DPRINT("OHCI_DisableInterrupts: Context - %p\n", Context);
}

NTSTATUS NTAPI DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;

    DPRINT("DriverEntry: DriverObject - %p, RegistryPath - %p\n", DriverObject, RegistryPath);
    DPRINT("DriverEntry: sizeof(USBPORT_REGISTRATION_PACKET) - %x\n", sizeof(USBPORT_REGISTRATION_PACKET));

    RtlZeroMemory(&RegPacket, sizeof(USBPORT_REGISTRATION_PACKET));

    RegPacket.Version                               = 1;                                          // Type: 1 - OHCI, 2 - UHCI, 3 - EHCI
    RegPacket.MiniPortExtensionSize                 = sizeof(OHCI_EXTENSION);                     // Size OHCI MiniPort Extension
    RegPacket.MiniPortResourcesSize                 = sizeof(OHCI_HC_RESOURCES);                  // 
    RegPacket.StartController                       = OHCI_StartController;
    RegPacket.EnableInterrupts                      = OHCI_EnableInterrupts;
    RegPacket.DisableInterrupts                     = OHCI_DisableInterrupts;

    Status = USBPORT_RegisterUSBPortDriver(DriverObject, 1, &RegPacket);
    DPRINT("DriverEntry: USBPORT_RegisterUSBPortDriver return Status - %x\n", Status);

    return Status;
}

