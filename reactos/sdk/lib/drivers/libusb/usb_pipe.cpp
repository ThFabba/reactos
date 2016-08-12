/*
 * PROJECT:     ReactOS Universal Serial Bus Bulk Driver Library
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        lib/drivers/libusb/usb_device.cpp
 * PURPOSE:     USB Common Driver Library.
 * PROGRAMMERS:
 *              Michael Martin (michael.martin@reactos.org)
 *              Johannes Anderwald (johannes.anderwald@reactos.org)
 */

#include "libusb.h"

//#define NDEBUG
#include <debug.h>

class CUSBPipe : public IUSBPipe
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

    // IUSBPipe interface functions
    virtual NTSTATUS Initialize(IN PUSBHARDWAREDEVICE Hardware, IN PUSBDEVICE DeviceHandle, IN PUSB_ENDPOINT_DESCRIPTOR EndpointDescriptor, IN PDMAMEMORYMANAGER DmaManager);
    virtual NTSTATUS OpenPipe();

    // constructor / destructor
    CUSBPipe(IUnknown *OuterUnknown){}
    virtual ~CUSBPipe(){}

protected:
    LONG m_Ref;
    PUSBHARDWAREDEVICE            m_Hardware;                  // 
    PUSBDEVICE                    m_DeviceHandle;              // 
    USB_ENDPOINT_DESCRIPTOR       m_EndpointDescriptor;        // 

    // DMA memory buffer
    PDMAMEMORYMANAGER             m_DmaManager;                // 
    PDMA_ADAPTER                  m_DmaAdapter;                // 
    PLIBUSB_COMMON_BUFFER_HEADER  m_HeaderDmaBuffer;           // 

    // parameters
    ULONG                         m_TransferType;              // 
    ULONG                         m_MaxTransferSize;           // 

};

//-----------------------------------------------------------------------------
NTSTATUS
STDMETHODCALLTYPE
CUSBPipe::QueryInterface(
    IN  REFIID refiid,
    OUT PVOID* Output)
{
    return STATUS_UNSUCCESSFUL;
}

//-----------------------------------------------------------------------------
NTSTATUS
CUSBPipe::Initialize(
    IN PUSBHARDWAREDEVICE Hardware,
    IN PUSBDEVICE DeviceHandle,
    IN PUSB_ENDPOINT_DESCRIPTOR EndpointDescriptor,
    IN PDMAMEMORYMANAGER DmaManager)
{
    NTSTATUS Status=0;

    /* initialize members */
    m_Hardware      = Hardware;
    m_DmaManager    = DmaManager;
    m_DeviceHandle  = DeviceHandle;

    RtlCopyMemory(&m_EndpointDescriptor, EndpointDescriptor, sizeof(USB_ENDPOINT_DESCRIPTOR));

    m_TransferType = m_EndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK;

    return Status;
}

//-----------------------------------------------------------------------------
NTSTATUS
CUSBPipe::OpenPipe()
{
    ULONG                         MaxTransferSize;
    ULONG                         RequiredBufferLength;
    PLIBUSB_COMMON_BUFFER_HEADER  HeaderBuffer;

    DPRINT("OpenPipe: PipeHandle - %p\n", this);

    switch (m_TransferType)
    {
      case USB_ENDPOINT_TYPE_CONTROL:
        if ( m_EndpointDescriptor.bEndpointAddress == 0 )
          MaxTransferSize = 0x400; // OUT Ep0
        else
          MaxTransferSize = 0x10000;
        break;

      case USB_ENDPOINT_TYPE_ISOCHRONOUS:
         //MaxTransferSize = ;
         ASSERT(FALSE);
       break;

      case USB_ENDPOINT_TYPE_BULK:
        MaxTransferSize = 0x10000;
        break;

      case USB_ENDPOINT_TYPE_INTERRUPT:
        MaxTransferSize = 0x400;
        break;

      default:
        ASSERT(FALSE);
        break;
    }

    m_MaxTransferSize = MaxTransferSize;

    m_Hardware->GetDmaAdapter(&m_DmaAdapter);

    m_Hardware->GetEndpointRequirements(m_TransferType, &MaxTransferSize, &RequiredBufferLength); 

    DPRINT("OpenPipe: MaxTransferSize      - %x\n", MaxTransferSize);
    DPRINT("OpenPipe: RequiredBufferLength - %x\n", RequiredBufferLength);

    if ( m_TransferType == USB_ENDPOINT_TYPE_BULK ||
         m_TransferType == USB_ENDPOINT_TYPE_INTERRUPT )
    {
        m_MaxTransferSize = MaxTransferSize;
    }

    if (RequiredBufferLength)
    {
        HeaderBuffer = m_DmaManager->AllocateCommonBuffer(m_DmaAdapter, RequiredBufferLength);
        DPRINT("OpenPipe: BufferLength - %x\n", HeaderBuffer->BufferLength);
    }
    else
    {
        HeaderBuffer = NULL;
    }

    if (!HeaderBuffer)
    {
        ASSERT(FALSE); //Error handle
        m_HeaderDmaBuffer = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    m_HeaderDmaBuffer = HeaderBuffer;

ASSERT(FALSE); //Error handle

    return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
NTSTATUS
NTAPI
CreateUSBPipe(
    OUT PUSBPIPE * OutPipe)
{
    CUSBPipe * This;

    // allocate pipe
    This = new(NonPagedPool, TAG_USBLIB) CUSBPipe(0);

    if (!This)
    {
        // failed to allocate
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // add reference count
    This->AddRef();

    // return result
    *OutPipe = (PUSBPIPE)This;

    // done
    return STATUS_SUCCESS;
}

