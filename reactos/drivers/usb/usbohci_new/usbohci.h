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


#define OHCI_NUMBER_OF_INTERRUPTS   32
#define ED_EOF                      -1
#define MAXIMUM_OVERHEAD            210

#define OHCI_HC_STATE_RESET         0
#define OHCI_HC_STATE_RESUME        1
#define OHCI_HC_STATE_OPERATIONAL   2
#define OHCI_HC_STATE_SUSPEND       3

#define OHCI_HCD_TD_FLAG_ALLOCATED  0x00000001
#define OHCI_HCD_TD_FLAG_DONE       0x00000008

#define OHCI_TD_CC_NOT_ACCESSED     0x0E


typedef struct _OHCI_TRANSFER *POHCI_TRANSFER;

//---------------------------------------------------------------------
typedef union _OHCI_HC_TRANSFER_CONTROL {

  struct {
    ULONG  Reserved        : 18;
    ULONG  BufferRounding  : 1;
    ULONG  DirectionPID    : 2;
    ULONG  DelayInterrupt  : 3;
    ULONG  DataToggle      : 2;
    ULONG  ErrorCount      : 2;
    ULONG  ConditionCode   : 4;
  };

  ULONG  AsULONG;

} OHCI_HC_TRANSFER_CONTROL, *POHCI_HC_TRANSFER_CONTROL;

typedef struct _OHCI_TRANSFER_DESCRIPTOR { // 16-byte boundary

  // Hardware part
  OHCI_HC_TRANSFER_CONTROL  Control;         // dword 0
  PVOID                     CurrentBuffer;   // physical pointer of the memory location
  PULONG                    NextTD;          // physical pointer to the next OHCI_TRANSFER_DESCRIPTOR
  PVOID                     BufferEnd;       // physical pointer of the last byte in the buffer

} OHCI_TRANSFER_DESCRIPTOR, *POHCI_TRANSFER_DESCRIPTOR;

C_ASSERT(sizeof(OHCI_TRANSFER_DESCRIPTOR) == 16);

//---------------------------------------------------------------------
typedef union _OHCI_HC_ENDPOINT_CONTROL {

  struct {
    ULONG  FunctionAddress   : 7;
    ULONG  EndpointNumber    : 4;
    ULONG  Direction         : 2;
    ULONG  Speed             : 1;
    ULONG  sKip              : 1;
    ULONG  Format            : 1;
    ULONG  MaximumPacketSize : 11;
    ULONG  Reserved          : 5;
  };

  ULONG  AsULONG;

} OHCI_HC_ENDPOINT_CONTROL, *POHCI_HC_ENDPOINT_CONTROL;

typedef struct _OHCI_ENDPOINT_DESCRIPTOR { // 16-byte boundary

  // Hardware part
  OHCI_HC_ENDPOINT_CONTROL  EndpointControl; // dword 0
  ULONG_PTR                 TailPointer;     // 
  ULONG_PTR                 HeadPointer;     // physical pointer to the next OHCI_TRANSFER_DESCRIPTOR
  ULONG_PTR                 NextED;          // points to the next OHCI_ENDPOINT_DESCRIPTOR

} OHCI_ENDPOINT_DESCRIPTOR, *POHCI_ENDPOINT_DESCRIPTOR;

C_ASSERT(sizeof(OHCI_ENDPOINT_DESCRIPTOR) == 16);

//---------------------------------------------------------------------
typedef struct _OHCI_HCD_TRANSFER_DESCRIPTOR {

  // Hardware part
  OHCI_TRANSFER_DESCRIPTOR                HwTD;                      // dword 0

  // Hardware part for Isochronous Transfer Descriptor (FIXME)
  // Software part for General Transfer Descriptor
  USB_DEFAULT_PIPE_SETUP_PACKET           SetupPacket;               // FIXME for Isochronous (may be union ...)
  ULONG                                   Padded[2];                 // 

  // Software part
  struct _OHCI_HCD_TRANSFER_DESCRIPTOR *  PhysicalAddress;           // TdPA (+32)
  ULONG                                   Flags;                     // 
  POHCI_TRANSFER                          OhciTransfer;              // 
  struct _OHCI_HCD_TRANSFER_DESCRIPTOR *  HcdNextTD;                 // 
  ULONG                                   TransferLen;               // 
  LIST_ENTRY                              DoneLink;                  // 
  ULONG                                   Pad[1];

} OHCI_HCD_TRANSFER_DESCRIPTOR, *POHCI_HCD_TRANSFER_DESCRIPTOR;

