#include "usbohci.h"

#define NDEBUG
#include <debug.h>


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
