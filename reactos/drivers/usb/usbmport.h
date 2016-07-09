#ifndef USBMPORT_H__
#define USBMPORT_H__


typedef struct _USBPORT_RESOURCES {

  ULONG                         TypesResources;               // 1 | 2 | 4  (Port | Interrupt | Memory)
  ULONG                         InterruptVector;              // 
  KIRQL                         InterruptLevel;               // 
  BOOLEAN                       ShareVector;                  // 
  KINTERRUPT_MODE               InterruptMode;                // 
  KAFFINITY                     InterruptAffinity;            // 
  PVOID                         ResourceBase;                 // 
  SIZE_T                        IoSpaceLength;                // 
  PVOID                         StartVA;                      // 
  PVOID                         StartPA;                      // 

} USBPORT_RESOURCES, *PUSBPORT_RESOURCES;

typedef ULONG    (NTAPI * PHCI_START_CONTROLLER                        )(PVOID, PUSBPORT_RESOURCES);
typedef VOID     (NTAPI * PHCI_INTERRUPT_DPC                           )(PVOID, BOOLEAN);
typedef BOOLEAN  (NTAPI * PHCI_INTERRUPT_SERVICE                       )(PVOID);
typedef VOID     (NTAPI * PHCI_ENABLE_INTERRUPTS                       )(PVOID);
typedef VOID     (NTAPI * PHCI_DISABLE_INTERRUPTS                      )(PVOID);
typedef ULONG    (NTAPI * PHCI_RH_GET_ROOT_HUB_DATA                    )(PVOID, PVOID);
typedef ULONG    (NTAPI * PHCI_RH_GET_STATUS                           )(PVOID, PULONG);
typedef ULONG    (NTAPI * PHCI_RH_GET_PORT_STATUS                      )(PVOID, USHORT, PULONG);
typedef ULONG    (NTAPI * PHCI_RH_GET_HUB_STATUS                       )(PVOID, PULONG);
typedef ULONG    (NTAPI * PHCI_RH_SET_FEATURE_PORT_RESET               )(PVOID, USHORT);
typedef ULONG    (NTAPI * PHCI_RH_SET_FEATURE_PORT_POWER               )(PVOID, USHORT);
typedef ULONG    (NTAPI * PHCI_RH_CLEAR_FEATURE_PORT_CONNECT_CHANGE    )(PVOID, USHORT);
typedef ULONG    (NTAPI * PHCI_RH_CLEAR_FEATURE_PORT_RESET_CHANGE      )(PVOID, USHORT);
typedef VOID     (NTAPI * PHCI_RH_DISABLE_IRQ                          )(PVOID);
typedef VOID     (NTAPI * PHCI_RH_ENABLE_IRQ                           )(PVOID);

typedef VOID     (NTAPI * PUSBPORT_INVALIDATE_ROOT_HUB                 )(PVOID);


typedef struct _USBPORT_REGISTRATION_PACKET {

  ULONG                                          Version;                               // Type: 1 - OHCI, 2 - UHCI, 3 - EHCI
  ULONG                                          MiniPortExtensionSize;                 // 
  ULONG                                          MiniPortEndpointSize;                  // 
  ULONG                                          MiniPortTransferSize;                  // 
  ULONG                                          MiniPortResourcesSize;                 // 
  PHCI_START_CONTROLLER                          StartController;                       //
  PHCI_INTERRUPT_SERVICE                         InterruptService;                      // 
  PHCI_INTERRUPT_DPC                             InterruptDpc;                          // 
  PHCI_ENABLE_INTERRUPTS                         EnableInterrupts;                      // 
  PHCI_DISABLE_INTERRUPTS                        DisableInterrupts;                     // 
  PHCI_RH_GET_ROOT_HUB_DATA                      RH_GetRootHubData;                     // 
  PHCI_RH_GET_STATUS                             RH_GetStatus;                          // 
  PHCI_RH_GET_PORT_STATUS                        RH_GetPortStatus;                      // 
  PHCI_RH_GET_HUB_STATUS                         RH_GetHubStatus;                       // 
  PHCI_RH_SET_FEATURE_PORT_RESET                 RH_SetFeaturePortReset;
  PHCI_RH_SET_FEATURE_PORT_POWER                 RH_SetFeaturePortPower;                // 
  PHCI_RH_CLEAR_FEATURE_PORT_CONNECT_CHANGE      RH_ClearFeaturePortConnectChange;      // 
  PHCI_RH_CLEAR_FEATURE_PORT_RESET_CHANGE        RH_ClearFeaturePortResetChange;        // 
  PHCI_RH_DISABLE_IRQ                            RH_DisableIrq;                         // 
  PHCI_RH_ENABLE_IRQ                             RH_EnableIrq;                          // 

  PUSBPORT_INVALIDATE_ROOT_HUB                   UsbPortInvalidateRootHub;              // 

} USBPORT_REGISTRATION_PACKET, *PUSBPORT_REGISTRATION_PACKET;

typedef struct _USBPORT_MINIPORT_INTERFACE {

  PDRIVER_OBJECT               DriverObject;
  LIST_ENTRY                   DriverList;
  PDRIVER_UNLOAD               DriverUnload;
  USBPORT_REGISTRATION_PACKET  Packet;

} USBPORT_MINIPORT_INTERFACE, *PUSBPORT_MINIPORT_INTERFACE;

//----------------------------------------------
typedef struct _USBPORT_ENDPOINT_PROPERTIES {

  USHORT                        DeviceAddress;
  USHORT                        EndpointAddress;
  USHORT                        MaxPacketSize;
  UCHAR                         Period;
  UCHAR                         Direction;
  ULONG                         DeviceSpeed;
  ULONG                         TransferType;
  ULONG                         MaxTransferSize;

} USBPORT_ENDPOINT_PROPERTIES, *PUSBPORT_ENDPOINT_PROPERTIES;

//----------------------------------------------
typedef struct _USBPORT_SCATTER_GATHER_ELEMENT {

  PHYSICAL_ADDRESS                SgPhysicalAddress;
  ULONG                           SgTransferLength;
  ULONG                           SgOffset;

} USBPORT_SCATTER_GATHER_ELEMENT, *PUSBPORT_SCATTER_GATHER_ELEMENT;

//----------------------------------------------
typedef struct _USBPORT_SCATTER_GATHER_LIST {

  ULONG_PTR                       CurrentVa;
  PVOID                           MappedSystemVa;
  ULONG                           SgElementCount;

  USBPORT_SCATTER_GATHER_ELEMENT  SgElement[1];

} USBPORT_SCATTER_GATHER_LIST, *PUSBPORT_SCATTER_GATHER_LIST;

//----------------------------------------------
typedef struct _USBPORT_TRANSFER_PARAMETERS {

  ULONG                           TransferFlags;
  ULONG                           TransferBufferLength;
  USB_DEFAULT_PIPE_SETUP_PACKET   SetupPacket;

} USBPORT_TRANSFER_PARAMETERS, *PUSBPORT_TRANSFER_PARAMETERS;

#endif /* USBMPORT_H__ */
