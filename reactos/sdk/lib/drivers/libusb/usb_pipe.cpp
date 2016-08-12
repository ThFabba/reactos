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
    virtual NTSTATUS Initialize(IN PUSBDEVICE DeviceHandle, IN PUSB_ENDPOINT_DESCRIPTOR EndpointDescriptor, IN PDMAMEMORYMANAGER DmaManager);

    // constructor / destructor
    CUSBPipe(IUnknown *OuterUnknown){}
    virtual ~CUSBPipe(){}

protected:
    LONG m_Ref;
    PUSBDEVICE                    m_DeviceHandle;              // 
    PDMAMEMORYMANAGER             m_DmaManager;                // 
    USB_ENDPOINT_DESCRIPTOR       m_EndpointDescriptor;        // 

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
    IN PUSBDEVICE DeviceHandle,
    IN PUSB_ENDPOINT_DESCRIPTOR EndpointDescriptor,
    IN PDMAMEMORYMANAGER DmaManager)
{
    NTSTATUS Status=0;

    /* initialize members */
    m_DmaManager    = DmaManager;
    m_DeviceHandle  = DeviceHandle;

    RtlCopyMemory(&m_EndpointDescriptor, EndpointDescriptor, sizeof(USB_ENDPOINT_DESCRIPTOR));

    return Status;
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

