//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1992 - 1993.
//
//  File:       api.cxx
//
//  Contents:   Exported APIs from this DLL
//
//  History:    5-Oct-95 BruceFo  Created
//
//--------------------------------------------------------------------------

#include "headers.hxx"
#pragma hdrstop

#include "resource.h"
#include "dllmain.hxx"
#include "shrpage.hxx"
#include "shrinfo.hxx"
#include "cache.hxx"
#include "util.hxx"

//+-------------------------------------------------------------------------
//
//  Function:   IsPathSharedW
//
//  Synopsis:   IsPathShared is used by the shell to determine whether to
//              put a "shared folder / hand" icon next to a directory.
//              Different from Windows 95, we don't allow sharing remote
//              directories (e.g., \\brucefo4\c$\foo\bar style paths).
//
//  Arguments:  [lpcszPath] - path to look for
//              [bRefresh]  - TRUE if cache should be refreshed
//
//  Returns:    TRUE if the path is shared, FALSE otherwise
//
//  History:    4-Apr-95    BruceFo  Created
//
//--------------------------------------------------------------------------

STDAPI_(BOOL)
IsPathSharedW(
    LPCWSTR lpcszPath,
    BOOL bRefresh
    )
{
    InterlockedIncrement((long*)&g_NonOLEDLLRefs);

    appDebugOut((DEB_TRACE,"IsPathSharedW(%ws, %d)\n", lpcszPath, bRefresh));

    OneTimeInit();
    BOOL bSuccess = g_ShareCache.IsPathShared(lpcszPath, bRefresh);

    appDebugOut((DEB_TRACE,
        "IsPathShared(%ws, %ws) = %ws\n",
        lpcszPath,
        bRefresh ? L"refresh" : L"no refresh",
        bSuccess ? L"yes" : L"no"));

    appAssert( 0 != g_NonOLEDLLRefs );
    InterlockedDecrement((long*)&g_NonOLEDLLRefs);
    return bSuccess;
}


//+-------------------------------------------------------------------------
//
//  Function:   IsPathSharedA
//
//  Synopsis:   See IsPathSharedW
//
//  Arguments:  See IsPathSharedW
//
//  Returns:    See IsPathSharedW
//
//  History:    1-Mar-96    BruceFo  Created
//              27-Feb-02   JeffreyS Stubbed out
//
//--------------------------------------------------------------------------

STDAPI_(BOOL)
IsPathSharedA(
    LPCSTR /*lpcszPath*/,
    BOOL /*bRefresh*/
    )
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

//+-------------------------------------------------------------------------
//
//  Function:   SharingDialog
//
//  Synopsis:   This API brings up the "Sharing" dialog. This entrypoint is
//              only used by the FAX team, as far as I know. Note that the
//              paths passed in are ANSI---that's because that's what they
//              were in Win95 when the API was defined.
//
//              This API, on NT, only works locally. It does not do remote
//              sharing, as Win95 does. Thus, the pszComputerName parameter
//              is ignored.
//
//  Arguments:  hwndParent      -- parent window
//              pszComputerName -- a computer name. This is ignored!
//              pszPath         -- the path to share.
//
//  Returns:    TRUE if everything went OK, FALSE otherwise
//
//  History:    5-Oct-95    BruceFo  Created
//
//--------------------------------------------------------------------------

