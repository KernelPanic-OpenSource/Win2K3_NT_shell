/*
 *  Microsoft Confidential
 *  Copyright (C) Microsoft Corporation 1991
 *  All Rights Reserved.
 *
 *
 *  PIFLIB.C
 *  User interface routines for PIFMGR.DLL
 *
 *  History:
 *  Created 31-Jul-1992 3:30pm by Jeff Parsons
 */

#include "shellprv.h"
#pragma hdrstop

#ifdef _X86_

#define LIB_SIG                 0x504A

#define LIB_DEFER               LOADPROPLIB_DEFER

typedef struct LIBLINK {        /* ll */
    struct  LIBLINK *pllNext;   //
    struct  LIBLINK *pllPrev;   //
    int     iSig;               // liblink signature
    int     flLib;              // proplink flags (LIB_*)
    HINSTANCE hDLL;             // if NULL, then load has been deferred
    TCHAR    achDLL[80];        // name of DLL
} LIBLINK;
typedef LIBLINK *PLIBLINK;


#define SHEET_SIG               0x504A

typedef struct SHEETLINK {      /* sl */
    struct  SHEETLINK *pslNext;
    struct  SHEETLINK *pslPrev;
    int     iSig;
    int     iType;
    PROPSHEETPAGE psi;
} SHEETLINK;
typedef SHEETLINK *PSHEETLINK;


UINT    cEdits;                 // number of edit sessions in progress

PLIBLINK pllHead;               // pointer to first lib link
HANDLE   offHighestLibLink;      // highest offset of a lib link thus far recorded

PSHEETLINK pslHead;             // pointer to first sheet link
UINT    cSheetLinks;            // number of sheet links
HANDLE  offHighestSheetLink;    // highest offset of a sheet link thus far recorded


struct {                        // built-in property sheet info
    LPCTSTR  lpTemplateName;
    DLGPROC lpfnDlgProc;
    int     iType;
} const aPSInfo[] = {
    { MAKEINTRESOURCE(IDD_PROGRAM), DlgPrgProc, SHEETTYPE_SIMPLE},
    { MAKEINTRESOURCE(IDD_FONT),    DlgFntProc, SHEETTYPE_SIMPLE},
    { MAKEINTRESOURCE(IDD_MEMORY),  DlgMemProc, SHEETTYPE_SIMPLE},
    { MAKEINTRESOURCE(IDD_SCREEN),  DlgVidProc, SHEETTYPE_SIMPLE},
    { MAKEINTRESOURCE(IDD_MISC),    DlgMscProc, SHEETTYPE_SIMPLE},
};


/** EnumPropertyLibs - enumerate property libraries
 *
 * INPUT
 *  iLib    == 0 to begin enumeration, or result of previous call
 *  lphDLL  -> where to store handle (NULL if don't care)
 *  lpszDLL -> where to store name of library (NULL if don't care)
 *  cchszDLL == size of space (in chars) to store name
 *
 * OUTPUT
 *  lphDLL and lpszDLL filled in as appropriate, 0 if no more libs (or error)
 */

HANDLE WINAPI EnumPropertyLibs(HANDLE iLib, LPHANDLE lphDLL, LPTSTR lpszDLL, int cchszDLL)
{
    register PLIBLINK pll;
    FunctionName(EnumPropertyLibs);

    if (!iLib)
        pll = pllHead;
    else
        pll = ((PLIBLINK)iLib)->pllNext;

    // Validate the handle

    if (!pll)
        return 0;

    if ((HANDLE) pll > offHighestLibLink)
        return 0;

    if (pll->iSig != LIB_SIG)
        return 0;

    if (lphDLL)
        *lphDLL = pll->hDLL;

    if (lpszDLL)
        StringCchCopy(lpszDLL, min(cchszDLL, ARRAYSIZE(pll->achDLL)), pll->achDLL);

    return pll;
}


/** LoadPropertySheets - load property sheets
 *
 * INPUT
 *  hProps = property handle
 *  flags = 0 (reserved)
 *
 * OUTPUT
 *  # of sheets loaded, 0 if error
 */

