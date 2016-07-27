/*
 * PROJECT:     ReactOS Universal Serial Bus Bulk Driver Library
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        lib/drivers/libusb/hcd_controller.cpp
 * PURPOSE:     USB Common Driver Library.
 * PROGRAMMERS:
 *              Michael Martin (michael.martin@reactos.org)
 *              Johannes Anderwald (johannes.anderwald@reactos.org)
 */

#include "libusb.h"

//#define NDEBUG
#include <debug.h>

class CHCDController : public IHCDController,
                       public IDispatchIrp
{
public:
    STDMETHODIMP QueryInterface( REFIID InterfaceId, PVOID* Interface);

    STDMETHODIMP_(ULONG) AddRef()
    {
        InterlockedIncrement(&m_Ref);
        return m_Ref;
    }
    STDMETHODIMP_(ULONG) Release()
    {
        InterlockedDecrement(&m_Ref);

        if (!m_Ref)
        {
            delete this;
            return 0;
        }
        return m_Ref;
    }

    // IHCDController interface functions
    NTSTATUS Initialize(IN PROOTHDCCONTROLLER RootHCDController, IN PDRIVER_OBJECT DriverObject, IN PDEVICE_OBJECT PhysicalDeviceObject);
    NTSTATUS GetControllerDetails(OUT OPTIONAL PUSHORT VendorId, OUT OPTIONAL PUSHORT DeviceId, OUT OPTIONAL PUCHAR RevisionId, OUT OPTIONAL PUCHAR ProgIf, OUT OPTIONAL PUCHAR SubClass, OUT OPTIONAL PUCHAR BaseClass);

    // IDispatchIrp interface functions
    NTSTATUS HandlePnp(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp);
    NTSTATUS HandlePower(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp);
    NTSTATUS HandleDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp);
    NTSTATUS HandleSystemControl(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp);

    // local functions
    NTSTATUS CreateFDO(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT * OutDeviceObject);
    NTSTATUS SetSymbolicLink(BOOLEAN Enable);

    // constructor / destructor
    CHCDController(IUnknown *OuterUnknown){}
    virtual ~CHCDController(){}

protected:
    LONG m_Ref;
    PROOTHDCCONTROLLER m_RootController;
    PDRIVER_OBJECT m_DriverObject;
    PDEVICE_OBJECT m_PhysicalDeviceObject;
    PDEVICE_OBJECT m_FunctionalDeviceObject;
    PDEVICE_OBJECT m_NextDeviceObject;
    PUSBHARDWAREDEVICE m_Hardware;
    PHUBCONTROLLER m_HubController;
    ULONG m_FDODeviceNumber;
    LPCSTR m_USBType;
    LIBUSB_RESOURCES m_Resources;                                                      // 
    PDMA_ADAPTER m_DmaAdapter;                                                         // dma adapter object
    ULONG m_MapRegisters;                                                              // map registers count
    PKINTERRUPT m_Interrupt;                                                           // interrupt object
    PDMAMEMORYMANAGER m_MemoryManager;                                                 // memory manager
    PLIBUSB_COMMON_BUFFER_HEADER m_CommonBufferHeader;                                 // 
    KSPIN_LOCK m_Lock;                                                                 // hardware lock
    USHORT m_VendorID;                                                                 // vendor id
    USHORT m_DeviceID;                                                                 // device id
    UCHAR  m_RevisionID;                                                               // 
    UCHAR  m_ProgIf;                                                                   // 
    UCHAR  m_SubClass;                                                                 // 
    UCHAR  m_BaseClass;                                                                // 
};

//=================================================================================================
// COM
//
NTSTATUS
STDMETHODCALLTYPE
CHCDController::QueryInterface(
    IN  REFIID refiid,
    OUT PVOID* Output)
{
    return STATUS_UNSUCCESSFUL;
}

