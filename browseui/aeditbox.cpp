/**************************************************************\
    FILE: aeditbox.cpp

    DESCRIPTION:
        The Class CAddressEditBox exists to support a typical
    set of functionality used in Editboxes or ComboBoxes.  This
    object will add AutoComplete functionality to the EditBox and
    specify default "AutoComplete" Lists.  If the control is a
    ComboBoxEd, this object will populate the drop down list
    appropriately.
\**************************************************************/

#include "priv.h"
#include "sccls.h"
#include "addrlist.h"
#include "itbar.h"
#include "itbdrop.h"
#include "util.h"
#include "aclhist.h"
#include "aclmulti.h"
#include "autocomp.h"
#include "address.h"
#include "shellurl.h"
#include "bandprxy.h"
#include "uemapp.h"
#include "apithk.h"
#include "accdel.h"

#include "resource.h"
#include "mluisupp.h"


extern DWORD g_dwStopWatchMode;

// Needed in order to track down NTRAID#187504-Bryanst-Tracking Winsta for corruption
HWINSTA g_hWinStationBefore = NULL;
HWINSTA g_hWinStationAfter = NULL;
HWINSTA g_hWinStationAfterEx = NULL;

// Internal message for async processing of the IDList when navigating
UINT    g_nAEB_AsyncNavigation = 0;

///////////////////////////////////////////////////////////////////
// #DEFINEs
#define SZ_ADDRESSCOMBO_PROP            TEXT("CAddressCombo_This")
#define SZ_ADDRESSCOMBOEX_PROP          TEXT("CAddressComboEx_This")
#define SEL_ESCAPE_PRESSED  (-2)



///////////////////////////////////////////////////////////////////
// Data Structures
enum ENUMLISTTYPE
{
    LT_NONE,
    LT_SHELLNAMESPACE,
    LT_TYPEIN_MRU,
};

///////////////////////////////////////////////////////////////////
// class AsyncNav: this object contains all the necessary information
//                 to execute an asynchronous navigation task, so that
//                 the user doesn't have to wait for navigation to
//                 finish before doing anything, and the navigation
//                 can be canceled if it takes too long.

class AsyncNav
{
public:
// Public Functions ***************************************

    AsyncNav()
    {
        _cRef = 1;
        _pShellUrl = NULL;
        _pszUrl = NULL;
    }

    LONG AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    LONG Release()
    {
        ASSERT( 0 != _cRef );
        LONG cRef = InterlockedDecrement(&_cRef);
        if ( 0 == cRef )
        {
            delete this;
        }
        return cRef;
    }
    void    SetCanceledFlag() {_fWasCanceled = TRUE;}

// Data members ***************************************
    
    CShellUrl * _pShellUrl;
    DWORD       _dwParseFlags;
    BOOL        _fWasCorrected;
    BOOL        _fPidlCheckOnly;
    HRESULT     _hr;
    LPTSTR      _pszUrl;
    BOOL        _fWasCanceled;

    HWND        _hwnd;  // HWND that receives the message when processing is done.
    
    BOOL        _fReady; // This ensures that we will not try to use the object before it's ready
                         // CONSIDER: the memory can be released and then re-used by the same object
                         // CONSIDER: which would have us believe that the navigation should be done.
                         // CONSIDER: But if the navigation had been canceled and the memory re-used by the next AsyncNav alloc
                         // CONSIDER: we would handle the message g_nAEB_AsyncNavigation with an
                         // CONSIDER: unprocessed AsyncNav object. (See the handler for g_nAEB_AsyncNavigation).

private:
    LONG _cRef;
    ~AsyncNav()
    {
        delete _pShellUrl;
        _pShellUrl = NULL;
        Str_SetPtr(&_pszUrl, NULL);
    }
};

///////////////////////////////////////////////////////////////////
// Prototypes

/**************************************************************\
    CLASS: CAddressEditBox
\**************************************************************/
class CAddressEditBox
                : public IWinEventHandler
                , public IDispatch
                , public IAddressBand
                , public IAddressEditBox
                , public IOleCommandTarget
                , public IPersistStream
                , public IShellService
{
public:
    //////////////////////////////////////////////////////
    // Public Interfaces
    //////////////////////////////////////////////////////

    // *** IUnknown ***
    virtual STDMETHODIMP_(ULONG) AddRef(void);
    virtual STDMETHODIMP_(ULONG) Release(void);
    virtual STDMETHODIMP QueryInterface(REFIID riid, LPVOID * ppvObj);

    // *** IOleCommandTarget methods ***
    virtual STDMETHODIMP QueryStatus(const GUID *pguidCmdGroup, ULONG cCmds,
                        OLECMD rgCmds[], OLECMDTEXT *pcmdtext);
    virtual STDMETHODIMP Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt,
                        VARIANTARG *pvarargIn, VARIANTARG *pvarargOut);

    // *** IWinEventHandler methods ***
    virtual STDMETHODIMP OnWinEvent (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plre);
    virtual STDMETHODIMP IsWindowOwner(HWND hwnd);

    // *** IDispatch methods ***
    virtual STDMETHODIMP GetTypeInfoCount(UINT *pctinfo) {return E_NOTIMPL;}
    virtual STDMETHODIMP GetTypeInfo(UINT itinfo,LCID lcid,ITypeInfo **pptinfo) {return E_NOTIMPL;}
    virtual STDMETHODIMP GetIDsOfNames(REFIID riid,OLECHAR **rgszNames,UINT cNames, LCID lcid, DISPID * rgdispid) {return E_NOTIMPL;}
    virtual STDMETHODIMP Invoke(DISPID dispidMember,REFIID riid,LCID lcid,WORD wFlags,
                  DISPPARAMS * pdispparams, VARIANT * pvarResult, EXCEPINFO * pexcepinfo,UINT * puArgErr);

    // *** IPersistStream methods ***
    virtual STDMETHODIMP GetClassID(CLSID *pClassID){ *pClassID = CLSID_AddressEditBox; return S_OK; }
    virtual STDMETHODIMP Load(IStream *pStm) {return S_OK;}
    virtual STDMETHODIMP Save(IStream *pStm, BOOL fClearDirty) { Save(0); return S_OK;}
    virtual STDMETHODIMP IsDirty(void) {return S_OK;}       // Indicate that we are dirty and ::Save() needs to be called.
    virtual STDMETHODIMP GetSizeMax(ULARGE_INTEGER *pcbSize) {return E_NOTIMPL;}

    // *** IAddressBand methods ***
    virtual STDMETHODIMP FileSysChange(DWORD dwEvent, LPCITEMIDLIST *ppidl);
    virtual STDMETHODIMP Refresh(VARIANT * pvarType);

    // *** IAddressEditBox methods ***
    virtual STDMETHODIMP Init(HWND hwndComboBox, HWND hwndEditBox, DWORD dwFlags, IUnknown * punkParent);
    virtual STDMETHODIMP SetCurrentDir(LPCOLESTR pwzDir);
    virtual STDMETHODIMP ParseNow(DWORD dwFlags);
    virtual STDMETHODIMP Execute(DWORD dwExecFlags);
    virtual STDMETHODIMP Save(DWORD dwReserved);

    // *** IShellService methods ***
    STDMETHODIMP SetOwner(IUnknown* punkOwner);

protected:
    //////////////////////////////////////////////////////
    // Private Member Functions
    //////////////////////////////////////////////////////

    // Constructor / Destructor
    CAddressEditBox();
    ~CAddressEditBox(void);        // This is now an OLE Object and cannot be used as a normal Class.

    LRESULT _OnNotify(LPNMHDR pnm);
    LRESULT _OnCommand(WPARAM wParam, LPARAM lParam);
    LRESULT _OnBeginEdit(LPNMHDR pnm) ;
    LRESULT _OnEndEditW(LPNMCBEENDEDITW pnmW);
    LRESULT _OnEndEditA(LPNMCBEENDEDITA pnmA);

    HRESULT _ConnectToBrwsrConnectionPoint(BOOL fConnect, IUnknown * punk);
    HRESULT _ConnectToBrwsrWnd(IUnknown* punk);
    HRESULT _UseNewList(ENUMLISTTYPE eltNew);
    HRESULT _CreateCShellUrl(void);

    HRESULT _HandleUserAction(LPCTSTR pszUrl, int iNewSelection);
    HRESULT _NavigationComplete(LPCTSTR pszUrl, BOOL fChangeLists, BOOL fAddToMRU);
    void    _SetAutocompleteOptions();
    void    _GetUrlAndCache(void);
    BOOL _IsShellUrl(void);

    static HRESULT _NavigateToUrlCB(LPARAM lParam, LPTSTR lpUrl);
    static LRESULT CALLBACK _ComboSubclassWndProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

    // Functions for keeping dirty contents from getting clobbered
    BOOL    _IsDirty();
    void    _ClearDirtyFlag();
    void    _InstallHookIfDirty();
    void    _RemoveHook();
    LRESULT _MsgHook(int nCode, WPARAM wParam, MOUSEHOOKSTRUCT *pmhs);
    static LRESULT CALLBACK CAddressEditBox::_MsgHook(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK _ComboExSubclassWndProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK _EditSubclassWndProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK _EnumFindWindow(HWND hwnd, LPARAM lParam);

    HRESULT _FinishNavigate();
    static DWORD WINAPI _AsyncNavigateThreadProc(LPVOID pvData); // do async navigation: figure out the PIDL on a separate thread.
    
    void _JustifyAddressBarText( void );
    HRESULT _AsyncNavigate(AsyncNav *pAsyncNav);
    HRESULT _CancelNavigation();

    // Friend Functions
    friend HRESULT CAddressEditBox_CreateInstance(IUnknown *punkOuter, IUnknown **ppunk, LPCOBJECTINFO poi);

    //////////////////////////////////////////////////////
    //  Private Member Variables
    //////////////////////////////////////////////////////
    int             m_cRef;              // COM Object Ref Count

    IUnknown *      m_punkParent;        // Our Parent that will receive events if something happens.
    DWORD           m_dwFlags;           // Flags that will modify the behavior of this object.
    HWND            m_hwnd;              // Address ComboBoxEx Control if we control a ComboBoxEx.
    HWND            m_hwndEdit;          // Address EditBox Control Window
    WNDPROC         m_lpfnComboWndProc;  // Former WndProc of Combo child
    int             m_nOldSelection;     // Previous Drop Down Selection.

    // Objects for Navigation
    IBandProxy *    m_pbp;               // The BandProxy that will take care of finding the window to Navigate.
    IBrowserService*m_pbs;               // Only valid when we are in a Browser Windows Toolbar. (Not Toolband)
    DWORD           m_dwcpCookie;        // ConnectionPoint cookie for DWebBrowserEvents2 from the Browser Window.
    LPTSTR          m_pszCurrentUrl;     // Needed in case refresh occurs.
    LPTSTR          m_pszPendingURL;     // Pending URL.  We hang on to it until navigation finished before adding to MRU.
    LPTSTR          m_pszUserEnteredURL; // Keep the exact text the user entered just in case we need to do a search.
    LPTSTR          m_pszHttpErrorUrl;
    BOOL            m_fDidShellExec;     // Was the last navigation handled by calleding ShellExec()? (Used when refreshing)
    BOOL            m_fConnectedToBrowser; // Are we connected to a browser?

    AsyncNav *      m_pAsyncNav;

    // AutoComplete Functionality
    IAutoComplete2* m_pac;               // AutoComplete object
    IShellService * m_pssACLISF;         // AutoComplete ISF List.  Needed if we need to change browsers.

    // AddressLists
    ENUMLISTTYPE    m_elt;
    ENUMLISTTYPE    m_eltPrevious;
    IAddressList *  m_palCurrent;        // CurrentList.
    IAddressList *  m_palSNS;            // Shell Name Space List.
    IAddressList *  m_palMRU;            // Type-in MRU List.
    IMRU *          m_pmru;              // MRU List.
    CShellUrl *     m_pshuUrl;

    // Variables for keeping dirty contens from getting clobbered
    static CAssociationList m_al;        // associate thread id with this class
    WNDPROC         m_lpfnComboExWndProc;// Former WndProc of ComboBoxEx
    WNDPROC         m_lpfnEditWndProc;   // Former WndProc of Edit control in ComboBox
    HHOOK           m_hhook;             // mouse message hook
    COMBOBOXEXITEM  m_cbex;              // last change received while dirty
    HWND            m_hwndBrowser;       // top-level browser window
    BOOL            m_fAssociated;       // if we are entered in m_al for this thread
    BOOL            m_fAsyncNavInProgress; // tells if we have a pending async navigate already in progress
};