int WINAPI LoadPropertySheets(HANDLE hProps, int flags)
{
    register PLIBLINK pll;
    FunctionName(LoadPropertySheets);

    // If this is the first edit session, do global init now

    if (cEdits++ == 0)
        if (!LoadGlobalEditData())
            return 0;

    pll = NULL;
    while (NULL != (pll = (PLIBLINK)EnumPropertyLibs(pll, NULL, NULL, 0))) {
        if (!pll->hDLL && (pll->flLib & LIB_DEFER)) {

            pll->hDLL = LoadLibrary(pll->achDLL);

            // If the load failed, to us that simply means those sheets
            // will not be available; the particular error is not interesting,
            // so nullify the handle

            if (pll->hDLL < (HINSTANCE)HINSTANCE_ERROR)
                pll->hDLL = NULL;
        }
    }
    return cSheetLinks + ARRAYSIZE(aPSInfo);
}


/** EnumPropertySheets - enumerate property sheets
 *
 * INPUT
 *  hProps == property handle
 *  iType  == sheet type (see SHEETTYPE_* constants)
 *  iSheet == 0 to begin enumeration, or result of previous call
 *  lppsi -> property sheet info structure to be filled in
 *
 * OUTPUT
 *  lppsi filled in as appropriate, 0 if no more sheets (or error)
 */

INT_PTR WINAPI EnumPropertySheets(HANDLE hProps, int iType, INT_PTR iSheet, LPPROPSHEETPAGE lppsp)
{
    register PSHEETLINK psl;
    FunctionName(EnumPropertySheets);

    while (iSheet < ARRAYSIZE(aPSInfo)) {
        if (aPSInfo[iSheet].iType <= iType) {
            if (lppsp) {
                lppsp->dwSize      = SIZEOF(PROPSHEETPAGE);
                lppsp->dwFlags     = PSP_DEFAULT;
                lppsp->hInstance   = HINST_THISDLL;
                lppsp->pszTemplate = aPSInfo[iSheet].lpTemplateName;
                lppsp->pfnDlgProc  = aPSInfo[iSheet].lpfnDlgProc;
                // lppsp->pszTitle    = NULL;
                lppsp->lParam      = (LONG_PTR)hProps;
            }
            return ++iSheet;
        }
        ++iSheet;
    }
    if (iSheet == ARRAYSIZE(aPSInfo))
        psl = pslHead;
    else
        psl = ((PSHEETLINK)iSheet)->pslNext;

    // Validate the handle

    while (psl && (HANDLE) psl <= offHighestSheetLink && psl->iSig == SHEET_SIG) {

        if (psl->iType <= iType) {

            *lppsp = psl->psi;
            lppsp->lParam = (LONG_PTR)hProps;

            return (INT_PTR) psl;
        }
        psl = psl->pslNext;
    }
    return 0;                   // no more matching sheets
}


/** FreePropertySheets - free property sheets
 *
 * INPUT
 *  hProps = property handle
 *  flags = 0 (reserved)
 *
 * OUTPUT
 *  Nothing
 */

HANDLE WINAPI FreePropertySheets(HANDLE hProps, int flags)
{
    register PLIBLINK pll;
    FunctionName(FreePropertySheets);

    pll = NULL;
    while (NULL != (pll = (PLIBLINK)EnumPropertyLibs(pll, NULL, NULL, 0))) {
        if (pll->hDLL && (pll->flLib & LIB_DEFER)) {
            FreeLibrary(pll->hDLL);
            pll->hDLL = NULL;
        }
    }
    // If this is the last edit session, do global un-init now

    if (--cEdits == 0)
        FreeGlobalEditData();

    return 0;
}


/** InitRealModeFlag - Initialize PROP_REALMODE
 *
 * INPUT
 *  ppl = properties
 *
 * OUTPUT
 *  ppl->flProp PROP_REALMODE bit set if sheet is for real-mode app,
 *  else clear.
 */

void InitRealModeFlag(PPROPLINK ppl)
{
    PROPPRG prg;

    if (!PifMgr_GetProperties(ppl, MAKELP(0,GROUP_PRG),
                        &prg, SIZEOF(prg), GETPROPS_NONE)) {
        return;                 /* Weird */
    }
    if (prg.flPrgInit & PRGINIT_REALMODE) {
        ppl->flProp |= PROP_REALMODE;
    } else {
        ppl->flProp &= ~PROP_REALMODE;
    }
}


