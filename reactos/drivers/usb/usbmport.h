#ifndef USBMPORT_H__
#define USBMPORT_H__

typedef struct _USBPORT_RESOURCES {
  ULONG TypesResources; // 1 | 2 | 4  (Port | Interrupt | Memory)
  ULONG InterruptVector;
  KIRQL InterruptLevel;
  BOOLEAN ShareVector;
  KINTERRUPT_MODE InterruptMode;
  KAFFINITY InterruptAffinity;
  PVOID ResourceBase;
  SIZE_T IoSpaceLength;
  PVOID StartVA;
  PVOID StartPA;
} USBPORT_RESOURCES, *PUSBPORT_RESOURCES;

typedef ULONG MPSTATUS;
typedef ULONG RHSTATUS;

/* Miniport */

typedef MPSTATUS
(NTAPI *PHCI_OPEN_ENDPOINT)(
  PVOID,
  PVOID,
  PVOID);

typedef MPSTATUS
(NTAPI *PHCI_REOPEN_ENDPOINT)(
  PVOID,
  PVOID,
  PVOID);

typedef VOID
(NTAPI *PHCI_QUERY_ENDPOINT_REQUIREMENTS)(
  PVOID,
  PVOID,
  PULONG);

typedef VOID
(NTAPI *PHCI_CLOSE_ENDPOINT)(
  PVOID,
  PVOID,
  BOOLEAN);

typedef MPSTATUS
(NTAPI *PHCI_START_CONTROLLER)(
  PVOID,
  PUSBPORT_RESOURCES);

typedef VOID
(NTAPI *PHCI_STOP_CONTROLLER)(
  PVOID,
  ULONG);

typedef VOID
(NTAPI *PHCI_SUSPEND_CONTROLLER)(PVOID);

typedef MPSTATUS
(NTAPI *PHCI_RESUME_CONTROLLER)(PVOID);

typedef BOOLEAN
(NTAPI *PHCI_INTERRUPT_SERVICE)(PVOID);

typedef VOID
(NTAPI *PHCI_INTERRUPT_DPC)(
  PVOID,
  BOOLEAN);

typedef MPSTATUS
(NTAPI *PHCI_SUBMIT_TRANSFER)(
  PVOID,
  PVOID,
  PVOID,
  PVOID,
  PVOID);

typedef MPSTATUS
(NTAPI *PHCI_SUBMIT_ISO_TRANSFER)(
  PVOID,
  PVOID,
  PVOID,
  PVOID,
  PVOID);

typedef ULONG
(NTAPI *PHCI_ABORT_TRANSFER)(
  PVOID,
  PVOID,
  PVOID,
  PVOID);

typedef ULONG
(NTAPI *PHCI_GET_ENDPOINT_STATE)(
  PVOID,
  PVOID);

typedef VOID
(NTAPI *PHCI_SET_ENDPOINT_STATE)(
  PVOID,
  PVOID,
  ULONG);

typedef ULONG
(NTAPI *PHCI_POLL_ENDPOINT)(
  PVOID,
  PVOID);

typedef VOID
(NTAPI *PHCI_CHECK_CONTROLLER)(PVOID);

typedef ULONG
(NTAPI *PHCI_GET_32BIT_FRAME_NUMBER)(PVOID);

typedef VOID
(NTAPI *PHCI_INTERRUPT_NEXT_SOF)(PVOID);

typedef VOID
(NTAPI *PHCI_ENABLE_INTERRUPTS)(PVOID);

typedef VOID
(NTAPI *PHCI_DISABLE_INTERRUPTS)(PVOID);

typedef VOID
(NTAPI *PHCI_POLL_CONTROLLER)(PVOID);

typedef ULONG
(NTAPI *PHCI_SET_ENDPOINT_DATA_TOGGLE)(
  PVOID,
  PVOID,
  ULONG);

typedef ULONG
(NTAPI *PHCI_GET_ENDPOINT_STATUS)(
  PVOID,
  PVOID);

typedef VOID
(NTAPI *PHCI_SET_ENDPOINT_STATUS)(
  PVOID,
  PVOID,
  ULONG);

typedef VOID
(NTAPI *PHCI_RESET_CONTROLLER)(PVOID);

/* Roothub */

typedef RHSTATUS
(NTAPI *PHCI_RH_GET_ROOT_HUB_DATA)(
  PVOID,
  PVOID);

typedef RHSTATUS
(NTAPI *PHCI_RH_GET_STATUS)(
  PVOID,
  PUSHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_GET_PORT_STATUS)(
  PVOID,
  USHORT,
  PULONG);

typedef RHSTATUS
(NTAPI *PHCI_RH_GET_HUB_STATUS)(
  PVOID,
  PULONG);

