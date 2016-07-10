#include "usbohci.h"

//#define NDEBUG
#include <debug.h>


USBPORT_REGISTRATION_PACKET RegPacket;


VOID NTAPI
OHCI_InterruptDpc(
    IN PVOID Context,
    IN BOOLEAN InterruptEnable)
{
  POHCI_EXTENSION              OhciExtension;
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs;
  ULONG                        InterruptStatus;

  OhciExtension = (POHCI_EXTENSION)Context;
  OperationalRegs = OhciExtension->OperationalRegs;

  DPRINT("OHCI_InterruptDpc: OhciExtension - %p, InterruptEnable - %p\n", OhciExtension, InterruptEnable);

  InterruptStatus = READ_REGISTER_ULONG(&OperationalRegs->HcInterruptStatus.AsULONG);

  if ( InterruptStatus & 0x40 ) // RootHubStatusChange
  {
    DPRINT("OHCI_InterruptDpc: RootHubStatusChange\n");
    RegPacket.UsbPortInvalidateRootHub(OhciExtension);
  }

  if ( InterruptStatus & 2 ) // WritebackDoneHead
  {
ASSERT(FALSE);
  }

  if ( InterruptStatus & 4 ) // StartofFrame
  {
    WRITE_REGISTER_ULONG(&OperationalRegs->HcInterruptDisable.AsULONG, 4);
  }

  if ( InterruptStatus & 8 ) // ResumeDetected
  {
ASSERT(FALSE);
  }

  if ( InterruptStatus & 0x10 ) // UnrecoverableError
  {
ASSERT(FALSE);
  }

  WRITE_REGISTER_ULONG(&OperationalRegs->HcInterruptStatus.AsULONG,
                       InterruptStatus);

  if ( InterruptEnable )
  {
    WRITE_REGISTER_ULONG(&OperationalRegs->HcInterruptEnable.AsULONG,
                         0x80000000);
  }
}

BOOLEAN NTAPI
OHCI_InterruptService(
    PVOID Context)
{
  POHCI_EXTENSION                   OhciExtension = (POHCI_EXTENSION)Context;
  POHCI_OPERATIONAL_REGISTERS       OperationalRegs;
  OHCI_HC_INTERRUPT_ENABLE_DISABLE  InterruptEnable;
  OHCI_HC_INTERRUPT_STATUS          InterruptStatus; 
  BOOLEAN                           Result = FALSE;

  DPRINT("OHCI_InterruptService: OhciExtension - %p\n", OhciExtension);

  OperationalRegs = OhciExtension->OperationalRegs;

  InterruptEnable.AsULONG = READ_REGISTER_ULONG(&OperationalRegs->HcInterruptEnable.AsULONG);
  InterruptStatus.AsULONG = InterruptEnable.AsULONG &
                            READ_REGISTER_ULONG(&OperationalRegs->HcInterruptStatus.AsULONG);

  if ( InterruptStatus.AsULONG && InterruptEnable.MasterInterruptEnable )
  {
    if ( InterruptStatus.FrameNumberOverflow )
      DPRINT("OHCI_InterruptService: FIXME. FrameNumberOverflow. InterruptStatus.AsULONG - %p\n", InterruptStatus.AsULONG);

    if ( InterruptStatus.AsULONG & 0x10 )
ASSERT(FALSE);

    Result = TRUE;

    WRITE_REGISTER_ULONG(&OperationalRegs->HcInterruptDisable.AsULONG, 0x80000000);
  }

  return Result;
}