BOOL LoadGlobalEditData()
{
    FunctionName(LoadGlobalEditData);

    if (!LoadGlobalFontEditData())
        return FALSE;

    return TRUE;
}


void FreeGlobalEditData()
{
    FunctionName(FreeGlobalEditData);
    FreeGlobalFontEditData();
}


UINT CALLBACK PifPropPageRelease(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE lppsp)
{
    FunctionName(PifPropPageRelease);

    if (uMsg == PSPCB_RELEASE) {
        PPROPLINK ppl = (PPROPLINK)(INT_PTR)lppsp->lParam;

        if ((--ppl->iSheetUsage) == 0) {

            FreePropertySheets(ppl, 0);

            PifMgr_CloseProperties(ppl, CLOSEPROPS_NONE);
        }
    }
    return 1;
}

#define MZMAGIC      ((WORD)'M'+((WORD)'Z'<<8))

//
// call SHELL.DLL to get the EXE type.
//
BOOL IsWinExe(LPCTSTR lpszFile)
{
    DWORD dw = (DWORD) SHGetFileInfo(lpszFile, 0, NULL, 0, SHGFI_EXETYPE);

    return dw && LOWORD(dw) != MZMAGIC;
}

BOOL WINAPI PifPropGetPages(LPVOID lpv,
                            LPFNADDPROPSHEETPAGE lpfnAddPage,
                            LPARAM lParam)
{
#define hDrop   (HDROP)lpv
    PPROPLINK ppl;
    PROPSHEETPAGE psp;
    int iType, cSheets;
    INT_PTR iSheet;
    HPROPSHEETPAGE hpage;
    TCHAR szFileName[MAXPATHNAME];
    FunctionName(PifPropGetPages);

    // only process things if hDrop contains only one file
    if (DragQueryFile(hDrop, (UINT)-1, NULL, 0) != 1)
    {
        return TRUE;
    }

    // get the name of the file
    DragQueryFile(hDrop, 0, szFileName, ARRAYSIZE(szFileName));

    if (GetFileAttributes( szFileName) & FILE_ATTRIBUTE_OFFLINE)
    {
        return FALSE;
    }

    // if this is a windows app, don't do no properties
    if (IsWinExe(szFileName))
        return TRUE;

    // if we can't get a property handle, don't do no properties either
    if (!(ppl = (PPROPLINK)PifMgr_OpenProperties(szFileName, NULL, 0, OPENPROPS_NONE)))
        return TRUE;

    InitRealModeFlag(ppl);

    if (!(cSheets = LoadPropertySheets(ppl, 0)))
        goto CloseProps;

    // Since the user wishes to *explicitly* change settings for this app
    // we make sure that the DONTWRITE flag isn't going to get in his way...

    ppl->flProp &= ~PROP_DONTWRITE;

    iSheet = cSheets = 0;
    iType = (GetKeyState(VK_CONTROL) >= 0? SHEETTYPE_SIMPLE : SHEETTYPE_ADVANCED);

    while (TRUE) {

        if (!(iSheet = EnumPropertySheets(ppl, iType, iSheet, &psp))) {
            // done with enumeration
            break;
        }
        psp.dwFlags |= PSP_USECALLBACK;
        psp.pfnCallback = PifPropPageRelease;
        psp.pcRefParent = 0;

        hpage = CreatePropertySheetPage(&psp);
        if (hpage)
        {
            // the PROPLINK is now being used by this property sheet as well

            if (lpfnAddPage(hpage, lParam))
            {
                ppl->iSheetUsage++;
                cSheets++;
            }
            else
            {
                PifPropPageRelease(NULL, PSPCB_RELEASE, &psp);
            }
        }
    }

    if (!cSheets) {
        FreePropertySheets(ppl, 0);

CloseProps:
        PifMgr_CloseProperties(ppl, CLOSEPROPS_NONE);
    }
    return TRUE;
}
#undef hDrop

#endif