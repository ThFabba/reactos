#ifndef USBMPORT_H__
#define USBMPORT_H__


typedef struct _USBPORT_REGISTRATION_PACKET {

  ULONG                                          Version;                               // Type: 1 - OHCI, 2 - UHCI, 3 - EHCI
  ULONG                                          MiniportHwResourcesSize;               // 

} USBPORT_REGISTRATION_PACKET, *PUSBPORT_REGISTRATION_PACKET;

typedef struct _USBPORT_MINIPORT_INTERFACE {

  PDRIVER_OBJECT               DriverObject;
  LIST_ENTRY                   DriverList;
  PDRIVER_UNLOAD               DriverUnload;
  USBPORT_REGISTRATION_PACKET  Packet;

} USBPORT_MINIPORT_INTERFACE, *PUSBPORT_MINIPORT_INTERFACE;


#endif /* USBMPORT_H__ */
