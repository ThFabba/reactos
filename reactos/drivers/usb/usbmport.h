#ifndef USBMPORT_H__
#define USBMPORT_H__


typedef struct _USBPORT_REGISTRATION_PACKET {

  ULONG                                          Version;                               // Type: 1 - OHCI, 2 - UHCI, 3 - EHCI
  ULONG                                          MiniportHwResourcesSize;               // 

} USBPORT_REGISTRATION_PACKET, *PUSBPORT_REGISTRATION_PACKET;


#endif /* USBMPORT_H__ */
