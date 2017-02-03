#include "usbport.h"

#define NDEBUG
#include <debug.h>

#define NDEBUG_USBPORT_CORE
#include "usbdebug.h"

IO_COMPLETION_ROUTINE USBPORT_FdoStartCompletion;

NTSTATUS
NTAPI
USBPORT_FdoStartCompletion(IN PDEVICE_OBJECT DeviceObject,
                           IN PIRP Irp,
                           IN PVOID Context)
{
    KeSetEvent((PKEVENT)Context, EVENT_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
NTAPI
USBPORT_RegisterDeviceInterface(IN PDEVICE_OBJECT PdoDevice,
                                IN PDEVICE_OBJECT DeviceObject,
                                IN CONST GUID *InterfaceClassGuid,
                                IN BOOLEAN Enable)
{
    PUSBPORT_RHDEVICE_EXTENSION DeviceExtension;
    PUNICODE_STRING SymbolicLinkName;
    NTSTATUS Status;

    DPRINT("USBPORT_RegisterDeviceInterface: Enable - %x\n", Enable);

    DeviceExtension = (PUSBPORT_RHDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    SymbolicLinkName = &DeviceExtension->CommonExtension.SymbolicLinkName;

    if (Enable)
    {
        Status = IoRegisterDeviceInterface(PdoDevice,
                                           InterfaceClassGuid,
                                           NULL,
                                           SymbolicLinkName);

        if (NT_SUCCESS(Status))
        {
            DeviceExtension->CommonExtension.IsInterfaceEnabled = 1;

            Status = USBPORT_SetRegistryKeyValue(PdoDevice,
                                                 (HANDLE)0,
                                                 REG_SZ,
                                                 L"SymbolicName",
                                                 SymbolicLinkName->Buffer,
                                                 SymbolicLinkName->Length);

            if (NT_SUCCESS(Status))
            {
                DPRINT("USBPORT_RegisterDeviceInterface: LinkName  - %wZ\n",
                       &DeviceExtension->CommonExtension.SymbolicLinkName);

                Status = IoSetDeviceInterfaceState(SymbolicLinkName, TRUE);
            }
        }
    }
    else if (DeviceExtension->CommonExtension.IsInterfaceEnabled)
    {
        // Disable device interface
        Status = IoSetDeviceInterfaceState(SymbolicLinkName, FALSE);

        if (NT_SUCCESS(Status))
            RtlFreeUnicodeString(SymbolicLinkName);

        DeviceExtension->CommonExtension.IsInterfaceEnabled = 0; // Disabled interface
    }

    return Status;
}

BOOLEAN
NTAPI
USBPORT_IsSelectiveSuspendEnabled(IN PDEVICE_OBJECT FdoDevice)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    ULONG IsEnabled = 0;

    DPRINT("USBPORT_IsSelectiveSuspendEnabled: ... \n");

    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;

    USBPORT_GetRegistryKeyValueFullInfo(FdoDevice,
                                        FdoExtension->CommonExtension.LowerPdoDevice,
                                        1,
                                        L"HcDisableSelectiveSuspend",
                                        52, //FIXME: Length "HcDisableSelectiveSuspend" + 0
                                        &IsEnabled,
                                        sizeof(IsEnabled));

    return (IsEnabled == 0);
}

NTSTATUS
NTAPI
USBPORT_GetConfigValue(IN PWSTR ValueName,
                       IN ULONG ValueType,
                       IN PVOID ValueData,
                       IN ULONG ValueLength,
                       IN PVOID Context,
                       IN PVOID EntryContext)
{
    NTSTATUS Status = STATUS_SUCCESS;

    DPRINT("USBPORT_GetConfigValue \n");

    if (ValueType == REG_BINARY)
    {
        *(PUCHAR)EntryContext = *(PUCHAR)ValueData;
    }
    else if (ValueType == REG_DWORD)
    {
        *(PULONG)EntryContext = *(PULONG)ValueData;
    }
    else
    {
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

NTSTATUS
NTAPI
USBPORT_GetDefaultBIOSx(IN PDEVICE_OBJECT FdoDevice,
                         IN PULONG UsbBIOSx,
                         IN PULONG DisableSelectiveSuspend,
                         IN PULONG DisableCcDetect,
                         IN PULONG IdleEpSupport,
                         IN PULONG IdleEpSupportEx,
                         IN PULONG SoftRetry)
{
    RTL_QUERY_REGISTRY_TABLE QueryTable[7];

    DPRINT("USBPORT_GetDefaultBIOS_X: ... \n");

    RtlZeroMemory(QueryTable, 7 * sizeof(RTL_QUERY_REGISTRY_TABLE));

    *UsbBIOSx = 2;

    QueryTable[0].QueryRoutine = USBPORT_GetConfigValue;
    QueryTable[0].Flags = 0;
    QueryTable[0].Name = L"UsbBIOSx";
    QueryTable[0].EntryContext = UsbBIOSx;
    QueryTable[0].DefaultType = REG_DWORD;
    QueryTable[0].DefaultData = UsbBIOSx;
    QueryTable[0].DefaultLength = sizeof(ULONG);

    QueryTable[1].QueryRoutine = USBPORT_GetConfigValue;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = L"DisableSelectiveSuspend";
    QueryTable[1].EntryContext = DisableSelectiveSuspend;
    QueryTable[1].DefaultType = REG_DWORD;
    QueryTable[1].DefaultData = DisableSelectiveSuspend;
    QueryTable[1].DefaultLength = sizeof(ULONG);

    QueryTable[2].QueryRoutine = USBPORT_GetConfigValue;
    QueryTable[2].Flags = 0;
    QueryTable[2].Name = L"DisableCcDetect";
    QueryTable[2].EntryContext = DisableCcDetect;
    QueryTable[2].DefaultType = REG_DWORD;
    QueryTable[2].DefaultData = DisableCcDetect;
    QueryTable[2].DefaultLength = sizeof(ULONG);

    QueryTable[3].QueryRoutine = USBPORT_GetConfigValue;
    QueryTable[3].Flags = 0;
    QueryTable[3].Name = L"EnIdleEndpointSupport";
    QueryTable[3].EntryContext = IdleEpSupport;
    QueryTable[3].DefaultType = REG_DWORD;
    QueryTable[3].DefaultData = IdleEpSupport;
    QueryTable[3].DefaultLength = sizeof(ULONG);

    QueryTable[4].QueryRoutine = USBPORT_GetConfigValue;
    QueryTable[4].Flags = 0;
    QueryTable[4].Name = L"EnIdleEndpointSupportEx";
    QueryTable[4].EntryContext = IdleEpSupportEx;
    QueryTable[4].DefaultType = REG_DWORD;
    QueryTable[4].DefaultData = IdleEpSupportEx;
    QueryTable[4].DefaultLength = sizeof(ULONG);

    QueryTable[5].QueryRoutine = USBPORT_GetConfigValue;
    QueryTable[5].Flags = 0;
    QueryTable[5].Name = L"EnSoftRetry";
    QueryTable[5].EntryContext = SoftRetry;
    QueryTable[5].DefaultType = REG_DWORD;
    QueryTable[5].DefaultData = SoftRetry;
    QueryTable[5].DefaultLength = sizeof(ULONG);

    return RtlQueryRegistryValues(RTL_REGISTRY_SERVICES,
                                  L"usb",
                                  QueryTable,
                                  NULL,
                                  NULL);
}

NTSTATUS
NTAPI
USBPORT_IsCompanionController(IN PDEVICE_OBJECT DeviceObject,
                              IN BOOLEAN *IsCompanion)
{
    PDEVICE_OBJECT HighestDevice;
    PIRP Irp;
    KEVENT Event;
    PIO_STACK_LOCATION IoStack;
    PCI_DEVICE_PRESENT_INTERFACE PciInterface = {0};
    PCI_DEVICE_PRESENCE_PARAMETERS Parameters   = {0};
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS Status;
    BOOLEAN IsPresent;

    DPRINT("USBPORT_IsCompanionController: ... \n");

    *IsCompanion = FALSE;

    KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

    HighestDevice = IoGetAttachedDeviceReference(DeviceObject);

    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP,
                                       HighestDevice,
                                       0,
                                       0,
                                       0,
                                       &Event,
                                       &IoStatusBlock);

    if (!Irp)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        ObDereferenceObject(HighestDevice);
        return Status;
    }

    IoStack = IoGetNextIrpStackLocation(Irp);

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = 0;

    IoStack->MinorFunction = IRP_MN_QUERY_INTERFACE;

    IoStack->Parameters.QueryInterface.InterfaceType = &GUID_PCI_DEVICE_PRESENT_INTERFACE;
    IoStack->Parameters.QueryInterface.Size = sizeof(PCI_DEVICE_PRESENT_INTERFACE);
    IoStack->Parameters.QueryInterface.Version = 1;
    IoStack->Parameters.QueryInterface.Interface = (PINTERFACE)&PciInterface;
    IoStack->Parameters.QueryInterface.InterfaceSpecificData = 0;

    Status = IoCallDriver(HighestDevice, Irp);

    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        Status = IoStatusBlock.Status;
    }

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBPORT_IsCompanionController: query interface failed\\n");
        ObDereferenceObject(HighestDevice);
        return Status;
    }

    DPRINT("USBPORT_IsCompanionController: query interface succeeded\n");

    if (PciInterface.Size < sizeof(PCI_DEVICE_PRESENT_INTERFACE))
    {
        DPRINT1("USBPORT_IsCompanionController: old version\n");
        ObDereferenceObject(HighestDevice);
        return Status;
    }

    Parameters.Size = sizeof(PCI_DEVICE_PRESENT_INTERFACE);

    Parameters.BaseClass = PCI_CLASS_SERIAL_BUS_CTLR;
    Parameters.SubClass = PCI_SUBCLASS_SB_USB;
    Parameters.ProgIf = PCI_INTERFACE_USB_ID_EHCI;

    Parameters.Flags = PCI_USE_LOCAL_BUS |
                       PCI_USE_LOCAL_DEVICE |
                       PCI_USE_CLASS_SUBCLASS |
                       PCI_USE_PROGIF;

    IsPresent = (PciInterface.IsDevicePresentEx)(PciInterface.Context, &Parameters);

    if (IsPresent)
    {
        DPRINT("USBPORT_IsCompanionController: Present EHCI controller for FDO - %p\n",
               DeviceObject);
    }
    else
    {
        DPRINT("USBPORT_IsCompanionController: No EHCI controller for FDO - %p\n",
               DeviceObject);
    }

    *IsCompanion = IsPresent;

    (PciInterface.InterfaceDereference)(PciInterface.Context);

    ObDereferenceObject(HighestDevice);

    return Status;
}