C_ASSERT(sizeof(OHCI_HCD_TRANSFER_DESCRIPTOR) == 0x40);

//---------------------------------------------------------------------
typedef struct _OHCI_HCD_ENDPOINT_DESCRIPTOR {

  // Hardware part
  OHCI_ENDPOINT_DESCRIPTOR                HwED;                      // dword 0

  // Software part
  ULONG_PTR                               PhysicalAddress;           // EdPA
  LIST_ENTRY                              HcdEDLink;
  ULONG                                   Pad[9];

} OHCI_HCD_ENDPOINT_DESCRIPTOR, *POHCI_HCD_ENDPOINT_DESCRIPTOR;

C_ASSERT(sizeof(OHCI_HCD_ENDPOINT_DESCRIPTOR) == 0x40);

//---------------------------------------------------------------------
typedef struct _OHCI_STATIC_ENDPOINT_DESCRIPTOR {

  // Software only part
  POHCI_ENDPOINT_DESCRIPTOR   HwED;              // 
  ULONG_PTR                   PhysicalAddress;   // 
  UCHAR                       HeadIndex;         // 
  UCHAR                       Reserved[3];       // 
  LIST_ENTRY                  Link;              // 
  ULONG                       Type;              //  
  POHCI_ENDPOINT_DESCRIPTOR * pNextED;           //  
  ULONG                       HccaIndex;         //  

} OHCI_STATIC_ENDPOINT_DESCRIPTOR, *POHCI_STATIC_ENDPOINT_DESCRIPTOR;

//---------------------------------------------------------------------
typedef struct _OHCI_HCCA { // 256-byte boundary

  POHCI_ENDPOINT_DESCRIPTOR  InterrruptTable[OHCI_NUMBER_OF_INTERRUPTS];
  USHORT                     FrameNumber;
  USHORT                     Pad1;
  ULONG                      DoneHead;
  UCHAR                      Reserved[120];

} OHCI_HCCA, *POHCI_HCCA;

C_ASSERT(sizeof(OHCI_HCCA) == 256);

//---------------------------------------------------------------------
//- OHCI REGISTERS ----------------------------------------------------
//---------------------------------------------------------------------
typedef union _OHCI_HC_CONTROL {

  struct {
    ULONG  ControlBulkServiceRatio        : 2;
    ULONG  PeriodicListEnable             : 1;
    ULONG  IsochronousEnable              : 1;
    ULONG  ControlListEnable              : 1;
    ULONG  BulkListEnable                 : 1;
    ULONG  HostControllerFunctionalState  : 2;
    ULONG  InterruptRouting               : 1;
    ULONG  RemoteWakeupConnected          : 1;
    ULONG  RemoteWakeupEnable             : 1;
    ULONG  Reserved                       : 21;
  };

  ULONG  AsULONG;

} OHCI_HC_CONTROL, *POHCI_HC_CONTROL;

//---------------------------------------------------------------------
typedef union _OHCI_HC_COMMAND_STATUS {

  struct {
    ULONG  HostControllerReset     : 1;
    ULONG  ControlListFilled       : 1;
    ULONG  BulkListFilled          : 1;
    ULONG  OwnershipChangeRequest  : 1;
    ULONG  Reserved1               : 12;
    ULONG  SchedulingOverrunCount  : 1;
    ULONG  Reserved2               : 15;
  };

  ULONG  AsULONG;

} OHCI_HC_COMMAND_STATUS, *POHCI_HC_COMMAND_STATUS;