class CAddressEditAccessible : public CDelegateAccessibleImpl
{
public:
    CAddressEditAccessible(HWND hwndCombo, HWND hwndEdit);

    // *** IUnknown ***
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID * ppvObj);

    // *** IAccessible ***
    STDMETHODIMP get_accName(VARIANT varChild, BSTR  *pszName);
    STDMETHODIMP get_accValue(VARIANT varChild, BSTR  *pszValue);

protected:
    virtual ~CAddressEditAccessible();

private:
    LONG    m_cRefCount;
    HWND    m_hwndEdit;
    LPWSTR  m_pwszName;
};


//=================================================================
// Static variables
//=================================================================
CAssociationList CAddressEditBox::m_al;

//=================================================================
// Implementation of CAddressEditBox
//=================================================================

//===========================
// *** IUnknown Interface ***

HRESULT CAddressEditBox::QueryInterface(REFIID riid, void **ppvObj)
{
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IWinEventHandler))
    {
        *ppvObj = SAFECAST(this, IWinEventHandler*);
    }
    else if (IsEqualIID(riid, IID_IDispatch))
    {
        *ppvObj = SAFECAST(this, IDispatch*);
    }
    else if (IsEqualIID(riid, IID_IAddressBand))
    {
        *ppvObj = SAFECAST(this, IAddressBand*);
    }
    else if (IsEqualIID(riid, IID_IAddressEditBox))
    {
        *ppvObj = SAFECAST(this, IAddressEditBox*);
    }
    else if (IsEqualIID(riid, IID_IOleCommandTarget))
    {
        *ppvObj = SAFECAST(this, IOleCommandTarget*);
    }
    else if (IsEqualIID(riid, IID_IPersistStream))
    {
        *ppvObj = SAFECAST(this, IPersistStream*);
    }
    else if (IsEqualIID(riid, IID_IShellService))
    {
        *ppvObj = SAFECAST(this, IShellService*);
    }
    else
    {
        *ppvObj = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG CAddressEditBox::AddRef(void)
{
    m_cRef++;
    return m_cRef;
}

ULONG CAddressEditBox::Release(void)
{
    ASSERT(m_cRef > 0);

    m_cRef--;

    if (m_cRef > 0)
    {
        return m_cRef;
    }

    delete this;
    return 0;
}


//=====================================
// *** IOleCommandTarget Interface ***

HRESULT CAddressEditBox::QueryStatus(const GUID *pguidCmdGroup,
    ULONG cCmds, OLECMD rgCmds[], OLECMDTEXT *pcmdtext)
{
    HRESULT hr = OLECMDERR_E_UNKNOWNGROUP;

    if (rgCmds == NULL)
    {
        return(E_INVALIDARG);
    }

    if (pguidCmdGroup==NULL)
    {
        hr = S_OK;

        for (UINT i=0; i<cCmds; i++)
        {
            ULONG l;
            rgCmds[i].cmdf = 0;

            switch (rgCmds[i].cmdID)
            {
            case OLECMDID_PASTE:
                if (m_hwndEdit && OpenClipboard(m_hwndEdit))
                {
                    // IDEA: We might want to support CF_URL here (SatoNa)
                    if (GetClipboardData(CF_TEXT))
                    {
                        rgCmds[i].cmdf = OLECMDF_ENABLED;
                    }
                    CloseClipboard();
                }
                break;

            case OLECMDID_COPY:
            case OLECMDID_CUT:
                if (m_hwndEdit)
                {
                    l=(ULONG)SendMessage(m_hwndEdit, EM_GETSEL, 0, 0);
                    if (LOWORD(l) != HIWORD(l))
                    {
                        rgCmds[i].cmdf = OLECMDF_ENABLED;
                    }
                }
                break;
            case OLECMDID_SELECTALL:
                if (m_hwndEdit)
                {
                    // Select All -- not allowed if there's no text or if everything is
                    // selected.   Latter case takes care of first one.
                    int ichMinSel;
                    int ichMaxSel;
                    int cch = (int)SendMessage(m_hwndEdit, WM_GETTEXTLENGTH, 0, 0);
                    SendMessage(m_hwndEdit, EM_GETSEL, (WPARAM)&ichMinSel, (LPARAM)&ichMaxSel);

                    if ((ichMinSel != 0) || (ichMaxSel != cch))
                    {
                        rgCmds[i].cmdf = OLECMDF_ENABLED;
                    }
                }
            }
        }
    }

    return(hr);
}

HRESULT CAddressEditBox::Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt,
                        VARIANTARG *pvarargIn, VARIANTARG *pvarargOut)
{
    HRESULT hr = OLECMDERR_E_UNKNOWNGROUP;

    if (pguidCmdGroup == NULL)
    {
        hr = S_OK;

        switch(nCmdID)
        {
        case OLECMDID_COPY:
            if (m_hwndEdit)
                SendMessage(m_hwndEdit, WM_COPY, 0, 0);
            break;

        case OLECMDID_PASTE:
            // IDEA: We might want to support CF_URL here (SatoNa)
            if (m_hwndEdit)
                SendMessage(m_hwndEdit, WM_PASTE, 0, 0);
            break;

        case OLECMDID_CUT:
            if (m_hwndEdit)
                SendMessage(m_hwndEdit, WM_CUT, 0, 0);
            break;

        case OLECMDID_SELECTALL:
            if (m_hwndEdit)
                Edit_SetSel(m_hwndEdit, 0, (LPARAM)-1);
            break;

        default:
            hr = OLECMDERR_E_UNKNOWNGROUP;
            break;
        }
    }
    else if (pguidCmdGroup && IsEqualGUID(CGID_Explorer, *pguidCmdGroup))
    {
        hr = S_OK;

        switch (nCmdID)
        {
        case SBCMDID_ERRORPAGE:
            {
                // We save urls to error pages so that they don't get placed
                // into the MRU
                if (pvarargIn && pvarargIn->vt == VT_BSTR)
                {
                    // Save the location where the error occured
                    Str_SetPtr(&m_pszHttpErrorUrl, pvarargIn->bstrVal);
                }
                break;
            }
        case SBCMDID_AUTOSEARCHING:
            {
                // The address did not resolve so the string is about to be sent
                // to the search engine or autoscanned.  There is a good chance
                // the pending url had "http:\\" prefixed which is a bogus url.
                // So let's put what the user typed into the mru instead.
                //
                Str_SetPtr(&m_pszPendingURL, m_pszUserEnteredURL);
                break;
            }

        case SBCMDID_GETUSERADDRESSBARTEXT:
            UINT cb = (m_pszUserEnteredURL ? (lstrlen(m_pszUserEnteredURL) + 1) : 0);
            BSTR bstr = NULL;

            VariantInit(pvarargOut);

            if (cb)
                bstr = SysAllocStringLen(NULL, cb);
            if (bstr)
            {
                SHTCharToUnicode(m_pszUserEnteredURL, bstr, cb);
                pvarargOut->vt = VT_BSTR|VT_BYREF;
                pvarargOut->byref = bstr;
            }
            else
            {
                // VariantInit() might do this for us.
                pvarargOut->vt = VT_EMPTY;
                pvarargOut->byref = NULL;
                return E_FAIL;   // Edit_GetText gave us nothing
            }
            break;
        }
    }
    else if (pguidCmdGroup && IsEqualGUID(CGID_AddressEditBox, *pguidCmdGroup))
    {
        switch (nCmdID)
        {
        case AECMDID_SAVE:
            hr = Save(0);
            break;
        default:
            hr = E_NOTIMPL;
            break;
        }
    }
    return(hr);
}


//================================
//  ** IWinEventHandler Interface ***

/****************************************************\
    FUNCTION: OnWinEvent

    DESCRIPTION:
        This function will give receive events from
    the parent ShellToolbar.
\****************************************************/
HRESULT CAddressEditBox::OnWinEvent(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *plres)
{
    LRESULT lres = 0;

    switch (uMsg) {
    case WM_WININICHANGE:
        {
            HWND hwndLocal = (m_hwnd ? m_hwnd : m_hwndEdit);
            if (hwndLocal)
                SendMessage(hwndLocal, uMsg, wParam, lParam);

            // MRU Needs it because it May Need to purge the MRU even if it isn't the current list.
            if ((m_palCurrent != m_palMRU) && m_palMRU)
                m_palMRU->OnWinEvent(m_hwnd, uMsg, wParam, lParam, plres);

            _SetAutocompleteOptions();
        }
        break;

    case WM_COMMAND:
        lres = _OnCommand(wParam, lParam);
        break;

    case WM_NOTIFY:
        lres = _OnNotify((LPNMHDR)lParam);
        break;
    }

    if (plres)
        *plres = lres;

    // All Events get all events, and they need to determine
    // if they are active to act on most of the events.

    if (m_hwnd)
    {
        if (m_palCurrent)
        {
            m_palCurrent->OnWinEvent(m_hwnd, uMsg, wParam, lParam, plres);
        }

        // If we are dropping down the list, the above call could have
        // changed the selection, so grab it again...

        if ((uMsg == WM_COMMAND) && (GET_WM_COMMAND_CMD(wParam, lParam) == CBN_DROPDOWN))
        {
            m_nOldSelection = ComboBox_GetCurSel(m_hwnd);

            // If nothing selected, and something matches the contents of the editbox, select that
            if (m_nOldSelection == -1)
            {
                TCHAR szBuffer[MAX_URL_STRING];
                GetWindowText(m_hwnd, szBuffer, SIZECHARS(szBuffer));

                m_nOldSelection = (int)SendMessage(m_hwnd, CB_FINDSTRINGEXACT, (WPARAM)-1,  (LPARAM)szBuffer);
                if (m_nOldSelection != CB_ERR)
                {
                    ComboBox_SetCurSel(m_hwnd, m_nOldSelection);
                }
            }
        }
    }

    return S_OK;
}


/****************************************************\
    FUNCTION: IsWindowOwner

    DESCRIPTION:
        This function will return TRUE if the HWND
    passed in is a HWND owned by this band.
\****************************************************/
HRESULT CAddressEditBox::IsWindowOwner(HWND hwnd)
{
    if (hwnd == m_hwnd)
        return S_OK;

    if (m_hwndEdit && (hwnd == m_hwndEdit))
        return S_OK;

    return S_FALSE;
}


void CAddressEditBox::_GetUrlAndCache(void)
{
    TCHAR szTemp[MAX_URL_STRING];

    // This will fail when the browser first opens and the first navigation to the
    // default home page doesn't start downloading yet.
    if (SUCCEEDED(m_pshuUrl->GetUrl(szTemp, SIZECHARS(szTemp))))
    {
        SHRemoveURLTurd(szTemp);
        SHCleanupUrlForDisplay(szTemp);
        Str_SetPtr(&m_pszCurrentUrl, szTemp);      // Used when refreshing
    }
    else
    {
        Str_SetPtr(&m_pszCurrentUrl, NULL);
    }
}