NTSTATUS
NTAPI
USBPORT_QueryPciBusInterface(IN PDEVICE_OBJECT FdoDevice)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    PBUS_INTERFACE_STANDARD BusInterface;
    PIO_STACK_LOCATION IoStack;
    IO_STATUS_BLOCK IoStatusBlock;
    PDEVICE_OBJECT HighestDevice;
    KEVENT Event;
    PIRP Irp;
    NTSTATUS Status;

    DPRINT("USBPORT_QueryPciBusInterface: ...  \n");

    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;
    BusInterface = &FdoExtension->BusInterface;

    RtlZeroMemory(BusInterface, sizeof(BUS_INTERFACE_STANDARD));
    KeInitializeEvent(&Event, SynchronizationEvent, FALSE);
    HighestDevice = IoGetAttachedDeviceReference(FdoDevice);

    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP,
                                       HighestDevice,
                                       NULL,
                                       0,
                                       NULL,
                                       &Event,
                                       &IoStatusBlock);

    if (Irp)
    {
        IoStack = IoGetNextIrpStackLocation(Irp);

        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        Irp->IoStatus.Information = 0;

        IoStack->MinorFunction = IRP_MN_QUERY_INTERFACE;

        IoStack->Parameters.QueryInterface.InterfaceType = &GUID_BUS_INTERFACE_STANDARD;
        IoStack->Parameters.QueryInterface.Size = sizeof(BUS_INTERFACE_STANDARD);
        IoStack->Parameters.QueryInterface.Version = 1;
        IoStack->Parameters.QueryInterface.Interface = (PINTERFACE)BusInterface;
        IoStack->Parameters.QueryInterface.InterfaceSpecificData = 0;

        Status = IoCallDriver(HighestDevice, Irp);

        if (Status == STATUS_PENDING)
        {
            KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Status = IoStatusBlock.Status;
        }
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    ObDereferenceObject(HighestDevice);

    DPRINT("USBPORT_QueryPciBusInterface: return Status - %x\n", Status);

    return Status;
}

NTSTATUS
NTAPI
USBPORT_QueryCapabilities(IN PDEVICE_OBJECT FdoDevice,
                          IN PDEVICE_CAPABILITIES Capabilities)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtention;
    PIRP Irp;
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    KEVENT Event;

    DPRINT("USBPORT_QueryCapabilities: ... \n");

    FdoExtention = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;

    RtlZeroMemory(Capabilities, sizeof(DEVICE_CAPABILITIES));

    Capabilities->Size     = 64;
    Capabilities->Version  = 1;
    Capabilities->Address  = -1;
    Capabilities->UINumber = -1;

    Irp = IoAllocateIrp(FdoExtention->CommonExtension.LowerDevice->StackSize, FALSE);

    if (!Irp)
    {
        DPRINT1("USBPORT_QueryCapabilities: No resources - IoAllocateIrp!\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    IoStack = IoGetNextIrpStackLocation(Irp);
    IoStack->MajorFunction = IRP_MJ_PNP;
    IoStack->MinorFunction = IRP_MN_QUERY_CAPABILITIES;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    IoSetCompletionRoutine(Irp,
                          USBPORT_FdoStartCompletion,
                          &Event,
                          TRUE,
                          TRUE,
                          TRUE);

    IoStack->Parameters.DeviceCapabilities.Capabilities = Capabilities;

    Status = IoCallDriver(FdoExtention->CommonExtension.LowerDevice, Irp);

    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
        Status = Irp->IoStatus.Status;
    }

    if (NT_SUCCESS(Status) && Capabilities)
    {
        USBPORT_DumpingCapabilities(Capabilities);
    }

    IoFreeIrp(Irp);

    return Status;
}