typedef RHSTATUS
(NTAPI *PHCI_RH_SET_FEATURE_PORT_RESET)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_SET_FEATURE_PORT_POWER)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_SET_FEATURE_PORT_ENABLE)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_SET_FEATURE_PORT_SUSPEND)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_CLEAR_FEATURE_PORT_ENABLE)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_CLEAR_FEATURE_PORT_POWER)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_CLEAR_FEATURE_PORT_SUSPEND)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_CLEAR_FEATURE_PORT_ENABLE_CHANGE)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_CLEAR_FEATURE_PORT_CONNECT_CHANGE)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_CLEAR_FEATURE_PORT_RESET_CHANGE)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_CLEAR_FEATURE_PORT_SUSPEND_CHANGE)(
  PVOID,
  USHORT);

typedef RHSTATUS
(NTAPI *PHCI_RH_CLEAR_FEATURE_PORT_OVERCURRENT_CHANGE)(
  PVOID,
  USHORT);

typedef VOID
(NTAPI *PHCI_RH_DISABLE_IRQ)(PVOID);

typedef VOID
(NTAPI *PHCI_RH_ENABLE_IRQ)(PVOID);

/* Miniport ioctl */

typedef MPSTATUS
(NTAPI *PHCI_START_SEND_ONE_PACKET)(
  PVOID,
  PVOID,
  PVOID,
  PULONG,
  PVOID,
  PVOID,
  PVOID,
  PVOID);

typedef MPSTATUS
(NTAPI *PHCI_END_SEND_ONE_PACKET)(
  PVOID,
  PVOID,
  PVOID,
  PULONG,
  PVOID,
  PVOID,
  PVOID,
  PVOID);

typedef MPSTATUS
(NTAPI *PHCI_PASS_THRU)(
  PVOID,
  PVOID,
  ULONG,
  PVOID);

/* Port */

typedef ULONG
(NTAPI *PUSBPORT_DBG_PRINT)(
  PVOID,
  ULONG,
  PCH,
  ULONG,
  ULONG,
  ULONG,
  ULONG,
  ULONG,
  ULONG);

typedef ULONG
(NTAPI *PUSBPORT_TEST_DEBUG_BREAK)(PVOID);

typedef ULONG
(NTAPI *PUSBPORT_ASSERT_FAILURE)(
  PVOID,
  PVOID,
  PVOID,
  ULONG,
  PCHAR);

typedef MPSTATUS
(NTAPI *PUSBPORT_GET_MINIPORT_REGISTRY_KEY_VALUE)(
  PVOID,
  ULONG,
  PCWSTR,
  SIZE_T,
  PVOID,
  SIZE_T);

typedef ULONG
(NTAPI *PUSBPORT_INVALIDATE_ROOT_HUB)(PVOID);

typedef ULONG
(NTAPI *PUSBPORT_INVALIDATE_ENDPOINT)(
  PVOID,
  PVOID);

typedef ULONG
(NTAPI *PUSBPORT_COMPLETE_TRANSFER)(
  PVOID,
  PVOID,
  PVOID,
  USBD_STATUS,
  SIZE_T);

typedef ULONG
(NTAPI *PUSBPORT_COMPLETE_ISO_TRANSFER)(
  PVOID,
  PVOID,
  PVOID,
  ULONG);

typedef ULONG
(NTAPI *PUSBPORT_LOG_ENTRY)(
  PVOID,
  PVOID,
  PVOID,
  ULONG,
  ULONG,
  ULONG);

typedef PVOID
(NTAPI *PUSBPORT_GET_MAPPED_VIRTUAL_ADDRESS)(
  PVOID,
  PVOID,
  PVOID);

typedef ULONG
(NTAPI *PUSBPORT_REQUEST_ASYNC_CALLBACK)(
  PVOID,
  ULONG,
  PVOID,
  SIZE_T,
  ULONG);

typedef MPSTATUS
(NTAPI *PUSBPORT_READ_WRITE_CONFIG_SPACE)(
  PVOID,
  BOOLEAN,
  PVOID,
  ULONG,
  ULONG);

typedef NTSTATUS
(NTAPI *PUSBPORT_WAIT)(
  PVOID,
  ULONG); 

typedef ULONG
(NTAPI *PUSBPORT_INVALIDATE_CONTROLLER)(
  PVOID,
  ULONG); 

typedef VOID
(NTAPI *PUSBPORT_BUG_CHECK)(PVOID);

typedef ULONG
(NTAPI *PUSBPORT_NOTIFY_DOUBLE_BUFFER)(
  PVOID,
  PVOID,
  PVOID,
  SIZE_T);

/* Miniport */

