#ifndef _LOGO_H
#define _LOGO_H

typedef HRESULT (* LPUPDATEFN)( void *pData, DWORD dwItem, HBITMAP hImage, LPCWSTR pszCache, BOOL fCache );

#define LOGO_HEIGHT 32
#define LOGO_WIDE_WIDTH 194
#define LOGO_WIDTH  80

class CLogoBase
{
public:
    CLogoBase( BOOL fWide = FALSE );
    ~CLogoBase();

    virtual STDMETHODIMP_(ULONG) AddRef(void)  PURE;
    virtual STDMETHODIMP_(ULONG) Release(void) PURE;

    virtual IShellFolder * GetSF() PURE;
    virtual HWND GetHWND() PURE;

    inline HIMAGELIST GetLogoHIML( void );
    
    // intialisation functions
    HRESULT InitLogoView( void );
    HRESULT ExitLogoView( void );

    int GetLogoIndex( DWORD dwItem, LPCITEMIDLIST pidl, IRunnableTask **ppTask, DWORD * pdwPriority, DWORD * pdwFlags );
    int GetDefaultLogo( LPCITEMIDLIST pidl, BOOL fQuick );

    HRESULT AddTaskToQueue( IRunnableTask *pTask, DWORD dwPriority, DWORD lParam );
    
    // create the default logo for an item....
    HRESULT CreateDefaultLogo(int iIcon, int cxLogo, int cyLogo, LPCTSTR pszText, HBITMAP * phBmpLogo);

    HRESULT AddRefLogoCache( void );
    HRESULT ReleaseLogoCache( void );

    // get the task ID used with the task scheduler
    virtual REFTASKOWNERID GetTOID( void ) PURE;

    virtual HRESULT UpdateLogoCallback( DWORD dwItem, int iIcon, HBITMAP hImage, LPCWSTR pszCache, BOOL fCache ) PURE;

    int AddIndicesToLogoList( int iIcon, UINT uIndex );

protected:
    
    int GetCachedLogoIndex(DWORD dwItem, LPCITEMIDLIST pidl, IRunnableTask **ppTask, DWORD * pdwPriority, DWORD * pdwFlags );
    
    IImageCache * _pLogoCache;              // My be NULL in low memory conditions.
    IShellTaskScheduler * _pTaskScheduler;
    HIMAGELIST _himlLogos;
    SIZEL _rgLogoSize;
    DWORD _dwClrDepth;
    HDSA  _hdsaLogoIndices;

    static CRITICAL_SECTION s_csSharedLogos;
    static long             s_lSharedWideLogosRef;
    static IImageCache *    s_pSharedWideLogoCache;
    static HDSA             s_hdsaWideLogoIndices;

    HPALETTE _hpalHalftone;
    BOOL     _fWide;
};

inline HIMAGELIST CLogoBase::GetLogoHIML( )
{
    return _himlLogos;
}

struct LogoIndex
{
    int iIcon;
    int iLogo;
};


#endif