NTSTATUS
NTAPI
USBPORT_CreateLegacySymbolicLink(IN PDEVICE_OBJECT FdoDevice)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    WCHAR CharName[255] = {0};
    WCHAR CharDosName[255] = {0};
    UNICODE_STRING DeviceName;
    NTSTATUS Status;

    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;

    swprintf(CharName, L"\\Device\\USBFDO-%d", FdoExtension->FdoNameNumber);
    RtlInitUnicodeString(&DeviceName, CharName);

    swprintf(CharDosName, L"\\DosDevices\\HCD%d", FdoExtension->FdoNameNumber);
    RtlInitUnicodeString(&FdoExtension->DosDeviceSymbolicName, CharDosName);

    DPRINT("USBPORT_CreateLegacySymbolicLink: DeviceName - %wZ, DosSymbolicName - %wZ\n",
           &DeviceName,
           &FdoExtension->DosDeviceSymbolicName);

    Status = IoCreateSymbolicLink(&FdoExtension->DosDeviceSymbolicName,
                                  &DeviceName);

    if (NT_SUCCESS(Status))
    {
        FdoExtension->Flags |= USBPORT_FLAG_DOS_SYMBOLIC_NAME;
    }

    return Status;
}

NTSTATUS
NTAPI
USBPORT_StopDevice(IN PDEVICE_OBJECT FdoDevice)
{
    DPRINT1("USBPORT_StopDevice: UNIMPLEMENTED. FIXME\n");
    DbgBreakPoint();
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
USBPORT_StartDevice(IN PDEVICE_OBJECT FdoDevice,
                    IN PUSBPORT_RESOURCES UsbPortResources)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    PUSBPORT_REGISTRATION_PACKET Packet;
    NTSTATUS Status;
    PCI_COMMON_CONFIG PciConfig;
    ULONG BytesRead;
    DEVICE_DESCRIPTION DeviceDescription;
    PDMA_ADAPTER DmaAdapter = NULL;
    ULONG MiniPortStatus;
    PUSBPORT_COMMON_BUFFER_HEADER HeaderBuffer;
    ULONG ResultLength;
    KIRQL OldIrql;
    ULONG DisableSelectiveSuspend = 0;
    ULONG DisableCcDetect = 0;
    ULONG IdleEpSupport = 0;
    ULONG IdleEpSupportEx = 0;
    ULONG SoftRetry = 0;
    ULONG Limit2GB = 0;
    ULONG TotalBusBandwidth = 0;
    BOOLEAN IsCompanion = FALSE;
    ULONG LegacyBIOS;
    ULONG MiniportFlags;

    DPRINT("USBPORT_StartDevice: FdoDevice - %p, UsbPortResources - %p\n",
           FdoDevice,
           UsbPortResources);

    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;
    Packet = &FdoExtension->MiniPortInterface->Packet;

    Status = USBPORT_QueryPciBusInterface(FdoDevice);

    BytesRead = (*FdoExtension->BusInterface.GetBusData)(FdoExtension->BusInterface.Context,
                                                         PCI_WHICHSPACE_CONFIG,
                                                         &PciConfig,
                                                         0,
                                                         PCI_COMMON_HDR_LENGTH);

    if (BytesRead != PCI_COMMON_HDR_LENGTH)
    {
        DPRINT1("USBPORT_StartDevice: Failed to get pci config information!\n");
        goto ExitWithError;
    }

    FdoExtension->VendorID = PciConfig.VendorID;
    FdoExtension->DeviceID = PciConfig.DeviceID;
    FdoExtension->RevisionID = PciConfig.RevisionID;
    FdoExtension->ProgIf = PciConfig.ProgIf;
    FdoExtension->SubClass = PciConfig.SubClass;
    FdoExtension->BaseClass = PciConfig.BaseClass;

    RtlZeroMemory(&DeviceDescription, sizeof(DeviceDescription));

    DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
    DeviceDescription.Master = TRUE;
    DeviceDescription.ScatterGather = TRUE;
    DeviceDescription.Dma32BitAddresses = TRUE;
    DeviceDescription.InterfaceType = PCIBus;
    DeviceDescription.DmaWidth = Width32Bits;
    DeviceDescription.DmaSpeed = Compatible;
    DeviceDescription.MaximumLength = MAXULONG;

    DmaAdapter = IoGetDmaAdapter(FdoExtension->CommonExtension.LowerPdoDevice,
                                 &DeviceDescription,
                                 &FdoExtension->NumberMapRegs);

    FdoExtension->DmaAdapter = DmaAdapter;

    if (!DmaAdapter)
    {
        DPRINT1("USBPORT_StartDevice: Failed to get DmaAdapter!\n");
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ExitWithError;
    }

    Status = USBPORT_CreateWorkerThread(FdoDevice);
    if (!NT_SUCCESS(Status))
        goto ExitWithError;

    Status = USBPORT_QueryCapabilities(FdoDevice, &FdoExtension->Capabilities);
    if (!NT_SUCCESS(Status))
        goto ExitWithError;

    FdoExtension->PciDeviceNumber = FdoExtension->Capabilities.Address >> 16;
    FdoExtension->PciFunctionNumber = FdoExtension->Capabilities.Address & 0xFFFF;

    Status = IoGetDeviceProperty(FdoExtension->CommonExtension.LowerPdoDevice,
                                 DevicePropertyBusNumber,
                                 sizeof(ULONG),
                                 &FdoExtension->BusNumber,
                                 &ResultLength);

    if (!NT_SUCCESS(Status))
        goto ExitWithError;

    KeInitializeSpinLock(&FdoExtension->EndpointListSpinLock);
    KeInitializeSpinLock(&FdoExtension->EpStateChangeSpinLock);
    KeInitializeSpinLock(&FdoExtension->EndpointClosedSpinLock);
    KeInitializeSpinLock(&FdoExtension->DeviceHandleSpinLock);
    KeInitializeSpinLock(&FdoExtension->IdleIoCsqSpinLock);
    KeInitializeSpinLock(&FdoExtension->BadRequestIoCsqSpinLock);
    KeInitializeSpinLock(&FdoExtension->MapTransferSpinLock);
    KeInitializeSpinLock(&FdoExtension->FlushTransferSpinLock);
    KeInitializeSpinLock(&FdoExtension->FlushPendingTransferSpinLock);
    KeInitializeSpinLock(&FdoExtension->DoneTransferSpinLock);
    KeInitializeSpinLock(&FdoExtension->WorkerThreadEventSpinLock);
    KeInitializeSpinLock(&FdoExtension->MiniportSpinLock);
    KeInitializeSpinLock(&FdoExtension->TimerFlagsSpinLock);
    KeInitializeSpinLock(&FdoExtension->PowerWakeSpinLock);
    KeInitializeSpinLock(&FdoExtension->SetPowerD0SpinLock);
    KeInitializeSpinLock(&FdoExtension->RootHubCallbackSpinLock);

    KeInitializeDpc(&FdoExtension->IsrDpc, USBPORT_IsrDpc, FdoDevice);

    KeInitializeDpc(&FdoExtension->TransferFlushDpc,
                    USBPORT_TransferFlushDpc,
                    FdoDevice);

    KeInitializeDpc(&FdoExtension->WorkerRequestDpc,
                    USBPORT_WorkerRequestDpc,
                    FdoDevice);

    KeInitializeSemaphore(&FdoExtension->DeviceSemaphore, 1, 1);

    IoCsqInitialize(&FdoExtension->IdleIoCsq,
                    USBPORT_InsertIdleIrp,
                    USBPORT_RemoveIdleIrp,
                    USBPORT_PeekNextIdleIrp,
                    USBPORT_AcquireIdleLock,
                    USBPORT_ReleaseIdleLock,
                    USBPORT_CompleteCanceledIdleIrp);

    IoCsqInitialize(&FdoExtension->BadRequestIoCsq,
                    USBPORT_InsertBadRequest,
                    USBPORT_RemoveBadRequest,
                    USBPORT_PeekNextBadRequest,
                    USBPORT_AcquireBadRequestLock,
                    USBPORT_ReleaseBadRequestLock,
                    USBPORT_CompleteCanceledBadRequest);

    FdoExtension->IsrDpcCounter = -1;
    FdoExtension->IsrDpcHandlerCounter = -1;
    FdoExtension->IdleLockCounter = -1;
    FdoExtension->BadRequestLockCounter = -1;

    FdoExtension->UsbAddressBitMap[0] = 1;
    FdoExtension->UsbAddressBitMap[1] = 0;
    FdoExtension->UsbAddressBitMap[2] = 0;
    FdoExtension->UsbAddressBitMap[3] = 0;

    USBPORT_GetDefaultBIOSx(FdoDevice,
                            &FdoExtension->UsbBIOSx,
                            &DisableSelectiveSuspend,
                            &DisableCcDetect,
                            &IdleEpSupport,
                            &IdleEpSupportEx,
                            &SoftRetry);

    if (DisableSelectiveSuspend)
        FdoExtension->Flags |= USBPORT_FLAG_BIOS_DISABLE_SS;

    if (!DisableSelectiveSuspend &&
        USBPORT_IsSelectiveSuspendEnabled(FdoDevice))
    {
        FdoExtension->Flags |= USBPORT_FLAG_SELECTIVE_SUSPEND;
    }

    MiniportFlags = Packet->MiniPortFlags;

    if (MiniportFlags & USB_MINIPORT_FLAGS_POLLING)
        FdoExtension->Flags |= 4;

    if (MiniportFlags & USB_MINIPORT_FLAGS_WAKE_SUPPORT)
        FdoExtension->Flags |= 0x00200000;

    if (MiniportFlags & 0x20)
        FdoExtension->Flags = (FdoExtension->Flags & ~USBPORT_FLAG_SELECTIVE_SUSPEND) |
                              USBPORT_FLAG_BIOS_DISABLE_SS;

    USBPORT_SetRegistryKeyValue(FdoExtension->CommonExtension.LowerPdoDevice,
                                (PVOID)1,
                                REG_DWORD,
                                L"EnIdleEndpointSupport",
                                &IdleEpSupport,
                                sizeof(IdleEpSupport));

    USBPORT_SetRegistryKeyValue(FdoExtension->CommonExtension.LowerPdoDevice,
                                (PVOID)1,
                                REG_DWORD,
                                L"EnIdleEndpointSupportEx",
                                &IdleEpSupportEx,
                                sizeof(IdleEpSupportEx));

    USBPORT_SetRegistryKeyValue(FdoExtension->CommonExtension.LowerPdoDevice,
                                (PVOID)1,
                                REG_DWORD,
                                L"EnSoftRetry",
                                &SoftRetry,
                                sizeof(SoftRetry));

    USBPORT_GetRegistryKeyValueFullInfo(FdoDevice,
                                        FdoExtension->CommonExtension.LowerPdoDevice,
                                        1,
                                        L"CommonBuffer2GBLimit",
                                        42,
                                        &Limit2GB,
                                        sizeof(Limit2GB));

    FdoExtension->CommonBufferLimit = (Limit2GB != 0);

    if (FdoExtension->BaseClass == PCI_CLASS_SERIAL_BUS_CTLR &&
        FdoExtension->SubClass == PCI_SUBCLASS_SB_USB  &&
        FdoExtension->ProgIf < PCI_INTERFACE_USB_ID_EHCI)
    {
        Status = USBPORT_IsCompanionController(FdoDevice, &IsCompanion);

        if (!NT_SUCCESS(Status))
        {
            if (IsCompanion)
                FdoExtension->Flags |= USBPORT_FLAG_COMPANION_HC;
            else
                FdoExtension->Flags &= ~USBPORT_FLAG_COMPANION_HC;
        }
    }

    if (DisableCcDetect)
        FdoExtension->Flags &= USBPORT_FLAG_COMPANION_HC;

    TotalBusBandwidth = Packet->MiniPortBusBandwidth;
    FdoExtension->TotalBusBandwidth = TotalBusBandwidth;

    USBPORT_GetRegistryKeyValueFullInfo(FdoDevice,
                                        FdoExtension->CommonExtension.LowerPdoDevice,
                                        1,
                                        L"TotalBusBandwidth",
                                        36,
                                        &TotalBusBandwidth,
                                        sizeof(TotalBusBandwidth));

    if (TotalBusBandwidth != FdoExtension->TotalBusBandwidth)
        FdoExtension->TotalBusBandwidth = TotalBusBandwidth;

    FdoExtension->ActiveIrpTable = ExAllocatePoolWithTag(NonPagedPool,
                                                         sizeof(USBPORT_IRP_TABLE),
                                                         USB_PORT_TAG);

    if (!FdoExtension->ActiveIrpTable)
    {
        DPRINT1("USBPORT_StartDevice: Allocate ActiveIrpTable failed!\n");
        goto ExitWithError;
    }

    RtlZeroMemory(FdoExtension->ActiveIrpTable, sizeof(USBPORT_IRP_TABLE));

    FdoExtension->PendingIrpTable = ExAllocatePoolWithTag(NonPagedPool,
                                                          sizeof(USBPORT_IRP_TABLE),
                                                          USB_PORT_TAG);

    if (!FdoExtension->PendingIrpTable)
    {
        DPRINT1("USBPORT_StartDevice: Allocate PendingIrpTable failed!\n");
        goto ExitWithError;
    }

    RtlZeroMemory(FdoExtension->PendingIrpTable, sizeof(USBPORT_IRP_TABLE));

    Status = IoConnectInterrupt(&FdoExtension->InterruptObject,
                                USBPORT_InterruptService,
                                (PVOID)FdoDevice,
                                0,
                                UsbPortResources->InterruptVector,
                                UsbPortResources->InterruptLevel,
                                UsbPortResources->InterruptLevel,
                                UsbPortResources->InterruptMode,
                                UsbPortResources->ShareVector,
                                UsbPortResources->InterruptAffinity,
                                0);


    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBPORT_StartDevice: IoConnectInterrupt failed!\n");
        goto ExitWithError;
    }

    FdoExtension->Flags &= ~USBPORT_FLAG_INT_CONNECTED;

    if (Packet->MiniPortExtensionSize)
    {
        RtlZeroMemory(FdoExtension->MiniPortExt, Packet->MiniPortExtensionSize);
    }

    if (Packet->MiniPortResourcesSize)
    {
        HeaderBuffer = USBPORT_AllocateCommonBuffer(FdoDevice,
                                                    Packet->MiniPortResourcesSize);

        if (!HeaderBuffer)
        {
            DPRINT1("USBPORT_StartDevice: Failed to AllocateCommonBuffer!\n");
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ExitWithError;
        }

        UsbPortResources->StartVA = (PVOID)HeaderBuffer->VirtualAddress;
        UsbPortResources->StartPA = (PVOID)HeaderBuffer->PhysicalAddress;

        FdoExtension->MiniPortCommonBuffer = HeaderBuffer;
    }
    else
    {
        FdoExtension->MiniPortCommonBuffer = NULL;
    }

    KeAcquireSpinLock(&FdoExtension->MiniportSpinLock, &OldIrql);
    MiniPortStatus = Packet->StartController(FdoExtension->MiniPortExt,
                                             UsbPortResources);
    KeReleaseSpinLock(&FdoExtension->MiniportSpinLock, OldIrql);

    if (UsbPortResources->LegacySupport)
    {
        FdoExtension->Flags |= USBPORT_FLAG_LEGACY_SUPPORT;
        LegacyBIOS = 1;
    }
    else
    {
        LegacyBIOS = 0;
    }

    USBPORT_SetRegistryKeyValue(FdoExtension->CommonExtension.LowerPdoDevice,
                                (HANDLE)0,
                                REG_DWORD,
                                L"DetectedLegacyBIOS",
                                &LegacyBIOS,
                                sizeof(LegacyBIOS));

    if (MiniPortStatus)
    {
        DPRINT1("USBPORT_StartDevice: Failed to Start MiniPort. MiniPortStatus - %x\n",
                MiniPortStatus);

        if (FdoExtension->Flags & USBPORT_FLAG_INT_CONNECTED)
        {
            IoDisconnectInterrupt(FdoExtension->InterruptObject);
            FdoExtension->Flags &= ~USBPORT_FLAG_INT_CONNECTED;
        }

        if (FdoExtension->MiniPortCommonBuffer)
        {
            USBPORT_FreeCommonBuffer(FdoDevice, FdoExtension->MiniPortCommonBuffer);
            FdoExtension->MiniPortCommonBuffer = NULL;
        }

        goto ExitWithError;
    }
    else
    {
        USBPORT_MiniportInterrupts(FdoDevice, TRUE);
        FdoExtension->MiniPortFlags |= USBPORT_MPFLAG_INTERRUPTS_ENABLED;
    }

    FdoExtension->TimerValue = 500;
    USBPORT_StartTimer((PVOID)FdoDevice, 500);

    Status = USBPORT_RegisterDeviceInterface(FdoExtension->CommonExtension.LowerPdoDevice,
                                             FdoDevice,
                                             (CONST GUID *)&GUID_DEVINTERFACE_USB_HOST_CONTROLLER,
                                             TRUE);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("USBPORT_StartDevice: RegisterDeviceInterface failed!\n");
        goto ExitWithError;
    }

    USBPORT_CreateLegacySymbolicLink(FdoDevice);

    FdoExtension->Flags |= USBPORT_FLAG_HC_STARTED;

    DPRINT("USBPORT_StartDevice: Exit Status - %p\n", Status);
    return Status;

