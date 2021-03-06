// printres.h : interface of the Cprintres class
//

#define MARGINS_UNITS 2540 // Store hundredths of MM
#define MARGINS_DEFAULT (MARGINS_UNITS * 3/4) // 3/4 inch default margins

class CImgWnd;

/***************************************************************************/

class CPrintResObj : public CObject
{
    DECLARE_DYNAMIC( CPrintResObj )

    public:

    CPrintResObj( CPBView* pView, CPrintInfo* pInfo );
    ~CPrintResObj();

    void BeginPrinting( CDC* pDC, CPrintInfo* pInfo );
    void PrepareDC    ( CDC* pDC, CPrintInfo* pInfo );
    BOOL PrintPage    ( CDC* pDC, CPrintInfo* pInfo );
    void EndPrinting  ( CDC* pDC, CPrintInfo* pInfo );

    // Attributes

    CPBView*  m_pView;
    LPVOID    m_pDIB;
    LPVOID    m_pDIBits;
    int       m_iZoom;
    CPalette* m_pDIBpalette;
    CSize     m_cSizeScroll;
    int       m_iPicWidth;
    int       m_iPicHeight;
    CRect     m_rtMargins;
    CPoint    m_PhysicalOrigin;
    CSize     m_PhysicalScaledImageSize;
    CSize     m_PhysicalPageSize;
    int       m_nPagesWide;
};

/***************************************************************************/