STDAPI_(BOOL)
SharingDialogW(
    HWND    hwndParent,
    LPCWSTR pszComputerName,
    LPCWSTR pszPath
    )
{
    InterlockedIncrement((long*)&g_NonOLEDLLRefs);
    appDebugOut((DEB_TRACE,"SharingDialogW(%ws)\n", pszPath));

    BOOL bReturn = FALSE;

    // Parameter validation
    if (NULL != pszComputerName)
    {
        appDebugOut((DEB_TRACE,
            "SharingDialog() API called with a computer name which will be ignored\n"));
    }

    if (NULL != pszPath)
    {
        // Make sure the DLL is initialized.
        OneTimeInit(TRUE);

        CSharingPropertyPage* pPage = new CSharingPropertyPage(TRUE);
        if (NULL != pPage)
        {
            HRESULT hr = pPage->InitInstance(pszPath);
            if (SUCCEEDED(hr))
            {
                PROPSHEETPAGE psp;

                psp.dwSize      = sizeof(psp);    // no extra data.
                psp.dwFlags     = PSP_USEREFPARENT | PSP_USECALLBACK;
                psp.hInstance   = g_hInstance;
                psp.pszTemplate = MAKEINTRESOURCE(IDD_SHARE_PROPERTIES);
                psp.hIcon       = NULL;
                psp.pszTitle    = NULL;
                psp.pfnDlgProc  = CSharingPropertyPage::DlgProcPage;
                psp.lParam      = (LPARAM)pPage;  // transfer ownership
                psp.pfnCallback = CSharingPropertyPage::PageCallback;
                psp.pcRefParent = &g_NonOLEDLLRefs;

                INT_PTR ret = DialogBoxParam(
                                g_hInstance,
                                MAKEINTRESOURCE(IDD_SHARE_PROPERTIES),
                                hwndParent,
                                CSharingPropertyPage::DlgProcPage,
                                (LPARAM)&psp);
                if (-1 != ret)
                {
                    bReturn = TRUE;
                }
            }

            pPage->Release();
        }
    }

    appAssert( 0 != g_NonOLEDLLRefs );
    InterlockedDecrement((long*)&g_NonOLEDLLRefs);
    return bReturn;
}

//+-------------------------------------------------------------------------
//
//  Function:   SharingDialogA
//
//  Synopsis:   see SharingDialogW
//
//  Arguments:  see SharingDialogW
//
//  Returns:    see SharingDialogW
//
//  History:    1-Mar-96    BruceFo  Created
//              27-Feb-02   JeffreyS Stubbed out
//
//--------------------------------------------------------------------------

STDAPI_(BOOL)
SharingDialogA(
    HWND   /*hwndParent*/,
    LPCSTR /*pszComputerName*/,
    LPCSTR /*pszPath*/
    )
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


DWORD
CopyShareNameToBuffer(
    IN     CShareInfo* p,
    IN OUT LPWSTR lpszNameBuf,
    IN     DWORD cchNameBufLen
    )
{
    appAssert(NULL != lpszNameBuf);
    appAssert(0 != cchNameBufLen);

    WCHAR szLocalComputer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD nSize = ARRAYLEN(szLocalComputer);
    if (!GetComputerName(szLocalComputer, &nSize))
    {
        return GetLastError();
    }

    DWORD computerLen = lstrlenW(szLocalComputer);
    DWORD shareLen    = lstrlenW(p->GetNetname());

    /* Two slashes + server name + slash + share name + null terminator. */
    if (2 + computerLen + 1 + shareLen + 1 > cchNameBufLen)
    {
        return ERROR_MORE_DATA;
    }

    /* Return network resource name as UNC path. */

    lpszNameBuf[0] = L'\\';
    lpszNameBuf[1] = L'\\';
    lpszNameBuf += 2;
    cchNameBufLen -= 2;

    lstrcpynW(lpszNameBuf, szLocalComputer, cchNameBufLen);
    lpszNameBuf += computerLen;
    cchNameBufLen -= computerLen;
    
    *lpszNameBuf++ = L'\\';
    cchNameBufLen--;

    lstrcpynW(lpszNameBuf, p->GetNetname(), cchNameBufLen);

    return ERROR_SUCCESS;
}


