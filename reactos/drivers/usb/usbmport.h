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
typedef VOID     (NTAPI * PHCI_ENABLE_INTERRUPTS                       )(PVOID);
typedef VOID     (NTAPI * PHCI_DISABLE_INTERRUPTS                      )(PVOID);
typedef ULONG    (NTAPI * PHCI_RH_GET_ROOT_HUB_DATA                    )(PVOID, PVOID);

typedef struct _USBPORT_REGISTRATION_PACKET {

  ULONG                                          Version;                               // Type: 1 - OHCI, 2 - UHCI, 3 - EHCI
  ULONG                                          MiniPortExtensionSize;                 // 
  ULONG                                          MiniPortEndpointSize;                  // 
  ULONG                                          MiniPortTransferSize;                  // 
  ULONG                                          MiniPortResourcesSize;                 // 
  PHCI_START_CONTROLLER                          StartController;                       //
  PHCI_ENABLE_INTERRUPTS                         EnableInterrupts;                      // 
  PHCI_DISABLE_INTERRUPTS                        DisableInterrupts;                     // 
  PHCI_RH_GET_ROOT_HUB_DATA                      RH_GetRootHubData;                     // 

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

#endif /* USBMPORT_H__ */
