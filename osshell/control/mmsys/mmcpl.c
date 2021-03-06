/*==========================================================================*/
//
//  mmcpl.c
//
//  Copyright (C) 1993-1994 Microsoft Corporation.  All Rights Reserved.
//
//    06/94    -Created- VijR
//
/*==========================================================================*/

#pragma warning( disable: 4103)
#include "mmcpl.h"
#include <cpl.h>
#define NOSTATUSBAR
#include <commctrl.h>
#include <prsht.h>
#include <regstr.h>
#include <infstr.h>
#include <devguid.h>

#include "draw.h"
#include "utils.h"
#include "drivers.h"
#include "sulib.h"
#include <tchar.h>
#include <hwtab.h>
#include "debug.h"
#include "start.h"

#ifndef cchRESOURCE
    #define cchRESOURCE 256
#endif

/*
 ***************************************************************
 * Globals
 ***************************************************************
 */
HINSTANCE   ghInstance  = NULL;
BOOL        gfNukeExt   = -1;
HWND        ghwndMsgBox = NULL;
HWND        ghwndAdvProp = NULL;
BOOL        gfVoiceTab  = FALSE;
BOOL        gfRedisplayCPL = FALSE;

#ifdef FIX_BUG_15451
static TCHAR cszFORKLINE[] = TEXT("RUNDLL32.EXE MMSYS.CPL,ShowDriverSettingsAfterFork %s");
#endif // FIX_BUG_15451

SZCODE cszAUDIO[] = AUDIO;
SZCODE cszVIDEO[] = VIDEO;
SZCODE cszCDAUDIO[] = CDAUDIO;
SZCODE cszMIDI[] = MIDI;
SZCODE cszVOICE[]    = VOICE;
SZCODE cszVOLUME[]    = VOLUME;

/*
 ***************************************************************
 *  Typedefs
 ***************************************************************
 */

typedef struct _ExtPropSheetCBParam //Callback Parameter
{
    HTREEITEM hti;
    LPPROPSHEETHEADER    ppsh;
    LPARAM lParam1;    //PIRESOURCE/PINSTRUMENT etc. depending on node. (OR) Simple propsheet class
    LPARAM lParam2; //hwndTree (OR) Simple propsheet name
} EXTPROPSHEETCBPARAM, *PEXTPROPSHEETCBPARAM;

typedef struct _MBInfo
{
    LPTSTR szTitle;
    LPTSTR szMsg;
    UINT  uStyle;
} MBINFO, *PMBINFO;


/*
 ***************************************************************
 * Defines
 ***************************************************************
 */

#define    MAXPAGES    8    // MAX number of sheets allowed
#define    MAXMODULES    32    // MAX number of external modules allowed
#define    MAXCLASSSIZE    64

#define cComma    TEXT(',')
#define PROPTABSIZE 13

#define GetString(_str,_id,_hi)  LoadString (_hi, _id, _str, sizeof(_str)/sizeof(TCHAR))

/*
 ***************************************************************
 * File Globals
 ***************************************************************
 */
static SZCODE    aszSimpleProperties[] = REGSTR_PATH_MEDIARESOURCES TEXT("\\MediaExtensions\\shellx\\SimpleProperties\\");
static SZCODE    aszShellName[]    = TEXT("ShellName");

static UINT     g_cRefCnt;            // keeps track of the ref count
static int      g_cProcesses        = 0;
static int      g_nStartPage        = 0;

/*
 ***************************************************************
 * Prototypes
 ***************************************************************
 */