//+-------------------------------------------------------------------------
//
//  Function:   GetNetResourceFromLocalPathW
//
//  Synopsis:   Used by shell link tracking code.
//
//  Arguments:  [lpcszPath]      Path we're concerned about.
//              [lpszNameBuf]    If path is shared, UNC path to share goes here.
//              [cchNameBufLen] length of lpszNameBuf buffer in characters
//              [pdwNetType]     net type of local server, e.g., WNNC_NET_LANMAN
//
//  Returns:    TRUE if path is shared and net resource information
//              returned, else FALSE.
//
//  Notes:      *lpszNameBuf and *pwNetType are only valid if TRUE is returned.
//
//  Example:    If c:\documents is shared as MyDocs on machine Scratch, then
//              calling GetNetResourceFromLocalPath(c:\documents, ...) will
//              set lpszNameBuf to \\Scratch\MyDocs.
//
//  History:    3-Mar-96    BruceFo  Created from Win95 sources
//
//--------------------------------------------------------------------------

STDAPI_(BOOL)
GetNetResourceFromLocalPathW(
    IN     LPCWSTR lpcszPath,
    IN OUT LPWSTR lpszNameBuf,
    IN     DWORD cchNameBufLen,
    OUT    PDWORD pdwNetType
    )
{
    InterlockedIncrement((long*)&g_NonOLEDLLRefs);
    appDebugOut((DEB_TRACE,"GetNetResourceFromLocalPathW(%ws)\n", lpcszPath));

    // do some parameter validation
    if (NULL == lpcszPath || NULL == lpszNameBuf || NULL == pdwNetType || 0 == cchNameBufLen)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        appAssert( 0 != g_NonOLEDLLRefs );
        InterlockedDecrement((long*)&g_NonOLEDLLRefs);
        return FALSE;
    }

    OneTimeInit();

    // Parameters seem OK (pointers might still point to bad memory);
    // do the work.

    CShareInfo* pShareList = new CShareInfo();  // dummy head node
    if (NULL == pShareList)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        appAssert( 0 != g_NonOLEDLLRefs );
        InterlockedDecrement((long*)&g_NonOLEDLLRefs);
        return FALSE;   // out of memory
    }

    BOOL bReturn = FALSE;
    DWORD dwLastError;
    DWORD cShares;
    HRESULT hr = g_ShareCache.ConstructList(lpcszPath, pShareList, &cShares);
    if (SUCCEEDED(hr))
    {
        // Now, we have a list of (possibly zero) shares. The user is asking for
        // one of them. Give them the first normal, non-special share. If there
        // doesn't exist a non-special share, then give them a special share.

        if (cShares > 0)
        {
            BOOL bFoundOne = FALSE;
            CShareInfo* p;

            for (p = (CShareInfo*) pShareList->Next();
                 p != pShareList;
                 p = (CShareInfo*) p->Next())
            {
                if (p->GetType() == STYPE_DISKTREE)
                {
                    // found a share for this one.
                    bFoundOne = TRUE;
                    break;
                }
            }

            if (!bFoundOne)
            {
                for (p = (CShareInfo*) pShareList->Next();
                     p != pShareList;
                     p = (CShareInfo*) p->Next())
                {
                    if (p->GetType() == (STYPE_SPECIAL | STYPE_DISKTREE))
                    {
                        bFoundOne = TRUE;
                        break;
                    }
                }
            }

            if (bFoundOne)
            {
                dwLastError = CopyShareNameToBuffer(p, lpszNameBuf, cchNameBufLen);
                if (ERROR_SUCCESS == dwLastError)
                {
                    bReturn = TRUE;
                    *pdwNetType = WNNC_NET_LANMAN; // we only support LanMan
                }
            }
            else
            {
                // nothing found!
                dwLastError = ERROR_BAD_NET_NAME;
            }
        }
        else
        {
            dwLastError = ERROR_BAD_NET_NAME;
        }
    }
    else
    {
        dwLastError = ERROR_OUTOFMEMORY;
    }

    DeleteShareInfoList(pShareList, TRUE);

    if (!bReturn)
    {
        SetLastError(dwLastError);
    }

    appAssert( 0 != g_NonOLEDLLRefs );
    InterlockedDecrement((long*)&g_NonOLEDLLRefs);
    return bReturn;
}