//---------------------------------------------------------------------
typedef union _OHCI_HC_INTERRUPT_STATUS {

  struct {
    ULONG  SchedulingOverrun    : 1;
    ULONG  WritebackDoneHead    : 1;
    ULONG  StartofFrame         : 1;
    ULONG  ResumeDetected       : 1;
    ULONG  UnrecoverableError   : 1;
    ULONG  FrameNumberOverflow  : 1;
    ULONG  RootHubStatusChange  : 1;
    ULONG  Reserved1            : 23;
    ULONG  OwnershipChange      : 1;
    ULONG  Reserved2            : 1; //NULL
  };

  ULONG  AsULONG;

} OHCI_HC_INTERRUPT_STATUS, *POHCI_HC_INTERRUPT_STATUS;

//---------------------------------------------------------------------
typedef union _OHCI_HC_INTERRUPT_ENABLE_DISABLE {

  struct {
    ULONG  SchedulingOverrun      : 1;
    ULONG  WritebackDoneHead      : 1;
    ULONG  StartofFrame           : 1;
    ULONG  ResumeDetected         : 1;
    ULONG  UnrecoverableError     : 1;
    ULONG  FrameNumberOverflow    : 1;
    ULONG  RootHubStatusChange    : 1;
    ULONG  Reserved1              : 23;
    ULONG  OwnershipChange        : 1;
    ULONG  MasterInterruptEnable  : 1;
  };

  ULONG  AsULONG;

} OHCI_HC_INTERRUPT_ENABLE_DISABLE, *POHCI_HC_INTERRUPT_ENABLE_DISABLE;

//---------------------------------------------------------------------
typedef union _OHCI_HC_FRAME_INTERVAL {

  struct {
    ULONG  FrameInterval        : 14;
    ULONG  Reserved             : 2;
    ULONG  FSLargestDataPacket  : 15;
    ULONG  FrameIntervalToggle  : 1;
  };

  ULONG  AsULONG;

} OHCI_HC_FRAME_INTERVAL, *POHCI_HC_FRAME_INTERVAL;

//---------------------------------------------------------------------
typedef union _OHCI_HC_RH_DESCRIPTORA {

  struct {
    ULONG  NumberDownstreamPorts      : 8;
    ULONG  PowerSwitchingMode         : 1;
    ULONG  NoPowerSwitching           : 1;
    ULONG  DeviceType                 : 1;
    ULONG  OverCurrentProtectionMode  : 1;
    ULONG  NoOverCurrentProtection    : 1;
    ULONG  Reserved2                  : 13;
    ULONG  PowerOnToPowerGoodTime     : 1;
  };

  ULONG  AsULONG;

} OHCI_HC_RH_DESCRIPTORA, *POHCI_HC_RH_DESCRIPTORA;

//---------------------------------------------------------------------
typedef union _OHCI_HC_RH_STATUS {

  union {

    struct {
      // When read
      ULONG  LocalPowerStatus            : 1;
      ULONG  OverCurrentIndicator        : 1;
      ULONG  Reserved10                  : 13;
      ULONG  DeviceRemoteWakeupEnable    : 1;
      ULONG  LocalPowerStatusChange      : 1;
      ULONG  OverCurrentIndicatorChangeR : 1;
      ULONG  Reserved20                  : 14;
    };

    struct {
      // When written 
      ULONG  ClearGlobalPower            : 1;
      ULONG  Reserved11                  : 14;
      ULONG  SetRemoteWakeupEnable       : 1;
      ULONG  SetGlobalPower              : 1;
      ULONG  OverCurrentIndicatorChangeW : 1;
      ULONG  Reserved22                  : 13;
      ULONG  ClearRemoteWakeupEnable     : 1;
    };

  };

  ULONG  AsULONG;

} OHCI_HC_RH_STATUS, *POHCI_HC_RH_STATUS;