ULONG NTAPI
OHCI_StartController(
    IN PVOID Context,
    IN PUSBPORT_RESOURCES HcResources)
{
  POHCI_EXTENSION                   OhciExtension;
  POHCI_OPERATIONAL_REGISTERS       OperationalRegs;
  OHCI_HC_INTERRUPT_ENABLE_DISABLE  Interrupts;
  OHCI_HC_RH_STATUS                 HcRhStatus;
  OHCI_HC_FRAME_INTERVAL            FrameInterval;
  OHCI_HC_CONTROL                   Control;
  PVOID                             ScheduleStartVA;
  PVOID                             ScheduleStartPA;
  ULONG                             ix, jx;
  UCHAR                             HeadIndex;
  POHCI_ENDPOINT_DESCRIPTOR         StaticED;
  ULONG_PTR                         SchedulePA;
  POHCI_HCCA                        OhciHCCA;
  LARGE_INTEGER                     SystemTime;
  LARGE_INTEGER                     CurrentTime;
  ULONG                             Result = 0;

  DPRINT("OHCI_StartController: Context - %p, HcResources - %p\n", Context, HcResources);

  OhciExtension = (POHCI_EXTENSION)Context;
  OperationalRegs = (POHCI_OPERATIONAL_REGISTERS)HcResources->ResourceBase; //HC on-chip operational registers
  OhciExtension->OperationalRegs = OperationalRegs;

  Result = 0;//OHCI_TakeControlHC(OhciExtension);
  DPRINT("OHCI_StartController: FIXME OHCI_TakeControlHC \n");

  if ( Result != 0 )
  {
    DPRINT1("OHCI_StartController: OHCI_TakeControlHC return Result - %x\n", Result);
    return Result;
  }

  OhciExtension->HcResourcesVA = (ULONG_PTR)HcResources->StartVA;
  OhciExtension->HcResourcesPA = (ULONG_PTR)HcResources->StartPA;
  DPRINT("OHCI_StartController: HcResourcesVA - %p, HcResourcesPA - %p\n", OhciExtension->HcResourcesVA, OhciExtension->HcResourcesPA);

  ScheduleStartVA = (PVOID)((ULONG_PTR)HcResources->StartVA + sizeof(OHCI_HCCA));
  ScheduleStartPA = (PVOID)((ULONG_PTR)HcResources->StartPA + sizeof(OHCI_HCCA));

  OhciExtension->ScheduleStartVA = ScheduleStartVA; // HcResources->CommonBufferVa + 0x100
  OhciExtension->ScheduleStartPA = ScheduleStartPA; // +0x100

  StaticED = (POHCI_ENDPOINT_DESCRIPTOR)ScheduleStartVA;
  SchedulePA = (ULONG_PTR)ScheduleStartPA;
  ix = 0;

  for ( ix = 0; ix < 63; ix++ )  // FIXME 63 == 32+16+8+4+2+1 (Endpoint Poll Interval (ms))
  {
    if ( ix == 0 )
    {
      HeadIndex = ED_EOF;
      StaticED->NextED = 0;
    }
    else
    {
      HeadIndex = ((ix - 1) >> 1);
      ASSERT(HeadIndex >= 0 && HeadIndex < 31);
      StaticED->NextED = OhciExtension->InterrruptHeadED[HeadIndex].PhysicalAddress;
    }
  
    StaticED->EndpointControl.sKip = 1;
    StaticED->TailPointer          = 0;
    StaticED->HeadPointer          = 0;
  
    OhciExtension->InterrruptHeadED[ix].HwED            = StaticED;
    OhciExtension->InterrruptHeadED[ix].PhysicalAddress = SchedulePA;
    OhciExtension->InterrruptHeadED[ix].HeadIndex       = HeadIndex;
    OhciExtension->InterrruptHeadED[ix].pNextED         = (POHCI_ENDPOINT_DESCRIPTOR *)&StaticED->NextED;
  
    InitializeListHead(&OhciExtension->InterrruptHeadED[ix].Link);
  
    StaticED   += 1;
    SchedulePA += sizeof(OHCI_ENDPOINT_DESCRIPTOR);
  }

  OhciHCCA = (POHCI_HCCA)OhciExtension->HcResourcesVA;
  DPRINT("OHCI_InitializeSchedule: OhciHCCA - %p\n", OhciHCCA);

  for (ix = 0, jx = 31; ix < 32; ix++, jx++)
  {
    static UCHAR Balance[32] =
    { 0, 16, 8, 24, 4, 20, 12, 28, 2, 18, 10, 26, 6, 22, 14, 30, 1, 17, 9, 25, 5, 21, 13, 29, 3, 19, 11, 27, 7, 23, 15, 31  };
  
      OhciHCCA->InterrruptTable[Balance[ix]] = (POHCI_ENDPOINT_DESCRIPTOR)(OhciExtension->InterrruptHeadED[jx].PhysicalAddress);
  
      OhciExtension->InterrruptHeadED[jx].pNextED   = (POHCI_ENDPOINT_DESCRIPTOR *)&OhciHCCA->InterrruptTable[Balance[ix]];
      OhciExtension->InterrruptHeadED[jx].HccaIndex = Balance[ix];
    }

  DPRINT("OHCI_InitializeSchedule: ix - %x\n", ix);

  InitializeListHead(&OhciExtension->ControlStaticED.Link);

  OhciExtension->ControlStaticED.HeadIndex = ED_EOF;
  OhciExtension->ControlStaticED.Type      = OHCI_NUMBER_OF_INTERRUPTS + 1;
  OhciExtension->ControlStaticED.pNextED   = (POHCI_ENDPOINT_DESCRIPTOR *)&OperationalRegs->HcControlHeadED;

  InitializeListHead(&OhciExtension->BulkStaticED.Link);

  OhciExtension->BulkStaticED.HeadIndex = ED_EOF;
  OhciExtension->BulkStaticED.Type      = OHCI_NUMBER_OF_INTERRUPTS + 2;
  OhciExtension->BulkStaticED.pNextED   = (POHCI_ENDPOINT_DESCRIPTOR *)&OperationalRegs->HcBulkHeadED;

  FrameInterval = (OHCI_HC_FRAME_INTERVAL)READ_REGISTER_ULONG(&OperationalRegs->HcFmInterval.AsULONG);

  if ( (FrameInterval.FrameInterval) < (11999 - 120) || (FrameInterval.FrameInterval) > (11999 + 120) ) // FIXME 10%
    FrameInterval.FrameInterval = 11999;

  FrameInterval.FSLargestDataPacket = ((FrameInterval.FrameInterval - MAXIMUM_OVERHEAD) * 6) / 7;
  FrameInterval.FrameIntervalToggle = 1;

  DPRINT("OHCI_StartController: FrameInterval - %p\n", FrameInterval.AsULONG);

  OhciExtension->FrameInterval = FrameInterval;

  //reset
  WRITE_REGISTER_ULONG(&OperationalRegs->HcCommandStatus.AsULONG, 1);
  KeStallExecutionProcessor(25);

  Control.AsULONG = READ_REGISTER_ULONG(&OperationalRegs->HcControl.AsULONG);
  Control.HostControllerFunctionalState = OHCI_HC_STATE_RESET;

  WRITE_REGISTER_ULONG(&OperationalRegs->HcControl.AsULONG, Control.AsULONG);

  KeQuerySystemTime(&CurrentTime);
  CurrentTime.QuadPart += 5000000; // 0.5 sec

  while ( 1 )
  {
    WRITE_REGISTER_ULONG(&OperationalRegs->HcFmInterval.AsULONG, OhciExtension->FrameInterval.AsULONG);
    FrameInterval.AsULONG = READ_REGISTER_ULONG(&OperationalRegs->HcFmInterval.AsULONG);

    KeQuerySystemTime(&SystemTime);

    if ( SystemTime.QuadPart >= CurrentTime.QuadPart )
    {
      return 1;
    }

    if ( FrameInterval.AsULONG != OhciExtension->FrameInterval.AsULONG )
    {
      return 1;
    }

    break;
  }

  WRITE_REGISTER_ULONG(&OperationalRegs->HcPeriodicStart, (OhciExtension->FrameInterval.FrameInterval * 9) / 10); //90%
  WRITE_REGISTER_ULONG(&OperationalRegs->HcHCCA, OhciExtension->HcResourcesPA);

  Interrupts.AsULONG = 0;

  Interrupts.SchedulingOverrun   = 1;
  Interrupts.WritebackDoneHead   = 1;
  Interrupts.UnrecoverableError  = 1;
  Interrupts.FrameNumberOverflow = 1;
  Interrupts.OwnershipChange     = 1;
  WRITE_REGISTER_ULONG(&OperationalRegs->HcInterruptEnable.AsULONG, Interrupts.AsULONG);

  Control.AsULONG = READ_REGISTER_ULONG(&OperationalRegs->HcControl.AsULONG);

  Control.ControlBulkServiceRatio       = 0; // FIXME (1 : 1)
  Control.PeriodicListEnable            = 1;
  Control.IsochronousEnable             = 1;
  Control.ControlListEnable             = 1;
  Control.BulkListEnable                = 1;
  Control.HostControllerFunctionalState = OHCI_HC_STATE_OPERATIONAL;
  WRITE_REGISTER_ULONG(&OperationalRegs->HcControl.AsULONG, Control.AsULONG);
  
  HcRhStatus.AsULONG = 0;

  HcRhStatus.SetGlobalPower = 1;
  WRITE_REGISTER_ULONG(&OperationalRegs->HcRhStatus.AsULONG, HcRhStatus.AsULONG);

  return 0;
}