STDAPI_(BOOL)
GetNetResourceFromLocalPathA(
    IN     LPCSTR /*lpcszPath*/,
    IN OUT LPSTR  /*lpszNameBuf*/,
    IN     DWORD  /*cchNameBufLen*/,
    OUT    PDWORD /*pdwNetType*/
    )
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


//+-------------------------------------------------------------------------
//
//  Function:   GetLocalPathFromNetResourceW
//
//  Synopsis:   Used by shell link tracking code.
//
//  Arguments:  [lpcszName]     A UNC path we're concerned about.
//              [dwNetType]     net type of local server, e.g., WNNC_NET_LANMAN
//              [lpszLocalPathBuf]   Buffer to place local path of UNC path
//              [cchLocalPathBufLen] length of lpszLocalPathBuf buffer in
//                                   characters
//              [pbIsLocal]     Set to TRUE if lpcszName points to a local
//                              resource.
//
//  Returns:
//
//  Notes:      *lpszLocalPathBuf and *pbIsLocal are only valid if
//              TRUE is returned.
//
//  Example:    If c:\documents is shared as MyDocs on machine Scratch, then
//              calling GetLocalPathFromNetResource(\\Scratch\MyDocs, ...) will
//              set lpszLocalPathBuf to c:\documents.
//
//  History:    3-Mar-96    BruceFo  Created from Win95 sources
//
//--------------------------------------------------------------------------

