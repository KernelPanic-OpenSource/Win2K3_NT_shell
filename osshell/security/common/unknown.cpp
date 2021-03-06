//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1997 - 1999
//
//  File:       unknown.cpp
//
//  Helper functions for handling IUnknown
//
//--------------------------------------------------------------------------

#include "pch.h"
#pragma hdrstop


/*-----------------------------------------------------------------------------
/ CUnknown
/   Helper functions to aid the implementation of IUnknown within objects,
/   handles not only AddRef and Release, but also QueryInterface.
/----------------------------------------------------------------------------*/

LONG g_cRefCount = 0;          // global reference count

CUnknown::CUnknown()
{
    m_cRefCount = 0;
    InterlockedIncrement(&g_cRefCount);
}

CUnknown::~CUnknown()
{
    TraceAssert( m_cRefCount == 0 );    // ref counting problem
    InterlockedDecrement(&g_cRefCount);
}


/*-----------------------------------------------------------------------------
/ CUnknown::HandleQueryInterface
/ ------------------------------
/   A table driven implementation of QueryInterface that scans through trying
/   to find a suitable match for the object.
/
/ In:
/   riid = interface being requested
/   ppvObject -> receives a pointer to the object
/   aIntefaces = array of interface descriptions
/   cif = number of interfaces in array
/
/ Out:
/   -
/----------------------------------------------------------------------------*/
STDMETHODIMP CUnknown::HandleQueryInterface(REFIID riid, LPVOID* ppvObject, LPINTERFACES aInterfaces, int cif)
{
    HRESULT hr = S_OK;
    int i;

    TraceAssert(ppvObject);
    TraceAssert(aInterfaces);
    TraceAssert(cif);

    *ppvObject = NULL;          // no interface yet

    for ( i = 0; i != cif; i++ )
    {
        if ( IsEqualIID(riid, *aInterfaces[i].piid) || IsEqualIID(riid, IID_IUnknown) )
        {
            *ppvObject = aInterfaces[i].pvObject;
            goto exit_gracefully;
        }
    }

    hr = E_NOINTERFACE;         // failed.

exit_gracefully:

    if ( SUCCEEDED(hr) )
        ((LPUNKNOWN)*ppvObject)->AddRef();

    return hr;
}


/*-----------------------------------------------------------------------------
/ CUnknown::HandleAddRef
/ ----------------------
/   Increase the objects reference count.  Global reference count increase
/   by the constructor.
/
/ In:
/   -
/ Out:
/   current reference count
/----------------------------------------------------------------------------*/
STDMETHODIMP_(ULONG) CUnknown::HandleAddRef()
{
    return InterlockedIncrement(&m_cRefCount);
}


/*-----------------------------------------------------------------------------
/ CUnknown::HandleRelease
/ -----------------------
/   Decrease the reference counts, when the objects reaches zero then 
/   destroy it (which inturn will decrease the global reference count).
/
/ In:
/   -
/ Out:
/   current reference count == 0 if destroyed
/----------------------------------------------------------------------------*/
STDMETHODIMP_(ULONG) CUnknown::HandleRelease()
{
    TraceAssert( 0 == m_cRefCount );
    ULONG cRef = InterlockedDecrement(&m_cRefCount);
    if ( 0 == cRef )
    {
        delete this;    
    }
    return cRef;
}