VOID NTAPI
OHCI_EnableInterrupts(
    IN PVOID Context)
{
  POHCI_EXTENSION              OhciExtension;
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs;
 
  OhciExtension = (POHCI_EXTENSION)Context;
  DPRINT("OHCI_EnableInterrupts: OhciExtension - %p\n", OhciExtension);
  OperationalRegs = OhciExtension->OperationalRegs;
  WRITE_REGISTER_ULONG(&OperationalRegs->HcInterruptEnable.AsULONG, 0x80000000); //HcInterruptEnable.MIE - Master Interrupt Enable
}

VOID NTAPI
OHCI_DisableInterrupts(
    IN PVOID Context)
{
  DPRINT("OHCI_DisableInterrupts: Context - %p\n", Context);
}

VOID NTAPI
OHCI_EnableList(
    POHCI_EXTENSION OhciExtension,
    POHCI_ENDPOINT OhciEndpoint)
{
  POHCI_OPERATIONAL_REGISTERS  OperationalRegs;
  ULONG                        TransferType;
  ULONG                        CommandStatus;

  DPRINT("OHCI_EnableList: ... \n");

  OperationalRegs = OhciExtension->OperationalRegs;

  CommandStatus = 0;

  if ( READ_REGISTER_ULONG((PULONG)&OperationalRegs->HcControlHeadED) )
    CommandStatus = 2;

  if ( READ_REGISTER_ULONG((PULONG)&OperationalRegs->HcBulkHeadED) )
    CommandStatus |= 4;

  TransferType = OhciEndpoint->OhciEndpointProperties.TransferType;

  if ( TransferType == 2 ) // ENDPOINT_TYPE_BULK
  {
    CommandStatus |= 4;
  }
  else if ( TransferType == 0 ) // ENDPOINT_TYPE_CONTROL
  {
    CommandStatus |= 2;
  }

  WRITE_REGISTER_ULONG(&OperationalRegs->HcCommandStatus.AsULONG, CommandStatus);
}