INT_PTR CALLBACK AudioDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK VideoDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK CDDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ACMDlg(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK SoundDlg(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK VolumeDlg(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK AddDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AdvDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK HardwareDlgProc(HWND hdlg, UINT uMsg, WPARAM wp, LPARAM lp);
INT_PTR CALLBACK VoiceDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK StartDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK RebootDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
//INT_PTR CALLBACK EffectDlg(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);



//
// This is the dialog procedure for the "Hardware" page.
//


INT_PTR CALLBACK HardwareDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    static HWND s_hwndHW = NULL;

    switch (uMessage)
    {
        case WM_NOTIFY:
        {
            NMHDR * pnmhdr = (NMHDR *) lParam;
            int code = pnmhdr->code;

            switch (code)
            {
                case HWN_FILTERITEM:
                {
                    NMHWTAB *pnmht = (NMHWTAB *) lParam;
                    BOOL fFilter = FALSE;

                    if (!pnmht->fHidden)    // Let's not bother looking at devices already hidden
                    {
                        fFilter = FALSE;
                    }

                    return(TRUE);
                }
                break;

                case HWN_SELECTIONCHANGED:
                {
                    NMHWTAB *pnmht = (NMHWTAB *) lParam;

                    if (pnmht)
                    {
                        if (pnmht->pdinf)
                        {
                            if (IsEqualGUID(&(pnmht->pdinf->ClassGuid),&GUID_DEVCLASS_CDROM))
                            {
                                SetWindowText(s_hwndHW, TEXT("hh.exe ms-its:tshoot.chm::/hdw_drives.htm"));
                            }
                            else
                            {
                                SetWindowText(s_hwndHW, TEXT("hh.exe ms-its:tshoot.chm::/tssound.htm"));
                            }
                        }
                    }
                }
                break;
            }
        }
        break;

        case WM_INITDIALOG:
        {
            GUID guidClass[2];

            guidClass[0] = GUID_DEVCLASS_CDROM;
            guidClass[1] = GUID_DEVCLASS_MEDIA;

            s_hwndHW = DeviceCreateHardwarePageEx(hDlg, (const GUID *) &guidClass, 2, HWTAB_LARGELIST );

            if (s_hwndHW)
            {
                SetWindowText(s_hwndHW, TEXT("hh.exe ms-its:tshoot.chm::/tssound.htm"));
            }
            else
            {
                DestroyWindow(hDlg); // catastrophic failure
            }
        }
        return FALSE;
    }

    return FALSE;
}



INT_PTR CALLBACK CD_HardwareDlgProc(HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
    switch (uMessage)
    {
        case WM_INITDIALOG:
        {
            HWND hwndHW;

            hwndHW = DeviceCreateHardwarePageEx(hDlg, &GUID_DEVCLASS_CDROM, 1, HWTAB_SMALLLIST);

            if (hwndHW)
            {
                SetWindowText(hwndHW, TEXT("hh.exe ms-its:tshoot.chm::/hdw_multi.htm"));
            }
            else
            {
                DestroyWindow(hDlg); // catastrophic failure
            }
        }
        return FALSE;
    }

    return FALSE;
}



/*
 ***************************************************************
 ***************************************************************
 */

INT_PTR FAR PASCAL mmse_MessageBoxProc(HWND hDlg, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
    switch (wMsg)
    {
    case WM_INITDIALOG:
        {
            PMBINFO pmbInfo = (PMBINFO)lParam;
            UINT uStyle = pmbInfo->uStyle;

            SetWindowText(hDlg, pmbInfo->szTitle);
            SetWindowText(GetDlgItem(hDlg, MMSE_TEXT), pmbInfo->szMsg);
            if (IsFlagClear(uStyle, MMSE_OK))
                DestroyWindow(GetDlgItem(hDlg, MMSE_OK));
            if (IsFlagClear(uStyle, MMSE_YES))
                DestroyWindow(GetDlgItem(hDlg, MMSE_YES));
            if (IsFlagClear(uStyle, MMSE_NO))
                DestroyWindow(GetDlgItem(hDlg, MMSE_NO));
            ghwndMsgBox = hDlg;
            break;
        }
    case WM_DESTROY:
        ghwndMsgBox = NULL;
        break;
    case WM_COMMAND:
        {
            switch (GET_WM_COMMAND_ID(wParam, lParam))
            {
            case MMSE_YES:
                EndDialog(hDlg, MMSE_YES);
                break;
            case MMSE_NO:
                EndDialog(hDlg, MMSE_NO);
                break;
            case MMSE_OK:
                EndDialog(hDlg, MMSE_OK);
                break;
            }
            break;
        }
    default:
        return FALSE;
    }
    return TRUE;
}

INT_PTR mmse_MessageBox(HWND hwndP,  LPTSTR szMsg, LPTSTR szTitle, UINT uStyle)
{
    MBINFO mbInfo;

    mbInfo.szMsg = szMsg;
    mbInfo.szTitle = szTitle;
    mbInfo.uStyle = uStyle;

    return DialogBoxParam(ghInstance, MAKEINTRESOURCE(DLG_MESSAGE_BOX), hwndP, mmse_MessageBoxProc, (LPARAM)&mbInfo);
}

/*==========================================================================*/
int StrByteLen(LPTSTR sz)
{
    LPTSTR psz;

    if (!sz)
        return 0;
    for (psz = sz; *psz; psz = CharNext(psz))
        ;
    return (int)(psz - sz);
}

static void NukeExt(LPTSTR sz)
{
    int len;

    len = StrByteLen(sz);

    if (len > 4 && sz[len-4] == TEXT('.'))
        sz[len-4] = 0;
}

static LPTSTR NukePath(LPTSTR sz)
{
    LPTSTR pTmp, pSlash;

    for (pSlash = pTmp = sz; *pTmp; pTmp = CharNext(pTmp))
    {
        if (*pTmp == TEXT('\\'))
            pSlash = pTmp;
    }
    return (pSlash == sz ? pSlash : pSlash+1);
}

void    CheckNukeExtOption(LPTSTR sz)
{
    SHFILEINFO sfi;

    SHGetFileInfo(sz, 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME);
    if (lstrcmpi((LPTSTR)(sfi.szDisplayName+lstrlen(sfi.szDisplayName)-4), cszWavExt))
        gfNukeExt = TRUE;
    else
        gfNukeExt = FALSE;
}

LPTSTR PASCAL NiceName(LPTSTR sz, BOOL fNukePath)
{
    SHFILEINFO sfi;

    if (gfNukeExt == -1)
        CheckNukeExtOption(sz);

    if (!SHGetFileInfo(sz, 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME))
        return sz;

    if (fNukePath)
    {
        lstrcpy(sz, sfi.szDisplayName);
    }
    else
    {
        LPTSTR lpszFileName;

        lpszFileName = NukePath(sz);
        lstrcpy(lpszFileName, sfi.szDisplayName);
        if (lpszFileName != sz)
            CharUpperBuff(sz, 1);
    }
    return sz;
}



/*
 ***************************************************************
 * ErrorBox
 *
 * Description:
 *        Brings up error Dialog displaying error
 *
 * Parameters:
 *        HWND    hDlg  - Window handle
 *        int        iResource    - id of the resource to be loaded
 *        LPTSTR    lpszDesc - The string to be inserted in the resource string
 *
 * Returns:            BOOL
 *
 ***************************************************************
 */
BOOL PASCAL ErrorBox(HWND hDlg, int iResource, LPTSTR lpszDesc)
{
    TCHAR szBuf[MAXMSGLEN];
    TCHAR szTitle[MAXSTR];
    TCHAR szResource[MAXMSGLEN];

    LoadString(ghInstance, iResource, szResource, MAXSTR);
    LoadString(ghInstance, IDS_ERROR, szTitle, MAXSTR);
    wsprintf(szBuf, szResource, lpszDesc);
    MessageBox(hDlg, szBuf, szTitle, MB_APPLMODAL | MB_OK |MB_ICONSTOP);
    return TRUE;
}

int PASCAL DisplayMessage(HWND hDlg, int iResTitle, int iResMsg, UINT uStyle)
{
    TCHAR szBuf[MAXMSGLEN];
    TCHAR szTitle[MAXSTR];
    UINT uAddStyle = MB_APPLMODAL;

    if (!LoadString(ghInstance, iResTitle, szTitle, MAXSTR))
        return FALSE;
    if (!LoadString(ghInstance, iResMsg, szBuf, MAXSTR))
        return FALSE;
    if (uStyle & MB_OK)
        uAddStyle |= MB_ICONASTERISK;
    else
        uAddStyle |= MB_ICONQUESTION;
    return MessageBox(hDlg, szBuf, szTitle,  uStyle | uAddStyle);
}


//Adds spaces around Tab Names to make them all approx. same size.
STATIC void PadWithSpaces(LPTSTR szName, LPTSTR szPaddedName)
{
    static SZCODE cszFmt[] = TEXT("%s%s%s");
    TCHAR szPad[8];
    int i;

    i = PROPTABSIZE - lstrlen(szName);

    i = (i <= 0) ? 0 : i/2;
    for (szPad[i] = TEXT('\0');i; i--)
        szPad[i-1] =  TEXT(' ');
    wsprintf(szPaddedName, cszFmt, szPad, szName, szPad);
}

/*==========================================================================*/
UINT CALLBACK  CallbackPage(
                           HWND        hwnd,
                           UINT        uMsg,
                           LPPROPSHEETPAGE    ppsp)
{
    if (uMsg == PSPCB_RELEASE)
    {
        DPF_T("* RelasePage %s *", (LPTSTR)ppsp->pszTitle);
    }
    return 1;
}

/*==========================================================================*/
static BOOL PASCAL NEAR AddPage(
                               LPPROPSHEETHEADER    ppsh,
                               LPCTSTR            pszTitle,
                               DLGPROC            pfnDialog,
                               UINT            idTemplate,
                               LPARAM            lParam)
{
    if (ppsh->nPages < MAXPAGES)
    {

        if (pfnDialog)
        {
            PROPSHEETPAGE    psp;
            psp.dwSize = sizeof(PROPSHEETPAGE);
            psp.dwFlags = PSP_DEFAULT | PSP_USETITLE | PSP_USECALLBACK;
            psp.hInstance = ghInstance;
            psp.pszTemplate = MAKEINTRESOURCE(idTemplate);
            psp.pszIcon = NULL;
            psp.pszTitle = pszTitle;
            psp.pfnDlgProc = pfnDialog;
            psp.lParam = (LPARAM)lParam;
            psp.pfnCallback = CallbackPage;
            psp.pcRefParent = NULL;
            if (ppsh->phpage[ppsh->nPages] = CreatePropertySheetPage(&psp))
            {
                ppsh->nPages++;
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*==========================================================================*/
BOOL CALLBACK MMExtPropSheetCallback(DWORD dwFunc, DWORD_PTR dwParam1, DWORD_PTR dwParam2, DWORD_PTR dwInstance)
{
    PEXTPROPSHEETCBPARAM pcbp = (PEXTPROPSHEETCBPARAM)dwInstance;

    if (!pcbp && dwFunc != MM_EPS_BLIND_TREECHANGE)
        return FALSE;
    switch (dwFunc)
    {
    case MM_EPS_GETNODEDESC:
        {
            if (!dwParam1)
                return FALSE;
            if (pcbp->hti == NULL)
                lstrcpy((LPTSTR)dwParam1, (LPTSTR)pcbp->lParam2);
            else
            {
                GetTreeItemNodeDesc ((LPTSTR)dwParam1,
                                     (PIRESOURCE)pcbp->lParam1);
            }
            break;
        }
    case MM_EPS_GETNODEID:
        {
            if (!dwParam1)
                return FALSE;
            if (pcbp->hti == NULL)
                lstrcpy((LPTSTR)dwParam1, (LPTSTR)pcbp->lParam2);
            else
            {
                GetTreeItemNodeID ((LPTSTR)dwParam1,
                                   (PIRESOURCE)pcbp->lParam1);
            }
            break;
        }
    case MM_EPS_ADDSHEET:
        {
            HPROPSHEETPAGE    hpsp = (HPROPSHEETPAGE)dwParam1;

            if (hpsp && (pcbp->ppsh->nPages < MAXPAGES))
            {
                pcbp->ppsh->phpage[pcbp->ppsh->nPages++] = hpsp;
                return TRUE;
            }
            return FALSE;
        }
    case MM_EPS_TREECHANGE:
        {
            RefreshAdvDlgTree ();
            break;
        }
    case MM_EPS_BLIND_TREECHANGE:
        {
            RefreshAdvDlgTree ();
            break;
        }
    default:
        return FALSE;
    }
    return TRUE;
}

INT_PTR CALLBACK SpeechDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/*==========================================================================*/
static BOOL PASCAL NEAR AddSpeechPage(LPPROPSHEETHEADER    ppsh)
{
    TCHAR    aszTitleRes[128];
    TCHAR     szTmp[32];

    LoadString(ghInstance, IDS_SPEECH_NAME, aszTitleRes, sizeof(aszTitleRes)/sizeof(TCHAR));
    PadWithSpaces((LPTSTR)aszTitleRes, (LPTSTR)szTmp);
    return AddPage(ppsh, szTmp, SpeechDlgProc, IDD_SPEECH, (LPARAM)NULL);
}

/*==========================================================================*/
static BOOL PASCAL NEAR AddAdvancedPage(
                                       LPPROPSHEETHEADER    ppsh)
{
    TCHAR    aszTitleRes[128];
    TCHAR     szTmp[32];

    LoadString(ghInstance, IDS_ADVANCED, aszTitleRes, sizeof(aszTitleRes)/sizeof(TCHAR));
    PadWithSpaces((LPTSTR)aszTitleRes, (LPTSTR)szTmp);
    return AddPage(ppsh, szTmp, AdvDlg, ADVDLG, (LPARAM)NULL);
}

/*==========================================================================*/
static BOOL PASCAL NEAR AddStartPage(
                                       LPPROPSHEETHEADER    ppsh)
{
    TCHAR    aszTitleRes[128];
    TCHAR    szTmp[32];

    LoadString(ghInstance, IDS_START, aszTitleRes, sizeof(aszTitleRes)/sizeof(TCHAR));
    PadWithSpaces((LPTSTR)aszTitleRes, (LPTSTR)szTmp);
    return AddPage(ppsh, szTmp, StartDlg, STARTDLG, (LPARAM)NULL);
}

/*==========================================================================*/
static BOOL PASCAL NEAR AddRebootPage(
                                       LPPROPSHEETHEADER    ppsh)
{
    TCHAR    aszTitleRes[128];
    TCHAR    szTmp[32];

    LoadString(ghInstance, IDS_START, aszTitleRes, sizeof(aszTitleRes)/sizeof(TCHAR));
    PadWithSpaces((LPTSTR)aszTitleRes, (LPTSTR)szTmp);
    return AddPage(ppsh, szTmp, RebootDlg, REBOOTDLG, (LPARAM)NULL);
}

/*==========================================================================*/
static BOOL PASCAL NEAR AddHardwarePage(
                                       LPPROPSHEETHEADER    ppsh)
{
    TCHAR    aszTitleRes[128];
    TCHAR     szTmp[32];

    // Don't add a hardware tab if the admin restricted it
    if (SHRestricted(REST_NOHARDWARETAB))
        return FALSE;

    LoadString(ghInstance, IDS_HARDWARE, aszTitleRes, sizeof(aszTitleRes)/sizeof(TCHAR));
    PadWithSpaces((LPTSTR)aszTitleRes, (LPTSTR)szTmp);
    return AddPage(ppsh, szTmp, HardwareDlgProc, HWDLG, (LPARAM)NULL);
}

/*==========================================================================*/
static BOOL PASCAL NEAR AddSchemesPage(
                                      LPPROPSHEETHEADER    ppsh)
{
    TCHAR    aszTitleRes[128];

    LoadString(ghInstance, IDS_EVENTSNAME, aszTitleRes, sizeof(aszTitleRes)/sizeof(TCHAR));
    return AddPage(ppsh, aszTitleRes, SoundDlg, SOUNDDIALOG, (LPARAM)NULL);
}

/*==========================================================================*/

static void PASCAL NEAR AddInternalPages (LPPROPSHEETHEADER ppsh)
{
    static EXTPROPSHEETCBPARAM cbp;
    TCHAR  szText[ cchRESOURCE ];
    TCHAR  szPadded[ cchRESOURCE ];


    // Add the Volume page
    //
    GetString (szText, IDS_VOLUMENAME, ghInstance);
    PadWithSpaces (szText, szPadded);
    AddPage (ppsh, szPadded, VolumeDlg, IDD_VOLUME, (LPARAM)NULL);

    // Add the Sound Scheme page
    //
    GetString (szText, IDS_EVENTSNAME, ghInstance);
    PadWithSpaces (szText, szPadded);
    AddPage (ppsh, szPadded, SoundDlg, SOUNDDIALOG, (LPARAM)NULL);

    // Add the Audio page
    //
    GetString (szText, IDS_AUDIO_TAB, ghInstance);
    PadWithSpaces (szText, szPadded);
    AddPage (ppsh, szPadded, AudioDlg, AUDIODLG, (LPARAM)NULL);

    // Add the Voice page
    //
    GetString (szText, IDS_VOICE, ghInstance);
    PadWithSpaces (szText, szPadded);
    AddPage (ppsh, szPadded, VoiceDlg, VOICEDLG, (LPARAM)NULL);
}


static void InitPSH(LPPROPSHEETHEADER ppsh, HWND hwndParent, LPTSTR pszCaption, HPROPSHEETPAGE    FAR * phpsp)
{
    ppsh->dwSize = sizeof(PROPSHEETHEADER);
    ppsh->dwFlags = PSH_PROPTITLE;
    ppsh->hwndParent = hwndParent;
    ppsh->hInstance = ghInstance;
    ppsh->pszCaption = pszCaption;
    ppsh->nPages = 0;
    ppsh->nStartPage = 0;
    ppsh->phpage = phpsp;
}


/*==========================================================================*/
#ifdef FIX_BUG_15451
static void PASCAL cplMMDoubleClick (HWND hCPlWnd, int nStartPage)
#else // FIX_BUG_15451
static void PASCAL cplMMDoubleClick (HWND hCPlWnd)
#endif // FIX_BUG_15451
{
    PROPSHEETHEADER   psh;
    HPROPSHEETPAGE    hpsp[MAXPAGES];
    TCHAR strOldDir[MAX_PATH], strSysDir[MAX_PATH];

    strOldDir[0] = TEXT('\0');
    strSysDir[0] = TEXT('\0');

    GetSystemDirectory(strSysDir, MAX_PATH);
    GetCurrentDirectory(MAX_PATH, strOldDir);
    SetCurrentDirectory(strSysDir);
    wsInfParseInit();

    InitCommonControls();
    OleInitialize(NULL);

    RegSndCntrlClass((LPCTSTR)DISPFRAMCLASS);

    do
    {
        gfRedisplayCPL = FALSE;

        InitPSH(&psh,hCPlWnd,(LPTSTR)MAKEINTRESOURCE(IDS_MMNAME),hpsp);
#ifdef FIX_BUG_15451
        psh.nStartPage = nStartPage;
#else // FIX_BUG_15451
        psh.nStartPage = g_nStartPage;
#endif // FIX_BUG_15451
        g_nStartPage = 0;

        if (AudioServiceStarted())
        {
            if (RebootNeeded())
            {
                AddRebootPage(&psh);
            }
            else
            {
                AddInternalPages(&psh);
                AddHardwarePage(&psh);
            }
        }
        else
        {
            AddStartPage(&psh);
        }
        PropertySheet(&psh);
    }
    while( gfRedisplayCPL == TRUE );

    OleUninitialize();

    infClose(NULL);
    SetCurrentDirectory(strOldDir);
}

/*==========================================================================*/
static void PASCAL cplEventsDoubleClick (HWND hCPlWnd)
{
    PROPSHEETHEADER    psh;
    HPROPSHEETPAGE    hpsp[MAXPAGES];

    InitCommonControls();
    RegSndCntrlClass((LPCTSTR)DISPFRAMCLASS);
    InitPSH(&psh,hCPlWnd,(LPTSTR)MAKEINTRESOURCE(IDS_EVENTSNAME),hpsp);
    AddSchemesPage(&psh);
    PropertySheet(&psh);
}

#ifdef FIX_BUG_15451
/*==========================================================================*/
/*
 * ShowDriverSettings
 * ShowDriverSettingsAfterFork
 *
 * When the user selects DevicesTab.<anydevice>.Properties.Settings, a
 * DRV_CONFIGURE message is sent to the selected user-mode driver, to cause
 * it to display its configuration dialog.  The sound drivers shipped with
 * NT (SNDBLST,MVAUDIO,SNDSYS) exhibit a bug in this condition: when the
 * configuration dialog is complete (regardless of whether OK or CANCEL was
 * selected), these drivers attempt to unload-and-reload their kernel-mode
 * component in order to begin using the new (or restore the original)
 * driver settings.  The unload request fails, because both the Audio tab
 * and SNDVOL.EXE have open mixer handles and pending IRPs within the kernel
 * driver (the latter are used to provide notifications of volume changes).
 * Worse, when the unload fails, it leaves the driver useless: its state
 * remains STOP_PENDING, and it cannot be resurrected without logging off
 * and back on.
 *
 * These routines have been provided as a temporary workaround for bug 15451,
 * which describes the problem mentioned above.  The theory behind this
 * solution is two-fold:
 *   1- close SNDVOL.EXE as soon as a driver's configuration dialog is
 *      to be displayed, and restart it directly thereafter.  This prevents
 *      it from maintaining any open handles to and/or pending IRPs within the
 *      kernel driver.
 *   2- if the Audio tab has ever been displayed, it will have open mixers
 *      which must be closed.  Because a bug/design flaw within these sound
 *      drivers prevents the mixers from being closed without killing this
 *      process (the sound drivers each cache open mixer handles), the
 *      routine ShowDriverSettings forks a new MMSYS.CPL process, which is
 *      then used to display the driver's settings dialog.
 *
 * The flow of this solution follows:
 *
 * 1- MMSYS.CPL starts on Audio tab, setting fHaveStartedAudioDialog to TRUE.
 * 2- User selects Devices tab.
 * 3- User selects a device driver.
 * 4- User selects Properties+Settings; control reaches ShowDriverSettings().
 * 5- ShowDriverSettings() determines if there is a need to fork a new process:
 *    this will be the case if the Audio tab has been displayed, and the
 *    device for which it is to display settings contains mixers.  If either
 *    of these conditions is false, ShowDriverSettings displays the driver's
 *    settings dialog directly (via ConfigureDriver()).
 * 6- ShowDriverSettings() uses WinExec() to fork a new process, using
 *    the routine ShowDriverSettingsAfterFork() as an entry point.  If the
 *    exec fails, ShowDriverSettings() displays the driver's settings dialog
 *    directly (via ConfigureDriver()).
 * PROCESS 1:                           PROCESS 2:
 * 7- Enters WaitForNewCPLWindow(),     1- ShowDriverSettingsAfterFork() will
 *    which will wait up to 5 seconds      receive on its command-line the
 *    for the new MMSYS.CPL process        name of the driver for which
 *    to open a driver Properties          settings have been requested.  It
 *    dialog which matches its own:        opens the primary dialog, using the
 *    if it finds such a dialog,           Devices tab as the initial tab--
 *    WaitForNewCPLWindow() will post      so that the Advanced tab is never
 *    IDCANCEL messages to both the        displayed, and because the Devices
 *    current driver Properties dialog,    tab is the active tab on the other
 *    and to this process's main           process.
 *    dialog, terminating this process. 2- During WM_INITDIALOG of the Devices
 *                                         dialog, this process searches for
 *                                       the previous process' MMSYS.CPL dialog.
 *                                     If successful, it moves this MMSYS.CPL
 *                                   dialog directly behind the previous dialog.
 *                              3- During ID_INIT of the Devices dialog, this
 *                               process searches the TreeView for the driver
 *                             which was named on the comand-line: if found,
 *                           it highlights the TreeItem and simulates a press
 *                         of the Properties button
 *                    4- During WM_INITDIALOG of the device's Properties dialog,
 *                     this process searches for the previous process' device's
 *                   properties dialog.  If successful, it moves this dialog
 *                 directly behind its counterpart.
 *            5- During ID_INIT of the device's Properties dialog, this process
 *             simulates a press of the Settings button
 *        6- When the Settings button is pressed, this process recognizes that
 *         it has been forked and skips the call to ShowDriverSettings(),
 *       instead simply displaying the driver's settings dialog (via
 *     ConfigureDriver()).
 *
 * Let it be known that this is a hack, and should be removed post-beta.
 *
 */

extern BOOL fHaveStartedAudioDialog;    // in MSACMCPL.C

void ShowDriverSettings (HWND hDlg, LPTSTR pszName)
{
    if (fHaveStartedAudioDialog && fDeviceHasMixers (pszName))
    {
        TCHAR  szForkLine[ cchRESOURCE *2 ];

        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.wShowWindow = SW_SHOW;
        si.dwFlags = STARTF_USESHOWWINDOW;

        wsprintf (szForkLine, cszFORKLINE, pszName);

        if (CreateProcess(NULL,szForkLine,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi))
        {
            (void)WaitForNewCPLWindow (hDlg);
        }
        else
        {
            ConfigureDriver (hDlg, pszName);
        }
    }
    else
    {
        ConfigureDriver (hDlg, pszName);
    }
}


void WINAPI ShowDriverSettingsAfterFork (
                                        HWND hwndStub,
                                        HINSTANCE hAppInstance,
                                        LPTSTR lpszCmdLine,
                                        int nCmdShow)
{
    #ifdef UNICODE
    WCHAR szCmdLine[ cchRESOURCE ];
    #else
        #define szCmdLine lpszCmdLine
    #endif

    if(szCmdLine)
        return;

    lstrcpy (szDriverWhichNeedsSettings, szCmdLine);
    cplMMDoubleClick (NULL, 4); // 4==Start on Advanced ("Devices") tab
}

void WINAPI ShowDriverSettingsAfterForkW (
                                         HWND hwndStub,
                                         HINSTANCE hAppInstance,
                                         LPWSTR lpwszCmdLine,
                                         int nCmdShow)
{
    #ifdef UNICODE
        #define szCmdLine lpwszCmdLine
    #else
    CHAR szCmdLine[ cchRESOURCE ];
    wcstombs(szCmdLine, lpwszCmdLine, cchRESOURCE);
    #endif

    lstrcpy (szDriverWhichNeedsSettings, szCmdLine);
    cplMMDoubleClick (NULL, 4); // 4==Start on Advanced ("Devices") tab
}

#endif // FIX_BUG_15451

// Globals to support sound event command line parameters.

#define MAX_SND_EVNT_CMD_LINE 32
TCHAR    gszCmdLineApp[MAX_SND_EVNT_CMD_LINE];
TCHAR    gszCmdLineEvent[MAX_SND_EVNT_CMD_LINE];

/*==========================================================================*/
LONG CPlApplet(
              HWND    hCPlWnd,
              UINT    Msg,
              LPARAM  lParam1,
              LPARAM  lParam2)
{
    switch (Msg)
    {
    case CPL_INIT:
        wHelpMessage = RegisterWindowMessage(TEXT("ShellHelp"));
        DPF_T("*CPL_INIT*");
        g_cRefCnt++;
        return (LRESULT)TRUE;

    case CPL_GETCOUNT:
        return (LRESULT)1;

    case CPL_INQUIRE:
        DPF_T("*CPL_INQUIRE*");
        switch (lParam1)
        {
        case 0:
            ((LPCPLINFO)lParam2)->idIcon = IDI_MMICON;
            ((LPCPLINFO)lParam2)->idName = IDS_MMNAME;
            ((LPCPLINFO)lParam2)->idInfo = IDS_MMINFO;
            break;
        default:
            return FALSE;
        }
        ((LPCPLINFO)lParam2)->lData = 0L;
        return TRUE;

    case CPL_NEWINQUIRE:
        switch (lParam1)
        {
        case 0:
            ((LPNEWCPLINFO)lParam2)->hIcon = LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_MMICON));
            LoadString(ghInstance, IDS_MMNAME, ((LPNEWCPLINFO)lParam2)->szName, sizeof(((LPNEWCPLINFO)lParam2)->szName)/sizeof(TCHAR));
            LoadString(ghInstance, IDS_MMINFO, ((LPNEWCPLINFO)lParam2)->szInfo, sizeof(((LPNEWCPLINFO)lParam2)->szInfo)/sizeof(TCHAR));
            break;
        default:
            return FALSE;
        }
        ((LPNEWCPLINFO)lParam2)->dwHelpContext = 0;
        ((LPNEWCPLINFO)lParam2)->dwSize = sizeof(NEWCPLINFO);
        ((LPNEWCPLINFO)lParam2)->lData = 0L;
        ((LPNEWCPLINFO)lParam2)->szHelpFile[0] = 0;
        return TRUE;

    case CPL_DBLCLK:
        DPF_T("* CPL_DBLCLICK*");
        // Do the applet thing.
        switch (lParam1)
        {
        case 0:
            // Check for obsolete command line (see comments
            // under CPL_STARTWPARAMS)
            if ((-1) == g_nStartPage) break;

#ifdef FIX_BUG_15451
            lstrcpy (szDriverWhichNeedsSettings, TEXT(""));
            cplMMDoubleClick(hCPlWnd, g_nStartPage);
#else // FIX_BUG_15451
            cplMMDoubleClick(hCPlWnd);
#endif // FIX_BUG_15451
            break;
        }
        break;

    case CPL_STARTWPARMS:
        switch (lParam1)
        {
        case 0:
            if (lParam2 && *((LPTSTR)lParam2))
            {
                TCHAR c;

                c = *((LPTSTR)lParam2);
                if (c > TEXT('0') && c < TEXT('5'))
                {
                    g_nStartPage = c - TEXT('0');
                    break;
                }

                // The "S" command line was used on Windows 98 and Windows 98
                // SE.  The command line was written to the Active Setup
                // registry to run logic to setup preferred devices on a
                // specific PnP device instance.  It is obsolete and handled in
                // winmm.dll itself.  This cpl should do nothing for this
                // command line.
                if ((c == TEXT('S')) || (c == TEXT('s'))) {
                    g_nStartPage = (-1);
                    break;
                }
            }
            g_nStartPage = 0;
            break;

            // For sound events, the passed in parameter indicates a module
            // name and event. If a name is passed only show it's sound events.
            // If a name and event are passed only show the event.
            /*        case 1:

                        if (lParam2 && *((LPTSTR)lParam2))
                        {
                            TCHAR *psz;

                            if ((psz = wcschr((LPTSTR)lParam2, TEXT(','))) != NULL)
                            {
                                *psz++ = TEXT('\0');
                                wcsncpy(gszCmdLineEvent, psz, MAX_SND_EVNT_CMD_LINE/sizeof(TCHAR));
                                gszCmdLineEvent[MAX_SND_EVNT_CMD_LINE-sizeof(TCHAR)] = TEXT('\0');
                            }
                            wcsncpy(gszCmdLineApp, (LPTSTR)lParam2,
                                    MAX_SND_EVNT_CMD_LINE/sizeof(TCHAR));
                            gszCmdLineApp[MAX_SND_EVNT_CMD_LINE-sizeof(TCHAR)] = TEXT('\0');
                        }
                        break; */
        }

        break;

    case CPL_EXIT:
        DPF_T("* CPL_EXIT*");
        g_cRefCnt--;
        break;
    }
    return 0;
}


void PASCAL ShowPropSheet(LPCTSTR            pszTitle,
                          DLGPROC            pfnDialog,
                          UINT            idTemplate,
                          HWND            hWndParent,
                          LPTSTR            pszCaption,
                          LPARAM lParam)
{
    PROPSHEETHEADER psh;
    HPROPSHEETPAGE  hpsp[MAXPAGES];


    InitPSH(&psh,hWndParent,pszCaption,hpsp);
    AddPage(&psh, pszTitle,  pfnDialog, idTemplate, lParam);
    PropertySheet(&psh);

}

void PASCAL ShowMidiPropSheet(LPPROPSHEETHEADER ppshExt,
                              LPCTSTR    pszTitle,
                              HWND      hWndParent,
                              short     iMidiPropType,
                              LPTSTR     pszCaption,
                              HTREEITEM hti,
                              LPARAM    lParam1,
                              LPARAM    lParam2)
{
    PROPSHEETHEADER psh;
    LPPROPSHEETHEADER ppsh;
    HPROPSHEETPAGE  hpsp[MAXPAGES];
    static EXTPROPSHEETCBPARAM cbp;

    if (!ppshExt)
    {
        ppsh = &psh;
        InitPSH(ppsh,hWndParent,pszCaption,hpsp);
    }
    else
        ppsh = ppshExt;

    cbp.lParam1 = lParam1;
    cbp.lParam2 = lParam2;
    cbp.hti = hti;
    cbp.ppsh = ppsh;

    if (iMidiPropType == MIDI_CLASS_PROP)
    {
        if (AddMidiPages((LPVOID)pszTitle, MMExtPropSheetCallback, (LPARAM)&cbp))
        {
            PropertySheet(ppsh);
        }
    }
    else if (iMidiPropType == MIDI_INSTRUMENT_PROP)
    {
        if (AddInstrumentPages((LPVOID)pszTitle, MMExtPropSheetCallback, (LPARAM)&cbp))
        {
            PropertySheet(ppsh);
        }
    }
    else
    {
        if (AddDevicePages((LPVOID)pszTitle, MMExtPropSheetCallback, (LPARAM)&cbp))
        {
            PropertySheet(ppsh);
        }
    }
}

void PASCAL ShowWithMidiDevPropSheet(LPCTSTR            pszTitle,
                                     DLGPROC            pfnDialog,
                                     UINT            idTemplate,
                                     HWND            hWndParent,
                                     LPTSTR            pszCaption,
                                     HTREEITEM    hti,
                                     LPARAM lParam, LPARAM lParamExt1, LPARAM lParamExt2)
{
    PROPSHEETHEADER psh;
    HPROPSHEETPAGE  hpsp[MAXPAGES];


    InitPSH(&psh,hWndParent,pszCaption,hpsp);
    AddPage(&psh, pszTitle,  pfnDialog, idTemplate, lParam);
    PropertySheet(&psh);
    // Disabling the details sheet - obsolete 01/10/2001
    //ShowMidiPropSheet(&psh, pszCaption, hWndParent,MIDI_DEVICE_PROP,pszCaption,hti,lParamExt1,lParamExt2);
}

BOOL WINAPI ShowMMCPLPropertySheetW(HWND hwndParent, LPCTSTR pszPropSheetID, LPTSTR pszTabName, LPTSTR pszCaption)
{
    DLGPROC pfnDlgProc;
    UINT    idTemplate;
    HWND    hwndP;
    PROPSHEETHEADER psh;
    HPROPSHEETPAGE  hpsp[MAXPAGES];

    if (GetWindowLongPtr(hwndParent, GWL_EXSTYLE) & WS_EX_TOPMOST)
        hwndP = NULL;
    else
        hwndP = hwndParent;

    InitPSH(&psh,hwndP,pszCaption,hpsp);
    psh.dwFlags = 0;

    if (!lstrcmpi(pszPropSheetID, cszAUDIO))
    {
        pfnDlgProc = AudioDlg;
        idTemplate = AUDIODLG;
        goto ShowSheet;
    }
	if (!lstrcmpi(pszPropSheetID, cszVOICE))
    {
        pfnDlgProc = VoiceDlg;
        idTemplate = VOICEDLG;
        goto ShowSheet;
    } 
	if (!lstrcmpi(pszPropSheetID, cszVOLUME))
    {
        pfnDlgProc = VolumeDlg;
        idTemplate = IDD_VOLUME;
        goto ShowSheet;
    } 
    if (!lstrcmpi(pszPropSheetID, cszVIDEO))
    {
        pfnDlgProc = VideoDlg;
        idTemplate = VIDEODLG;
        goto ShowSheet;
    }
    if (!lstrcmpi(pszPropSheetID, cszCDAUDIO))
    {
        pfnDlgProc = CD_HardwareDlgProc;
        idTemplate = HWDLG;
        goto ShowSheet;
    }
    if (!lstrcmpi(pszPropSheetID, cszMIDI))
    {
    /*
        static EXTPROPSHEETCBPARAM cbpMIDI;

        cbpMIDI.ppsh = &psh;
        cbpMIDI.hti = NULL;
        cbpMIDI.lParam1 = (LPARAM)pszPropSheetID;
        cbpMIDI.lParam2 = (LPARAM)pszTabName;
        AddSimpleMidiPages((LPVOID)pszTabName, MMExtPropSheetCallback, (LPARAM)&cbpMIDI);
        PropertySheet(&psh);
        return TRUE;
      */

        pfnDlgProc = AudioDlg;
        idTemplate = AUDIODLG;
        goto ShowSheet;

    }

    return FALSE;
    ShowSheet:
    AddPage(&psh, pszTabName,  pfnDlgProc, idTemplate, (LPARAM)NULL);
    PropertySheet(&psh);
    return TRUE;
}

BOOL WINAPI ShowMMCPLPropertySheet(HWND hwndParent, LPCSTR pszPropSheetID, LPSTR pszTabName, LPSTR pszCaption)
{
    DLGPROC pfnDlgProc;
    UINT    idTemplate;
    HWND    hwndP;
    PROPSHEETHEADER psh;
    HPROPSHEETPAGE  hpsp[MAXPAGES];
    TCHAR szPropSheetID[MAX_PATH];
    TCHAR szTabName[MAX_PATH];
    TCHAR szCaption[MAX_PATH];

    //convert three params into UNICODE strings
    MultiByteToWideChar( GetACP(), 0, pszPropSheetID, -1, szPropSheetID, sizeof(szPropSheetID) / sizeof(TCHAR) );
    MultiByteToWideChar( GetACP(), 0, pszTabName,     -1, szTabName,     sizeof(szTabName)     / sizeof(TCHAR) );
    MultiByteToWideChar( GetACP(), 0, pszCaption,     -1, szCaption,     sizeof(szCaption)     / sizeof(TCHAR) );

    return (ShowMMCPLPropertySheetW(hwndParent,szPropSheetID,szTabName,szCaption));
}

//allows you to show control panel from RUNDLL32
DWORD WINAPI ShowFullControlPanel(HWND hwndP, HINSTANCE hInst, LPTSTR szCmd, int nShow)
{
    cplMMDoubleClick(hwndP, 0);
    return 0;
}

DWORD WINAPI ShowAudioPropertySheet(HWND hwndP, HINSTANCE hInst, LPTSTR szCmd, int nShow)
{
    TCHAR szAudio[MAXLNAME];
    TCHAR szAudioProperties[MAXLNAME];
    char mbcszAUDIO[MAXLNAME];
    char mbszAudio[MAXLNAME];
    char mbszAudioProperties[MAXLNAME];
    HWND hwndPrev;

    LoadString(ghInstance, IDS_AUDIOPROPERTIES, szAudioProperties, sizeof(szAudioProperties)/sizeof(TCHAR));
    hwndPrev = FindWindow(NULL,szAudioProperties);
    if (hwndPrev)
    {
        SetForegroundWindow(hwndPrev);
    }
    else
    {
        LoadString(ghInstance, IDS_WAVE_HEADER, szAudio, sizeof(szAudio)/sizeof(TCHAR));
        ShowMMCPLPropertySheetW(hwndP, cszAUDIO, szAudio, szAudioProperties);
    }
    return 0;
}


DWORD WINAPI mmseRunOnce(HWND hwnd, HINSTANCE hInst, LPSTR lpszCmdLine, int nShow)
{
    // This is an obsolete function formerly used to migrate
    // registry and driver information.  We leave the export
    // in place in case an old installation has left a RunOnce
    // command in place to execute this function.
    return 0;
}

DWORD WINAPI mmseRunOnceW(HWND hwnd, HINSTANCE hInst, LPWSTR lpwszCmdLine, int nShow)
{
    // This is an obsolete function formerly used to migrate
    // registry and driver information.  We leave the export
    // in place in case an old installation has left a RunOnce
    // command in place to execute this function.
    return 0;
}

extern BOOL DriversDllInitialize (IN PVOID, IN DWORD, IN PCONTEXT OPTIONAL);

BOOL DllInitialize (IN PVOID hInstance,
                    IN DWORD ulReason,
                    IN PCONTEXT pctx OPTIONAL)
{
    // patch in the old DRIVERS.DLL code (see DRIVERS.C)
    //
    DriversDllInitialize (hInstance, ulReason, pctx);

    if (ulReason == DLL_PROCESS_ATTACH)
    {
        ++g_cProcesses;
        ghInstance = hInstance;
        DisableThreadLibraryCalls(hInstance);
        return TRUE;
    }

    if (ulReason == DLL_PROCESS_DETACH)
    {
        --g_cProcesses;
        return TRUE;
    }

    return TRUE;
}

DWORD
    WINAPI
    MediaClassInstaller(
                       IN DI_FUNCTION      InstallFunction,
                       IN HDEVINFO         DeviceInfoSet,
                       IN PSP_DEVINFO_DATA DeviceInfoData OPTIONAL
                       )
/*++

Routine Description:

    This routine acts as the class installer for Media devices.

Arguments:

    InstallFunction - Specifies the device installer function code indicating
        the action being performed.

    DeviceInfoSet - Supplies a handle to the device information set being
        acted upon by this install action.

    DeviceInfoData - Optionally, supplies the address of a device information
        element being acted upon by this install action.

Return Value:

    If this function successfully completed the requested action, the return
        value is NO_ERROR.

    If the default behavior is to be performed for the requested action, the
        return value is ERROR_DI_DO_DEFAULT.

    If an error occurred while attempting to perform the requested action, a
        Win32 error code is returned.

--*/
{
    DWORD dwRet=ERROR_DI_DO_DEFAULT;

    switch (InstallFunction)
    {

    case DIF_SELECTBESTCOMPATDRV:
        dwRet = Media_SelectBestCompatDrv(DeviceInfoSet,DeviceInfoData);
        break;

    case DIF_ALLOW_INSTALL:
        dwRet = Media_AllowInstall(DeviceInfoSet,DeviceInfoData);
        break;

    case DIF_INSTALLDEVICE :
        dwRet = Media_InstallDevice(DeviceInfoSet, DeviceInfoData);
        break;

    case DIF_REMOVE:
        dwRet = Media_RemoveDevice(DeviceInfoSet,DeviceInfoData);
        break;

    case DIF_SELECTDEVICE:
        dwRet = Media_SelectDevice(DeviceInfoSet,DeviceInfoData);
        break;

    case DIF_FIRSTTIMESETUP:
        // Fall through

    case DIF_DETECT:
        dwRet = Media_MigrateLegacy(DeviceInfoSet,DeviceInfoData);
        break;

    }

    return dwRet;

}

DWORD WINAPI mmWOW64MediaInstallDevice(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData)
{
    SP_DEVINSTALL_PARAMS DeviceInstallParams;
    HWND hWnd;

    //
    // Get the device install parameters, so we'll know what parent window to use for any
    // UI that occurs during configuration of this device.
    //
    DeviceInstallParams.cbSize = sizeof(DeviceInstallParams);
    if (SetupDiGetDeviceInstallParams(DeviceInfoSet, DeviceInfoData, &DeviceInstallParams))
    {
        hWnd = DeviceInstallParams.hwndParent;
    }
    else
    {
        hWnd = NULL;
    }

    //
    // The INF will have created a "Drivers" subkey under the device's software key.
    // This tree, in turn, contains subtrees for each type of driver (aux, midi, etc.)
    // applicable for this device.  We must now traverse this tree, and create entries
    // in Drivers32 for each function alias.
    //
    return InstallDriversForPnPDevice(hWnd, DeviceInfoSet, DeviceInfoData);
}


DWORD WINAPI mmWOW64MediaRemoveDevice(HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData)
{
    if( RemoveDriver(DeviceInfoSet, DeviceInfoData) )
    {
        return NO_ERROR;
    }
    else
    {
        return ERROR_BAD_DRIVER;
    }
}


DWORD WINAPI mmWOW64MediaClassInstallerA(HWND hwnd, HINSTANCE hInst, LPSTR lpszCmdLine, int nShow)
/*++

Routine Description:

    This routine acts as a thunking layer for calling the 32-bit
        installation functions from the 64-bit setup via RunDLL32.exe.

Arguments:

    hwnd - not used

    hInst - not used

    lpwszCmdLine - command line arguments: "Instance ID (string)" DI_FUNCTION (as an integer)

    nShow - not used

Return Value:

    If this function successfully completed the requested action, the return
        value is NO_ERROR.

    If an error occurred while attempting to perform the requested action, a
        Win32 error code is returned.

--*/
{
    LPSTR strInstanceID = NULL;
    LPSTR strInstallIndex = NULL;
    DWORD dwInstallIndex = 0;
    LPSTR strTemp = NULL;
    HDEVINFO DeviceInfoSet = NULL;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD dwResult = NO_ERROR;

    // Find first quote
    strTemp = strchr( lpszCmdLine, '\"' );
    if( !strTemp )
    {
        return ERROR_INVALID_PARAMETER;
    }


    // Instance ID

    // Skip first quote
    strInstanceID = ++strTemp;

    // Find second quote
    strTemp = strchr( strTemp, '\"' );
    if( !strTemp )
    {
        return ERROR_INVALID_PARAMETER;
    }

    // NULL-terminate the InstanceID
    *strTemp = 0;


    // Install Index

    // Skip the NULL
    strInstallIndex = ++strTemp;


    // Convert the installation index
    dwInstallIndex = atoi( strInstallIndex );


    // Create a device handle
    DeviceInfoSet = SetupDiCreateDeviceInfoList( NULL, NULL );
    if( INVALID_HANDLE_VALUE == DeviceInfoSet )
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    // Create the device info structure
    ZeroMemory( &DeviceInfoData, sizeof(SP_DEVINFO_DATA) );
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if( 0 == SetupDiOpenDeviceInfoA( DeviceInfoSet, strInstanceID, NULL, 0, &DeviceInfoData ) )
    {
        dwResult = GetLastError();
    }

    // Do the installation task
    if( NO_ERROR == dwResult )
    {
        switch( dwInstallIndex )
        {
        case DIF_INSTALLDEVICE:
            dwResult = mmWOW64MediaInstallDevice(DeviceInfoSet, &DeviceInfoData);
            break;
        case DIF_REMOVE:
            dwResult = mmWOW64MediaRemoveDevice(DeviceInfoSet, &DeviceInfoData);
            break;
        default:
            dwResult = ERROR_INVALID_PARAMETER;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList( DeviceInfoSet );

    return dwResult;
}