//================================
// *** IDispatch Interface ***
/****************************************************\
    FUNCTION: Invoke

    DESCRIPTION:
        This function will give receive events from
    the Browser Window if this band is connected
    to one.  This will allow this band to remain up
    todate when the browser window changes URL by
    another means.
\****************************************************/
HRESULT CAddressEditBox::Invoke(DISPID dispidMember,REFIID riid,LCID lcid,WORD wFlags,
                  DISPPARAMS * pdispparams, VARIANT * pvarResult,
                  EXCEPINFO * pexcepinfo,UINT * puArgErr)
{
    HRESULT hr = S_OK;

    ASSERT(pdispparams);
    if (!pdispparams)
        return E_INVALIDARG;

    switch(dispidMember)
    {
        case DISPID_NAVIGATECOMPLETE: // This is when we have bits back?
            ASSERT(0);      // We didn't ask to synch these.
            break;

        // The event DISPID_NAVIGATECOMPLETE2 may be sent several times during
        // redirects.
        // The event DISPID_DOCUMENTCOMPLETE will only happen after navigation is
        // finished.
        case DISPID_DOCUMENTCOMPLETE:
            Str_SetPtr(&m_pszUserEnteredURL, NULL);
            break;

        case DISPID_NAVIGATECOMPLETE2:
        {
            DWORD dwCurrent;
            BOOL fFound = FALSE;

            ASSERT(m_elt != LT_NONE);
            IBrowserService* pbs = NULL;

            for (dwCurrent = 0; dwCurrent < pdispparams->cArgs; dwCurrent++)
            {
                if (pdispparams->rgvarg[dwCurrent].vt == VT_DISPATCH)
                {
                    // See who's sending us this event
                    hr = IUnknown_QueryService(pdispparams->rgvarg[dwCurrent].pdispVal, SID_SShellBrowser, IID_IBrowserService, (void**)&pbs);
                    if (pbs)
                    {
                        // We don't really need this interface, just its address
                        pbs->Release();
                    }
                    if (FAILED(hr) || pbs != m_pbs)
                    {
                        // Notification must have come from a frame, so ignore it because
                        // it doesn't affect the URL in the address bar.
                        return S_OK;
                    }
                }
                else if (!fFound)
                {
                    if ((pdispparams->rgvarg[dwCurrent].vt == VT_BSTR) ||
                        ((pdispparams->rgvarg[dwCurrent].vt == (VT_VARIANT|VT_BYREF)) &&
                        (pdispparams->rgvarg[dwCurrent].pvarVal->vt == VT_BSTR)))
                    {
                        fFound = TRUE;
                    }
                }
            }
            ASSERT(fFound);
            hr = _CreateCShellUrl();
            if (FAILED(hr))
                return hr;

            // Yes, so let's set our current working directory to the current window.
            ASSERT(m_pbs);
            LPITEMIDLIST pidl;

            if (SUCCEEDED(hr = m_pbs->GetPidl(&pidl)))
            {
                DEBUG_CODE(TCHAR szDbgBuffer[MAX_PATH];)
                TraceMsg(TF_BAND|TF_GENERAL, "CAddressEditBox::Invoke(), Current Pidl in TravelLog. PIDL=%s;", Dbg_PidlStr(pidl, szDbgBuffer, SIZECHARS(szDbgBuffer)));

                ASSERT(pidl);
                // m_pshuUrl will free pshuCurrWorkDir, so we can't.

                hr = m_pshuUrl->SetPidl(pidl);
                ILFree(pidl);

                _GetUrlAndCache();      // We call this function so stack space is only used temporarily.  It will set m_pszCurrentUrl.
                if (SUCCEEDED(hr))
                {
                    LPTSTR pszTempURL = NULL;

                    // WARNING: This code looks really strange, but it is necessary.  Normally,
                    // I would like to pass m_pszCurrentUrl as an arg to _NavigationComplete.  The problem
                    // is that the _NavigationComplete calls m_palCurrent->NavigationComplete() which will replace
                    // the value in m_pszCurrentUrl.  So I need to pass a value that will still be valid when 
                    // m_pszCurrentUrl gets reoplaced.
                    // (That function causes the string to change values indirectly because it ends up sending
                    // a CBEM_SETITEM message to the combobox which will update m_pszCurrentUrl.)
                    //
                    // We put this string on the heap because it can be very large (MAX_URL_STRING) and
                    // this code that calls us and the code we call, use an incredible amount of stack space.
                    // This code needs to highly optimize on how much stack space it uses or it will cause
                    // out of memory faults when trying to grow the stack.
                    Str_SetPtr(&pszTempURL, m_pszCurrentUrl);

                    if (pszTempURL)
                    {
                        hr = _NavigationComplete(pszTempURL, TRUE, TRUE);
                    }
                    Str_SetPtr(&pszTempURL, NULL);
                }
            }
            else
            {
                Str_SetPtr(&m_pszCurrentUrl, NULL);      // Init incase it's null
            }
        }
        break;
        default:
            hr = E_INVALIDARG;
    }

    return hr;
}


/****************************************************\
    FUNCTION: _UseNewList

    DESCRIPTION:
        This function will switch the list we use to
    populate the contents of the combobox.
\****************************************************/
HRESULT CAddressEditBox::_UseNewList(ENUMLISTTYPE eltNew)
{
    HRESULT hr = S_OK;

    ASSERT(m_hwnd);     // It's invalid for use to use a AddressList if we are only and EditBox.
    if (m_elt == eltNew)
        return S_OK;  // We are already using this list.

    if (m_palCurrent)
    {
        m_palCurrent->Connect(FALSE, m_hwnd, m_pbs, m_pbp, m_pac);
        m_palCurrent->Release();
    }

    switch(eltNew)
    {
    case LT_SHELLNAMESPACE:
        ASSERT(m_palSNS);
        m_palCurrent = m_palSNS;
        break;

    case LT_TYPEIN_MRU:
        ASSERT(m_palMRU);
        m_palCurrent = m_palMRU;
        break;
    default:
        ASSERT(0); // Someone messed up.
        m_palCurrent = NULL;
        break;
    }
    if (m_palCurrent)
    {
        m_palCurrent->AddRef();
        m_palCurrent->Connect(TRUE, m_hwnd, m_pbs, m_pbp, m_pac);
    }
    m_elt = eltNew;

    return hr;
}


//================================
// *** IAddressEditBox Interface ***

/****************************************************\
    FUNCTION: Save

    DESCRIPTION:
\****************************************************/
HRESULT CAddressEditBox::Save(DWORD dwReserved)
{
    HRESULT hr = S_OK;

    ASSERT(0 == dwReserved);        // Reserved for later.

    if (m_palMRU)
        hr = m_palMRU->Save();

    return hr;
}



/****************************************************\
    FUNCTION: Init

    PARAMETERS:
        hwnd - Points to ComboBoxEx otherwise NULL.
        hwndEditBox - EditBox.
        dwFlags - AEB_INIT_XXXX flags (Defined in iedev\inc\shlobj.w)
        punkParent - Pointer to parent object that should receive events.

    DESCRIPTION:
        This function will Hook this CAddressEditBox
    object to the ComboBoxEx or EditBox control.  If
    this object is being hooked up to a ComboBoxEx control,
    then hwnd is of the ComboBoxEx control and hwndEditBox
    is of that ComboBox's edit control.  If this is
    being hooked up to only an EditBox, then hwnd is NULL
    and hwndEditBox points to the edit box.  If punkParent
    is NULL, we will not be connected to a browser window
    at all.
\****************************************************/
HRESULT CAddressEditBox::Init(HWND hwnd,              OPTIONAL
                        HWND hwndEditBox,
                        DWORD dwFlags,
                        IUnknown * punkParent)  OPTIONAL
{
    HRESULT hr = S_OK;

    ASSERT(!m_hwnd);
    m_hwnd = hwnd;
    m_hwndEdit = hwndEditBox;
    m_dwFlags = dwFlags;
    IUnknown_Set(&m_punkParent, punkParent);

    // Get and save our top-level window
    m_hwndBrowser = hwnd;
    HWND hwndParent;
    while (hwndParent = GetParent(m_hwndBrowser))
    {
        m_hwndBrowser = hwndParent;
    }

    ASSERT(!(AEB_INIT_SUBCLASS &dwFlags));       // We don't support this yet.
    if (hwnd)  // Is this a ComboBox?
    {
        // Yes,

        ASSERT(!m_palSNS && !m_palMRU /*&& !m_palACP*/);

        m_palSNS = CSNSList_Create();
        m_palMRU = CMRUList_Create();
        if (!m_palSNS || !m_palMRU /*|| !m_palACP*/)
        {
            hr = E_FAIL;
        }

        if (SUCCEEDED(hr))
        {
            HWND hwndCombo;

            hwndCombo = (HWND)SendMessage(m_hwnd, CBEM_GETCOMBOCONTROL, 0, 0);
            if (!hwndCombo)
                hr = E_FAIL;  // This will happen if the user passed in a ComboBox instead of a ComboBoxEx for hwnd.
            if (hwndCombo && SetProp(hwndCombo, SZ_ADDRESSCOMBO_PROP, this))
            {
                g_hWinStationBefore = GetProcessWindowStation();
                // Subclass combobox for various tweaks.
                ASSERT(!m_lpfnComboWndProc);
                m_lpfnComboWndProc = (WNDPROC) SetWindowLongPtr(hwndCombo, GWLP_WNDPROC, (LONG_PTR) _ComboSubclassWndProc);

                TraceMsg(TF_BAND|TF_GENERAL, "CAddressEditBox::Init() wndproc=%x", m_lpfnComboWndProc);

                // Subclass the comboboxex too
                if (SetProp(hwnd, SZ_ADDRESSCOMBOEX_PROP, this))
                {
                    ASSERT(!m_lpfnComboExWndProc);
                    m_lpfnComboExWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)_ComboExSubclassWndProc);
                }
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        //
        // Set g_himl*
        //
        ASSERT(!m_pbp);
        hr = QueryService_SID_IBandProxy(punkParent, IID_IBandProxy, &m_pbp, NULL);

        // We need to set the list to MRU for the first time.
        // We need to do this to initialize the list because
        // it will be used even when other lists are selected.
        if (m_hwnd && LT_NONE == m_elt)
            _UseNewList(LT_TYPEIN_MRU);
    }

    if (hwndEditBox) {
        SendMessage(hwndEditBox, EM_SETLIMITTEXT, INTERNET_MAX_PATH_LENGTH - 1, 0);
    }

    return hr;
}


/****************************************************\
    FUNCTION: SetOwner

    PARAMETERS:
        punkOwner - Pointer to the parent object.

    DESCRIPTION:
        This function will be called to have this
    object try to obtain enough information about it's
    parent Toolbar to create the Band window and maybe
    connect to a Browser Window.
\****************************************************/
HRESULT CAddressEditBox::SetOwner(IUnknown* punkOwner)
{
    HRESULT hr = S_OK;

    if (m_pbs)
        _ConnectToBrwsrWnd(NULL);    // On-connect from Browser Window.

    if (m_hwnd && !punkOwner)
    {
        if (m_palSNS)
            m_palSNS->Save();
        if (m_palMRU)
            m_palMRU->Save();
    }

    IUnknown_Set(&m_punkParent, punkOwner);     // Needed to break ref count cycle.

    _ConnectToBrwsrWnd(punkOwner);    // On-connect from Browser Window.

    return hr;
}


/****************************************************\
    FUNCTION: SetCurrentDir

    DESCRIPTION:
        Set the Current Working directory so parsing
    will work correctly.
\****************************************************/
HRESULT CAddressEditBox::SetCurrentDir(LPCOLESTR pwzDir)
{
    HRESULT hr;
    SHSTR strWorkingDir;

    hr = strWorkingDir.SetStr(pwzDir);
    if (SUCCEEDED(hr))
    {
        LPITEMIDLIST pidl;

        hr = IECreateFromPath(strWorkingDir.GetStr(), &pidl);
        if (SUCCEEDED(hr))
        {
            hr = _CreateCShellUrl();
            ASSERT(SUCCEEDED(hr));
            if (SUCCEEDED(hr))
                hr = m_pshuUrl->SetCurrentWorkingDir(pidl);
            ILFree(pidl);
        }
    }
    return hr;
}