typedef ULONG
(NTAPI *PHCI_REBALANCE_ENDPOINT)(
  PVOID,
  PVOID,
  PVOID);

typedef VOID
(NTAPI *PHCI_FLUSH_INTERRUPTS)(PVOID);

typedef MPSTATUS
(NTAPI *PHCI_RH_CHIRP_ROOT_PORT)(
  PVOID,
  USHORT);

typedef VOID
(NTAPI *PHCI_TAKE_PORT_CONTROL)(PVOID);

#define USB_MINIPORT_VERSION_OHCI 0x01
#define USB_MINIPORT_VERSION_UHCI 0x02
#define USB_MINIPORT_VERSION_EHCI 0x03

typedef struct _USBPORT_REGISTRATION_PACKET {
  ULONG MiniPortVersion;
  ULONG MiniPortFlags;
  ULONG MiniPortBusBandwidth;
  ULONG Reserved1;
  SIZE_T MiniPortExtensionSize;
  SIZE_T MiniPortEndpointSize;
  SIZE_T MiniPortTransferSize;
  ULONG Reserved2;
  ULONG Reserved3;
  SIZE_T MiniPortResourcesSize;

  /* Miniport */

  PHCI_OPEN_ENDPOINT OpenEndpoint;
  PHCI_REOPEN_ENDPOINT ReopenEndpoint;
  PHCI_QUERY_ENDPOINT_REQUIREMENTS QueryEndpointRequirements;
  PHCI_CLOSE_ENDPOINT CloseEndpoint;
  PHCI_START_CONTROLLER StartController;
  PHCI_STOP_CONTROLLER StopController;
  PHCI_SUSPEND_CONTROLLER SuspendController;
  PHCI_RESUME_CONTROLLER ResumeController;
  PHCI_INTERRUPT_SERVICE InterruptService;
  PHCI_INTERRUPT_DPC InterruptDpc;
  PHCI_SUBMIT_TRANSFER SubmitTransfer;
  PHCI_SUBMIT_ISO_TRANSFER SubmitIsoTransfer;
  PHCI_ABORT_TRANSFER AbortTransfer;
  PHCI_GET_ENDPOINT_STATE GetEndpointState;
  PHCI_SET_ENDPOINT_STATE SetEndpointState;
  PHCI_POLL_ENDPOINT PollEndpoint;
  PHCI_CHECK_CONTROLLER CheckController;
  PHCI_GET_32BIT_FRAME_NUMBER Get32BitFrameNumber;
  PHCI_INTERRUPT_NEXT_SOF InterruptNextSOF;
  PHCI_ENABLE_INTERRUPTS EnableInterrupts;
  PHCI_DISABLE_INTERRUPTS DisableInterrupts;
  PHCI_POLL_CONTROLLER PollController;
  PHCI_SET_ENDPOINT_DATA_TOGGLE SetEndpointDataToggle;
  PHCI_GET_ENDPOINT_STATUS GetEndpointStatus;
  PHCI_SET_ENDPOINT_STATUS SetEndpointStatus;
  PHCI_RESET_CONTROLLER ResetController;

  /* Roothub */

  PHCI_RH_GET_ROOT_HUB_DATA RH_GetRootHubData;
  PHCI_RH_GET_STATUS RH_GetStatus;
  PHCI_RH_GET_PORT_STATUS RH_GetPortStatus;
  PHCI_RH_GET_HUB_STATUS RH_GetHubStatus;
  PHCI_RH_SET_FEATURE_PORT_RESET RH_SetFeaturePortReset;
  PHCI_RH_SET_FEATURE_PORT_POWER RH_SetFeaturePortPower;
  PHCI_RH_SET_FEATURE_PORT_ENABLE RH_SetFeaturePortEnable;
  PHCI_RH_SET_FEATURE_PORT_SUSPEND RH_SetFeaturePortSuspend;
  PHCI_RH_CLEAR_FEATURE_PORT_ENABLE RH_ClearFeaturePortEnable;
  PHCI_RH_CLEAR_FEATURE_PORT_POWER RH_ClearFeaturePortPower;
  PHCI_RH_CLEAR_FEATURE_PORT_SUSPEND RH_ClearFeaturePortSuspend;
  PHCI_RH_CLEAR_FEATURE_PORT_ENABLE_CHANGE RH_ClearFeaturePortEnableChange;
  PHCI_RH_CLEAR_FEATURE_PORT_CONNECT_CHANGE RH_ClearFeaturePortConnectChange;
  PHCI_RH_CLEAR_FEATURE_PORT_RESET_CHANGE RH_ClearFeaturePortResetChange;
  PHCI_RH_CLEAR_FEATURE_PORT_SUSPEND_CHANGE RH_ClearFeaturePortSuspendChange;
  PHCI_RH_CLEAR_FEATURE_PORT_OVERCURRENT_CHANGE RH_ClearFeaturePortOvercurrentChange;
  PHCI_RH_DISABLE_IRQ RH_DisableIrq;
  PHCI_RH_ENABLE_IRQ RH_EnableIrq;

  /* Miniport ioctl */

  PHCI_START_SEND_ONE_PACKET StartSendOnePacket;
  PHCI_END_SEND_ONE_PACKET EndSendOnePacket;
  PHCI_PASS_THRU PassThru;

  /* Port */

  PUSBPORT_DBG_PRINT UsbPortDbgPrint;
  PUSBPORT_TEST_DEBUG_BREAK UsbPortTestDebugBreak;
  PUSBPORT_ASSERT_FAILURE UsbPortAssertFailure;
  PUSBPORT_GET_MINIPORT_REGISTRY_KEY_VALUE UsbPortGetMiniportRegistryKeyValue;
  PUSBPORT_INVALIDATE_ROOT_HUB UsbPortInvalidateRootHub;
  PUSBPORT_INVALIDATE_ENDPOINT UsbPortInvalidateEndpoint;
  PUSBPORT_COMPLETE_TRANSFER UsbPortCompleteTransfer;
  PUSBPORT_COMPLETE_ISO_TRANSFER UsbPortCompleteIsoTransfer;
  PUSBPORT_LOG_ENTRY UsbPortLogEntry;
  PUSBPORT_GET_MAPPED_VIRTUAL_ADDRESS UsbPortGetMappedVirtualAddress;
  PUSBPORT_REQUEST_ASYNC_CALLBACK UsbPortRequestAsyncCallback;
  PUSBPORT_READ_WRITE_CONFIG_SPACE UsbPortReadWriteConfigSpace;
  PUSBPORT_WAIT UsbPortWait;
  PUSBPORT_INVALIDATE_CONTROLLER UsbPortInvalidateController;
  PUSBPORT_BUG_CHECK UsbPortBugCheck;
  PUSBPORT_NOTIFY_DOUBLE_BUFFER UsbPortNotifyDoubleBuffer;

  /* Miniport */

  PHCI_REBALANCE_ENDPOINT RebalanceEndpoint;
  PHCI_FLUSH_INTERRUPTS FlushInterrupts;
  PHCI_RH_CHIRP_ROOT_PORT RH_ChirpRootPort;
  PHCI_TAKE_PORT_CONTROL TakePortControl;
  ULONG Reserved4;
  ULONG Reserved5;
} USBPORT_REGISTRATION_PACKET, *PUSBPORT_REGISTRATION_PACKET;