VOID NTAPI
OHCI_SetEndpointState(
    IN PVOID Context,
    IN PVOID MiniportEndpoint,
    IN ULONG State)
{
  POHCI_EXTENSION                OhciExtension = (POHCI_EXTENSION)Context;
  POHCI_ENDPOINT                 OhciEndpoint = (POHCI_ENDPOINT)MiniportEndpoint;
  POHCI_HCD_ENDPOINT_DESCRIPTOR  ED;

  DPRINT("OHCI_SetEndpointState: State - %x\n", State);

  ED = OhciEndpoint->ED;

  switch ( State )
  {
    case 2: // ENDPOINT_PAUSED
      ED->HwED.EndpointControl.sKip = 1;
ASSERT(FALSE);
      break;

    case 3: // ENDPOINT_ACTIVE
      ED->HwED.EndpointControl.sKip = 0;
      OHCI_EnableList(OhciExtension, OhciEndpoint);
      break;

    case 4: // ENDPOINT_CLOSED
ASSERT(FALSE);
      break;

    default:
ASSERT(FALSE);
      break;
  }
}

VOID NTAPI
OHCI_InterruptNextSOF(
    IN PVOID Context)
{
  POHCI_EXTENSION  OhciExtension = (POHCI_EXTENSION)Context;
  DPRINT("OHCI_InterruptNextSOF: OhciExtension - %p\n", OhciExtension);
  WRITE_REGISTER_ULONG(&OhciExtension->OperationalRegs->HcInterruptEnable.AsULONG, 4);
}