//-------------------------------------------------------------------------------------------------
NTSTATUS
CHCDController::Initialize(
    IN PROOTHDCCONTROLLER RootHCDController,
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS Status;
    PCOMMON_DEVICE_EXTENSION DeviceExtension;

    //
    // create usb hardware
    //
    Status = CreateUSBHardware(&m_Hardware);
    if (!NT_SUCCESS(Status))
    {
        //
        // failed to create hardware object
        //
        DPRINT1("Failed to create hardware object\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // initialize members
    //
    m_DriverObject = DriverObject;
    m_PhysicalDeviceObject = PhysicalDeviceObject;
    m_RootController = RootHCDController;

    //
    // create FDO
    //
    Status = CreateFDO(m_DriverObject, &m_FunctionalDeviceObject);
    if (!NT_SUCCESS(Status))
    {
        //
        // failed to create PDO
        //
        return Status;
    }

    //
    // now attach to device stack
    //
    m_NextDeviceObject = IoAttachDeviceToDeviceStack(m_FunctionalDeviceObject, m_PhysicalDeviceObject);
    if (!m_NextDeviceObject)
    {
        //
        // failed to attach to device stack
        //
        IoDeleteDevice(m_FunctionalDeviceObject);
        m_FunctionalDeviceObject = 0;

        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // initialize device lock
    //
    KeInitializeSpinLock(&m_Lock);

    //
    // Create DMAMemoryManager for use with QueueHeads and Transfer Descriptors.
    //
    Status =  CreateDMAMemoryManager(&m_MemoryManager);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("Failed to create DMAMemoryManager Object\n");
        return Status;
    }

    //
    // initialize hardware object
    //
    Status = m_Hardware->Initialize(m_DriverObject,
                                    m_FunctionalDeviceObject,
                                    m_PhysicalDeviceObject,
                                    m_NextDeviceObject,
                                    this,
                                    m_MemoryManager);

    if (!NT_SUCCESS(Status))
    {
        DPRINT1("[%s] Failed to initialize hardware object %x\n", m_Hardware->GetUSBType(), Status);

        //
        // failed to initialize hardware object, detach from device stack
        //
        IoDetachDevice(m_NextDeviceObject);

        //
        // now delete the device
        //
        IoDeleteDevice(m_FunctionalDeviceObject);

        //
        // nullify pointers :)
        //
        m_FunctionalDeviceObject = 0;
        m_NextDeviceObject = 0;

        return Status;
    }

    //
    // get usb controller type
    //
    m_USBType = m_Hardware->GetUSBType();


    //
    // set device flags
    //
    //m_FunctionalDeviceObject->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;


    //
    // get device extension
    //
    DeviceExtension = (PCOMMON_DEVICE_EXTENSION)m_FunctionalDeviceObject->DeviceExtension;
    PC_ASSERT(DeviceExtension);

    //
    // initialize device extension
    //
    DeviceExtension->IsFDO = TRUE;
    DeviceExtension->IsHub = FALSE;
    DeviceExtension->Dispatcher = PDISPATCHIRP(this);

    //
    // device is initialized
    //
    m_FunctionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;


    //
    // is there a root controller
    //
    if (m_RootController)
    {
        //
        // add reference
        //
        m_RootController->AddRef();

        //
        // register with controller
        //
        m_RootController->RegisterHCD(this);
    }


    //
    // done
    //
    return STATUS_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
NTSTATUS
CHCDController::GetControllerDetails(
    OUT OPTIONAL PUSHORT VendorId,
    OUT OPTIONAL PUSHORT DeviceId,
    OUT OPTIONAL PUCHAR RevisionId,
    OUT OPTIONAL PUCHAR ProgIf,
    OUT OPTIONAL PUCHAR SubClass,
    OUT OPTIONAL PUCHAR BaseClass)
{
    if (VendorId)
      *VendorId = m_VendorID;
    if (DeviceId)
      *DeviceId = m_DeviceID;
    if (RevisionId)
      *RevisionId = m_RevisionID;
    if (ProgIf)
      *ProgIf = m_ProgIf;
    if (SubClass)
      *SubClass = m_SubClass;
    if (BaseClass)
      *BaseClass = m_BaseClass;

    return STATUS_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
NTSTATUS
CHCDController::HandleDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;
    PCOMMON_DEVICE_EXTENSION DeviceExtension;
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;
    PUSB_HCD_DRIVERKEY_NAME DriverKey;
    ULONG ResultLength;

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    //
    // get device extension
    //
    DeviceExtension = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // sanity check
    //
    PC_ASSERT(DeviceExtension->IsFDO);

    DPRINT1("[%s] HandleDeviceControl>Type: IoCtl %x InputBufferLength %lu OutputBufferLength %lu\n", m_USBType,
        IoStack->Parameters.DeviceIoControl.IoControlCode,
        IoStack->Parameters.DeviceIoControl.InputBufferLength,
        IoStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // perform ioctl for FDO
    //
    if (IoStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_GET_HCD_DRIVERKEY_NAME)
    {
        //
        // check if sizee is at least >= USB_HCD_DRIVERKEY_NAME
        //
        if(IoStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(USB_HCD_DRIVERKEY_NAME))
        {
            //
            // get device property size
            //
            Status = IoGetDeviceProperty(m_PhysicalDeviceObject, DevicePropertyDriverKeyName, 0, NULL, &ResultLength);

            //
            // get input buffer
            //
            DriverKey = (PUSB_HCD_DRIVERKEY_NAME)Irp->AssociatedIrp.SystemBuffer;

            //
            // check result
            //
            if (Status == STATUS_BUFFER_TOO_SMALL)
            {
                //
                // does the caller provide enough buffer space
                //
                if (IoStack->Parameters.DeviceIoControl.OutputBufferLength >= ResultLength)
                {
                    //
                    // it does
                    //
                    Status = IoGetDeviceProperty(m_PhysicalDeviceObject, DevicePropertyDriverKeyName, IoStack->Parameters.DeviceIoControl.OutputBufferLength - sizeof(ULONG), DriverKey->DriverKeyName, &ResultLength);
                }

                //
                // store result
                //
                DriverKey->ActualLength = ResultLength + FIELD_OFFSET(USB_HCD_DRIVERKEY_NAME, DriverKeyName) + sizeof(WCHAR);
                Irp->IoStatus.Information = IoStack->Parameters.DeviceIoControl.OutputBufferLength;
                Status = STATUS_SUCCESS;
            }
        }
        else
        {
            //
            // buffer is certainly too small
            //
            Status = STATUS_BUFFER_OVERFLOW;
            Irp->IoStatus.Information = sizeof(USB_HCD_DRIVERKEY_NAME);
        }
    }
    else if (IoStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_USB_GET_ROOT_HUB_NAME)
    {
        //
        // check if sizee is at least >= USB_HCD_DRIVERKEY_NAME
        //
        if(IoStack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(USB_HCD_DRIVERKEY_NAME))
        {
            //
            // sanity check
            //
            PC_ASSERT(m_HubController);

            //
            // get input buffer
            //
            DriverKey = (PUSB_HCD_DRIVERKEY_NAME)Irp->AssociatedIrp.SystemBuffer;

            //
            // get symbolic link
            //
            Status = m_HubController->GetHubControllerSymbolicLink(IoStack->Parameters.DeviceIoControl.OutputBufferLength - sizeof(ULONG), DriverKey->DriverKeyName, &ResultLength);


            if (NT_SUCCESS(Status))
            {
                //
                // null terminate it
                //
                PC_ASSERT(IoStack->Parameters.DeviceIoControl.OutputBufferLength - sizeof(ULONG) - sizeof(WCHAR) >= ResultLength);

                DriverKey->DriverKeyName[ResultLength / sizeof(WCHAR)] = L'\0';
            }

            //
            // store result
            //
            DriverKey->ActualLength = ResultLength + FIELD_OFFSET(USB_HCD_DRIVERKEY_NAME, DriverKeyName) + sizeof(WCHAR);
            Irp->IoStatus.Information = IoStack->Parameters.DeviceIoControl.OutputBufferLength;
            Status = STATUS_SUCCESS;
        }
        else
        {
            //
            // buffer is certainly too small
            //
            Status = STATUS_BUFFER_OVERFLOW;
            Irp->IoStatus.Information = sizeof(USB_HCD_DRIVERKEY_NAME);
        }
    }

    //
    // complete the request
    //
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    //
    // done
    //
    return Status;
}

BOOLEAN NTAPI
InterruptService(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext)
{
  PUSBHARDWAREDEVICE Hardware;
  BOOLEAN Result = FALSE;

  //DPRINT("InterruptService: ...\n");

  Hardware = PUSBHARDWAREDEVICE(ServiceContext);
  Result = Hardware->InterruptServiceRoutine(Interrupt, ServiceContext);

  //DPRINT("InterruptService: return - %x\n", Result);
  return Result;
}

NTSTATUS
CHCDController::HandlePnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PIO_STACK_LOCATION IoStack;
    PCOMMON_DEVICE_EXTENSION DeviceExtension;
    DEVICE_DESCRIPTION DeviceDescription;
    //PCM_RESOURCE_LIST RawResourceList;
    PCM_RESOURCE_LIST TranslatedResourceList;
    PDEVICE_RELATIONS DeviceRelations;
    SIZE_T HwResoursesSize;
    BUS_INTERFACE_STANDARD BusInterface;
    PCI_COMMON_CONFIG PciConfig;
    ULONG BytesRead;
    NTSTATUS Status;

    //
    // get device extension
    //
    DeviceExtension = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //
    // sanity check
    //
    PC_ASSERT(DeviceExtension->IsFDO);

    //
    // get current stack location
    //
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    switch(IoStack->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
        {
            DPRINT("[%s] HandlePnp IRP_MN_START FDO\n", m_USBType);

            //
            // first start lower device object
            //
            Status = SyncForwardIrp(m_NextDeviceObject, Irp);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Lower device failed start\n");
                return Status;
            }

            //
            // operation succeeded, lets start the device
            //
            //RawResourceList = IoStack->Parameters.StartDevice.AllocatedResources;
            TranslatedResourceList = IoStack->Parameters.StartDevice.AllocatedResourcesTranslated;

            if ( !TranslatedResourceList )
            {
                DPRINT1("No translated resources\n");
                return STATUS_NONE_MAPPED;
            }

            Status = ParseResources(TranslatedResourceList, &m_Resources);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("PnpStart: ParseResources() failed with %x\n", Status);
                return Status;
            }

        #if 0
            DPRINT("PnpStart: m_Resources.TypesResources    - %x\n", m_Resources.TypesResources);
            DPRINT("PnpStart: m_Resources.InterruptVector   - %x\n", m_Resources.InterruptVector);
            DPRINT("PnpStart: m_Resources.InterruptLevel    - %x\n", m_Resources.InterruptLevel);
            DPRINT("PnpStart: m_Resources.InterruptAffinity - %x\n", m_Resources.InterruptAffinity);
            DPRINT("PnpStart: m_Resources.ShareVector       - %x\n", m_Resources.ShareVector);
            DPRINT("PnpStart: m_Resources.InterruptMode     - %x\n", m_Resources.InterruptMode);
            DPRINT("PnpStart: m_Resources.ResourceBase      - %p\n", m_Resources.ResourceBase);
            DPRINT("PnpStart: m_Resources.IoSpaceLength     - %x\n", m_Resources.IoSpaceLength);
        #endif

            m_VendorID   = 0;
            m_DeviceID   = 0;
            m_RevisionID = 0;
            m_ProgIf     = 0;
            m_SubClass   = 0;
            m_BaseClass  = 0;

            Status = GetBusInterface(m_PhysicalDeviceObject, &BusInterface);
            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Failed to get BusInteface!\n");
                return Status;
            }

            BytesRead = (*BusInterface.GetBusData)(BusInterface.Context,
                                                   PCI_WHICHSPACE_CONFIG,
                                                   &PciConfig,
                                                   0,
                                                   PCI_COMMON_HDR_LENGTH);

            if (BytesRead != PCI_COMMON_HDR_LENGTH)
            {
                DPRINT1("Failed to get pci config information!\n");
                return STATUS_SUCCESS;
            }

            m_VendorID   = PciConfig.VendorID;
            m_DeviceID   = PciConfig.DeviceID;
            m_RevisionID = PciConfig.RevisionID;
            m_ProgIf     = PciConfig.ProgIf;
            m_SubClass   = PciConfig.SubClass;
            m_BaseClass  = PciConfig.BaseClass;

        #if 0
            DPRINT("PnpStart: m_VendorID   - %x\n", m_VendorID);
            DPRINT("PnpStart: m_DeviceID   - %x\n", m_DeviceID);
            DPRINT("PnpStart: m_RevisionID - %x\n", m_RevisionID);
            DPRINT("PnpStart: m_ProgIf     - %x\n", m_ProgIf);
            DPRINT("PnpStart: m_SubClass   - %x\n", m_SubClass);
            DPRINT("PnpStart: m_BaseClass  - %x\n", m_BaseClass);
        #endif

            //
            // zero device description
            //
            RtlZeroMemory(&DeviceDescription, sizeof(DeviceDescription));

            //
            // initialize device description
            //
            DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
            DeviceDescription.Master = TRUE;
            DeviceDescription.ScatterGather = TRUE;
            DeviceDescription.Dma32BitAddresses = TRUE;
            DeviceDescription.DmaWidth = Width32Bits;
            DeviceDescription.InterfaceType = PCIBus;
            DeviceDescription.MaximumLength = MAXULONG;

            //
            // get dma adapter
            //
            m_DmaAdapter = IoGetDmaAdapter(
                                m_PhysicalDeviceObject,
                                &DeviceDescription,
                                &m_MapRegisters);

            if (!m_DmaAdapter)
            {
                //
                // failed to get dma adapter
                //
                DPRINT1("Failed to acquire dma adapter\n");
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            if ( m_Resources.TypesResources & 4 )
            {
                Status = IoConnectInterrupt(
                                 &m_Interrupt,
                                 InterruptService,
                                 (PVOID)m_Hardware,
                                 NULL,
                                 m_Resources.InterruptVector,
                                 m_Resources.InterruptLevel,
                                 m_Resources.InterruptLevel,
                                 (KINTERRUPT_MODE)m_Resources.InterruptMode,
                                 m_Resources.ShareVector,
                                 m_Resources.InterruptAffinity,
                                 FALSE);

                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("IoConnect Interrupt failed with %x\n", Status);
                    return Status;
                }
            }

            ASSERT(m_Hardware);

            Status = m_Hardware->GetHwResoursesSize(&HwResoursesSize);
            if (!NT_SUCCESS(Status))
            {
                //
                // failed
                //
                break;
            }

            m_CommonBufferHeader = m_MemoryManager->
                                       AllocateCommonBuffer(m_DmaAdapter, HwResoursesSize);

            //DPRINT("PnpStart: m_CommonBufferHeader - %p\n", m_CommonBufferHeader);

            if (!m_CommonBufferHeader)
            {
                DPRINT1("Failed to allocate a common buffer\n");
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            //Resources->StartVA = (PVOID)HeaderBuffer->VirtualAddress;
            //Resources->StartPA = (PVOID)HeaderBuffer->PhysicalAddress;

            // Initialize the DMAMemoryManager
            Status = m_MemoryManager->Initialize(
                             m_Hardware,
                             &m_Lock,
                             HwResoursesSize,
                             (PVOID)m_CommonBufferHeader->VirtualAddress,
                             m_CommonBufferHeader->LogicalAddress,
                             32);

            if (!NT_SUCCESS(Status))
            {
                DPRINT1("Failed to initialize the DMAMemoryManager\n");
                return Status;
            }

            // start the hardware
            Status = m_Hardware->PnpStart(&m_Resources, m_DmaAdapter);
            if (!NT_SUCCESS(Status))
            {
                //
                // failed to start controller
                //
                break;
            }

            // enable symbolic link
            Status = SetSymbolicLink(TRUE);

            DPRINT("[%s] HandlePnp IRP_MN_START FDO: Status %x\n", m_USBType ,Status);
            break;
        }
        case IRP_MN_QUERY_DEVICE_RELATIONS:
        {
            DPRINT("[%s] HandlePnp IRP_MN_QUERY_DEVICE_RELATIONS Type %lx\n", m_USBType, IoStack->Parameters.QueryDeviceRelations.Type);

            if (m_HubController == NULL)
            {
                //
                // create hub controller
                //
                Status = CreateHubController(&m_HubController);
                if (!NT_SUCCESS(Status))
                {
                    //
                    // failed to create hub controller
                    //
                    break;
                }

                //
                // initialize hub controller
                //
                Status = m_HubController->Initialize(
                                            m_DriverObject,
                                            m_FunctionalDeviceObject,
                                            PHCDCONTROLLER(this),
                                            m_Hardware,
                                            TRUE,
                                            0 /* FIXME*/);

                if (!NT_SUCCESS(Status))
                {
                    //
                    // failed to initialize hub controller
                    //
                    break;
                }

                //
                // add reference to prevent it from getting deleting while hub driver adds / removes references
                //
                m_HubController->AddRef();
            }

            if (IoStack->Parameters.QueryDeviceRelations.Type == BusRelations)
            {
                //
                // allocate device relations
                //
                DeviceRelations = (PDEVICE_RELATIONS)ExAllocatePool(PagedPool, sizeof(DEVICE_RELATIONS));
 
                if (!DeviceRelations)
                {
                    //
                    // no memory
                    //
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                }

                //
                // init device relations
                //
                DeviceRelations->Count = 1;
                Status = m_HubController->GetHubControllerDeviceObject(&DeviceRelations->Objects [0]);

                if ( !NT_SUCCESS(Status) )
                {
                  ExFreePool(DeviceRelations);
                  goto ForwardIrp;
                }

                ObReferenceObject(DeviceRelations->Objects [0]);

                //
                // store result
                //
                Irp->IoStatus.Information = (ULONG_PTR)DeviceRelations;
                Status = STATUS_SUCCESS;
            }
            else
            {
                //
                // not supported
                //
                Status = STATUS_NOT_SUPPORTED;
            }
            break;
        }
        case IRP_MN_STOP_DEVICE:
        {
            DPRINT("[%s] HandlePnp IRP_MN_STOP_DEVICE\n", m_USBType);

            if (m_Hardware)
            {
                //
                // stop the hardware
                //
                Status = m_Hardware->PnpStop();
            }
            else
            {
                //
                // fake success 
                //
                Status = STATUS_SUCCESS;
            }

            if (NT_SUCCESS(Status))
            {
                //
                // stop lower device
                //
                Status = SyncForwardIrp(m_NextDeviceObject, Irp);
            }
            break;
        }
        case IRP_MN_QUERY_REMOVE_DEVICE:
        case IRP_MN_QUERY_STOP_DEVICE:
        {
#if 0
            //
            // sure
            //
            Irp->IoStatus.Status = STATUS_SUCCESS;

            //
            // forward irp to next device object
            //
            IoSkipCurrentIrpStackLocation(Irp);
            return IoCallDriver(m_NextDeviceObject, Irp);
#else
            DPRINT1("[%s] Denying controller removal due to reinitialization bugs\n", m_USBType);
            Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_UNSUCCESSFUL;
#endif
        }
        case IRP_MN_REMOVE_DEVICE:
        {
            DPRINT("[%s] HandlePnp IRP_MN_REMOVE_DEVICE FDO\n", m_USBType);

            //
            // delete the symbolic link
            //
            SetSymbolicLink(FALSE);

            //
            // forward irp to next device object
            //
            IoSkipCurrentIrpStackLocation(Irp);
            IoCallDriver(m_NextDeviceObject, Irp);

            //
            // detach device from device stack
            //
            IoDetachDevice(m_NextDeviceObject);

            //
            // delete device
            //
            IoDeleteDevice(m_FunctionalDeviceObject);

            return STATUS_SUCCESS;
        }
        default:
        {
ForwardIrp:
            // forward irp to next device object
            IoSkipCurrentIrpStackLocation(Irp);
            return IoCallDriver(m_NextDeviceObject, Irp);
        }
    }

    //
    // store result and complete request
    //
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

NTSTATUS
CHCDController::HandlePower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(m_NextDeviceObject, Irp);
}

NTSTATUS
CHCDController::HandleSystemControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(m_NextDeviceObject, Irp);
}