//---------------------------------------------------------------------
typedef union _OHCI_HC_RH_PORT_STATUS {

  struct {

    union  {                    // 0 byte
      // When read
      UCHAR  CurrentConnectStatus            : 1;
      UCHAR  PortEnableStatus                : 1;
      UCHAR  PortSuspendStatus               : 1;
      UCHAR  PortOverCurrentIndicator        : 1;
      UCHAR  PortResetStatus                 : 1;
      UCHAR  Reserved1r                      : 3;

      // When written 
      UCHAR  ClearPortEnable                 : 1;
      UCHAR  SetPortEnable                   : 1;
      UCHAR  SetPortSuspend                  : 1;
      UCHAR  ClearSuspendStatus              : 1;
      UCHAR  SetPortReset                    : 1;
      UCHAR  Reserved1w                      : 3;
    };

    union  {                    // 1 byte
      // When read
      UCHAR  PortPowerStatus                 : 1;
      UCHAR  LowSpeedDeviceAttached          : 1;
      UCHAR  Reserved2r                      : 6;

      // When written 
      UCHAR  SetPortPower                    : 1;
      UCHAR  ClearPortPower                  : 1;
      UCHAR  Reserved2w                      : 6;
    };

      USHORT ConnectStatusChange             : 1;
      USHORT PortEnableStatusChange          : 1;
      USHORT PortSuspendStatusChange         : 1;
      USHORT PortOverCurrentIndicatorChange  : 1;
      USHORT PortResetStatusChange           : 1;
      USHORT Reserved3                       : 11;
  };

  ULONG  AsULONG;

} OHCI_HC_RH_PORT_STATUS, *POHCI_HC_RH_PORT_STATUS;

//---------------------------------------------------------------------
typedef struct _OHCI_OPERATIONAL_REGISTERS {

  ULONG                             HcRevision;          // +00 +0x00
  OHCI_HC_CONTROL                   HcControl;           // +04 +0x04
  OHCI_HC_COMMAND_STATUS            HcCommandStatus;     // +08 +0x08
  OHCI_HC_INTERRUPT_STATUS          HcInterruptStatus;   // +12 +0x0C
  OHCI_HC_INTERRUPT_ENABLE_DISABLE  HcInterruptEnable;   // +16 +0x10
  OHCI_HC_INTERRUPT_ENABLE_DISABLE  HcInterruptDisable;  // +20 +0x14
  ULONG                             HcHCCA;              // +24 +0x18
  ULONG                             HcPeriodCurrentED;   // +28 +0x1C
  ULONG                             HcControlHeadED;     // +32 +0x20
  ULONG                             HcControlCurrentED;  // +36 +0x24
  ULONG                             HcBulkHeadED;        // +40 +0x28
  ULONG                             HcBulkCurrentED;     // +44 +0x2C
  ULONG                             HcDoneHead;          // +48 +0x30
  OHCI_HC_FRAME_INTERVAL            HcFmInterval;        // +52 +0x34
  ULONG                             HcFmRemaining;       // +56 +0x38
  ULONG                             HcFmNumber;          // +60 +0x3C
  ULONG                             HcPeriodicStart;     // +64 +0x40
  ULONG                             HcLSThreshold;       // +68 +0x44
  OHCI_HC_RH_DESCRIPTORA            HcRhDescriptorA;     // +72 +0x48
  ULONG                             HcRhDescriptorB;     // +76 +0x4C
  OHCI_HC_RH_STATUS                 HcRhStatus;          // +80 +0x50
  OHCI_HC_RH_PORT_STATUS            HcRhPortStatus[1];   // +84 +0x54

} OHCI_OPERATIONAL_REGISTERS, *POHCI_OPERATIONAL_REGISTERS;
//---------------------------------------------------------------------
//- End of OHCI REGISTERS ---------------------------------------------
//---------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------
typedef struct _OHCI_HC_RESOURCES {

  OHCI_HCCA                     HcHCCA;                    // +0x000  (256 byte align)

  OHCI_ENDPOINT_DESCRIPTOR      InterrruptHeadED[63];      // +0x100  (16 byte align)
  OHCI_ENDPOINT_DESCRIPTOR      ControlHeadED;             // +0x4F0  (16 byte align)
  OHCI_ENDPOINT_DESCRIPTOR      BulkHeadED;                // +0x500  (16 byte align)

} OHCI_HC_RESOURCES, *POHCI_HC_RESOURCES;

