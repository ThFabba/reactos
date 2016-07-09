#include "usbohci.h"

#define NDEBUG
#include <debug.h>


ULONG NTAPI
OHCI_RH_ClearFeaturePortConnectChange(
    IN PVOID Context,
    IN USHORT Port)
{
  POHCI_EXTENSION              OhciExtension = (POHCI_EXTENSION)Context;
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs = OhciExtension->OperationalRegs;
  DPRINT("OHCI_RH_ClearFeaturePortConnectChange: Context - %p, Port - %x\n", Context, Port);
  WRITE_REGISTER_ULONG(&OperationalRegs->HcRhPortStatus[Port-1].AsULONG, 0x10000);
  return 0;
}

ULONG NTAPI
OHCI_RH_SetFeaturePortPower(
    IN PVOID Context,
    IN USHORT Port)
{
  POHCI_EXTENSION              OhciExtension = (POHCI_EXTENSION)Context;
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs = OhciExtension->OperationalRegs;
  DPRINT("OHCI_RH_SetFeaturePortPower: Context - %p, Port - %x\n", Context, Port);
  WRITE_REGISTER_ULONG(&OperationalRegs->HcRhPortStatus[Port-1].AsULONG, 0x100);
  return 0;
}

ULONG NTAPI 
OHCI_RH_GetRootHubData(
    IN PVOID Context,
    IN PVOID RootHubData)
{
  POHCI_EXTENSION              OhciExtension;
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs;
  PUSB_HUB_DESCRIPTOR          RootHubDescriptor;
  OHCI_HC_RH_DESCRIPTORA       DescriptorA;
  UCHAR                        PowerOnToPowerGoodTime;
  USHORT                       HubCharacteristics;
  ULONG                        ix = 0;
  ULONG                        Result;

  DPRINT("OHCI_RH_GetRootHubData: OhciExtension - %p, RootHubData - %p\n", OhciExtension, RootHubData);

  OhciExtension = (POHCI_EXTENSION)Context;
  OperationalRegs = OhciExtension->OperationalRegs;
  RootHubDescriptor = (PUSB_HUB_DESCRIPTOR)RootHubData;

  do
  {
    DescriptorA.AsULONG = READ_REGISTER_ULONG(&OperationalRegs->HcRhDescriptorA.AsULONG);

    if ( DescriptorA.AsULONG && !(DescriptorA.AsULONG & 0xFFE0F0) ) //Reserved
      break;
    DPRINT1("OHCI_ReadRhDescriptorA: ix - %p\n", ix);

    KeStallExecutionProcessor(5);

    ++ix;
  }
  while ( ix < 10 );

  //DescriptorA = (OHCI_HC_RH_DESCRIPTORA)OHCI_ReadRhDescriptorA(OhciExtension);

  Result = (ULONG)RootHubDescriptor;

  RootHubDescriptor->bNumberOfPorts = DescriptorA.NumberDownstreamPorts;

  PowerOnToPowerGoodTime = DescriptorA.PowerOnToPowerGoodTime;

  RootHubDescriptor->wHubCharacteristics = (USHORT)(DescriptorA.AsULONG >> 8);
  RootHubDescriptor->bPowerOnToPowerGood = DescriptorA.PowerOnToPowerGoodTime;

  if ( DescriptorA.PowerOnToPowerGoodTime <= 25 )
    PowerOnToPowerGoodTime = 25;

  HubCharacteristics = (DescriptorA.AsULONG >> 8) & 0xFFFC;

  RootHubDescriptor->bPowerOnToPowerGood = PowerOnToPowerGoodTime;
  RootHubDescriptor->wHubCharacteristics = HubCharacteristics;

  if ( DescriptorA.PowerSwitchingMode )
    RootHubDescriptor->wHubCharacteristics = (HubCharacteristics & 0xFFFD) | 1;

  RootHubDescriptor->bHubControlCurrent = 0;

  return Result;
}

ULONG NTAPI
OHCI_RH_GetStatus(
    IN PVOID Context,
    IN PULONG RhStatus)
{
  DPRINT("OHCI_RH_GetStatus: \n");
  *(PUSHORT)RhStatus = 1;
  return 0;
}

ULONG NTAPI
OHCI_RH_GetPortStatus(
    IN PVOID Context,
    IN USHORT Port,
    IN PULONG PortStatus)
{
  POHCI_EXTENSION              OhciExtension;
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs;
  ULONG                        portStatus;
  ULONG                        ix;

  DPRINT("OHCI_RH_GetPortStatus: Port - %x, PortStatus - %p\n", Port, *PortStatus);

  OhciExtension = (POHCI_EXTENSION)Context;
  OperationalRegs = ((POHCI_EXTENSION)OhciExtension)->OperationalRegs;

  ix = 0;

  do
  {
    portStatus = READ_REGISTER_ULONG(&OperationalRegs->HcRhPortStatus[Port-1].AsULONG);

    if ( portStatus && !(portStatus & 0xFFE0FCE0) )
      break;

    KeStallExecutionProcessor(5);

    ++ix;
  }
  while ( ix < 10 );

  *PortStatus = portStatus;

  return 0;
}

ULONG NTAPI
OHCI_RH_GetHubStatus(
    IN PVOID Context,
    IN PULONG HubStatus)
{
  POHCI_EXTENSION              OhciExtension = (POHCI_EXTENSION)Context;
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs = OhciExtension->OperationalRegs;
  DPRINT("OHCI_RH_GetHubStatus: Context - %p, HubStatus - %x\n", Context, HubStatus);
  *HubStatus &= ~0x10001;
  *HubStatus ^= (READ_REGISTER_ULONG(&OperationalRegs->HcRhStatus.AsULONG) ^ *HubStatus) & 0x20002;
  return 0;
}

VOID NTAPI
OHCI_RH_EnableIrq(
    IN PVOID Context)
{
  POHCI_EXTENSION  OhciExtension = (POHCI_EXTENSION)Context;

  DPRINT("OHCI_RH_EnableIrq: OhciExtension - %p\n", OhciExtension);
  WRITE_REGISTER_ULONG(&OhciExtension->OperationalRegs->HcInterruptEnable.AsULONG, 0x40);
}

VOID NTAPI
OHCI_RH_DisableIrq(
    IN PVOID Context)
{
  POHCI_EXTENSION  OhciExtension = (POHCI_EXTENSION)Context;

  DPRINT("OHCI_RH_DisableIrq: OhciExtension - %p\n", OhciExtension);
  WRITE_REGISTER_ULONG(&OhciExtension->OperationalRegs->HcInterruptDisable.AsULONG, 0x40);
}

ULONG NTAPI
OHCI_RH_ClearFeaturePortResetChange(
    IN PVOID Context,
    IN USHORT Port)
{
  POHCI_EXTENSION              OhciExtension = (POHCI_EXTENSION)Context;
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs = OhciExtension->OperationalRegs;
  DPRINT("OHCI_RH_ClearFeaturePortResetChange: Context - %p, Port - %x\n", Context, Port);
  WRITE_REGISTER_ULONG(&OperationalRegs->HcRhPortStatus[Port-1].AsULONG, 0x100000);
  return 0;
}