STDAPI_(BOOL)
GetLocalPathFromNetResourceW(
    IN     LPCWSTR lpcszName,
    IN     DWORD dwNetType,
    IN OUT LPWSTR lpszLocalPathBuf,
    IN     DWORD cchLocalPathBufLen,
    OUT    PBOOL pbIsLocal
    )
{
    InterlockedIncrement((long*)&g_NonOLEDLLRefs);
    appDebugOut((DEB_TRACE,"GetLocalPathFromNetResourceW(%ws)\n", lpcszName));
    OneTimeInit();

    BOOL bReturn = FALSE;
    DWORD dwLastError;

    *pbIsLocal = FALSE;

    if (g_fSharingEnabled)
    {
        if (0 != dwNetType && HIWORD(dwNetType) == HIWORD(WNNC_NET_LANMAN))
        {
            /* Is the network resource name a UNC path on this machine? */

            WCHAR szLocalComputer[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD nSize = ARRAYLEN(szLocalComputer);
            if (!GetComputerName(szLocalComputer, &nSize))
            {
                dwLastError = GetLastError();
            }
            else
            {
                dwLastError = ERROR_BAD_NET_NAME;

                DWORD dwLocalComputerLen = lstrlenW(szLocalComputer);
                if (   lpcszName[0] == L'\\'
                    && lpcszName[1] == L'\\'
                    && (0 == _wcsnicmp(lpcszName + 2, szLocalComputer, dwLocalComputerLen))
                    )
                {
                    LPCWSTR lpcszSep = &(lpcszName[2 + dwLocalComputerLen]);
                    if (*lpcszSep == L'\\')
                    {
                        *pbIsLocal = TRUE;

                        WCHAR szLocalPath[MAX_PATH];
                        if (g_ShareCache.IsExistingShare(lpcszSep + 1, NULL, szLocalPath, ARRAYLEN(szLocalPath)))
                        {
                            if (wcslen(szLocalPath) < cchLocalPathBufLen)
                            {
                                lstrcpynW(lpszLocalPathBuf, szLocalPath, cchLocalPathBufLen);
                                dwLastError = ERROR_SUCCESS;
                                bReturn = TRUE;
                            }
                            else
                            {
                                dwLastError = ERROR_MORE_DATA;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            dwLastError = ERROR_BAD_PROVIDER;
        }
    }
    else
    {
        appDebugOut((DEB_TRACE,"GetLocalPathFromNetResourceW: sharing not enabled\n"));
        dwLastError = ERROR_BAD_NET_NAME;
    }

    if (!bReturn)
    {
        SetLastError(dwLastError);
    }

    appAssert( 0 != g_NonOLEDLLRefs );
    InterlockedDecrement((long*)&g_NonOLEDLLRefs);
    return bReturn;
}


STDAPI_(BOOL)
GetLocalPathFromNetResourceA(
    IN     LPCSTR /*lpcszName*/,
    IN     DWORD  /*dwNetType*/,
    IN OUT LPSTR  /*lpszLocalPathBuf*/,
    IN     DWORD  /*cchLocalPathBufLen*/,
    OUT    PBOOL  /*pbIsLocal*/
    )
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

STDAPI CanShareFolderW(LPCWSTR pszPath)
{
    InterlockedIncrement((long*)&g_NonOLEDLLRefs);
    appDebugOut((DEB_TRACE,"CanShareFolderW(%s)\n", pszPath));
    OneTimeInit();

    HRESULT hr = S_FALSE;

    if (g_fSharingEnabled || IsSimpleUI())
    {
        if ( (pszPath[0] >= L'A' && pszPath[0] <= L'Z') && pszPath[1] == L':')
        {
            WCHAR szRoot[4];

            szRoot[0] = pszPath[0];
            szRoot[1] = TEXT(':');
            szRoot[2] = TEXT('\\');
            szRoot[3] = 0;

            UINT uType = GetDriveType(szRoot);

            switch (uType)
            {
                case DRIVE_UNKNOWN:
                case DRIVE_NO_ROOT_DIR:
                case DRIVE_REMOTE:
                    hr = S_FALSE;
                    break;
        
                case DRIVE_FIXED:
                case DRIVE_REMOVABLE:
                    {
                        WCHAR szDesktopIni[MAX_PATH];
                        if (PathCombine(szDesktopIni, pszPath, TEXT("desktop.ini")))
                        {
                            hr = GetPrivateProfileInt(TEXT(".ShellClassInfo"), TEXT("Sharing"), TRUE, szDesktopIni) ? S_OK : S_FALSE;
                        }
                        else
                        {
                            hr = HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
                        }
                    }
                    break;

                default:
                   hr = S_OK;
                   break;
            }

            //
            // NTRAID#NTBUG9-353119-2001/04/10-jeffreys
            //
            // We need to call PathIsDirectory to prevent the "Share this
            // folder" task from appearing in the webview pane of CAB and
            // ZIP folders. (NTBUG9 #319149 and 319153)
            //
            // However, PathIsDirectory fails with ERROR_NOT_READY on an
            // empty CD or removable drive, which is a case we want to allow
            // or the Sharing page will not show. (NTBUG9 #353119)
            //
            if (S_OK == hr && !PathIsDirectory(pszPath))
            {
                hr = S_FALSE;

                if (GetLastError() == ERROR_NOT_READY &&
                    (DRIVE_CDROM == uType || DRIVE_REMOVABLE == uType) &&
                    PathIsRootW(pszPath))
                {
                    // Ok to share an empty CD or removable drive
                    hr = S_OK;
                }
            }
        }
    }

    appAssert( 0 != g_NonOLEDLLRefs );
    InterlockedDecrement((long*)&g_NonOLEDLLRefs);

    return hr;
}

STDAPI ShowShareFolderUIW(HWND hwndParent, LPCWSTR pszPath)
{
    InterlockedIncrement((long*)&g_NonOLEDLLRefs);
    appDebugOut((DEB_TRACE,"ShowShareFolderUIW(%s)\n", pszPath));

    TCHAR szShare[50];
    LoadString(g_hInstance, IDS_MSGTITLE, szShare, ARRAYLEN(szShare));
    SHObjectProperties(hwndParent, SHOP_FILEPATH, pszPath, szShare);

    appAssert( 0 != g_NonOLEDLLRefs );
    InterlockedDecrement((long*)&g_NonOLEDLLRefs);

    return S_OK;
}
