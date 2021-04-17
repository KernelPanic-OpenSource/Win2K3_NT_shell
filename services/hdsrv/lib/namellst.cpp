#include "namellst.h"

#include "sfstr.h"

#include "dbg.h"

#define ARRAYSIZE(a) (sizeof((a))/sizeof((a)[0]))

//=============================================================================
//=============================================================================
//==                          CNamedElem                                     ==
//=============================================================================
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// Public
HRESULT CNamedElem::GetName(LPTSTR psz, DWORD cch, DWORD* pcchRequired)
{
    return SafeStrCpyNReq(psz, _pszElemName, cch, pcchRequired);
}

#ifdef DEBUG
LPCTSTR CNamedElem::DbgGetName()
{
    return _pszElemName;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Protected
CNamedElem::CNamedElem() : _pszElemName(NULL)
{}

CNamedElem::~CNamedElem()
{
    if (_pszElemName)
    {
        _FreeName();
    }
}

HRESULT CNamedElem::_SetName(LPCTSTR pszElemName)
{
    ASSERT(!_pszElemName);
    HRESULT hres;
    DWORD cch = lstrlen(pszElemName) + 1;

    ASSERT(cch);

    _pszElemName = (LPTSTR)LocalAlloc(LPTR, cch * sizeof(TCHAR));

    if (_pszElemName)
    {
        hres = SafeStrCpyN(_pszElemName, pszElemName, cch);
    }
    else
    {
        hres = E_OUTOFMEMORY;
    }

    return hres;
}

HRESULT CNamedElem::_FreeName()
{
    ASSERT(_pszElemName);
    LocalFree((HLOCAL)_pszElemName);
    return S_OK;
}

//=============================================================================
//=============================================================================
//==                          CNamedElemList                                 ==
//=============================================================================
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// Public
HRESULT CNamedElemList::Init(NAMEDELEMCREATEFCT createfct,
    NAMEDELEMGETFILLENUMFCT enumfct)
{
    HRESULT hres;

    _createfct = createfct;
    _enumfct = enumfct;

    _pcs = new CRefCountedCritSect();

    if (_pcs)
    {
        if (InitializeCriticalSectionAndSpinCount(_pcs, 0))
        {
            hres = S_OK;
        }
        else
        {
            delete _pcs;
            _pcs = NULL;

            hres = E_FAIL;
        }
    }
    else
    {
        hres = E_OUTOFMEMORY;
    }

    return hres;
}

HRESULT CNamedElemList::GetOrAdd(LPCTSTR pszElemName, CNamedElem** ppelem)
{
    HRESULT hres = E_INVALIDARG;

    *ppelem = NULL;

    if (pszElemName)
    {
        CElemSlot* pes;

        EnterCriticalSection(_pcs);

        hres = _GetElemSlot(pszElemName, &pes);

        if (SUCCEEDED(hres))
        {
            if (S_FALSE != hres)
            {
                // Got one
                hres = pes->GetNamedElem(ppelem);

                pes->RCRelease();
            }
            else
            {
                // None found
                hres = _Add(pszElemName, ppelem);

                if (SUCCEEDED(hres))
                {
#ifdef DEBUG
                    TRACE(TF_NAMEDELEMLISTMODIF, TEXT("Added to '%s': '%s'"),
                        _szDebugName, pszElemName);
#endif
                    hres = S_FALSE;
                }
            }
        }

        LeaveCriticalSection(_pcs);
    }

    return hres;
}

HRESULT CNamedElemList::Get(LPCTSTR pszElemName, CNamedElem** ppelem)
{
    HRESULT hres = E_INVALIDARG;

    *ppelem = NULL;

    if (pszElemName)
    {
        CElemSlot* pes;

        EnterCriticalSection(_pcs);

        hres = _GetElemSlot(pszElemName, &pes);

        if (SUCCEEDED(hres))
        {
            if (S_FALSE != hres)
            {
                // Got one
                hres = pes->GetNamedElem(ppelem);

                pes->RCRelease();
            }
        }

        LeaveCriticalSection(_pcs);
    }

    return hres;
}

HRESULT CNamedElemList::Add(LPCTSTR pszElemName, CNamedElem** ppelem)
{
    HRESULT hres = E_INVALIDARG;

    if (pszElemName)
    {
        EnterCriticalSection(_pcs);

        hres = _Add(pszElemName, ppelem);

        LeaveCriticalSection(_pcs);

#ifdef DEBUG
        if (SUCCEEDED(hres))
        {
            TRACE(TF_NAMEDELEMLISTMODIF, TEXT("Added to '%s': '%s'"),
                _szDebugName, pszElemName);
        }
#endif
    }

    return hres;
}

HRESULT CNamedElemList::Remove(LPCTSTR pszElemName)
{
    HRESULT hres = E_INVALIDARG;

    if (pszElemName)
    {
        EnterCriticalSection(_pcs);

        hres = _Remove(pszElemName);

        LeaveCriticalSection(_pcs);

#ifdef DEBUG
        if (SUCCEEDED(hres))
        {
            if (S_FALSE != hres)
            {
                TRACE(TF_NAMEDELEMLISTMODIF, TEXT("Removed from '%s': '%s'"),
                    _szDebugName, pszElemName);
            }
            else
            {
                TRACE(TF_NAMEDELEMLISTMODIF,
                    TEXT("TRIED to remove from '%s': '%s'"),
                    _szDebugName, pszElemName);
            }
        }
#endif
    }

    return hres;
}

HRESULT CNamedElemList::ReEnum()
{
    HRESULT hres = E_FAIL;

    if (_enumfct)
    {
#ifdef DEBUG
        TRACE(TF_NAMEDELEMLISTMODIF, TEXT("ReEnum '%s' beginning"),
            _szDebugName);
#endif
        EnterCriticalSection(_pcs);
    
        hres = _EmptyList();

        if (SUCCEEDED(hres))
        {
            CFillEnum* pfillenum;

            hres = _enumfct(&pfillenum);

            if (SUCCEEDED(hres))
            {
                TCHAR szElemName[MAX_PATH];
                LPTSTR pszElemName = szElemName;
                DWORD cchReq;

                do
                {
                    hres = pfillenum->Next(szElemName, ARRAYSIZE(szElemName),
                        &cchReq);

                    if (S_FALSE != hres)
                    {
                        if (E_BUFFERTOOSMALL == hres)
                        {
                            pszElemName = (LPTSTR)LocalAlloc(LPTR, cchReq *
                                sizeof(TCHAR));

                            if (pszElemName)
                            {
                                hres = pfillenum->Next(pszElemName, cchReq,
                                    &cchReq);
                            }
                            else
                            {
                                hres = E_OUTOFMEMORY;
                            }
                        }

                        if (SUCCEEDED(hres) && (S_FALSE != hres))
                        {
                            hres = _Add(pszElemName, NULL);

#ifdef DEBUG
                            if (SUCCEEDED(hres))
                            {
                                TRACE(TF_NAMEDELEMLISTMODIF, TEXT("Added to '%s': '%s'"),
                                    _szDebugName, pszElemName);
                            }
#endif
                            if (FAILED(hres))
                            {
                                // We want to continue the enumeration
                                hres = S_OK;
                            }
                        }

                        if (pszElemName && (pszElemName != szElemName))
                        {
                            LocalFree((HLOCAL)pszElemName);
                        }
                    }
                }
                while (SUCCEEDED(hres) && (S_FALSE != hres));

                pfillenum->RCRelease();
            }
        }

        LeaveCriticalSection(_pcs);

#ifdef DEBUG
        if (SUCCEEDED(hres))
        {
            TRACE(TF_NAMEDELEMLISTMODIF, TEXT("ReEnumed '%s'"), _szDebugName);
        }
#endif
    }

    return hres;
}

HRESULT CNamedElemList::EmptyList()
{
    HRESULT hres;

    EnterCriticalSection(_pcs);

    hres = _EmptyList();

    LeaveCriticalSection(_pcs);

#ifdef DEBUG
    if (SUCCEEDED(hres))
    {
        TRACE(TF_NAMEDELEMLISTMODIF, TEXT("Emptied '%s'"), _szDebugName);
    }
#endif

    return hres;
}

HRESULT CNamedElemList::GetEnum(CNamedElemEnum** ppenum)
{
    HRESULT hres;

    CNamedElemEnum* penum = new CNamedElemEnum();

    if (penum)
    {
        CElemSlot* pesTail;

        EnterCriticalSection(_pcs);

        hres = _GetTail(&pesTail);

        if (SUCCEEDED(hres))
        {
            hres = penum->_Init(pesTail, _pcs);

            if (SUCCEEDED(hres))
            {
                *ppenum = penum;
            }
            else
            {
                delete penum;
            }

            if (pesTail)
            {
                pesTail->RCRelease();
            }
        }

        LeaveCriticalSection(_pcs);
    }
    else
    {
        hres = E_OUTOFMEMORY;
    }

    return hres;
}

CNamedElemList::CNamedElemList() : _pcs(NULL), _pesHead(NULL)
{
#ifdef DEBUG
    _szDebugName[0] = 0;
#endif
}

void CNamedElemList::RealRemoveElemSlotCallback(CElemSlot* pes)
{
    ASSERT(pes == _pesHead);

    _pesHead = pes->GetNext();

    if (_pesHead)
    {
        // we don't keep a ref on the head
        _pesHead->RCRelease();

        _pesHead->SetCallbackPointer(this);
    }
}

CNamedElemList::~CNamedElemList()
{
    if (_pcs)
    {
        EnterCriticalSection(_pcs);

        if (_pesHead)
        {
            // This list is going away.  We don't want this CElemSlot to
            // callback on an invalid pointer.
            _pesHead->SetCallbackPointer(NULL);
        }

        _EmptyList();

        LeaveCriticalSection(_pcs);

        DeleteCriticalSection(_pcs);

        _pcs->RCRelease();
    }
}
///////////////////////////////////////////////////////////////////////////////
// Private
// All these fcts have to be called from within The critical section
HRESULT CNamedElemList::_Add(LPCTSTR pszElemName, CNamedElem** ppelem)
{
    CNamedElem* pelem;
    HRESULT hres = _createfct(&pelem);

    if (SUCCEEDED(hres))
    {
        hres = pelem->Init(pszElemName);

        if (SUCCEEDED(hres))
        {
            CElemSlot* pes = new CElemSlot();

            if (pes)
            {
                // This takes an additionnal ref on pelem
                hres = pes->Init(pelem, NULL, _pesHead);

                if (SUCCEEDED(hres))
                {
                    if (_pesHead)
                    {
                        _pesHead->SetCallbackPointer(NULL);

                        _pesHead->SetPrev(pes);
                    }

                    _pesHead = pes;
                    _pesHead->SetCallbackPointer(this);
                }
                else
                {
                    pes->RCRelease();
                }
            }
            else
            {
                hres = E_OUTOFMEMORY;
            }
        }

        pelem->RCRelease();

        if (FAILED(hres))
        {
            pelem = NULL;
        }
    }
    
    if (ppelem)
    {
        if (pelem)
        {
            pelem->RCAddRef();
        }

        *ppelem = pelem;
    }

    return hres;
}

HRESULT CNamedElemList::_GetTail(CElemSlot** ppes)
{
    HRESULT hr;
    CElemSlot* pesLastValid = _GetValidHead();
        
    if (pesLastValid)
    {
        CElemSlot* pesOld = pesLastValid;

        while (NULL != (pesLastValid = pesLastValid->GetNextValid()))
        {
            pesOld->RCRelease();

            pesOld = pesLastValid;
        }

        pesLastValid = pesOld;

        hr = S_OK;
    }
    else
    {
        hr = S_FALSE;
    }

    // pesLastValid is already RCAddRef'ed
    *ppes = pesLastValid;

    return hr;
}

HRESULT CNamedElemList::_GetElemSlot(LPCTSTR pszElemName, CElemSlot** ppes)
{
    HRESULT hres = S_FALSE;

    CElemSlot* pes = _GetValidHead();
    CElemSlot* pesOld = pes;

    while (pes && SUCCEEDED(hres) && (S_FALSE == hres))
    {
        CNamedElem* pelem;

        hres = pes->GetNamedElem(&pelem);

        if (SUCCEEDED(hres))
        {
            TCHAR szElemName[MAX_PATH];
            LPTSTR pszElemNameLocal = szElemName;
            DWORD cchReq;

            hres = pelem->GetName(szElemName, ARRAYSIZE(szElemName), &cchReq);

            if (E_BUFFERTOOSMALL == hres)
            {
                pszElemNameLocal = (LPTSTR)LocalAlloc(LPTR, cchReq *
                    sizeof(TCHAR));

                if (pszElemNameLocal)
                {
                    hres = pelem->GetName(pszElemNameLocal, cchReq, &cchReq);
                }
                else
                {
                    hres = E_OUTOFMEMORY;
                }
            }

            pelem->RCRelease();

            if (SUCCEEDED(hres))
            {
                if (!lstrcmpi(pszElemNameLocal, pszElemName))
                {
                    // Found it!
                    pes->RCAddRef();

                    *ppes = pes;

                    hres = S_OK;
                }
                else
                {
                    ASSERT(pesOld == pes);

                    pes = pes->GetNextValid();

                    hres = S_FALSE;
                }
            }

            if (pszElemNameLocal && (pszElemNameLocal != szElemName))
            {
                LocalFree((HLOCAL)pszElemNameLocal);
            }
        }

        pesOld->RCRelease();
        pesOld = pes;
    }

    return hres;
}

HRESULT CNamedElemList::_Remove(LPCTSTR pszElemName)
{
    ASSERT(pszElemName);
    CElemSlot* pes;

    HRESULT hres = _GetElemSlot(pszElemName, &pes);

    if (SUCCEEDED(hres) && (S_FALSE != hres))
    {
        hres = pes->Remove();

        // 2: one to balance the _GetElemSlot and one to remove from list
        pes->RCRelease();
        pes->RCRelease();
    }

    return hres;
}

HRESULT CNamedElemList::_EmptyList()
{
    HRESULT hres = S_FALSE;

    CElemSlot* pes = _GetValidHead();

    if (_pesHead)
    {
        _pesHead->SetCallbackPointer(NULL);
    }

    while (pes)
    {
        CElemSlot* pesOld = pes;

        pes->Remove();

        pes = pes->GetNextValid();

        // 2: one to balance the _GetValidHead/GetNextValid and one to remove
        // from list
        pesOld->RCRelease();
        pesOld->RCRelease();
    }

    _pesHead = NULL;

    return hres;
}

CElemSlot* CNamedElemList::_GetValidHead()
{
    CElemSlot* pes = _pesHead;

    if (pes)
    {
        if (pes->IsValid())
        {
            pes->RCAddRef();
        }
        else
        {
            pes = pes->GetNextValid();
        }
    }
    
    return pes;
}

#ifdef DEBUG
HRESULT CNamedElemList::InitDebug(LPWSTR pszDebugName)
{
    return SafeStrCpyN(_szDebugName, pszDebugName,
        ARRAYSIZE(_szDebugName));
}

void CNamedElemList::AssertNoDuplicate()
{
    EnterCriticalSection(_pcs);

    CElemSlot* pes = _GetValidHead();

    while (pes)
    {
        CElemSlot* pesOld = pes;
        CNamedElem* pelem;
        WCHAR szName[1024];

        HRESULT hres = pes->GetNamedElem(&pelem);
        
        if (SUCCEEDED(hres))
        {
            DWORD cchReq;
            hres = pelem->GetName(szName, ARRAYSIZE(szName), &cchReq);

            if (SUCCEEDED(hres))
            {
                CElemSlot* pesIn = pes->GetNextValid();

                while (pesIn)
                {
                    CElemSlot* pesInOld = pesIn;
                    CNamedElem* pelemIn;
                    WCHAR szNameIn[1024];

                    hres = pesIn->GetNamedElem(&pelemIn);
        
                    if (SUCCEEDED(hres))
                    {
                        DWORD cchReqIn;
                        hres = pelemIn->GetName(szNameIn, ARRAYSIZE(szNameIn),
                            &cchReqIn);

                        if (SUCCEEDED(hres))
                        {
                            ASSERT(lstrcmp(szName, szNameIn));
                        }

                        pelemIn->RCRelease();
                    }

                    pesIn = pesIn->GetNextValid();

                    pesInOld->RCRelease();
                }
            }

            pelem->RCRelease();
        }

        pes = pes->GetNextValid();

        pesOld->RCRelease();
    }

    LeaveCriticalSection(_pcs);
}

void CNamedElemList::AssertAllElemsRefCount1()
{
    EnterCriticalSection(_pcs);

    CElemSlot* pes = _GetValidHead();

    while (pes)
    {
        CElemSlot* pesOld = pes;
        CNamedElem* pelem;

        HRESULT hres = pes->GetNamedElem(&pelem);
        
        if (SUCCEEDED(hres))
        {
            pelem->RCRelease();
        }

        pes = pes->GetNextValid();

        pesOld->RCRelease();
    }

    LeaveCriticalSection(_pcs);
}
#endif
//=============================================================================
//=============================================================================
//==                          CNamedElemEnum                                 ==
//=============================================================================
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// Public
HRESULT CNamedElemEnum::Next(CNamedElem** ppelem)
{
    HRESULT hres = S_FALSE;

    *ppelem = NULL;

    EnterCriticalSection(_pcsList);

    if (_pesCurrent)
    {
        CElemSlot* pes = _pesCurrent;

        if (!_fFirst || !pes->IsValid())
        {
            pes = pes->GetPrevValid();

            _pesCurrent->RCRelease();
            _pesCurrent = pes;
        }

        if (pes)
        {
            hres = pes->GetNamedElem(ppelem);
        }

        _fFirst = FALSE;
    }

    LeaveCriticalSection(_pcsList);

    return hres;
}

CNamedElemEnum::CNamedElemEnum() : _pesCurrent(NULL), _pcsList(NULL)
{
#ifdef DEBUG
    static TCHAR _szDebugName[] = TEXT("CNamedElemEnum");
#endif
}

CNamedElemEnum::~CNamedElemEnum()
{
    if (_pcsList)
    {
        EnterCriticalSection(_pcsList);

        if (_pesCurrent)
        {
            _pesCurrent->RCRelease();
        }

        LeaveCriticalSection(_pcsList);

        _pcsList->RCRelease();
    }
}
///////////////////////////////////////////////////////////////////////////////
// Private
HRESULT CNamedElemEnum::_Init(CElemSlot* pesHead, CRefCountedCritSect* pcsList)
{
    pcsList->RCAddRef();

    _pcsList = pcsList;
    _fFirst = TRUE;

    EnterCriticalSection(_pcsList);

    _pesCurrent = pesHead;

    if (_pesCurrent)
    {
        _pesCurrent->RCAddRef();
    }

    LeaveCriticalSection(_pcsList);

    return S_OK;
}

//=============================================================================
//=============================================================================
//==                          CElemSlot                                      ==
//=============================================================================
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// Public
HRESULT CElemSlot::Init(CNamedElem* pelem, CElemSlot* pesPrev,
    CElemSlot* pesNext)
{
    _pelem = pelem;
    pelem->RCAddRef();
    
    _fValid = TRUE;
    _pesPrev = pesPrev;
    _pesNext = pesNext;

    return S_OK;
}

HRESULT CElemSlot::Remove()
{
    _fValid = FALSE;
    _pelem->RCRelease();
    _pelem = NULL; 

    return S_OK;
}

HRESULT CElemSlot::GetNamedElem(CNamedElem** ppelem)
{
    ASSERT(_fValid);

    _pelem->RCAddRef();
    *ppelem = _pelem;
    
    return S_OK;
}

void CElemSlot::SetPrev(CElemSlot* pes)
{
    _pesPrev = pes;
}

CElemSlot* CElemSlot::GetNextValid()
{
    CElemSlot* pes = _pesNext;

    while (pes && !pes->IsValid())
    {
        pes = pes->_pesNext;
    }

    if (pes)
    {
        pes->RCAddRef();
    }
    
    return pes;
}

CElemSlot* CElemSlot::GetNext()
{
    if (_pesNext)
    {
        _pesNext->RCAddRef();
    }

    return _pesNext;
}

CElemSlot* CElemSlot::GetPrevValid()
{
    CElemSlot* pes = _pesPrev;

    while (pes && !pes->IsValid())
    {
        pes = pes->_pesPrev;
    }

    if (pes)
    {
        pes->RCAddRef();
    }
    
    return pes;
}

BOOL CElemSlot::IsValid()
{
    return _fValid;
}

void CElemSlot::SetCallbackPointer(CNamedElemList* pnel)
{
    _pnel = pnel;
}

CElemSlot::CElemSlot() : _fValid(FALSE), _pesPrev(NULL), _pesNext(NULL),
    _pnel(NULL)
{
#ifdef DEBUG
    static TCHAR _szDebugName[] = TEXT("CElemSlot");
#endif
}

CElemSlot::~CElemSlot()
{
    ASSERT(!_fValid);

    if (_pnel)
    {
        // This elem is the head of the list
        _pnel->RealRemoveElemSlotCallback(this);
    }

    if (_pesPrev)
    {
        _pesPrev->_pesNext = _pesNext;
    }

    if (_pesNext)
    {
        _pesNext->_pesPrev = _pesPrev;
    }
}