/****************************************************\
    FUNCTION: ParseNow

    PARAMETERS:
        dwFlags - Parse Flags

    DESCRIPTION:
        Parse the text that is currently in the EditBox.
\****************************************************/
HRESULT CAddressEditBox::ParseNow(DWORD dwFlags)
{
    HRESULT hr;

    TCHAR szBuffer[MAX_URL_STRING];
    ASSERT(m_hwnd);
    GetWindowText(m_hwnd, szBuffer, SIZECHARS(szBuffer));
    hr = _CreateCShellUrl();

    ASSERT(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
    {
        if (m_fConnectedToBrowser && !SHRegGetBoolUSValue(TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Band\\Address"), TEXT("Use Path"), FALSE, FALSE))
        {
            dwFlags |= SHURL_FLAGS_NOPATHSEARCH;
        }

        if (SHRegGetBoolUSValue(TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Band\\Address"), TEXT("AutoCorrect"), FALSE, /*default*/TRUE))
        {
            dwFlags |= SHURL_FLAGS_AUTOCORRECT;
        }

        hr = m_pshuUrl->ParseFromOutsideSource(szBuffer, dwFlags);
    }

    return hr;
}


/****************************************************\
    FUNCTION: Execute

    PARAMETERS:
        dwExecFlags - Execute Flags

    DESCRIPTION:
        This function will execute the last parsed string.
    In most cases, the caller should call ::ParseNow()
    first.
\****************************************************/
HRESULT CAddressEditBox::Execute(DWORD dwExecFlags)
{
    HRESULT hr = E_FAIL;

    ASSERT(m_pshuUrl);
    TCHAR   szShortcutFilePath[MAX_PATH];
    LPITEMIDLIST pidl;

    hr = m_pshuUrl->GetPidlNoGenerate(&pidl);

    if (SUCCEEDED(hr))
    {
        hr = IEGetNameAndFlags(pidl, SHGDN_FORPARSING, szShortcutFilePath, SIZECHARS(szShortcutFilePath), NULL);
        ILFree(pidl);
    }

    // if this is a .url and we can navigate to it
    // then we need to do that now, otherwise
    // we'll end up with a shell exec happening
    // which will open the .url in whatever
    // browse window the system happens to like

    if (SUCCEEDED(hr))
    {
        ASSERT(m_punkParent != NULL);

        // try navigating in the current browser window
        // NavFrameWithFile will exit without doing
        // anything if we're not dealing with a .url
        hr = NavFrameWithFile(szShortcutFilePath, m_punkParent);
    }

    // it's not a .url or we can't nav to it for some reason
    // let the general handlers have a shot now

    if (FAILED(hr))
    {
        hr = m_pshuUrl->Execute(m_pbp, &m_fDidShellExec,dwExecFlags);
    }
    return hr;
}




//================================
// *** IAddressBand Interface ***

/****************************************************\
    FUNCTION: FileSysChange

    DESCRIPTION:
        This function will handle file system change
    notifications.
\****************************************************/
HRESULT CAddressEditBox::FileSysChange(DWORD dwEvent, LPCITEMIDLIST *ppidl)
{
    // m_hwnd == NULL means we don't need to do anything
    // however we will probably never get that event
    // if that is the case.

    if (m_palSNS)
        m_palSNS->FileSysChangeAL(dwEvent, ppidl);

    return S_OK;
}


/****************************************************\
    FUNCTION: Refresh

    PARAMETERS:
        pvarType - NULL for a refress of everything.
                   OLECMD_REFRESH_TOPMOST will only update the top most.

    DESCRIPTION:
        This function will force a refress of part
    or all of the AddressBand.
\****************************************************/
HRESULT CAddressEditBox::Refresh(VARIANT * pvarType)
{
    //
    // Refreshing does not automatically refresh the contents of the
    // edit window because a DISPID_DOCUMENTCOMPLETE or DISPID_NAVIGATECOMPLETE2
    // is not sent.  So we restore the contents ourselves.
    //
    if (m_hwndEdit && m_pszCurrentUrl && !IsErrorUrl(m_pszCurrentUrl))
    {
        TCHAR szTemp[MAX_URL_STRING];

        StringCchCopy(szTemp,  ARRAYSIZE(szTemp), m_pszCurrentUrl);
        SendMessage(m_hwndEdit, WM_SETTEXT, (WPARAM)0, (LPARAM)szTemp);
    }

    DWORD dwType = OLECMD_REFRESH_ENTIRELIST; // Default

    if (pvarType)
    {
        if (VT_I4 != pvarType->vt)
            return E_INVALIDARG;

        dwType = pvarType->lVal;
    }

    if (m_hwnd && m_palCurrent && m_pbs)
    {
        if (!m_pszCurrentUrl)
        {
            if (!m_pshuUrl)
            {
                _CreateCShellUrl();
            }

            LPITEMIDLIST pidl;
            if (SUCCEEDED(m_pbs->GetPidl(&pidl)))
            {
                if (SUCCEEDED(m_pshuUrl->SetPidl(pidl)) && m_pshuUrl)
                {
                    TCHAR szDisplayName[MAX_URL_STRING];
                    if (SUCCEEDED(m_pshuUrl->GetUrl(szDisplayName, ARRAYSIZE(szDisplayName))))
                    {
                        Str_SetPtr(&m_pszCurrentUrl, szDisplayName);
                    }
                }
                ILFree(pidl);
            }
        }

        if (m_pszCurrentUrl)
        {
            _UseNewList(PathIsURL(m_pszCurrentUrl) ? LT_TYPEIN_MRU : LT_SHELLNAMESPACE);
            if (m_palCurrent)
            {
                m_palCurrent->Connect(TRUE, m_hwnd, m_pbs, m_pbp, m_pac);
                m_palCurrent->Refresh(dwType);
            }
        }
    }

    return S_OK;
}


//================================
// *** Internal/Private Methods ***

//=================================================================
// General Band Functions
//=================================================================

/****************************************************\

    Address Band Constructor

\****************************************************/
CAddressEditBox::CAddressEditBox()
{
    DllAddRef();
    TraceMsg(TF_SHDLIFE, "ctor CAddressEditBox %x", this);
    m_cRef = 1;

    // This needs to be allocated in Zero Inited Memory.
    // ASSERT that all Member Variables are inited to Zero.
    ASSERT(!m_punkParent);
    ASSERT(!m_hwnd);
    ASSERT(!m_hwndEdit);
    ASSERT(!m_lpfnComboWndProc);

    ASSERT(!m_pbp);
    ASSERT(!m_pbs);
    ASSERT(!m_dwcpCookie);
    ASSERT(!m_pszCurrentUrl);
    ASSERT(!m_pszPendingURL);
    ASSERT(!m_pac);
    ASSERT(!m_pssACLISF);
    ASSERT(!m_palCurrent);
    ASSERT(!m_palSNS);
    ASSERT(!m_palMRU);
    ASSERT(!m_pmru);
    ASSERT(!m_pshuUrl);
    ASSERT(!m_fDidShellExec);
    ASSERT(!m_pszUserEnteredURL);
    ASSERT(!m_fConnectedToBrowser);
    ASSERT(!m_pAsyncNav);
    ASSERT(!m_fAsyncNavInProgress);

    ASSERT(AEB_INIT_DEFAULT == m_dwFlags);

    m_nOldSelection = -1;
    m_elt = LT_NONE;
    m_cbex.mask = 0;
    m_cbex.pszText = 0;
    m_cbex.cchTextMax = 0;

    if (!g_nAEB_AsyncNavigation)
        g_nAEB_AsyncNavigation = RegisterWindowMessage(TEXT("CAEBAsyncNavigation"));

}


/****************************************************\

    Address Band destructor

\****************************************************/
CAddressEditBox::~CAddressEditBox()
{
    _CancelNavigation();

    ATOMICRELEASE(m_punkParent);
    ATOMICRELEASE(m_pac);
    ATOMICRELEASE(m_pssACLISF);
    ATOMICRELEASE(m_palSNS);
    ATOMICRELEASE(m_palMRU);
    ATOMICRELEASE(m_palCurrent);
    ATOMICRELEASE(m_pbp);
    ATOMICRELEASE(m_pbs);
    ATOMICRELEASE(m_pmru);

    if (m_pshuUrl)
    {
        delete m_pshuUrl;
    }

    Str_SetPtr(&m_pszCurrentUrl, NULL);
    Str_SetPtr(&m_pszPendingURL, NULL);
    Str_SetPtr(&m_pszUserEnteredURL, NULL);
    Str_SetPtr(&m_pszHttpErrorUrl, NULL);

    _RemoveHook();
    if (m_fAssociated)
    {
        m_al.Delete(GetCurrentThreadId());
    }

    Str_SetPtr(&m_cbex.pszText, NULL);

    TraceMsg(TF_SHDLIFE, "dtor CAddressEditBox %x", this);
    DllRelease();
}

/****************************************************\
    FUNCTION: CAddressEditBox_CreateInstance

    DESCRIPTION:
        This function will create an instance of the
    AddressBand COM object.
\****************************************************/
HRESULT CAddressEditBox_CreateInstance(IUnknown *punkOuter, IUnknown **ppunk, LPCOBJECTINFO poi)
{
    // aggregation checking is handled in class factory

    *ppunk = NULL;
    CAddressEditBox * p = new CAddressEditBox();
    if (p)
    {
        *ppunk = SAFECAST(p, IAddressBand *);
        return NOERROR;
    }

    return E_OUTOFMEMORY;
}


/****************************************************\
    FUNCTION: _OnNotify

    DESCRIPTION:
        This function will handle WM_NOTIFY messages.
\****************************************************/
LRESULT CAddressEditBox::_OnNotify(LPNMHDR pnm)
{
    // HACKHACK: combobox (comctl32\comboex.c) will pass a LPNMHDR, but it's really
    // a PNMCOMBOBOXEX (which has a first element of LPNMHDR).  This function
    // can use this type cast iff it's guaranteed that this will only come from
    // a function that behaves in this perverse way.
    PNMCOMBOBOXEX pnmce = (PNMCOMBOBOXEX)pnm;

    ASSERT(pnm);
    switch (pnm->code)
    {
    case CBEN_BEGINEDIT:
        _OnBeginEdit(pnm);
        break;

    case CBEN_ENDEDITA:
        _OnEndEditA((LPNMCBEENDEDITA)pnm);
        TraceMsg(TF_BAND|TF_GENERAL, "CAddressEditBox::_OnNotify(), pnm->code=CBEN_ENDEDITA");
        break;

    case CBEN_ENDEDITW:
        _OnEndEditW((LPNMCBEENDEDITW)pnm);
        TraceMsg(TF_BAND|TF_GENERAL, "CAddressEditBox::_OnNotify(), pnm->code=CBEN_ENDEDITW");
        break;

    default:
        break;
    }

    return 0;
}


LRESULT CAddressEditBox::_OnBeginEdit(LPNMHDR pnm)
{
    if (m_punkParent)
        IUnknown_OnFocusChangeIS(m_punkParent, m_punkParent, TRUE);

    return 0;

}

/****************************************************\
    FUNCTION: _OnEndEditW

    DESCRIPTION:
        Thunk to _OnEndEditA.
\****************************************************/

LRESULT CAddressEditBox::_OnEndEditW(LPNMCBEENDEDITW pnmW)
{
    NMCBEENDEDITA nmA;

    nmA.hdr = pnmW->hdr;
    nmA.fChanged = pnmW->fChanged;
    nmA.iNewSelection = pnmW->iNewSelection;
    nmA.iWhy = pnmW->iWhy;

    // don't we lose unicode information on this transition?!
    // We don't use pnmw->szText so don't bother converting it
    // SHUnicodeToAnsi(pnmW->szText, nmA.szText, ARRAYSIZE(nmA.szText));
    nmA.szText[0] = 0;

    return _OnEndEditA(&nmA);
}




/****************************************************\
    FUNCTION: _OnEndEditA

    DESCRIPTION:
        Handle the WM_NOTIFY/CBEN_ENDEDITA message.
\****************************************************/
LRESULT CAddressEditBox::_OnEndEditA(LPNMCBEENDEDITA pnmA)
{
    BOOL fRestoreIcons = TRUE;
    ASSERT(pnmA);

    //
    // Navigate only if the user pressed enter in the edit control.
    //
    ASSERT(m_hwnd);
    switch (pnmA->iWhy)
    {
        case CBENF_RETURN:
            {
                if (g_dwProfileCAP & 0x00000002) {
                    StartCAP();
                }

                // Use szUrl and ignore pnmA->szText because it truncates to MAX_PATH (=256)
                TCHAR szUrl[MAX_URL_STRING];

                if (m_hwndEdit)
                {
                    // Allow the edit text to be updated
                    _ClearDirtyFlag();

                    GetWindowText(m_hwndEdit, szUrl, SIZECHARS(szUrl));
                    Str_SetPtr(&m_pszUserEnteredURL, szUrl);

                    // If edit box is empty, don't show icon
                    if (*szUrl == L'\0')
                    {
                        fRestoreIcons = FALSE;
                    }

#ifndef NO_ETW_TRACING
                    // Event trace for windows enable by shlwapi.
                    if (g_dwStopWatchMode & SPMODE_EVENTTRACE) {
                        EventTraceHandler(EVENT_TRACE_TYPE_BROWSE_ADDRESS,
                                          szUrl);
                    }
#endif
                    if (g_dwStopWatchMode & (SPMODE_BROWSER | SPMODE_JAVA))
                    {
                        DWORD dwTime = GetPerfTime();
                        if (g_dwStopWatchMode & SPMODE_BROWSER)  // Used to get browser total download time
                            StopWatch_StartTimed(SWID_BROWSER_FRAME, TEXT("Browser Frame Same"), SPMODE_BROWSER | SPMODE_DEBUGOUT, dwTime);
                        if (g_dwStopWatchMode & SPMODE_JAVA)  // Used to get java applet load time
                            StopWatch_StartTimed(SWID_JAVA_APP, TEXT("Java Applet Same"), SPMODE_JAVA | SPMODE_DEBUGOUT, dwTime);
                    }

                    // If the WindowText matches the last URL we navigated
                    // to, then we need to call Refresh() instead of _HandleUserAction().
                    // This is because IWebBrowser2::Navigate2() ignores any commands that
                    // point to the same URL that it's already navigated to.
                    if (m_pszCurrentUrl && m_hwnd && !m_fDidShellExec &&
                        m_fConnectedToBrowser && (-1 == pnmA->iNewSelection) &&
                        (0 == lstrcmp(m_pszCurrentUrl, szUrl)))
                    {
                        IUnknown *punk = NULL;

                        // Refresh Browser.
                        if (m_pbp)
                        {
                            m_pbp->GetBrowserWindow(&punk);
                        }
                        if (punk) {
                            IWebBrowser* pwb;
                            punk->QueryInterface(IID_IWebBrowser, (LPVOID*)&pwb);
                            if (pwb) {
                                VARIANT v = {0};
                                v.vt = VT_I4;
                                v.lVal = OLECMDIDF_REFRESH_RELOAD|OLECMDIDF_REFRESH_CLEARUSERINPUT;
                                Refresh(NULL);
                                pwb->Refresh2(&v);
                                pwb->Release();
                            }
                            punk->Release();
                        }
                    }
                    else
                    {
                        SendMessage(m_hwnd, CB_SHOWDROPDOWN, FALSE, 0);
                        _HandleUserAction(szUrl, pnmA->iNewSelection);
                    }
                    UEMFireEvent(&UEMIID_BROWSER, UEME_INSTRBROWSER, UEMF_INSTRUMENT, UIBW_NAVIGATE, UIBL_NAVADDRESS);
                }
            }
            break;
        case CBENF_KILLFOCUS:
            fRestoreIcons = FALSE;
            break;

        case CBENF_ESCAPE:
            // Abort and clear the dirty flag
            _ClearDirtyFlag();
            if (m_hwndEdit && m_pszCurrentUrl && m_cbex.mask != 0)
            {
                SendMessage(m_hwnd, CBEM_SETITEM, (WPARAM)0, (LPARAM)(LPVOID)&m_cbex);
            }

            SendMessage(m_hwnd, CB_SHOWDROPDOWN, FALSE, 0);
            if (pnmA->iNewSelection != -1) {
                SendMessage(m_hwnd, CB_SETCURSEL, pnmA->iNewSelection, 0);
            }
            fRestoreIcons = FALSE;
            break;
    }

    if (fRestoreIcons)
    {
        SendMessage(m_hwnd, CBEM_SETEXTENDEDSTYLE, CBES_EX_NOEDITIMAGE, 0);
    }
    return 0;
}


/****************************************************\
    FUNCTION: _ConnectToBrwsrWnd

    DESCRIPTION:
        The IUnknown parameter needs to point to an
    object that supports the IBrowserService and
    IWebBrowserApp interfaces.
\****************************************************/
HRESULT CAddressEditBox::_ConnectToBrwsrWnd(IUnknown* punk)
{
    HRESULT hr = S_OK;

    if (m_pbs) {
        _ConnectToBrwsrConnectionPoint(FALSE, m_punkParent);
        ATOMICRELEASE(m_pbs);
    }

    if (punk)
    {
        IUnknown * punkHack;

        // HACK: We behave differently if we are hosted outside of a browser
        //       than we do if we are in a browser.  This call does nothing
        //       but identify our host.
        if (SUCCEEDED(IUnknown_QueryService(punk, SID_SShellDesktop, IID_IUnknown, (void**)&punkHack)))
            punkHack->Release();
        else
        {
            // No, we are not hosted on the desktop, so we can synch to the events of the browser.

            hr = IUnknown_QueryService(punk, SID_STopLevelBrowser, IID_IBrowserService, (void**)&m_pbs);
            if (SUCCEEDED(hr))
            {
                // We only want notifications if we are the AddressBar.
                _ConnectToBrwsrConnectionPoint(TRUE, punk);
            }
        }
    }

    // TODO: At some point we will need to implement IPropertyBag so
    //       the parent can specify if they want us to behave as though
    //       we are contected or not.  For now, we will use the fact
    //       that we are either have a IBrowserService pointer or not.
    m_fConnectedToBrowser = BOOLIFY(m_pbs);


    if (!m_pac)
    {
        // We need to wait to create the AutoComplete Lists until m_fConnectedToBrowser is set.
        if (m_hwndEdit)
            hr = SHUseDefaultAutoComplete(m_hwndEdit, NULL, &m_pac, &m_pssACLISF, m_fConnectedToBrowser);

        if (SUCCEEDED(hr))
        {
            _SetAutocompleteOptions();
        }
    }

    //
    // Subclass edit control of the combobox.  We do this here rather than when this
    // class is initialized so that we are first in the chain to receive messages.
    //
    if (!m_lpfnEditWndProc && m_hwndEdit && SetProp(m_hwndEdit, SZ_ADDRESSCOMBO_PROP, this))
    {
        m_lpfnEditWndProc = (WNDPROC)SetWindowLongPtr(m_hwndEdit, GWLP_WNDPROC, (LONG_PTR) _EditSubclassWndProc);
    }

    // This function will be called if: 1) we are becoming connected to a
    // browser, 2) switch from one browser to another, or 3) are
    // becoming unconnected from a browser.  In any case, we need to
    // update the ISF AutoComplete List so it can retrieve
    // the current location from the appropriate browser.
    if (m_pssACLISF)
        m_pssACLISF->SetOwner(m_pbs);

    return hr;
}


/****************************************************\
    FUNCTION: _ConnectToBrwsrConnectionPoint

    DESCRIPTION:
        Connect to Browser Window's ConnectionPoint
    that will provide events to let us keep up to date.
\****************************************************/
HRESULT CAddressEditBox::_ConnectToBrwsrConnectionPoint(BOOL fConnect, IUnknown * punk)
{
    HRESULT hr = S_OK;
    IConnectionPointContainer *pcpContainer;

    if (punk)
    {
        hr = IUnknown_QueryService(punk, SID_SWebBrowserApp, IID_IConnectionPointContainer, (void **)&pcpContainer);
        // Let's now have the Browser Window give us notification when something happens.
        if (SUCCEEDED(hr))
        {
            hr = ConnectToConnectionPoint(SAFECAST(this, IDispatch*), DIID_DWebBrowserEvents2, fConnect,
                                          pcpContainer, &m_dwcpCookie, NULL);
            pcpContainer->Release();
        }
    }

    return hr;
}


/****************************************************\
    FUNCTION: _OnCommand

    DESCRIPTION:
        Handle WM_COMMAND messages.
\****************************************************/
LRESULT CAddressEditBox::_OnCommand(WPARAM wParam, LPARAM lParam)
{
    UINT uCmd = GET_WM_COMMAND_CMD(wParam, lParam);

    switch (uCmd)
    {
        case CBN_EDITCHANGE:
        {
            HWND hwndFocus = GetFocus();
            if ((NULL != hwndFocus) && IsChild(m_hwnd, hwndFocus))
            {
                DWORD dwStyle = _IsDirty() ? CBES_EX_NOEDITIMAGE : 0;
                SendMessage(m_hwnd, CBEM_SETEXTENDEDSTYLE, CBES_EX_NOEDITIMAGE, dwStyle);
            }
            break;
        }

        case CBN_CLOSEUP:
            {
                //
                // Navigate to the selected string when the dropdown is not down.
                //
                int nSel = ComboBox_GetCurSel(m_hwnd);
                if ((m_nOldSelection != SEL_ESCAPE_PRESSED) &&
                    (m_nOldSelection != nSel) && (nSel > -1))
                {
                    _HandleUserAction(NULL, nSel);

                    // RedrawWindow eliminates annoying half-paint that
                    // occurs while navigating from one pidl to a smaller pidl.
                    RedrawWindow(m_hwnd, NULL, NULL, RDW_INTERNALPAINT | RDW_UPDATENOW);
                }
            }

            if (m_pac)
                m_pac->Enable(TRUE);
            break;

        case CBN_DROPDOWN:
            if (m_pac)
                m_pac->Enable(FALSE);
            break;
    }

    return 0;
}

/*******************************************************************
    FUNCTION: _CreateCShellUrl

    DESCRIPTION:
        Create the m_pshuUrl CShellUrl if needed.
********************************************************************/
HRESULT CAddressEditBox::_CreateCShellUrl(void)
{
    HRESULT hr = S_OK;
    // Do we need to create our Shell Url?
    if (!m_pshuUrl)
    {
        // Yes
        m_pshuUrl = new CShellUrl();
        if (!m_pshuUrl)
        {
            return E_FAIL;
        }
        else
        {
            m_pshuUrl->SetMessageBoxParent(m_hwndEdit);

            // We need to set the "Shell Path" which will allow
            // the user to enter Display Names of items in Shell
            // Folders that are frequently used.  We add "Desktop"
            // and "Desktop/My Computer" to the Shell Path because
            // that is what users use most often.
            SetDefaultShellPath(m_pshuUrl);
        }
    }
    return hr;
}



/*******************************************************************
    FUNCTION: _HandleUserAction

    PARAMETERS:
        pszUrl - string of URL to navigate to.
        iNewSelection - index of current selection in address bar combo box

    DESCRIPTION:
        Called when the user types in or selects a URL to navigate
    to through the address bar.
********************************************************************/
HRESULT CAddressEditBox::_HandleUserAction(LPCTSTR pszUrl, int iNewSelection)
{
    HRESULT hr = S_OK;
    TCHAR szDisplayName[MAX_URL_STRING];
    HCURSOR hCursorOld = SetCursor(LoadCursor(NULL, IDC_WAIT));
    static DWORD dwParseFlags = 0xFFFFFFFF;

    Str_SetPtr(&m_pszPendingURL, NULL);  // Clear if one exists.
    Str_SetPtr(&m_pszHttpErrorUrl, NULL);
    hr = _CreateCShellUrl();
    if (FAILED(hr))
        return hr;

    // Are we connected to a Browser Window?
    if (m_pbs)
    {
        // Yes, so let's set our current working directory to the current window.
        LPITEMIDLIST pidl;
        m_pbs->GetPidl(&pidl);

        DEBUG_CODE(TCHAR szDbgBuffer[MAX_PATH];)
        TraceMsg(TF_BAND|TF_GENERAL, "CAddressEditBox::_HandleUserAction(), Current Pidl in TravelLog. PIDL=%s;", Dbg_PidlStr(pidl, szDbgBuffer, SIZECHARS(szDbgBuffer)));

        if (pidl)
        {
            // m_pshuUrl will free pshuCurrWorkDir, so we can't.
            hr = m_pshuUrl->SetCurrentWorkingDir(pidl);
            ILFree(pidl);
        }
    }

    if (SUCCEEDED(hr))
    {
        // Cancel previous pending nav if any
        _CancelNavigation();

        // Did the user select the item from the drop down list?
        if (-1 != iNewSelection)
        {
            // Yes, so point our CShellUrl at the item. (Pidl or URL)
            if (m_palCurrent)
                m_palCurrent->SetToListIndex(iNewSelection, (LPVOID) m_pshuUrl);

            // if the index indicates this was a selection from the combo box,
            // remember which selection it was
            SendMessage(m_hwnd, CB_SETCURSEL, (WPARAM)iNewSelection, 0L);

            *szDisplayName = L'\0';
            GetWindowText(m_hwnd, szDisplayName, ARRAYSIZE(szDisplayName));
            Str_SetPtr(&m_pszUserEnteredURL, szDisplayName);
            pszUrl = NULL;
        }
        else
        {
            // No, the user hit return with some string.
            ASSERT(pszUrl); // must have valid URL

            if (0xFFFFFFFF == dwParseFlags)
            {
                dwParseFlags = SHURL_FLAGS_NONE;
                if (m_fConnectedToBrowser && !SHRegGetBoolUSValue(TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Band\\Address"), TEXT("Use Path"), FALSE, FALSE))
                    dwParseFlags = SHURL_FLAGS_NOPATHSEARCH;

                if (SHRegGetBoolUSValue(TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Band\\Address"), TEXT("AutoCorrect"), FALSE, /*default*/TRUE))
                    dwParseFlags |= SHURL_FLAGS_AUTOCORRECT;
            }
        }

        hr = E_FAIL;
        if (m_hwnd && m_pshuUrl)
        {
            if (!(m_dwFlags & AEB_INIT_NOASYNC))    // Is async navigate enabled?
            {
                // Create and initialize the AsyncNav object used to communicate with the thread
                m_pAsyncNav = new AsyncNav();
                if (m_pAsyncNav)
                {
                    if(m_punkParent)
                    {
                        // Get the globe spinning indicating our processing
                        hr = IUnknown_QueryServiceExec(m_punkParent, SID_SBrandBand, &CGID_BrandCmdGroup, CBRANDIDM_STARTGLOBEANIMATION, 0, NULL, NULL);
                    }

                    m_pAsyncNav->_dwParseFlags = dwParseFlags;
                    m_pAsyncNav->_hwnd = m_hwnd;
 
                    if (pszUrl)
                        Str_SetPtr(&(m_pAsyncNav->_pszUrl), pszUrl);
                    else
                        m_pAsyncNav->_fPidlCheckOnly = TRUE;

                
                    if (!pszUrl || (pszUrl && m_pAsyncNav->_pszUrl))
                    {
                        CShellUrl *pshu = new CShellUrl();

                        if (pshu)
                        {
                            hr = pshu->Clone(m_pshuUrl);
                            m_pAsyncNav->_pShellUrl = pshu;

                            // AddRef here to give it to the thread
                            m_pAsyncNav->AddRef();

                            // Create the thread that will do the PIDL creation
                            if (FAILED(hr) || !SHCreateThread(_AsyncNavigateThreadProc, (LPVOID)m_pAsyncNav, CTF_COINIT, NULL))
                            {
                                hr = E_FAIL;
                            }
                            else
                            {

                                hr = E_PENDING;
                            }
                        }
                    }
                }
            }

            if (FAILED(hr) && hr != E_PENDING)
            {
                // Cancel Async navigation leftovers
                _CancelNavigation();

                if (pszUrl)
                {
                    BOOL fWasCorrected = FALSE;
                    hr = m_pshuUrl->ParseFromOutsideSource(pszUrl, dwParseFlags, &fWasCorrected);

                    // If the URL was autocorrected, put the corrected url in the editbox
                    // so that an invalid url in not added to our MRU if navigation succeeds
                    if (SUCCEEDED(hr) && fWasCorrected)
                    {
                        if (SUCCEEDED(m_pshuUrl->GetUrl(szDisplayName, ARRAYSIZE(szDisplayName))))
                        {
                            SetWindowText(m_hwndEdit, szDisplayName);
                        }
                    }
                }
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        _FinishNavigate();
    }

    SetCursor(hCursorOld);
    return hr;
}

HRESULT CAddressEditBox::_FinishNavigate()
{
    HRESULT hr;

    hr = Execute( (m_fConnectedToBrowser ? SHURL_EXECFLAGS_NONE : SHURL_EXECFLAGS_DONTFORCEIE));
    // if we managed to navigate by one means or another, then do all the
    // associated processing
    if (SUCCEEDED(hr))
    {
        TCHAR szDisplayName[MAX_URL_STRING];
        hr = m_pshuUrl->GetDisplayName(szDisplayName, SIZECHARS(szDisplayName));
        ASSERT(SUCCEEDED(hr));

        Str_SetPtr(&m_pszPendingURL, szDisplayName);
        if (!m_fConnectedToBrowser || m_fDidShellExec)
        {
            // We aren't connected to a browser window
            // so we need to call _NavigationComplete() our selves
            // because it will not come from the Browser window
            // it self.

            // If m_fDidShellExec, we need to manually add this because
            // we won't receive a DISPID_NAVIGATECOMPLETE event, but
            // we pass NULL to indicate

            hr = _NavigationComplete(szDisplayName, !m_fDidShellExec, TRUE);
        }
    }
    return hr;
}


void CAddressEditBox::_JustifyAddressBarText( void )
{
    // Either of the following appear to work:
    //	(a) EM_SETSEL(0,0) followed by EM_SCROLLCARET(0,0)
    //		SendMessage( m_hwndEdit, EM_SETSEL, 0, 0 );
    //		SendMessage( m_hwndEdit, EM_SCROLLCARET, 0, 0 );
    //	(b) WM_KEYDOWN with VK_HOME
    //		SendMessage( m_hwndEdit, WM_KEYDOWN, VK_HOME, 0 );

    // Use the EM_SETSEL method to avoid user keyboard stroke interruption.
    SendMessage( m_hwndEdit, EM_SETSEL, 0, 0 );
    SendMessage( m_hwndEdit, EM_SCROLLCARET, 0, 0 );
}


HRESULT CAddressEditBox::_AsyncNavigate(AsyncNav *pAsyncNav)
{
    HRESULT hr;

    // we should only be called on one thread, but the interlocked can't hurt...
    if (InterlockedCompareExchange((LONG*)&m_fAsyncNavInProgress, TRUE, FALSE) == FALSE)
    {
        // this is the first call to _AsyncNavigate
        hr = pAsyncNav->_hr;

        if (SUCCEEDED(hr))
        {
            // Get the CShellUrl back after processing
            hr = m_pshuUrl->Clone(pAsyncNav->_pShellUrl);
        }

        // If the URL was autocorrected, put the corrected url in the editbox
        // so that an invalid url in not added to our MRU if navigation succeeds
        if (SUCCEEDED(hr) && pAsyncNav->_fWasCorrected)
        {
            TCHAR szDisplayName[MAX_URL_STRING];
            if (SUCCEEDED(m_pshuUrl->GetUrl(szDisplayName, ARRAYSIZE(szDisplayName))))
            {
                SetWindowText(m_hwndEdit, szDisplayName);
            }
        }

        if (SUCCEEDED(hr))
            hr = _FinishNavigate();

        if (FAILED(hr) && pAsyncNav->_fPidlCheckOnly)
        {
            // Maybe the user needs to insert the media, format, or
            // reconnect to the disk before this will succeed.  Check for that
            // and prompt now.
            // This fixes the common case where the floppy or CD isn't inserted and
            // we want to display the user friendly dialog.

            LPITEMIDLIST pidl;

            hr = pAsyncNav->_pShellUrl->GetPidlNoGenerate(&pidl);

            // We need to resolve the URL into its path so SHPathPrepareForWrite works correctly
            if (SUCCEEDED(hr))
            {
                TCHAR szShortcutFilePath[MAX_PATH];
                hr = IEGetNameAndFlags(pidl, SHGDN_FORPARSING, szShortcutFilePath, SIZECHARS(szShortcutFilePath), NULL);

                if (SUCCEEDED(hr))
                {
                    HRESULT hrPrompt = SHPathPrepareForWrite(pAsyncNav->_hwnd, NULL, szShortcutFilePath, SHPPFW_DEFAULT);
                    if (SUCCEEDED(hrPrompt))
                    {
                        hr = _FinishNavigate();
                    }
                    else
                    {
                        // Propagate out the fact that the user clicked the cancel button.
                        hr = hrPrompt;
                    }
                }
            
                ILFree(pidl);
            }

            // Never display a err if the user cancelled the operation.
            if (FAILED(hr) && (HRESULT_FROM_WIN32(ERROR_CANCELLED) != hr))
            {
                TCHAR szDisplayName[MAX_URL_STRING];
                if (SUCCEEDED(pAsyncNav->_pShellUrl->GetUrl(szDisplayName, ARRAYSIZE(szDisplayName))))
                {
                    MLShellMessageBox(pAsyncNav->_hwnd, MAKEINTRESOURCE(IDS_ADDRBAND_DEVICE_NOTAVAILABLE),
                        MAKEINTRESOURCE(IDS_SHURL_ERR_TITLE),
                        (MB_OK | MB_ICONERROR), szDisplayName);
                }
            }
        }

        // Cleanup async navigation stuff
        _CancelNavigation();

        InterlockedExchange((LONG*)&m_fAsyncNavInProgress, FALSE);
    }
    else
    {
        // we can only do one async navigate at a time
        hr = E_FAIL;
    }

    return hr;
}

HRESULT CAddressEditBox::_CancelNavigation()
{
    if (m_pAsyncNav)
    {
        if(m_punkParent)
        {
            HRESULT hr = IUnknown_QueryServiceExec(m_punkParent, SID_SBrandBand, &CGID_BrandCmdGroup, CBRANDIDM_STOPGLOBEANIMATION, 0, NULL, NULL);
        }

        m_pAsyncNav->SetCanceledFlag();
        m_pAsyncNav->Release();
        m_pAsyncNav = NULL;
    }

    return S_OK;
}

DWORD CAddressEditBox::_AsyncNavigateThreadProc(LPVOID pvData)
{
    AsyncNav *pAsyncNav = (AsyncNav *)pvData;

    if (pAsyncNav->_hwnd && g_nAEB_AsyncNavigation)
    {
        if(pAsyncNav->_fPidlCheckOnly)
        {
            LPITEMIDLIST pidl;

            pAsyncNav->_hr = pAsyncNav->_pShellUrl->GetPidlNoGenerate(&pidl);
            if (SUCCEEDED(pAsyncNav->_hr))
            {
                DWORD dwAttrib = SFGAO_VALIDATE;
                pAsyncNav->_hr = IEGetNameAndFlags(pidl, 0, NULL, 0, &dwAttrib);
            }
            else
            {
                // Special case for keywords. We want to proceed if we don't have a pidl
                pAsyncNav->_hr = S_OK;
            }
        }
        else
        {
            pAsyncNav->_hr = pAsyncNav->_pShellUrl->ParseFromOutsideSource(pAsyncNav->_pszUrl, pAsyncNav->_dwParseFlags, &(pAsyncNav->_fWasCorrected), &(pAsyncNav->_fWasCanceled));
        }
        pAsyncNav->_fReady = TRUE;
        PostMessage(pAsyncNav->_hwnd, g_nAEB_AsyncNavigation, (WPARAM)pAsyncNav, NULL);
    }

    // We are done with this now.
    // If the navigation was canceled, then the object will destruct now, and the posted
    // message above will be ignored.
    pAsyncNav->Release();


    return 0;
}


BOOL CAddressEditBox::_IsShellUrl(void)
{
    // 1. Check if we need to change the List.
    BOOL fIsShellUrl = !m_pshuUrl->IsWebUrl();

    if (fIsShellUrl)
    {
        // BUG #50703: Users want MRU when about: url is displayed.
        TCHAR szUrl[MAX_URL_STRING];

        if (SUCCEEDED(m_pshuUrl->GetUrl(szUrl, ARRAYSIZE(szUrl))))
        {
            if (URL_SCHEME_ABOUT == GetUrlScheme(szUrl))
            {
                fIsShellUrl = FALSE;  // Make it use the MRU List.
            }
        }
    }

    return fIsShellUrl;
}


/*******************************************************************
    FUNCTION: _NavigationComplete

    PARAMETERS:
        pszUrl - String user entered.
        fChangeLists - Should we modify the Drop Down list?
        fAddToMRU - Should we add it to the MRU?

    DESCRIPTION:
        This function is called when either: 1) a naviation completes,
    or 2) the user entered text into the AddressEditBox that needs
    to be handled but will not cause a NAVIGATION_COMPLETE message.
    This function will change the AddressList being used and will
    add the item to the Type-in MRU.
********************************************************************/
HRESULT CAddressEditBox::_NavigationComplete(LPCTSTR pszUrl /* Optional */, BOOL fChangeLists, BOOL fAddToMRU)
{
    HRESULT hr = S_OK;

    // Are we controlling a ComboBoxEx?
    if (m_hwnd)
    {
        // Yes, so do ComboBoxEx Specific things...

        // If the list is dropped, undrop it so the contents of the editbox and list
        // are properly updated.
        if (m_hwnd  && m_hwndEdit && ComboBox_GetDroppedState(m_hwnd))
        {
            SendMessage(m_hwndEdit, WM_KEYDOWN, VK_ESCAPE, 0);
        }

        if (fChangeLists)
        {
            BOOL fIsShellUrl = _IsShellUrl();

            // 2. Do we need to change lists to MRU List?
            if (!fIsShellUrl && m_elt != LT_TYPEIN_MRU)
            {
                // We need to start using the LT_TYPEIN_MRU list
                // because that list is what is needed for Internet Urls.
                _UseNewList(LT_TYPEIN_MRU);
            }

            // We only want to switch to using the shell name space
            // if we are connected to a browser.
            if (fIsShellUrl && (m_elt != LT_SHELLNAMESPACE) && m_fConnectedToBrowser)
            {
                // We need to start using the LT_SHELLNAMESPACE list
                // because that list is what is needed for File Urls.
                _UseNewList(LT_SHELLNAMESPACE);
            }

            ASSERT(m_palCurrent);
            hr = m_palCurrent ? m_palCurrent->NavigationComplete((LPVOID) m_pshuUrl) : E_FAIL;
            if ( SUCCEEDED( hr ) )
            {
                // Insure that after the navigation completes, the Address Bar Text is left justified.
                _JustifyAddressBarText();
            }
        }

        // Don't display the url to internal error pages. All internal error
        // urls start with res:// and we don't want these in our MRU.
        // We also don't want to display error pages from the server.
        if ((pszUrl && (TEXT('r') == pszUrl[0]) && (TEXT('e') == pszUrl[1]) && IsErrorUrl(pszUrl)) ||
            (m_pszHttpErrorUrl && StrCmp(m_pszHttpErrorUrl, pszUrl) == 0))
        {
            // We don't want this in our MRU!
            fAddToMRU = FALSE;
        }

        // Do we have a Pending URL, meaning the user hand typed it in
        // and the navigation finished (wasn't cancelled or failed).
        //
        // REARCHITECT: Currently there are a few cases when the URL (m_pszPendingURL)
        //         is added to the MRU when it shouldn't.
        // 1. If the user enters an URL and then cancels the navigation, we
        //    don't clear m_pszPendingURL.  If the user then causes the browser
        //    to navigate by some other means (HREF Click, Favorites/QLink navigation
        //    , or Floating AddressBand), we will receive the NAVIGATION_COMPLETE
        //    message and think it was for the originally cancelled URL.

        if (fAddToMRU && m_pszPendingURL)
        {
            // Yes, so add it to the MRU.
            if (SUCCEEDED(hr))
            {
                if (!m_pmru && m_palMRU)
                    hr = m_palMRU->QueryInterface(IID_IMRU, (LPVOID *)&m_pmru);

                if (SUCCEEDED(hr))
                {
                    SHCleanupUrlForDisplay(m_pszPendingURL);
                    hr = m_pmru->AddEntry(m_pszPendingURL); // Add to MRU
                }
            }
        }
    }

    Str_SetPtr(&m_pszPendingURL, NULL);
    Str_SetPtr(&m_pszHttpErrorUrl, NULL);

    return hr;
}

//=================================================================
// AddressEditBox Modification Functions
//=================================================================

/****************************************************\
    _ComboSubclassWndProc

    Input:
        Standard WndProc parameters

    Return:
        Standard WndProc return.
\****************************************************/
LRESULT CALLBACK CAddressEditBox::_ComboSubclassWndProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    HWND hwndBand = GetParent(hwnd);
    CAddressEditBox * paeb = (CAddressEditBox*)GetProp(hwnd, SZ_ADDRESSCOMBO_PROP);

    ASSERT(paeb);
    g_hWinStationAfter = GetProcessWindowStation();

    // In stress we see someone will stomp our property with -2.  We need to find out who it is.
    // Call ReinerF if this happens
    AssertMsg(((void *)-2 != paeb), TEXT("Someone corrupted our window property.  Call ReinerF"));
    if (!paeb)
    {
        return DefWindowProcWrap(hwnd, uMessage, wParam, lParam);
    }

    switch (uMessage)
    {
    case WM_SETCURSOR:
        {
            HWND hwndCursor = (HWND)wParam;
            int nHittest = LOWORD(lParam);
            if (hwndCursor == paeb->m_hwndEdit && nHittest == HTCLIENT)
            {
                //
                // If we don't have focus, we want to show an arrow because clicking will select
                // the contents of the edit box.  Otherwise show the I-beam.  Also, if the edit box
                // is empty we show the I-beam because there is nothing to select.
                //
                HWND hwndFocus = GetFocus();
                int cch = GetWindowTextLength(paeb->m_hwndEdit);

                LPCTSTR lpCursorName = (cch == 0 || hwndFocus == paeb->m_hwndEdit) ? IDC_IBEAM : IDC_ARROW;
                SetCursor(LoadCursor(NULL, lpCursorName));
                return TRUE;
            }
            break;
        }
    case WM_SETFOCUS:
        //
        // This is gross, but if the window was destroyed that had the
        // focus this would fail and we would not get this to the
        // combo box.
        //
        // This happens if you click on the combobox while
        // renaming a file in the defview.
        //
        if (wParam && !IsWindow((HWND)wParam))
            wParam = 0;
        break;

    case WM_DESTROY:
        // Unsubclass myself.
        if (!paeb->m_lpfnComboWndProc)
            return 0;
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) paeb->m_lpfnComboWndProc);
        RemoveProp(hwnd, SZ_ADDRESSCOMBO_PROP);

        ASSERT(paeb->m_hwnd); // We don't want to be called twice
        paeb->m_hwnd = NULL;      // We have been destroyed.
        break;

    case WM_COMMAND:
        if (EN_UPDATE == GET_WM_COMMAND_CMD(wParam, lParam))
        {
            paeb->_InstallHookIfDirty();
        }
        break;
    case WM_KEYDOWN:
            switch (wParam)
            {
                //
                // Pressing escape results in the dropdown being hidden.  If
                // the mouse hot-tracks over a different selection than when the
                // combo was first dropped, we get a CBN_SELCHANGE event which
                // causes a false navigation.  We suppress this by setting
                // m_nOldSelection to a special value (-2).
                //
                case VK_ESCAPE:
                {
                    paeb->m_nOldSelection = SEL_ESCAPE_PRESSED;

                    // Pass message on so that the content of the edit box is restored
                    SendMessage(paeb->m_hwndEdit, uMessage, wParam, lParam);
                    break;
                }
            }
            break;
    case WM_SYSKEYDOWN:
            switch (wParam)
            {
                case VK_DOWN:
                {
                    // Alt-down toggles the combobox dropdown.  We don't
                    // want a navigation if this key sequence closes the dropdown.
                    if (HIWORD(lParam) & KF_ALTDOWN)
                    {
                        paeb->m_nOldSelection = SEL_ESCAPE_PRESSED;
                    }
                    break;
                }
            }
            break;
    case CB_SHOWDROPDOWN:
            // If dropdown is hidden, suppress navigation. See comment above for VK_ESCAPE.
            if (!wParam)
            {
                paeb->m_nOldSelection = SEL_ESCAPE_PRESSED;
            }
            break;

    case WM_WINDOWPOSCHANGING:
        {
            LPWINDOWPOS pwp = (LPWINDOWPOS)lParam;
            pwp->flags |= SWP_NOCOPYBITS;
        }
        break;

    case WM_GETOBJECT:
        if ((DWORD)lParam == OBJID_CLIENT)
        {
            CAddressEditAccessible *paea = new CAddressEditAccessible(hwnd, paeb->m_hwndEdit);

            if (NULL != paea)
            {
                LRESULT lres = LresultFromObject(IID_IAccessible, wParam, SAFECAST(paea, IAccessible *));
                paea->Release();

                return lres;
            }
        }
        break;


    default:
        // FEATURE: Do we need this?
        if (!(AEB_INIT_SUBCLASS & paeb->m_dwFlags))
        {
            paeb->OnWinEvent(paeb->m_hwnd, uMessage, wParam, lParam, NULL);
        }
        break;
    }

    return CallWindowProc(paeb->m_lpfnComboWndProc, hwnd, uMessage, wParam, lParam);
}

void CAddressEditBox::_SetAutocompleteOptions()
{
    if (m_pac)
    {
        // Set the autocomplete options
        DWORD dwOptions = ACO_SEARCH | ACO_FILTERPREFIXES | ACO_USETAB | ACO_UPDOWNKEYDROPSLIST;
        if (SHRegGetBoolUSValue(REGSTR_PATH_AUTOCOMPLETE, REGSTR_VAL_USEAUTOAPPEND, FALSE, /*default:*/FALSE))
        {
            dwOptions |= ACO_AUTOAPPEND;
        }

        if (SHRegGetBoolUSValue(REGSTR_PATH_AUTOCOMPLETE, REGSTR_VAL_USEAUTOSUGGEST, FALSE, /*default:*/TRUE))
        {
            dwOptions |= ACO_AUTOSUGGEST;
        }

        m_pac->SetOptions(dwOptions);
    }
}

/****************************************************\

    FUNCTION: _NavigateToUrlCB

    PARAMETERS:
        lParam - The CAddressEditBox this pointer.
        lpUrl - The URL to navigate to.

    DESCRIPTION:
        This function is specifically for AutoComplete
        to call when it needs to navigate.
\****************************************************/

HRESULT CAddressEditBox::_NavigateToUrlCB(LPARAM lParam, LPTSTR lpUrl)
{
//  NOTE: We don't need to navigate because AutoComplete will
//  will send a message to the ComboBoxEx that will carry out
//  the navigation.
    return S_OK;
}


//=================================================================
// Functions to prevent clobbering the address contents while dirty
//=================================================================
#define TF_EDITBOX TF_BAND|TF_GENERAL
//#define TF_EDITBOX TF_ALWAYS

BOOL CAddressEditBox::_IsDirty()
{
    return m_hwndEdit && SendMessage(m_hwndEdit, EM_GETMODIFY, 0, 0L);
}

void CAddressEditBox::_ClearDirtyFlag()
{
    TraceMsg(TF_EDITBOX, "CAddressEditBox::_ClearDirtyFlag()");
    SendMessage(m_hwndEdit, EM_SETMODIFY, FALSE, 0);
    _RemoveHook();
}

void CAddressEditBox::_InstallHookIfDirty()
{
    //
    // We only need to install the hook if we are connected to a browser for update notifications
    //
    if (m_fConnectedToBrowser)
    {
        // Make sure we are associated with the current thread
        if (!m_fAssociated)
        {
            //
            // If a CAddressEditBox is already associated with this thread, remove that
            // association and remove any pending mouse hook.  This can happen if the
            // open dialog comes up and the address bar is visible.
            //
            DWORD dwThread = GetCurrentThreadId();
            CAddressEditBox* pAeb;
            if (SUCCEEDED(m_al.Find(dwThread, (LPVOID*)&pAeb)))
            {
                pAeb->_ClearDirtyFlag();
                pAeb->m_fAssociated = FALSE;
                m_al.Delete(dwThread);
            }

            // There should not be any other CAddressEditBox associated with this thread!
            ASSERT(FAILED(m_al.Find(dwThread, (LPVOID*)&pAeb)));

            //
            // Associate ourselves with the current thread id.  We need this because
            // windows hooks are global and have no data associated with them.
            // On the callback, we use our thread id as the key.
            //
            m_al.Add(dwThread, this);
            m_fAssociated = TRUE;
        }

        if (!m_hhook && _IsDirty())
        {
            // ML: HINST_THISDLL is valid in its use here
            m_hhook = SetWindowsHookEx(WH_MOUSE, _MsgHook, HINST_THISDLL, GetCurrentThreadId());
            TraceMsg(TF_EDITBOX, "CAddressEditBox::_InstallHookIfDirty(), Hook installed");

            //
            // Subclass edit control of the combobox.  We do this here rather than when this
            // class is initialized so that we are first in the chain to receive messages.
            //
            if (!m_lpfnEditWndProc && m_hwndEdit && SetProp(m_hwndEdit, SZ_ADDRESSCOMBO_PROP, this))
            {
                m_lpfnEditWndProc = (WNDPROC)SetWindowLongPtr(m_hwndEdit, GWLP_WNDPROC, (LONG_PTR) _EditSubclassWndProc);
            }

            // Clear and changes that we previously cached
            m_cbex.mask = 0;
        }
    }
}

void CAddressEditBox::_RemoveHook()
{
    if (m_hhook)
    {
        UnhookWindowsHookEx(m_hhook);
        m_hhook = FALSE;
        TraceMsg(TF_EDITBOX, "CAddressEditBox::_RemoveHook(), Hook removed");
    }
}

LRESULT CALLBACK CAddressEditBox::_MsgHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    //
    // Get the CAddressEditBox associated with this thread. We need this because
    // windows hooks are global and have no data associated with them.
    // On the callback, we use our thread id as the key
    //
    CAddressEditBox* pThis;
    if (SUCCEEDED(CAddressEditBox::m_al.Find(GetCurrentThreadId(), (LPVOID*)&pThis)))
    {
        return pThis->_MsgHook(nCode, wParam, (MOUSEHOOKSTRUCT*)lParam);
    }
    return 0;
}

LRESULT CAddressEditBox::_MsgHook(int nCode, WPARAM wParam, MOUSEHOOKSTRUCT *pmhs)
{
    ASSERT(NULL != pmhs);

    if (nCode >= 0)
    {
        if ((wParam == WM_LBUTTONDOWN) || (wParam == WM_RBUTTONDOWN))
        {
            // Ignore if the button was clicked in our combo box
            RECT rc;
            if (GetWindowRect(m_hwnd, &rc) && !PtInRect(&rc, pmhs->pt))
            {
                _ClearDirtyFlag();
                _RemoveHook();
            }
        }
    }

    return CallNextHookEx(m_hhook, nCode, wParam, (LPARAM)pmhs);
}

/****************************************************\
    _ComboExSubclassWndProc

    Input:
        Standard WndProc parameters

    Return:
        Standard WndProc return.

    Description:
        We subclass the outer combobox to prevent
        the contents from getting clobbered while
        and edit is in progress (ie dirty).

\****************************************************/
LRESULT CALLBACK CAddressEditBox::_ComboExSubclassWndProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    CAddressEditBox * paeb = (CAddressEditBox*)GetProp(hwnd, SZ_ADDRESSCOMBOEX_PROP);

    if (!paeb)
        return DefWindowProc(hwnd, uMessage, wParam, lParam);

    g_hWinStationAfterEx = GetProcessWindowStation();

    if (uMessage == g_nAEB_AsyncNavigation)
    {
        // If the navigation was not canceled before, then navigate now.
        if ((AsyncNav *)wParam == paeb->m_pAsyncNav && paeb->m_pAsyncNav->_fReady)
        {
            paeb->_AsyncNavigate((AsyncNav *)wParam);
        }
    }

    switch (uMessage)
    {
    case CBEM_SETITEM:
        {
            //
            // If we are still dirty, don't let anyone clobber our edit control contents!
            //
            const COMBOBOXEXITEM* pcCBItem = (const COMBOBOXEXITEM FAR *)lParam;
            if (paeb->_IsDirty() && pcCBItem->iItem == -1)
            {
                //
                // save this info so that if the user hits esc, we restore the right thing
                //
                if (IsFlagSet(pcCBItem->mask, CBEIF_TEXT))
                {
                    Str_SetPtr(&paeb->m_pszCurrentUrl, pcCBItem->pszText);
                }

                Str_SetPtr(&(paeb->m_cbex.pszText), NULL);      // Free the previous value
                paeb->m_cbex = *pcCBItem;
                paeb->m_cbex.pszText = NULL;
                Str_SetPtr(&(paeb->m_cbex.pszText), paeb->m_pszCurrentUrl);
                paeb->m_cbex.cchTextMax = lstrlen(paeb->m_cbex.pszText);
                return 0L;
            }
            else
            {
                // Make sure that the icon is visible
                SendMessage(paeb->m_hwnd, CBEM_SETEXTENDEDSTYLE, CBES_EX_NOEDITIMAGE, 0);
            }
        }
        break;

    case WM_DESTROY:
        // Release the lists now so that they don't try to use our
        // window after we're destroyed
        if (paeb->m_palCurrent)
        {
            paeb->m_palCurrent->Connect(FALSE, paeb->m_hwnd, NULL, NULL, NULL);
            ATOMICRELEASE(paeb->m_palCurrent);
        }
        ATOMICRELEASE(paeb->m_palSNS);
        ATOMICRELEASE(paeb->m_palMRU);

        //
        // Unsubclass myself.
        //
        RemoveProp(hwnd, SZ_ADDRESSCOMBOEX_PROP);
        if (!paeb->m_lpfnComboExWndProc)
            return 0;
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) paeb->m_lpfnComboExWndProc);
        break;

    default:
        break;
    }

    return CallWindowProc(paeb->m_lpfnComboExWndProc, hwnd, uMessage, wParam, lParam);
}

/****************************************************\
    _EnumFindWindow

    Description:
        Called by EnumChildWindows to see is the window
        passed in lParam is a child of a given
        parent.

\****************************************************/
BOOL CALLBACK CAddressEditBox::_EnumFindWindow
(
    HWND hwnd,      // handle to child window
    LPARAM lParam   // application-defined value
)
{
    // Stop enumeration when match found
    return (hwnd != (HWND)lParam);
}

/****************************************************\
    _EditSubclassWndProc

    Input:
        Standard WndProc parameters

    Return:
        Standard WndProc return.

    Description:
        We subclass the edit control in the combobox
        so that we can keep it from losing focus under
        certain conditions.

\****************************************************/
LRESULT CALLBACK CAddressEditBox::_EditSubclassWndProc(HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    CAddressEditBox * paeb = (CAddressEditBox*)GetProp(hwnd, SZ_ADDRESSCOMBO_PROP);

    if (!paeb)
        return DefWindowProc(hwnd, uMessage, wParam, lParam);

    switch (uMessage)
    {
    case WM_SETCURSOR:
        {
            HWND hwndCursor = (HWND)wParam;
            int nHittest = LOWORD(lParam);
            if (hwndCursor == hwnd && nHittest == HTCLIENT)
            {
                //
                // If we don't have focus, we want to show an arrow because clicking will select
                // the contents of the edit box.  Otherwise show the I-beam.  Also, if the edit box
                // is empty we show the I-beam because there is nothing to select.
                //
                int cch = GetWindowTextLength(paeb->m_hwndEdit);
                LPCTSTR lpCursorName = (cch == 0 || GetFocus() == hwnd) ? IDC_IBEAM : IDC_ARROW;
                SetCursor(LoadCursor(NULL, lpCursorName));
                return TRUE;
            }
            break;
        }
    case WM_KILLFOCUS:
        {
            //
            // If we lose focus with the mouse hook installed, the user probably did
            // not initiate the change so we try to grab it back.  The hook is removed
            // when the user clicks outside the edit box or presses a key to finish the edit
            // (tab, enter, or esc)
            //
            HWND hwndGetFocus = (HWND)wParam;

            if ((paeb->m_hhook) && hwndGetFocus && (hwnd != hwndGetFocus))
            {
                //
                // Make sure that this is not the drop-down portion of the combo.
                // Also, if we are in a dialog (open dialog) then we don't see the
                // tab key.  So if focus is going to a sibling we'll let it through.
                //
                HWND hwndGetFocusParent = GetParent(hwndGetFocus);
                HWND hwndSiblingParent = paeb->m_hwnd ? GetParent(paeb->m_hwnd) : GetParent(hwnd);
                if ((paeb->m_hwnd != hwndGetFocusParent) && (hwndGetFocusParent != hwndSiblingParent) &&
                     EnumChildWindows(hwndSiblingParent, _EnumFindWindow, (LPARAM)hwndGetFocus))
                {
                    // Get the top-level window of who's getting focus
                    HWND hwndFrame = hwndGetFocus;
                    HWND hwndParent;
                    while (hwndParent = GetParent(hwndFrame))
                        hwndFrame = hwndParent;

                    // If focus is going somewhere else in our browser window, grab focus back
                    if (hwndFrame == paeb->m_hwndBrowser)
                    {
                        DWORD dwStart, dwEnd;
                        SendMessage(paeb->m_hwndEdit, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);
                        SetFocus(paeb->m_hwndEdit);
                        SendMessage(paeb->m_hwndEdit, EM_SETSEL, dwStart, dwEnd);
                        TraceMsg(TF_BAND|TF_GENERAL, "CAddressEditBox::_EditSubclassWndProc, Restoring focus");
                        return 0L;
                    }
                }
            }

            //
            // Losing focus so allow others to change our contents
            //
            paeb->_ClearDirtyFlag();
        }
        break;

    case WM_KEYDOWN:
        {
            // If we are tabbing away, clear our dirty flag
            switch (wParam)
            {
                case VK_TAB:
                    paeb->_ClearDirtyFlag();
                    break;

                case VK_ESCAPE:
                {
                    if (paeb->m_hwnd && ComboBox_GetDroppedState(paeb->m_hwnd))
                    {
                        SendMessage(paeb->m_hwnd, CB_SHOWDROPDOWN, FALSE, 0);
                    }
                    else
                    {
                        IUnknown *punk = NULL;

                        if (paeb->m_pbp)
                        {
                            paeb->m_pbp->GetBrowserWindow(&punk);
                        }

                        if (punk)
                        {
                            IWebBrowser* pwb;
                            punk->QueryInterface(IID_IWebBrowser, (LPVOID*)&pwb);

                            if (pwb)
                            {
                                pwb->Stop();
                                pwb->Release();
                            }
                            punk->Release();
                        }

                        // Cancel pending navigation, if any
                        paeb->_CancelNavigation();
                    }

                    LRESULT lResult = CallWindowProc(paeb->m_lpfnEditWndProc, hwnd, uMessage, wParam, lParam);

                    // This bit of magic that restores the icon in the combobox.  Otherwise when we
                    // dismiss the dropwown with escape we get the icon last selected in the dropdown.
                    HWND hwndCombo = (HWND)SendMessage(paeb->m_hwnd, CBEM_GETCOMBOCONTROL, 0, 0);
                    SendMessage(hwndCombo, CB_SETCURSEL, -1, 0);
                    return lResult;
                }
            }

            break;
        }
    case WM_DESTROY:
        // Unsubclass myself.
        RemoveProp(hwnd, SZ_ADDRESSCOMBO_PROP);
        if (!paeb->m_lpfnEditWndProc)
            return 0;
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) paeb->m_lpfnEditWndProc);
        ASSERT(paeb->m_hwndEdit);
        paeb->m_hwndEdit = NULL;
        break;
    default:
        break;
    }

    return CallWindowProc(paeb->m_lpfnEditWndProc, hwnd, uMessage, wParam, lParam);
}

BOOL GetLabelStringW(HWND hwnd, LPWSTR pwszBuf, DWORD cchBuf)
{
    HWND    hwndLabel;
    LONG    lStyle;
    LRESULT lResult;
    BOOL    result = FALSE;

    ASSERT(pwszBuf && cchBuf);

    *pwszBuf = 0;

    if (IsWindow(hwnd))
    {
        hwndLabel = hwnd;

        while (hwndLabel = GetWindow(hwndLabel, GW_HWNDPREV))
        {
            lStyle = GetWindowLong(hwndLabel, GWL_STYLE);

            //
            // Skip if invisible
            //
            if (!(lStyle & WS_VISIBLE))
                continue;

            //
            // Is this a static dude?
            //
            lResult = SendMessage(hwndLabel, WM_GETDLGCODE, 0, 0);
            if (lResult & DLGC_STATIC)
            {
                //
                // Great, we've found our label.
                //
                result = GetWindowTextWrapW(hwndLabel, pwszBuf, cchBuf);
            }

            //
            // Is this a tabstop or group?  If so, bail out now.
            //
            if (lStyle & (WS_GROUP | WS_TABSTOP))
                break;
        }
    }

    return result;
}


CAddressEditAccessible::CAddressEditAccessible(HWND hwndCombo, HWND hwndEdit)
{
    m_cRefCount = 1;
    m_hwndEdit = hwndEdit;

    WCHAR wszTitle[MAX_PATH];

    if (!GetLabelStringW(GetParent(hwndCombo), wszTitle, ARRAYSIZE(wszTitle)))
    {
        MLLoadStringW(IDS_BAND_ADDRESS, wszTitle, ARRAYSIZE(wszTitle));
    }

    Str_SetPtr(&m_pwszName, wszTitle);

    CreateStdAccessibleObject(hwndCombo, OBJID_CLIENT, IID_IAccessible, (void **)&m_pDelegateAccObj);
}

CAddressEditAccessible::~CAddressEditAccessible()
{
    Str_SetPtr(&m_pwszName, NULL);
}

// *** IUnknown ***
STDMETHODIMP_(ULONG) CAddressEditAccessible::AddRef()
{
    return InterlockedIncrement((LPLONG)&m_cRefCount);
}

STDMETHODIMP_(ULONG) CAddressEditAccessible::Release()
{
    ASSERT( 0 != m_cRefCount );
    ULONG cRef = InterlockedDecrement(&m_cRefCount);
    if ( 0 == cRef )
    {
        delete this;
    }
    return cRef;
}

STDMETHODIMP CAddressEditAccessible::QueryInterface(REFIID riid, LPVOID * ppvObj)
{
    return _DefQueryInterface(riid, ppvObj);
}

// *** IAccessible ***
STDMETHODIMP CAddressEditAccessible::get_accName(VARIANT varChild, BSTR  *pszName)
{
    *pszName = (m_pwszName != NULL) ? SysAllocString(m_pwszName) : NULL;
    return (*pszName != NULL) ? S_OK : S_FALSE;
}

STDMETHODIMP CAddressEditAccessible::get_accValue(VARIANT varChild, BSTR  *pszValue)
{
    WCHAR wszValue[MAX_URL_STRING];

    if (Edit_GetText(m_hwndEdit, wszValue, ARRAYSIZE(wszValue)))
    {
        *pszValue = SysAllocString(wszValue);
    }
    else
    {
        *pszValue = NULL;
    }

    return (*pszValue != NULL) ? S_OK : S_FALSE;
}