VOID NTAPI
OHCI_QueryEndpointRequirements(
    IN PVOID Context,
    IN PVOID PortEndpointProperties,
    IN OUT PULONG TransferParams)
{
  PUSBPORT_ENDPOINT_PROPERTIES  EndpointProperties;
  ULONG                         TransferType;

  DPRINT("OHCI_QueryEndpointRequirements: ... \n");

  EndpointProperties = (PUSBPORT_ENDPOINT_PROPERTIES)PortEndpointProperties;
  TransferType = EndpointProperties->TransferType;

  switch ( TransferType )
  {
    case 0: // Control
      DPRINT("OHCI_QueryEndpointRequirements: TransferType == 1 ControlTransfer\n");
      *((PULONG)TransferParams + 1) = 0x10000;
      *TransferParams = sizeof(OHCI_HCD_ENDPOINT_DESCRIPTOR) +
                        0x26 * sizeof(OHCI_HCD_TRANSFER_DESCRIPTOR);
      break;

    case 1: // Iso
      ASSERT(FALSE);
      break;

    case 2: // Bulk
      DPRINT("OHCI_QueryEndpointRequirements: TransferType == 2 BulkTransfer\n");
      *((PULONG)TransferParams + 1) = 0x40000;
      *TransferParams = sizeof(OHCI_HCD_ENDPOINT_DESCRIPTOR) +
                        0x44 * sizeof(OHCI_HCD_TRANSFER_DESCRIPTOR);
      break;

    case 3:  // Interrupt
      DPRINT("OHCI_QueryEndpointRequirements: TransferType == 3 InterruptTransfer\n");
      *((PULONG)TransferParams + 1) = 0x1000;
      *TransferParams = sizeof(OHCI_HCD_ENDPOINT_DESCRIPTOR) +
                        4 * sizeof(OHCI_HCD_TRANSFER_DESCRIPTOR);
      break;

    default:
      ASSERT(FALSE);
      break;
  }
}

VOID NTAPI
OHCI_InsertEndpointInSchedule(
    POHCI_ENDPOINT OhciEndpoint)
{
  POHCI_STATIC_ENDPOINT_DESCRIPTOR  HeadED;
  POHCI_HCD_ENDPOINT_DESCRIPTOR     ED;
  POHCI_HCD_ENDPOINT_DESCRIPTOR     PrevED;
  PLIST_ENTRY                       HeadLink;

  DPRINT("OHCI_InsertEndpointInSchedule: OhciEndpoint - %p\n", OhciEndpoint);

  ED = OhciEndpoint->ED;

  HeadED = OhciEndpoint->HeadED;
  HeadLink = &HeadED->Link;

  if ( IsListEmpty(HeadLink) )
  {
    InsertHeadList(HeadLink, &ED->HcdEDLink);

    if ( HeadED->Type & 0x20 ) // ControlTransfer or BulkTransfer
    {
      ED->HwED.NextED = READ_REGISTER_ULONG((PULONG)HeadED->pNextED);
      WRITE_REGISTER_ULONG((PULONG)HeadED->pNextED, ED->PhysicalAddress);
    }
    else
    {
      ED->HwED.NextED = (ULONG_PTR)*HeadED->pNextED;
      *HeadED->pNextED = (POHCI_ENDPOINT_DESCRIPTOR)ED->PhysicalAddress;
    }
  }
  else
  {
    PrevED = CONTAINING_RECORD(
               HeadLink->Blink,
               OHCI_HCD_ENDPOINT_DESCRIPTOR,
               HcdEDLink);

    InsertTailList(HeadLink, &ED->HcdEDLink);

    ED->HwED.NextED = 0;
    PrevED->HwED.NextED = ED->PhysicalAddress;
  }
}