//---------------------------------------------------------------------
typedef struct _OHCI_ENDPOINT {

  USBPORT_ENDPOINT_PROPERTIES       OhciEndpointProperties;  // 
  POHCI_HCD_ENDPOINT_DESCRIPTOR     ED;                      // 
  ULONG                             MaxTransferDescriptors;  // TdCount
  POHCI_STATIC_ENDPOINT_DESCRIPTOR  HeadED;                  // 
  POHCI_HCD_TRANSFER_DESCRIPTOR     FirstTD;                 // 
  LIST_ENTRY                        TDList;                  // 
  POHCI_HCD_TRANSFER_DESCRIPTOR     HcdHeadP;                // 
  POHCI_HCD_TRANSFER_DESCRIPTOR     HcdTailP;                // 

} OHCI_ENDPOINT, *POHCI_ENDPOINT;

//---------------------------------------------------------------------
typedef struct _OHCI_TRANSFER {

  ULONG                             Flags;                   // 
  PUSBPORT_TRANSFER_PARAMETERS      TransferParameters;      // 
  POHCI_ENDPOINT                    OhciEndpoint;            // 
  ULONG                             PendingTds;              // 

} OHCI_TRANSFER, *POHCI_TRANSFER;

// -------------------------------------------------------------------------------------------------------------
typedef struct _OHCI_EXTENSION {

  POHCI_OPERATIONAL_REGISTERS      OperationalRegs;           // HC Operational Registers
  OHCI_HC_FRAME_INTERVAL           FrameInterval;             // 
  ULONG                            HcResourcesVA;             // 
  ULONG                            HcResourcesPA;             //  
  PVOID                            ScheduleStartVA;           // 
  PVOID                            ScheduleStartPA;           // 
  OHCI_STATIC_ENDPOINT_DESCRIPTOR  InterrruptHeadED[63];      //
  OHCI_STATIC_ENDPOINT_DESCRIPTOR  ControlStaticED;           // [64]
  OHCI_STATIC_ENDPOINT_DESCRIPTOR  BulkStaticED;              // [65]

} OHCI_EXTENSION, *POHCI_EXTENSION;


extern USBPORT_REGISTRATION_PACKET RegPacket;

//- roothub.c -----------------------------------------------------------------
ULONG NTAPI
OHCI_RH_ClearFeaturePortConnectChange(
    IN PVOID Context,
    IN USHORT Port);

ULONG NTAPI
OHCI_RH_SetFeaturePortPower(
    IN PVOID Context,
    IN USHORT Port);

ULONG NTAPI 
OHCI_RH_GetRootHubData(
    IN PVOID Context,
    IN PVOID RootHubData);

ULONG NTAPI
OHCI_RH_GetStatus(
    IN PVOID Context,
    IN PULONG RhStatus);

ULONG NTAPI
OHCI_RH_GetPortStatus(
    IN PVOID Context,
    IN USHORT Port,
    IN PULONG PortStatus);

ULONG NTAPI
OHCI_RH_GetHubStatus(
    IN PVOID Context,
    IN PULONG HubStatus);

VOID NTAPI
OHCI_RH_EnableIrq(
    IN PVOID Context);

VOID NTAPI
OHCI_RH_DisableIrq(
    IN PVOID Context);

ULONG NTAPI
OHCI_RH_ClearFeaturePortResetChange(
    IN PVOID Context,
    IN USHORT Port);

ULONG NTAPI
OHCI_RH_SetFeaturePortReset(
    IN PVOID Context,
    IN USHORT Port);

//- usbohci.c -----------------------------------------------------------------
NTSTATUS NTAPI
USBPORT_RegisterUSBPortDriver(
    IN PDRIVER_OBJECT DriverObject,
    IN ULONG Version,
    IN PUSBPORT_REGISTRATION_PACKET RegistrationPacket);


#endif /* USBOHCI_H__ */