NTSTATUS
CHCDController::CreateFDO(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT * OutDeviceObject)
{
    WCHAR CharDeviceName[64];
    NTSTATUS Status;
    ULONG UsbDeviceNumber = 0;
    UNICODE_STRING DeviceName;

    while (TRUE)
    {
        //
        // construct device name
        //
        swprintf(CharDeviceName, L"\\Device\\USBFDO-%d", UsbDeviceNumber);

        //
        // initialize device name
        //
        RtlInitUnicodeString(&DeviceName, CharDeviceName);

        //
        // create device
        //
        Status = IoCreateDevice(DriverObject,
                                sizeof(COMMON_DEVICE_EXTENSION),
                                &DeviceName,
                                FILE_DEVICE_CONTROLLER,
                                0,
                                FALSE,
                                OutDeviceObject);

        //
        // check for success
        //
        if (NT_SUCCESS(Status))
            break;

        //
        // is there a device object with that same name
        //
        if ((Status == STATUS_OBJECT_NAME_EXISTS) || (Status == STATUS_OBJECT_NAME_COLLISION))
        {
            //
            // Try the next name
            //
            UsbDeviceNumber++;
            continue;
        }

        //
        // bail out on other errors
        //
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("[%s] CreateFDO: Failed to create %wZ, Status %x\n", m_USBType, &DeviceName, Status);
            return Status;
        }
    }

    //
    // store FDO number
    //
    m_FDODeviceNumber = UsbDeviceNumber;

    DPRINT("[%s] CreateFDO: DeviceName %wZ\n", m_USBType, &DeviceName);

    /* done */
    return Status;
}