POHCI_HCD_ENDPOINT_DESCRIPTOR
OHCI_InitializeED(
    IN POHCI_ENDPOINT OhciEndpoint,
    IN POHCI_HCD_ENDPOINT_DESCRIPTOR ED,
    IN POHCI_HCD_TRANSFER_DESCRIPTOR FirstTD,
    IN ULONG_PTR EdPA)
{
  OHCI_HC_ENDPOINT_CONTROL      EndpointControl;
  PUSBPORT_ENDPOINT_PROPERTIES  EndpointProperties;

  DPRINT("OHCI_InitializeED: OhciEndpoint - %p, ED - %p, FirstTD - %p, EdPA - %p\n", OhciEndpoint, ED, FirstTD, EdPA);

  RtlZeroMemory(ED, sizeof(OHCI_HCD_ENDPOINT_DESCRIPTOR));

  ED->PhysicalAddress = EdPA;

  EndpointProperties = &OhciEndpoint->OhciEndpointProperties;

  ED->HwED.EndpointControl.FunctionAddress = EndpointProperties->DeviceAddress;
  ED->HwED.EndpointControl.EndpointNumber  = EndpointProperties->EndpointAddress;

  EndpointControl = ED->HwED.EndpointControl;

  if ( EndpointProperties->TransferType == 0 )
  {
    EndpointControl.Direction = 0; // Get direction From TD
  }
  else if ( EndpointProperties->Direction )
  {
    EndpointControl.Direction = 1; // OUT
  }
  else
  {
    EndpointControl.Direction = 2; // IN
  }

  ED->HwED.EndpointControl = EndpointControl;

  if ( EndpointProperties->DeviceSpeed == 0 )
    ED->HwED.EndpointControl.Speed = 1; // full-speed (S = 0) or low-speed (S = 1.)

  if ( EndpointProperties->TransferType == 1 )
    ED->HwED.EndpointControl.Format = 1;
  else
    ED->HwED.EndpointControl.sKip = 1;

  ED->HwED.EndpointControl.MaximumPacketSize = EndpointProperties->MaxPacketSize;

  ED->HwED.TailPointer = (ULONG_PTR)FirstTD->PhysicalAddress;
  ED->HwED.HeadPointer = (ULONG_PTR)FirstTD->PhysicalAddress;

  FirstTD->Flags |= OHCI_HCD_TD_FLAG_ALLOCATED;

  OhciEndpoint->HcdHeadP  = FirstTD;
  OhciEndpoint->HcdTailP = FirstTD;

  return ED;
}

ULONG NTAPI
OHCI_OpenControlEndpoint(
         IN POHCI_EXTENSION OhciExtension,
         IN PUSBPORT_ENDPOINT_PROPERTIES EndpointProperties,
         IN POHCI_ENDPOINT OhciEndpoint)
{
  POHCI_HCD_TRANSFER_DESCRIPTOR  TdPA;
  POHCI_HCD_TRANSFER_DESCRIPTOR  TdVA;
  ULONG                          TdCount;
  POHCI_HCD_ENDPOINT_DESCRIPTOR  ED;

  DPRINT("OHCI_OpenControlEndpoint: ... \n");

  TdCount = (EndpointProperties->BufferLength - sizeof(OHCI_HCD_ENDPOINT_DESCRIPTOR)) / 
            sizeof(OHCI_HCD_TRANSFER_DESCRIPTOR);

  OhciEndpoint->MaxTransferDescriptors = TdCount;
  OhciEndpoint->HeadED = &OhciExtension->ControlStaticED;

  OhciEndpoint->FirstTD = (POHCI_HCD_TRANSFER_DESCRIPTOR)
                          ((ULONG_PTR)EndpointProperties->BufferVA + 
                           sizeof(OHCI_HCD_ENDPOINT_DESCRIPTOR));

  if ( TdCount )
  {
    TdVA = OhciEndpoint->FirstTD;
    TdPA = (POHCI_HCD_TRANSFER_DESCRIPTOR)
           ((ULONG_PTR)EndpointProperties->BufferPA + 
           sizeof(OHCI_HCD_ENDPOINT_DESCRIPTOR));

    do
    {
      DPRINT("OHCI_OpenControlEndpoint: InitTD. TdVA - %p, TdPA - %p\n", TdVA, TdPA);

      TdVA->PhysicalAddress  = TdPA;
      TdVA->Flags            = 0;
      TdVA->OhciTransfer     = NULL;

      ++TdPA;
      ++TdVA;
      --TdCount;
    }
    while ( TdCount );
  }

  ED = (POHCI_HCD_ENDPOINT_DESCRIPTOR)EndpointProperties->BufferVA;

  OhciEndpoint->ED = OHCI_InitializeED(OhciEndpoint,
                                       ED,
                                       OhciEndpoint->FirstTD,
                                       EndpointProperties->BufferPA);

  OHCI_InsertEndpointInSchedule(OhciEndpoint);

  return 0;
}