typedef struct _USBPORT_MINIPORT_INTERFACE {
  PDRIVER_OBJECT DriverObject;
  LIST_ENTRY DriverLink;
  PDRIVER_UNLOAD DriverUnload;
  ULONG Version;
  USBPORT_REGISTRATION_PACKET Packet;
} USBPORT_MINIPORT_INTERFACE, *PUSBPORT_MINIPORT_INTERFACE;

typedef struct _USBPORT_ENDPOINT_PROPERTIES {
  USHORT DeviceAddress;
  USHORT EndpointAddress;
  USHORT MaxPacketSize;
  UCHAR Period;
  UCHAR Direction;
  ULONG DeviceSpeed;
  ULONG TransferType;
  ULONG MaxTransferSize;
  ULONG BufferVA;
  ULONG BufferPA;
  ULONG BufferLength;
} USBPORT_ENDPOINT_PROPERTIES, *PUSBPORT_ENDPOINT_PROPERTIES;

typedef struct _USBPORT_SCATTER_GATHER_ELEMENT {
  PHYSICAL_ADDRESS SgPhysicalAddress;
  ULONG SgTransferLength;
  ULONG SgOffset;
} USBPORT_SCATTER_GATHER_ELEMENT, *PUSBPORT_SCATTER_GATHER_ELEMENT;

typedef struct _USBPORT_SCATTER_GATHER_LIST {
  ULONG Flags;
  ULONG_PTR CurrentVa;
  PVOID MappedSystemVa;
  ULONG SgElementCount;
  USBPORT_SCATTER_GATHER_ELEMENT  SgElement[1];
} USBPORT_SCATTER_GATHER_LIST, *PUSBPORT_SCATTER_GATHER_LIST;

typedef struct _USBPORT_TRANSFER_PARAMETERS {
  ULONG TransferFlags;
  ULONG TransferBufferLength;
  USB_DEFAULT_PIPE_SETUP_PACKET SetupPacket;
} USBPORT_TRANSFER_PARAMETERS, *PUSBPORT_TRANSFER_PARAMETERS;

#endif /* USBMPORT_H__ */
