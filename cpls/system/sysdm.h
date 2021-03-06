/*++

Microsoft Confidential
Copyright (c) 1992-1997  Microsoft Corporation
All rights reserved

Module Name:

    sysdm.h

Abstract:

    Applet-wide declaraions and definitions for the 
    System Control Panel Applet.

Author:

    Eric Flo (ericflo) 19-Jun-1995

Revision History:

    15-Oct-1997 scotthal
        Complete overhaul

--*/
#ifndef _SYSDM_H_
#define _SYSDM_H_

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <ntexapi.h>
#include <windows.h>
#include <commctrl.h>
#include <comctrlp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlobjp.h>
#include <shlwapi.h>
#include <shlwapip.h>
#include <cpl.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include "resource.h"
#include "helpid.h"
#include "util.h"
#include "sid.h"
#include "general.h"
#include "netid.h"
#include "hardware.h"
#include "hwprof.h"
#include "profile.h"
#include "advanced.h"
#include "perf.h"
#include "virtual.h"
#include "startup.h"
#include "envvar.h"
#include "edtenvar.h"
#include "syspart.h"
#include "pfrscpl.h"
#include "srcfg.h"
#include "visualfx.h"

//
// Global variables
//
extern TCHAR g_szErrMem[ 200 ];         //  Low memory message
extern TCHAR g_szSystemApplet[ 100 ];   //  "System Control Panel Applet" title
extern HINSTANCE hInstance;
extern TCHAR g_szNull[];
extern BOOL g_fRebootRequired;


//
// Macros
//

#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))

#define SetLBWidth( hwndLB, szStr, cxCurWidth )     SetLBWidthEx( hwndLB, szStr, cxCurWidth, 0)

#define IsPathSep(ch)       ((ch) == TEXT('\\') || (ch) == TEXT('/'))
#define IsWhiteSpace(ch)    ((ch) == TEXT(' ') || (ch) == TEXT('\t') || (ch) == TEXT('\n') || (ch) == TEXT('\r'))
#define IsDigit(ch)         ((ch) >= TEXT('0') && (ch) <= TEXT('9'))

#define DigitVal(ch)        ((ch) - TEXT('0'))

#define MAX_PAGES           16  // Arbitrary Maximum number of pages in the System Control Panel.


typedef HPROPSHEETPAGE (*PSPCALLBACK)(int idd, DLGPROC pfnDlgProc);

typedef struct
{
    PSPCALLBACK pfnCreatePage;
    int idd;
    DLGPROC pfnDlgProc;
}
PSPINFO;

HPROPSHEETPAGE CreatePage(int idd, DLGPROC pfnDlgProc);


//
// Debugging macros
//
#if DBG
#   define  DBG_CODE    1

void DbgPrintf( LPTSTR szFmt, ... );
void DbgStopX(LPSTR mszFile, int iLine, LPTSTR szText );

#   define  DBGSTOP( t )        DbgStopX( __FILE__, __LINE__, TEXT(t) )
#   define  DBGSTOPX( f, l, t ) DbgStopX( f, l, TEXT(t) )
#   define  DBGPRINTF(p)        DbgPrintf p
#   define  DBGOUT(t)           DbgPrintf( TEXT("SYSDM.CPL: %s\n"), TEXT(t) )
#else

#   define  DBGSTOP( t )
#   define  DBGSTOPX( f, l, t )
#   define  DBGPRINTF(p)
#   define  DBGOUT(t)
#endif

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // _SYSDM_H_