ULONG NTAPI
OHCI_OpenBulkEndpoint(
         IN POHCI_EXTENSION OhciExtension,
         IN PUSBPORT_ENDPOINT_PROPERTIES EndpointProperties,
         IN POHCI_ENDPOINT OhciEndpoint)
{
  DPRINT("OHCI_OpenBulkEndpoint: ... \n");
ASSERT(FALSE);
  return 0;
}

ULONG NTAPI
OHCI_OpenEndpoint(
    IN PVOID Context,
    IN PVOID PortEndpointParameters,
    IN PVOID MiniportEndpoint)
{
  POHCI_EXTENSION               OhciExtension;
  POHCI_ENDPOINT                OhciEndpoint;
  PUSBPORT_ENDPOINT_PROPERTIES  EndpointProperties;
  ULONG                         TransferType;
  ULONG                         Result;

  DPRINT("OHCI_OpenEndpoint: ... \n");

  OhciExtension = (POHCI_EXTENSION)Context;
  OhciEndpoint = (POHCI_ENDPOINT)MiniportEndpoint;
  EndpointProperties = (PUSBPORT_ENDPOINT_PROPERTIES)PortEndpointParameters;

  RtlCopyMemory(&OhciEndpoint->OhciEndpointProperties,
                PortEndpointParameters,
                sizeof(USBPORT_ENDPOINT_PROPERTIES));

  InitializeListHead(&OhciEndpoint->TDList);

  TransferType = EndpointProperties->TransferType;

  switch ( TransferType )
  {
    case 0: // Control
      Result = OHCI_OpenControlEndpoint(
                        OhciExtension, EndpointProperties, OhciEndpoint);
      break;

    case 1: // Iso
      ASSERT(FALSE);
      break;

    case 2: // Bulk
      Result = OHCI_OpenBulkEndpoint(
                        OhciExtension, EndpointProperties, OhciEndpoint);
      break;

    case 3: // Interrupt
      ASSERT(FALSE);
      break;

    default:
      Result = 1;
      break;
  }

  return Result;
}

ULONG NTAPI
OHCI_ControlTransfer(
    POHCI_EXTENSION OhciExtension,
    POHCI_ENDPOINT OhciEndpoint,
    PUSBPORT_TRANSFER_PARAMETERS TransferParameters,
    POHCI_TRANSFER OhciTransfer,
    PUSBPORT_SCATTER_GATHER_LIST TransferSGList)
{
  DPRINT("OHCI_ControlTransfer: \n");
ASSERT(FALSE);
  return 0;
}