ExitWithError:
    USBPORT_StopDevice(FdoDevice);

    DPRINT1("USBPORT_StartDevice: ExitWithError Status - %p\n", Status);
    return Status;
}

NTSTATUS
NTAPI
USBPORT_ParseResources(IN PDEVICE_OBJECT FdoDevice,
                       IN PIRP Irp,
                       IN PUSBPORT_RESOURCES UsbPortResources)
{
    PCM_RESOURCE_LIST AllocatedResourcesTranslated;
    PCM_PARTIAL_RESOURCE_LIST ResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PortDescriptor = NULL;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR MemoryDescriptor = NULL;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR InterruptDescriptor = NULL;
    PIO_STACK_LOCATION IoStack;
    ULONG ix = 0;
    NTSTATUS Status = STATUS_SUCCESS;

    DPRINT("USBPORT_ParseResources: ... \n");

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    AllocatedResourcesTranslated = IoStack->Parameters.StartDevice.AllocatedResourcesTranslated;

    if (AllocatedResourcesTranslated)
    {
        RtlZeroMemory(UsbPortResources, sizeof(USBPORT_RESOURCES));

        ResourceList = &AllocatedResourcesTranslated->List[0].PartialResourceList;

        if (ResourceList->Count > 0)
        {
            PartialDescriptor = &ResourceList->PartialDescriptors[0];

            do
            {
                if (PartialDescriptor->Type == CmResourceTypePort) // 1
                {
                    if (!PortDescriptor)
                        PortDescriptor = PartialDescriptor;
                }
                else if (PartialDescriptor->Type == CmResourceTypeInterrupt) // 2
                {
                    if (!InterruptDescriptor)
                        InterruptDescriptor = PartialDescriptor;
                }
                else if (PartialDescriptor->Type == CmResourceTypeMemory) // 3
                {
                    if (!MemoryDescriptor)
                        MemoryDescriptor = PartialDescriptor;
                }
                ++ix;
                PartialDescriptor += 1;
            }
            while (ix < ResourceList->Count);
        }

        if (PortDescriptor)
        {
            if (PortDescriptor->Flags & CM_RESOURCE_PORT_IO)
            {
                UsbPortResources->ResourceBase = (PVOID)PortDescriptor->u.Port.Start.LowPart;
            }
            else
            {
                UsbPortResources->ResourceBase = MmMapIoSpace(PortDescriptor->u.Port.Start,
                                                              PortDescriptor->u.Port.Length,
                                                              0);
            }

            UsbPortResources->IoSpaceLength = PortDescriptor->u.Port.Length;

            if (UsbPortResources->ResourceBase)
            {
                UsbPortResources->TypesResources |= 1; // FIXME const
            }
            else
            {
                Status = STATUS_NONE_MAPPED;
            }
        }

        if (MemoryDescriptor && NT_SUCCESS(Status))
        {
            UsbPortResources->IoSpaceLength = MemoryDescriptor->u.Memory.Length;

            UsbPortResources->ResourceBase = MmMapIoSpace(MemoryDescriptor->u.Memory.Start,
                                                          MemoryDescriptor->u.Memory.Length,
                                                          0);

            if (UsbPortResources->ResourceBase)
            {
                UsbPortResources->TypesResources |= 4;// FIXME const
            }
            else
            {
                Status = STATUS_NONE_MAPPED;
            }
        }

        if (InterruptDescriptor && NT_SUCCESS(Status))
        {
            UsbPortResources->TypesResources |= 2; // FIXME const

            UsbPortResources->InterruptVector = InterruptDescriptor->u.Interrupt.Vector;
            UsbPortResources->InterruptLevel = InterruptDescriptor->u.Interrupt.Level;
            UsbPortResources->InterruptAffinity = InterruptDescriptor->u.Interrupt.Affinity;

            UsbPortResources->ShareVector = InterruptDescriptor->ShareDisposition ==
                                            CmResourceShareShared;

            UsbPortResources->InterruptMode = InterruptDescriptor->Flags == 
                                              CM_RESOURCE_INTERRUPT_LATCHED;
        }
    }
    else
    {
        Status = STATUS_NONE_MAPPED;
    }

    return Status;
}

