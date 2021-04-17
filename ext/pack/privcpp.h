#ifndef pack2cpp_h__
#define pack2cpp_h__

#include <priv.h>

#ifdef __cplusplus

#undef DebugMsg
#define DebugMsg TraceMsg

////////////////////////////////
// CPackage Definition
//
class CPackage : public IEnumOLEVERB,
                 public IOleCommandTarget,
                 public IOleObject,
                 public IViewObject2,
                 public IDataObject,
                 public IPersistStorage,
                 public IAdviseSink,
                 public IRunnableObject,
                 public IPersistFile,
                 public IOleCache,
                 public IExternalConnection

                 // cleanup -- inherit interfaces
{

    
public:
    CPackage();                 // constructor
   ~CPackage();                 // destructor
   
    HRESULT Init();             // used to initialze fields that could fail
    BOOL    RunWizard();

    // IUnknown methods...
    STDMETHODIMP            QueryInterface(REFIID,void **);
    STDMETHODIMP_(ULONG)    AddRef(void);
    STDMETHODIMP_(ULONG)    Release(void);

    // IEnumOLEVERB methods...
    STDMETHODIMP            Next(ULONG celt, OLEVERB* rgVerbs, ULONG* pceltFetched);
    STDMETHODIMP            Skip(ULONG celt);
    STDMETHODIMP            Reset();
    STDMETHODIMP            Clone(IEnumOLEVERB** ppEnum);

    // IOleCommandTarget methods
    STDMETHODIMP            QueryStatus(const GUID* pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT* pCmdText);
    STDMETHODIMP            Exec(const GUID* pguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* pvaIn, VARIANTARG* pvaOut);

    // IPersistStorage Methods...
    STDMETHODIMP        GetClassID(LPCLSID pClassID);
    STDMETHODIMP        IsDirty(void);
    STDMETHODIMP        InitNew(IStorage* pstg);
    STDMETHODIMP        Load(IStorage* pstg);
    STDMETHODIMP        Save(IStorage* pstg, BOOL fSameAsLoad);
    STDMETHODIMP        SaveCompleted(IStorage* pstg);
    STDMETHODIMP        HandsOffStorage(void);

    // IPersistFile Methods...
    // STDMETHODIMP        GetClassID(LPCLSID pClassID);
    // STDMETHODIMP        IsDirty(void);
    STDMETHODIMP        Load(LPCOLESTR pszFileName, DWORD dwdMode);
    STDMETHODIMP        Save(LPCOLESTR pszFileName, BOOL fRemember);
    STDMETHODIMP        SaveCompleted(LPCOLESTR pszFileName);
    STDMETHODIMP        GetCurFile(LPOLESTR *ppszFileName);

  
    // IDataObject Methods...
    STDMETHODIMP GetData(LPFORMATETC pFEIn, LPSTGMEDIUM pSTM);
    STDMETHODIMP GetDataHere(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    STDMETHODIMP QueryGetData(LPFORMATETC pFE);
    STDMETHODIMP GetCanonicalFormatEtc(LPFORMATETC pFEIn, LPFORMATETC pFEOut);
    STDMETHODIMP SetData(LPFORMATETC pFE, LPSTGMEDIUM pSTM, BOOL fRelease);
    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, LPENUMFORMATETC *ppEnum);
    STDMETHODIMP DAdvise(LPFORMATETC pFE, DWORD grfAdv, LPADVISESINK pAdvSink,
                            DWORD *pdwConnection);
    STDMETHODIMP DUnadvise(DWORD dwConnection);
    STDMETHODIMP EnumDAdvise(LPENUMSTATDATA *ppEnum);
 
    // IOleObject Methods...
    STDMETHODIMP SetClientSite(LPOLECLIENTSITE pClientSite);
    STDMETHODIMP GetClientSite(LPOLECLIENTSITE *ppClientSite);
    STDMETHODIMP SetHostNames(LPCOLESTR szContainerApp, LPCOLESTR szContainerObj);
    STDMETHODIMP Close(DWORD dwSaveOption);
    STDMETHODIMP SetMoniker(DWORD dwWhichMoniker, LPMONIKER pmk);
    STDMETHODIMP GetMoniker(DWORD dwAssign, DWORD dwWhichMonkier,LPMONIKER *ppmk);
    STDMETHODIMP InitFromData(LPDATAOBJECT pDataObject, BOOL fCreation, 
                                 DWORD dwReserved);
    STDMETHODIMP GetClipboardData(DWORD dwReserved, LPDATAOBJECT *ppDataObject);
    STDMETHODIMP DoVerb(LONG iVerb, LPMSG lpmsg, LPOLECLIENTSITE pActiveSite, 
                           LONG lindex, HWND hwndParent, LPCRECT lprcPosRect);
    STDMETHODIMP EnumVerbs(LPENUMOLEVERB *ppEnumOleVerb);
    STDMETHODIMP Update(void);
    STDMETHODIMP IsUpToDate(void);
    STDMETHODIMP GetUserClassID(LPCLSID pClsid);
    STDMETHODIMP GetUserType(DWORD dwFromOfType, LPOLESTR *pszUserType);
    STDMETHODIMP SetExtent(DWORD dwDrawAspect, LPSIZEL psizel);
    STDMETHODIMP GetExtent(DWORD dwDrawAspect, LPSIZEL psizel);
    STDMETHODIMP Advise(LPADVISESINK pAdvSink, DWORD *pdwConnection);
    STDMETHODIMP Unadvise(DWORD dwConnection);
    STDMETHODIMP EnumAdvise(LPENUMSTATDATA *ppenumAdvise);
    STDMETHODIMP GetMiscStatus(DWORD dwAspect, DWORD *pdwStatus);
    STDMETHODIMP SetColorScheme(LPLOGPALETTE pLogpal);

    // IViewObject2 Methods...
    STDMETHODIMP Draw(DWORD dwDrawAspect, LONG lindex, void *pvAspect,
                         DVTARGETDEVICE *ptd, HDC hdcTargetDev,
                         HDC hdcDraw, LPCRECTL lprcBounds, LPCRECTL lprcWBounds,
                         BOOL (CALLBACK *pfnContinue)(ULONG_PTR), ULONG_PTR dwContinue);
    STDMETHODIMP GetColorSet(DWORD dwAspect, LONG lindex, void *pvAspect,
                                DVTARGETDEVICE *ptd, HDC hdcTargetDev,
                                LPLOGPALETTE *ppColorSet);
    STDMETHODIMP Freeze(DWORD dwDrawAspect, LONG lindex, void * pvAspect, 
                           DWORD *pdwFreeze);
    STDMETHODIMP Unfreeze(DWORD dwFreeze);
    STDMETHODIMP SetAdvise(DWORD dwAspects, DWORD dwAdvf,
                              LPADVISESINK pAdvSink);
    STDMETHODIMP GetAdvise(DWORD *pdwAspects, DWORD *pdwAdvf,
                              LPADVISESINK *ppAdvSink);
    STDMETHODIMP GetExtent(DWORD dwAspect, LONG lindex, DVTARGETDEVICE *ptd,
                              LPSIZEL pszl);
    // IAdviseSink Methods...
    STDMETHODIMP_(void)  OnDataChange(LPFORMATETC, LPSTGMEDIUM);
    STDMETHODIMP_(void)  OnViewChange(DWORD, LONG);
    STDMETHODIMP_(void)  OnRename(LPMONIKER);
    STDMETHODIMP_(void)  OnSave(void);
    STDMETHODIMP_(void)  OnClose(void);

    // IRunnable Object methods...
    STDMETHODIMP        GetRunningClass(LPCLSID);
    STDMETHODIMP        Run(LPBC);
    STDMETHODIMP_(BOOL) IsRunning();
    STDMETHODIMP        LockRunning(BOOL,BOOL);
    STDMETHODIMP        SetContainedObject(BOOL);

    // IOleCache methods
    // We need an IOLECache Interface to Keep Office97 happy.
    STDMETHODIMP        Cache(FORMATETC * pFormatetc, DWORD advf, DWORD * pdwConnection);
    STDMETHODIMP        Uncache(DWORD dwConnection);
    STDMETHODIMP        EnumCache(IEnumSTATDATA ** ppenumSTATDATA);
    STDMETHODIMP        InitCache(IDataObject *pDataObject);

    // IExternalConnection
    // Some applications RealeaseConnect and then we never hear from them again.
    // This allows us to call OnClose() after the activations.
    STDMETHODIMP_(DWORD)        AddConnection(DWORD exconn, DWORD dwreserved );
    STDMETHODIMP_(DWORD)        ReleaseConnection(DWORD extconn, DWORD dwreserved, BOOL fLastReleaseCloses );

protected:
    LONG        _cRef;          // package reference count
    UINT        _cf;            // package clipboard format

    LPIC        _lpic;          // icon for the packaged object
    PANETYPE    _panetype;      // tells us whether we have a cmdlink or embed
    PSSTATE     _psState;               // persistent storage state
    // These are mutually exclusive, so should probably be made into a union,
    // but that's a minor point.
    LPEMBED     _pEmbed;        // embedded file structure
    LPCML       _pCml;          // command line structure

    BOOL        _fLoaded;       // true if data from persistent storage
    
    // IOleObject vars from SetHostNames
    LPOLESTR    _lpszContainerApp;
    LPOLESTR    _lpszContainerObj;
    
    BOOL        _fIsDirty;      // dirty flag for our internal storage from the pov of our container
    DWORD       _dwCookie;      // connection value for AdviseSink
        
    // Advise interfaces
    LPDATAADVISEHOLDER          _pIDataAdviseHolder;
    LPOLEADVISEHOLDER           _pIOleAdviseHolder;
    LPOLECLIENTSITE             _pIOleClientSite;

    // Excel hack: when Excel hosts what it thinks is a link it always NULLs out
    // it's "object" pointer.  If we call anything on IOleClientSite other than
    // save then it will fault.
    BOOL    _fNoIOleClientSiteCalls;

    BOOL                _fFrozen;

    // to be able to send view change notifications we need these vars
    IAdviseSink                *_pViewSink;
    DWORD                       _dwViewAspects;
    DWORD                       _dwViewAdvf;

    // IEnumOLEVERB variables:
    ULONG       _cVerbs;
    ULONG       _nCurVerb;
    OLEVERB*    _pVerbs;
    IContextMenu* _pcm;

    // IEnumOLEVERB helper methods:
    HRESULT InitVerbEnum(OLEVERB* pVerbs, ULONG cVerbs);
    HRESULT GetContextMenu(IContextMenu** ppcm);
    VOID ReleaseContextMenu();

    // if fInitFile is TRUE, then we will totally initialize ourselves
    // from the given filename.  In other words, all our structures will be
    // initialized after calling this is fInitFile = TRUE.  On the other hand,
    // if it's FALSE, then we'll just reinit our data and not update icon
    // and filename information.
    //
    HRESULT EmbedInitFromFile(LPCTSTR lpFileName, BOOL fInitFile);
    HRESULT CmlInitFromFile(LPTSTR lpFilename, BOOL fUpdateIcon, PANETYPE paneType);
    HRESULT InitFromPackInfo(LPPACKAGER_INFO lppi);
    
    HRESULT CreateTempFile(bool deleteExisting = false);
    HRESULT CreateTempFileName();
    HRESULT _IconRefresh();
    void  _DestroyIC();
    BOOL _IconCalcSize(LPIC lpic);
    VOID _IconDraw(LPIC,HDC, LPRECT);
    LPIC _IconCreateFromFile(LPCTSTR lpstrFile);
    VOID _GetCurrentIcon(LPIC lpic);
    void _CreateSaferIconTitle(LPTSTR szSaferTitle, LPCTSTR szIconTitle);
    void _DrawIconToDC(HDC hdcMF, LPIC lpic, bool stripAlpha, LPCTSTR pszActualFileName);
    
    // Data Transfer functions...
    HRESULT GetFileDescriptor(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    HRESULT GetFileContents(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    HRESULT GetMetafilePict(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    HRESULT GetEnhMetafile(LPFORMATETC pFE, LPSTGMEDIUM pSTM);
    HRESULT GetObjectDescriptor(LPFORMATETC pFE, LPSTGMEDIUM pSTM) ;

    HRESULT CreateShortcutOnStream(IStream* pstm); 

    // Packager Read/Write Functions...
    HRESULT PackageReadFromStream(IStream* pstm);
    HRESULT IconReadFromStream(IStream* pstm);
    HRESULT EmbedReadFromStream(IStream* pstm);
    HRESULT CmlReadFromStream(IStream* pstm);
    HRESULT PackageWriteToStream(IStream* pstm);
    HRESULT IconWriteToStream(IStream* pstm, DWORD *pdw);
    HRESULT EmbedWriteToStream(IStream* pstm, DWORD *pdw);
    HRESULT CmlWriteToStream(IStream* pstm, DWORD *pdw);

    // Some utility functions and data
    void _FixupTempFile(IPersistFile * ppf, LPEMBED pEmbed);

    int _GiveWarningMsg();

    // Misc AppCompat Stuff
    int _iPropertiesMenuItem;
    BOOL _bClosed;          // the close happened
    BOOL _bCloseIt;         // that we should close at the end of the activate

};


////////////////////////////////////////////
//
// Package Wizard and Edit Package Dialog Procs and functions
//

// Pages for Wizard
INT_PTR APIENTRY PackWiz_CreatePackageDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR APIENTRY PackWiz_SelectFileDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR APIENTRY PackWiz_SelectLabelDlgProc(HWND, UINT, WPARAM, LPARAM);

// Edit dialog procs
INT_PTR APIENTRY PackWiz_EditEmbedPackageDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR APIENTRY PackWiz_EditCmdPackakgeDlgProc(HWND, UINT, WPARAM, LPARAM);

// functions
int  PackWiz_CreateWizard(HWND,LPPACKAGER_INFO);
int  PackWiz_EditPackage(HWND,int,LPPACKAGER_INFO);
VOID PackWiz_FillInPropertyPage(PROPSHEETPAGE *, INT, DLGPROC);


#endif  // __cplusplus

#endif