ULONG NTAPI
OHCI_SubmitTransfer(
    IN PVOID Context,
    IN PVOID OhciEndpoint,
    IN PVOID TransferParameters,
    IN PVOID OhciTransfer,
    IN PVOID TransferSGList)
{
  POHCI_EXTENSION               ohciExtension;
  POHCI_ENDPOINT                ohciEndpoint;
  PUSBPORT_TRANSFER_PARAMETERS  transferParameters;
  POHCI_TRANSFER                ohciTransfer;
  PUSBPORT_SCATTER_GATHER_LIST  transferSGList;
  ULONG                         TransferType;

  DPRINT("OHCI_SubmitTransfer: ... \n");

  ohciExtension      = (POHCI_EXTENSION)Context;
  ohciEndpoint       = (POHCI_ENDPOINT)OhciEndpoint;
  transferParameters = (PUSBPORT_TRANSFER_PARAMETERS)TransferParameters;
  ohciTransfer       = (POHCI_TRANSFER)OhciTransfer;
  transferSGList     = (PUSBPORT_SCATTER_GATHER_LIST)TransferSGList;

  RtlZeroMemory(ohciTransfer, sizeof(OHCI_TRANSFER));

  ohciTransfer->TransferParameters = transferParameters;
  ohciTransfer->OhciEndpoint       = ohciEndpoint;

  TransferType = ohciEndpoint->OhciEndpointProperties.TransferType;

  if ( TransferType == 0 )
  {
ASSERT(FALSE);
    return OHCI_ControlTransfer(
               ohciExtension,
               ohciEndpoint,
               transferParameters,
               ohciTransfer,
               transferSGList);
  }

  if ( TransferType == 2 || TransferType == 3 )
  {
ASSERT(FALSE);
  }

  return 1;
}

NTSTATUS NTAPI
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath)
{
    NTSTATUS Status;

    DPRINT("DriverEntry: DriverObject - %p, RegistryPath - %p\n", DriverObject, RegistryPath);
    DPRINT("DriverEntry: sizeof(USBPORT_REGISTRATION_PACKET) - %x\n", sizeof(USBPORT_REGISTRATION_PACKET));

    RtlZeroMemory(&RegPacket, sizeof(USBPORT_REGISTRATION_PACKET));

    RegPacket.Version                               = 1;                                          // Type: 1 - OHCI, 2 - UHCI, 3 - EHCI
    RegPacket.MiniPortExtensionSize                 = sizeof(OHCI_EXTENSION);                     // Size OHCI MiniPort Extension
    RegPacket.MiniPortEndpointSize                  = sizeof(OHCI_ENDPOINT);                      // 
    RegPacket.MiniPortTransferSize                  = sizeof(OHCI_TRANSFER);                      // 
    RegPacket.MiniPortResourcesSize                 = sizeof(OHCI_HC_RESOURCES);                  // 
    RegPacket.OpenEndpoint                          = OHCI_OpenEndpoint;
    RegPacket.QueryEndpointRequirements             = OHCI_QueryEndpointRequirements;
    RegPacket.StartController                       = OHCI_StartController;
    RegPacket.InterruptService                      = OHCI_InterruptService;
    RegPacket.InterruptDpc                          = OHCI_InterruptDpc;
    RegPacket.SubmitTransfer                        = OHCI_SubmitTransfer;
    RegPacket.SetEndpointState                      = OHCI_SetEndpointState;
    RegPacket.InterruptNextSOF                      = OHCI_InterruptNextSOF;
    RegPacket.EnableInterrupts                      = OHCI_EnableInterrupts;
    RegPacket.DisableInterrupts                     = OHCI_DisableInterrupts;
    RegPacket.RH_GetRootHubData                     = OHCI_RH_GetRootHubData;
    RegPacket.RH_GetStatus                          = OHCI_RH_GetStatus;
    RegPacket.RH_GetPortStatus                      = OHCI_RH_GetPortStatus;
    RegPacket.RH_GetHubStatus                       = OHCI_RH_GetHubStatus;
    RegPacket.RH_SetFeaturePortReset                = OHCI_RH_SetFeaturePortReset;
    RegPacket.RH_SetFeaturePortPower                = OHCI_RH_SetFeaturePortPower;
    RegPacket.RH_ClearFeaturePortConnectChange      = OHCI_RH_ClearFeaturePortConnectChange;
    RegPacket.RH_ClearFeaturePortResetChange        = OHCI_RH_ClearFeaturePortResetChange;
    RegPacket.RH_DisableIrq                         = OHCI_RH_DisableIrq;
    RegPacket.RH_EnableIrq                          = OHCI_RH_EnableIrq;

    Status = USBPORT_RegisterUSBPortDriver(DriverObject, 1, &RegPacket);
    DPRINT("DriverEntry: USBPORT_RegisterUSBPortDriver return Status - %x\n", Status);

    return Status;
}