NTSTATUS
NTAPI
USBPORT_CreatePdo(IN PDEVICE_OBJECT FdoDevice,
                  OUT PDEVICE_OBJECT *RootHubPdo)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    PUSBPORT_RHDEVICE_EXTENSION PdoExtension;
    UNICODE_STRING DeviceName;
    ULONG DeviceNumber = 0;
    PDEVICE_OBJECT DeviceObject = NULL;
    WCHAR CharDeviceName[64];
    NTSTATUS Status = STATUS_SUCCESS;

    DPRINT("USBPORT_CreatePdo: FdoDevice - %p, RootHubPdo - %p\n",
           FdoDevice,
           RootHubPdo);

    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;

    do
    {
        swprintf(CharDeviceName, L"\\Device\\USBPDO-%d", DeviceNumber);
        RtlInitUnicodeString(&DeviceName, CharDeviceName);

        DPRINT("USBPORT_CreatePdo: DeviceName - %wZ\n", &DeviceName);

        Status = IoCreateDevice(FdoExtension->MiniPortInterface->DriverObject,
                                sizeof(USBPORT_RHDEVICE_EXTENSION),
                                &DeviceName,
                                FILE_DEVICE_BUS_EXTENDER,
                                0,
                                FALSE,
                                &DeviceObject);

        ++DeviceNumber;
    }
    while (Status == STATUS_OBJECT_NAME_COLLISION);

    if (!NT_SUCCESS(Status))
    {
        *RootHubPdo = NULL;
        DPRINT1("USBPORT_CreatePdo: Filed create HubPdo!\n");
        return Status;
    }

    if (DeviceObject)
    {
        PdoExtension = (PUSBPORT_RHDEVICE_EXTENSION)DeviceObject->DeviceExtension;

        RtlZeroMemory(PdoExtension, sizeof(USBPORT_RHDEVICE_EXTENSION));

        PdoExtension->CommonExtension.SelfDevice = DeviceObject;
        PdoExtension->CommonExtension.IsPDO = TRUE;

        PdoExtension->FdoDevice = FdoDevice;
        PdoExtension->PdoNameNumber = DeviceNumber;

        USBPORT_AdjustDeviceCapabilities(FdoDevice, DeviceObject);

        DeviceObject->StackSize = FdoDevice->StackSize;

        DeviceObject->Flags |= DO_POWER_PAGABLE;
        DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    }
    else
    {
        Status = STATUS_UNSUCCESSFUL;
    }

    if (!NT_SUCCESS(Status))
        *RootHubPdo = NULL;
    else
        *RootHubPdo = DeviceObject;

    DPRINT("USBPORT_CreatePdo: HubPdo - %p\n", DeviceObject);
    return Status;
}