NTSTATUS
CHCDController::SetSymbolicLink(
    BOOLEAN Enable)
{
    NTSTATUS Status;
    WCHAR LinkName[32];
    WCHAR FDOName[32];
    UNICODE_STRING Link, FDO;

    if (Enable)
    {
        //
        // create legacy link
        //
        swprintf(LinkName, L"\\DosDevices\\HCD%d", m_FDODeviceNumber);
        swprintf(FDOName, L"\\Device\\USBFDO-%d", m_FDODeviceNumber);
        RtlInitUnicodeString(&Link, LinkName);
        RtlInitUnicodeString(&FDO, FDOName);

        //
        // create symbolic link
        //
        Status = IoCreateSymbolicLink(&Link, &FDO);

        if (!NT_SUCCESS(Status))
        {
            //
            // FIXME: handle me
            //
            ASSERT(0);
        }
    }
    else
    {
        //
        // create legacy link
        //
        swprintf(LinkName, L"\\DosDevices\\HCD%d", m_FDODeviceNumber);
        RtlInitUnicodeString(&Link, LinkName);

        //
        // now delete the symbolic link
        //
        Status = IoDeleteSymbolicLink(&Link);

        if (!NT_SUCCESS(Status))
        {
            //
            // FIXME: handle me
            //
            ASSERT(0);
        }
    }

    //
    // done
    //
    return Status;
}

NTSTATUS
NTAPI
CreateHCDController(
    PHCDCONTROLLER *OutHcdController)
{
    PHCDCONTROLLER This;

    //
    // allocate controller
    //
    This = new(NonPagedPool, TAG_USBLIB) CHCDController(0);
    if (!This)
    {
        //
        // failed to allocate
        //
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // add reference count
    //
    This->AddRef();

    //
    // return result
    //
    *OutHcdController = (PHCDCONTROLLER)This;

    //
    // done
    //
    return STATUS_SUCCESS;
}