NTSTATUS
NTAPI
USBPORT_FdoPnP(IN PDEVICE_OBJECT FdoDevice,
               IN PIRP Irp)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    PUSBPORT_REGISTRATION_PACKET Packet;
    PUSBPORT_RESOURCES UsbPortResources;
    PIO_STACK_LOCATION IoStack;
    UCHAR Minor;
    KEVENT Event;
    NTSTATUS Status = STATUS_SUCCESS;
    DEVICE_RELATION_TYPE RelationType;
    PDEVICE_RELATIONS DeviceRelations;

    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;
    UsbPortResources = (PUSBPORT_RESOURCES)&FdoExtension->UsbPortResources;
    Packet = &FdoExtension->MiniPortInterface->Packet;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    Minor = IoStack->MinorFunction;

    DPRINT("USBPORT_FdoPnP: Minor - %d\n", Minor);

    RelationType = IoStack->Parameters.QueryDeviceRelations.Type;

    switch (Minor)
    {
        case IRP_MN_START_DEVICE: // 0
            DPRINT("IRP_MN_START_DEVICE\n");

            KeInitializeEvent(&Event, NotificationEvent, FALSE);

            IoCopyCurrentIrpStackLocationToNext(Irp);

            IoSetCompletionRoutine(Irp,
                                   USBPORT_FdoStartCompletion,
                                   &Event,
                                   TRUE,
                                   TRUE,
                                   TRUE);

            Status = IoCallDriver(FdoExtension->CommonExtension.LowerDevice,
                                  Irp);

            if (Status == STATUS_PENDING)
            {
                KeWaitForSingleObject(&Event,
                                      Suspended,
                                      KernelMode,
                                      FALSE,
                                      NULL);

                Status = Irp->IoStatus.Status;
            }

            if (NT_SUCCESS(Status))
            {
                Status = USBPORT_ParseResources(FdoDevice,
                                                Irp,
                                                UsbPortResources);

                if (NT_SUCCESS(Status))
                {
                    Status = USBPORT_StartDevice(FdoDevice, UsbPortResources);

                    if (NT_SUCCESS(Status))
                    {
                        FdoExtension->CommonExtension.DevicePowerState = PowerDeviceD0;

                        if (Packet->MiniPortFlags & USB_MINIPORT_FLAGS_USB2)
                        {
                            USBPORT_AddUSB2Fdo(FdoDevice);
                        }
                        else
                        {
                            USBPORT_AddUSB1Fdo(FdoDevice);
                        }

                    }
                }
            }

            Irp->IoStatus.Status = Status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Status;

        case IRP_MN_QUERY_REMOVE_DEVICE: // 1
            //DPRINT("IRP_MN_QUERY_REMOVE_DEVICE\n");
            DPRINT1("USBPORT_FdoPnP: IRP_MN_QUERY_REMOVE_DEVICE UNIMPLEMENTED. FIXME. \n");
            break;

        case IRP_MN_REMOVE_DEVICE: // 2
            //DPRINT("IRP_MN_REMOVE_DEVICE\n");
            DPRINT1("USBPORT_FdoPnP: IRP_MN_REMOVE_DEVICE UNIMPLEMENTED. FIXME. \n");
            break;

        case IRP_MN_CANCEL_REMOVE_DEVICE: // 3
            DPRINT("IRP_MN_CANCEL_REMOVE_DEVICE\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
            goto ForwardIrp;

        case IRP_MN_STOP_DEVICE: // 4
            //DPRINT("IRP_MN_STOP_DEVICE\n");
            DPRINT1("USBPORT_FdoPnP: IRP_MN_STOP_DEVICE UNIMPLEMENTED. FIXME. \n");
            break;

        case IRP_MN_QUERY_STOP_DEVICE: // 5
            DPRINT("IRP_MN_QUERY_STOP_DEVICE\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
            goto ForwardIrp;

        case IRP_MN_CANCEL_STOP_DEVICE: // 6
            DPRINT("IRP_MN_CANCEL_STOP_DEVICE\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
            goto ForwardIrp;

        case IRP_MN_QUERY_DEVICE_RELATIONS: // 7
            DPRINT("IRP_MN_QUERY_DEVICE_RELATIONS\n");
            if (RelationType == BusRelations)
            {
                DeviceRelations = ExAllocatePoolWithTag(PagedPool,
                                                        sizeof(DEVICE_RELATIONS),
                                                        USB_PORT_TAG);

                if (!DeviceRelations)
                {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    Irp->IoStatus.Information = 0;
                    Irp->IoStatus.Status = Status;
                    IoCompleteRequest(Irp, IO_NO_INCREMENT);
                    return Status;
                }

                DeviceRelations->Count = 0;
                DeviceRelations->Objects[0] = NULL;

                if (!FdoExtension->RootHubPdo)
                {
                    Status = USBPORT_CreatePdo(FdoDevice,
                                               &FdoExtension->RootHubPdo);

                    if (!NT_SUCCESS(Status))
                    {
                        ExFreePool(DeviceRelations);
                        goto ForwardIrp;
                    }
                }

                DeviceRelations->Count = 1;
                DeviceRelations->Objects[0] = FdoExtension->RootHubPdo;
                ObReferenceObject(FdoExtension->RootHubPdo);
                Irp->IoStatus.Information = (ULONG_PTR)DeviceRelations;

            }
            else
            {
                if (RelationType == RemovalRelations)
                {
                    DPRINT1("USBPORT_FdoPnP: FIXME IRP_MN_QUERY_DEVICE_RELATIONS/RemovalRelations\n");
                }

                goto ForwardIrp;
            }

            Irp->IoStatus.Status = Status;
            goto ForwardIrp;

        case IRP_MN_QUERY_INTERFACE: // 8
            DPRINT("IRP_MN_QUERY_INTERFACE\n");
            goto ForwardIrp;

        case IRP_MN_QUERY_CAPABILITIES:           // 9
            DPRINT("IRP_MN_QUERY_CAPABILITIES\n");
            goto ForwardIrp;

        case IRP_MN_QUERY_RESOURCES:              // 10
            DPRINT("IRP_MN_QUERY_RESOURCES\n");
            goto ForwardIrp;

        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:  // 11
            DPRINT("IRP_MN_QUERY_RESOURCE_REQUIREMENTS\n");
            goto ForwardIrp;

        case IRP_MN_QUERY_DEVICE_TEXT:            // 12
            DPRINT("IRP_MN_QUERY_DEVICE_TEXT\n");
            goto ForwardIrp;

        case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: // 13
            DPRINT("IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
            goto ForwardIrp;

        case IRP_MN_READ_CONFIG: // 15
            DPRINT("IRP_MN_READ_CONFIG\n");
            goto ForwardIrp;

        case IRP_MN_WRITE_CONFIG: // 16
            DPRINT("IRP_MN_WRITE_CONFIG\n");
            goto ForwardIrp;

        case IRP_MN_EJECT: // 17
            DPRINT("IRP_MN_EJECT\n");
            goto ForwardIrp;

        case IRP_MN_SET_LOCK: // 18
            DPRINT("IRP_MN_SET_LOCK\n");
            goto ForwardIrp;

        case IRP_MN_QUERY_ID: // 19
            DPRINT("IRP_MN_QUERY_ID\n");
            goto ForwardIrp;

        case IRP_MN_QUERY_PNP_DEVICE_STATE: // 20
            DPRINT("IRP_MN_QUERY_PNP_DEVICE_STATE\n");
            goto ForwardIrp;

        case IRP_MN_QUERY_BUS_INFORMATION: // 21
            DPRINT("IRP_MN_QUERY_BUS_INFORMATION\n");
            goto ForwardIrp;

        case IRP_MN_DEVICE_USAGE_NOTIFICATION: // 22
            DPRINT("IRP_MN_DEVICE_USAGE_NOTIFICATION\n");
            goto ForwardIrp;

        case IRP_MN_SURPRISE_REMOVAL: // 23
            //DPRINT("IRP_MN_SURPRISE_REMOVAL\n");
            DPRINT1("USBPORT_FdoPnP: IRP_MN_SURPRISE_REMOVAL UNIMPLEMENTED. FIXME. \n");
            goto ForwardIrp;

        default:
            DPRINT("unknown IRP_MN_???\n");
ForwardIrp:
            // forward irp to next device object
            IoSkipCurrentIrpStackLocation(Irp);
            return IoCallDriver(FdoExtension->CommonExtension.LowerDevice, Irp);
            break;
    }

    return Status;
}

ULONG_PTR
NTAPI
USBPORT_GetDeviceHwIds(IN PDEVICE_OBJECT FdoDevice,
                       IN USHORT VendorID,
                       IN USHORT DeviceID,
                       IN USHORT RevisionID)
{
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    PUSBPORT_REGISTRATION_PACKET Packet;
    PVOID Id;
    WCHAR Buffer[300] = {0};
    ULONG Index = 0;

    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;
    Packet = &FdoExtension->MiniPortInterface->Packet;

    if (Packet->MiniPortFlags & USB_MINIPORT_FLAGS_USB2)
    {
        /* USB 2.0 hub */
        Index += swprintf(&Buffer[Index],
                          L"USB\\ROOT_HUB20&VID%04x&PID%04x&REV%04x",
                          VendorID,
                          DeviceID,
                          RevisionID) + 1;

        Index += swprintf(&Buffer[Index],
                          L"USB\\ROOT_HUB20&VID%04x&PID%04x",
                          VendorID,
                          DeviceID) + 1;

        Index += swprintf(&Buffer[Index], L"USB\\ROOT_HUB20") + 1;
    }
    else
    {
        /* USB 1.1 */
        Index += swprintf(&Buffer[Index],
                          L"USB\\ROOT_HUB&VID%04x&PID%04x&REV%04x",
                          VendorID,
                          DeviceID,
                          RevisionID) + 1;

        Index += swprintf(&Buffer[Index],
                          L"USB\\ROOT_HUB&VID%04x&PID%04x",
                          VendorID,
                          DeviceID) + 1;

        Index += swprintf(&Buffer[Index], L"USB\\ROOT_HUB") + 1;
    }

    Buffer[Index] = UNICODE_NULL;
    DPRINT("USBPORT_GetIdString: Buffer - %S\n", Buffer);

    Id = (LPWSTR)ExAllocatePoolWithTag(PagedPool,
                                       Index * sizeof(WCHAR),
                                       USB_PORT_TAG);

    if (!Id)
        return 0;

    RtlZeroMemory(Id, Index * sizeof(WCHAR));
    RtlMoveMemory(Id, Buffer, Index * sizeof(WCHAR)); // copy device name

    return (ULONG_PTR)Id;
}

NTSTATUS
NTAPI
USBPORT_PdoPnP(IN PDEVICE_OBJECT PdoDevice,
               IN PIRP Irp)
{
    PUSBPORT_RHDEVICE_EXTENSION PdoExtension;
    PDEVICE_OBJECT FdoDevice;
    PUSBPORT_DEVICE_EXTENSION FdoExtension;
    PIO_STACK_LOCATION IoStack;
    UCHAR Minor;
    NTSTATUS Status;
    PPNP_BUS_INFORMATION BusInformation;
    PDEVICE_CAPABILITIES DeviceCapabilities;
    //ULONG Index = 0;
    //ULONG_PTR Information = Irp->IoStatus.Information;

    PdoExtension = (PUSBPORT_RHDEVICE_EXTENSION)PdoDevice->DeviceExtension;
    FdoDevice = PdoExtension->FdoDevice;
    FdoExtension = (PUSBPORT_DEVICE_EXTENSION)FdoDevice->DeviceExtension;

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    Minor = IoStack->MinorFunction;
    Status = Irp->IoStatus.Status;

    DPRINT("USBPORT_PdoPnP: Minor - %d\n", Minor);

    switch (Minor)
    {
        case IRP_MN_START_DEVICE: // 0
            DPRINT("IRP_MN_START_DEVICE\n");

            Status = USBPORT_RootHubCreateDevice(FdoDevice, PdoDevice);
            if (NT_SUCCESS(Status))
            {
                Status = USBPORT_RegisterDeviceInterface(PdoDevice,
                                                         PdoDevice,
                                                         (CONST GUID *)&GUID_DEVINTERFACE_USB_HUB,
                                                         TRUE);

                if (NT_SUCCESS(Status))
                {
                    PdoExtension->CommonExtension.DevicePowerState = PowerDeviceD0;
                    // TODO Flags |= DeviceIsStarted;
                }
            }

            break;

        case IRP_MN_QUERY_REMOVE_DEVICE: // 1
            //DPRINT("IRP_MN_QUERY_REMOVE_DEVICE\n");
            DPRINT1("USBPORT_PdoPnP: IRP_MN_QUERY_REMOVE_DEVICE UNIMPLEMENTED. FIXME. \n");
            break;

        case IRP_MN_REMOVE_DEVICE: // 2
            //DPRINT("IRP_MN_REMOVE_DEVICE\n");
            DPRINT1("USBPORT_PdoPnP: IRP_MN_REMOVE_DEVICE UNIMPLEMENTED. FIXME. \n");
            break;

        case IRP_MN_CANCEL_REMOVE_DEVICE: // 3
            DPRINT("IRP_MN_CANCEL_REMOVE_DEVICE\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IRP_MN_STOP_DEVICE: // 4
            //DPRINT("IRP_MN_STOP_DEVICE\n");
            DPRINT1("USBPORT_PdoPnP: IRP_MN_STOP_DEVICE UNIMPLEMENTED. FIXME. \n");
            break;

        case IRP_MN_QUERY_STOP_DEVICE: // 5
            DPRINT("IRP_MN_QUERY_STOP_DEVICE\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IRP_MN_CANCEL_STOP_DEVICE: // 6
            DPRINT("IRP_MN_CANCEL_STOP_DEVICE\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IRP_MN_QUERY_DEVICE_RELATIONS: // 7
        {
            PDEVICE_RELATIONS DeviceRelations;

            DPRINT("IRP_MN_QUERY_DEVICE_RELATIONS\n");
            if (IoStack->Parameters.QueryDeviceRelations.Type != TargetDeviceRelation)
            {
                Status = Irp->IoStatus.Status;
                break;
            }

            DeviceRelations = (PDEVICE_RELATIONS)ExAllocatePoolWithTag(PagedPool,
                                                                       sizeof(DEVICE_RELATIONS),
                                                                       USB_PORT_TAG);

            if (!DeviceRelations)
            {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                Irp->IoStatus.Information = 0;
                break;
            }

            DeviceRelations->Count = 1;
            DeviceRelations->Objects[0] = PdoDevice;

            ObReferenceObject(PdoDevice);

            Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = (ULONG_PTR)DeviceRelations;
            break;
        }

        case IRP_MN_QUERY_INTERFACE: // 8
            DPRINT("IRP_MN_QUERY_INTERFACE\n");
            Status = USBPORT_PdoQueryInterface(FdoDevice, PdoDevice, Irp);
            break;

        case IRP_MN_QUERY_CAPABILITIES: // 9
            DPRINT("IRP_MN_QUERY_CAPABILITIES\n");

            DeviceCapabilities = (PDEVICE_CAPABILITIES)
                                 IoStack->Parameters.DeviceCapabilities.Capabilities;

            RtlCopyMemory(DeviceCapabilities,
                          &PdoExtension->Capabilities,
                          sizeof(DEVICE_CAPABILITIES));

            Status = STATUS_SUCCESS;
            break;

        case IRP_MN_QUERY_RESOURCES: // 10
            //DPRINT("IRP_MN_QUERY_RESOURCES\n");
            DPRINT("USBPORT_PdoPnP: IRP_MN_QUERY_RESOURCES\n");
            break;

        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS: // 11
            DPRINT("IRP_MN_QUERY_RESOURCE_REQUIREMENTS\n");
            //ASSERT(FALSE);
            break;

        case IRP_MN_QUERY_DEVICE_TEXT: // 12
            DPRINT("IRP_MN_QUERY_DEVICE_TEXT\n");
            //ASSERT(FALSE);
            break;

        case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: // 13
            DPRINT("IRP_MN_FILTER_RESOURCE_REQUIREMENTS\n");
            //ASSERT(FALSE);
            break;

        case IRP_MN_READ_CONFIG: // 15
            DPRINT("IRP_MN_READ_CONFIG\n");
            ASSERT(FALSE);
            break;

        case IRP_MN_WRITE_CONFIG: // 16
            DPRINT("IRP_MN_WRITE_CONFIG\n");
            ASSERT(FALSE);
            break;

        case IRP_MN_EJECT: // 17
            DPRINT("IRP_MN_EJECT\n");
            ASSERT(FALSE);
            break;

        case IRP_MN_SET_LOCK: // 18
            DPRINT("IRP_MN_SET_LOCK\n");
            ASSERT(FALSE);
            break;

        case IRP_MN_QUERY_ID: // 19
        {
            ULONG IdType;
            LONG Length;
            WCHAR Buffer[64] = {0};
            ULONG_PTR Id;

            Status = STATUS_SUCCESS;
            IdType = IoStack->Parameters.QueryId.IdType;

            DPRINT("IRP_MN_QUERY_ID/Type %x\n", IdType);

            if (IdType == BusQueryDeviceID) // 0
            {
                PUSBPORT_REGISTRATION_PACKET Packet;
                Packet = &FdoExtension->MiniPortInterface->Packet;

                if (Packet->MiniPortFlags & USB_MINIPORT_FLAGS_USB2)
                {
                    swprintf(Buffer, L"USB\\ROOT_HUB20");
                }
                else
                {
                    swprintf(Buffer, L"USB\\ROOT_HUB");
                }

                Length = (wcslen(Buffer) + 1);

                Id = (ULONG_PTR)ExAllocatePoolWithTag(PagedPool,
                                                      Length * sizeof(WCHAR),
                                                      USB_PORT_TAG);

                RtlZeroMemory((PVOID)Id, Length * sizeof(WCHAR));

                if (!Id)
                {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                }

                wcscpy((PVOID)Id, Buffer);
                Irp->IoStatus.Information = Id;
                break;
            }

            if (IdType == BusQueryHardwareIDs) // 1
            {
                Id = USBPORT_GetDeviceHwIds(FdoDevice,
                                            FdoExtension->VendorID,
                                            FdoExtension->DeviceID,
                                            FdoExtension->RevisionID);

                Irp->IoStatus.Information = Id;
            }

            break;
        }

        case IRP_MN_QUERY_PNP_DEVICE_STATE: // 20
            DPRINT("IRP_MN_QUERY_PNP_DEVICE_STATE\n");
            Status = STATUS_SUCCESS;
            break;

        case IRP_MN_QUERY_BUS_INFORMATION: // 21
            DPRINT("IRP_MN_QUERY_BUS_INFORMATION\n");

            // Allocate buffer for bus information
            BusInformation = (PPNP_BUS_INFORMATION)ExAllocatePool(PagedPool,
                                                                  sizeof(PNP_BUS_INFORMATION));

            RtlZeroMemory(BusInformation, sizeof(PNP_BUS_INFORMATION));

            if (BusInformation)
            {
                // Copy BUS GUID
                RtlMoveMemory(&BusInformation->BusTypeGuid,
                              &GUID_BUS_TYPE_USB,
                              sizeof(GUID));

                // Set bus type
                BusInformation->LegacyBusType = PNPBus;
                BusInformation->BusNumber = 0;

                Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = (ULONG_PTR)BusInformation;
            }
            else
            {
                // No memory
                Status = STATUS_INSUFFICIENT_RESOURCES;
            }

            break;

        case IRP_MN_DEVICE_USAGE_NOTIFICATION: // 22
            DPRINT("IRP_MN_DEVICE_USAGE_NOTIFICATION\n");
            break;

        case IRP_MN_SURPRISE_REMOVAL: // 23
            //DPRINT("IRP_MN_SURPRISE_REMOVAL\n");
            DPRINT1("USBPORT_PdoPnP: IRP_MN_SURPRISE_REMOVAL UNIMPLEMENTED. FIXME. \n");
            break;

        default:
            DPRINT("unknown IRP_MN_???\n");
            Status = Irp->IoStatus.Status;
            break;
    }

    //Irp->IoStatus.Information = Information;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
