//
//  Link.C
//
//  Copyright (C) Microsoft, 1994,1995 All Rights Reserved.
//
//  History:
//  ral 6/23/94 - First pass
//  3/20/95  [stevecat] - NT port & real clean up, unicode, etc.
//
//
#include "priv.h"
#include "appwiz.h"

const static TCHAR szExplorer[] = TEXT("Explorer");
const static TCHAR szExpSelParams[] = TEXT("/Select,");

//
//  Returns the fully-qualified path name of the link.
//
BOOL GetLinkName(LPTSTR lpszLinkName, UINT cchLinkName, LPWIZDATA lpwd)
{
    if (PathCombine(lpszLinkName, lpwd->lpszFolder, lpwd->szProgDesc) == NULL )
        return( FALSE );

#ifdef NO_NEW_SHORTCUT_HOOK
    StringCchCat(lpszLinkName, cchLinkName, (lpwd->dwFlags & WDFLAG_DOSAPP) ? c_szPIF : c_szLNK);
#else
    if ((lstrlen(lpszLinkName) + lstrlen(lpwd->szExt)) >= MAX_PATH)
        return FALSE;

    StringCchCat(lpszLinkName, cchLinkName, lpwd->szExt);
#endif

    return( TRUE );
}


//
//  Opens the folder of the newly created link.
//

BOOL OpenLinkFolder(LPWIZDATA lpwd, LPTSTR lpszLinkName)
{
    SHELLEXECUTEINFO ei;

    TCHAR szParams[MAX_PATH];

    StringCchPrintf(szParams, ARRAYSIZE(szParams), TEXT("%s%s"), szExpSelParams, lpszLinkName);

    ei.cbSize = sizeof(ei);
    ei.hwnd = lpwd->hwnd;
    ei.fMask = 0;
    ei.lpVerb = NULL;
    ei.lpFile = szExplorer;
    ei.lpParameters = szParams;
    ei.lpDirectory = NULL;
    ei.lpClass = NULL;
    ei.nShow = SW_SHOWDEFAULT;
    ei.hInstApp = g_hinst;

    return(ShellExecuteEx(&ei));
}


//
//  Cretates a link.
//

BOOL CreateLink(LPWIZDATA lpwd)
{
    BOOL          bWorked = FALSE;
    IShellLink   *psl;
    HCURSOR       hcurOld = SetCursor(LoadCursor(NULL, IDC_WAIT));
    IPersistFile *ppf;
    TCHAR         szLinkName[MAX_PATH];
    WCHAR         wszPath[MAX_PATH];

    szLinkName[0] = TEXT('\0');
    GetLinkName(szLinkName, ARRAYSIZE(szLinkName), lpwd);

    if (lpwd->lpszOriginalName)
    {
        if (PathFileExists(szLinkName))
        {
            DeleteFile(lpwd->lpszOriginalName);
            SHChangeNotify(SHCNE_DELETE, SHCNF_FLUSH | SHCNF_PATH,
                        lpwd->lpszOriginalName, NULL);
        }
        else
        {
            // we use full pidls here since simple net pidls fail to compare to full net pidls,
            // and thus the changenotify will never make it to the client and it will not update.
            LPITEMIDLIST pidlOriginal = ILCreateFromPath(lpwd->lpszOriginalName);   // need to do this before the move!
            LPITEMIDLIST pidlLink = NULL;

            if (MoveFile(lpwd->lpszOriginalName, szLinkName))
            {
                pidlLink = ILCreateFromPath(szLinkName);    // need to do this after the move (or it won't exist)!

                if (pidlOriginal && pidlLink)
                {
                    SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_FLUSH | SHCNF_IDLIST, pidlOriginal, pidlLink);
                }
                else
                {
                    TraceMsg(TF_ERROR, "%s", "Unable to generate pidls for rename notify");
                    SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_FLUSH | SHCNF_PATH, lpwd->lpszOriginalName, szLinkName);
                }
            }
            else
            {
                TraceMsg(TF_ERROR, "%s", "Unable to rename link -- Will end up with two");
                TraceMsg(TF_ERROR, "%s", szLinkName);
            }
            
            if (pidlOriginal)
                ILFree(pidlOriginal);

            if (pidlLink)
                ILFree(pidlLink);
        }

        //
        // Now get rid of this in case we fail later and then re-enter
        // this routine later.
        //

        lpwd->lpszOriginalName = NULL;
    }

    //
    //    If we're just supposed to copy it, it's simple!
    //

    if (lpwd->dwFlags & WDFLAG_COPYLINK)
    {
        bWorked = CopyFile(lpwd->szExeName, szLinkName, FALSE);
        goto ExitNoFree;
    }

#ifndef NO_NEW_SHORTCUT_HOOK
    if (lpwd->pnshhk)
    {
        //
        // The object is ready to be saved to a file.
        //

        if (FAILED(lpwd->pnshhk->lpVtbl->QueryInterface(lpwd->pnshhk, &IID_IPersistFile, &ppf)))
            goto ExitFreePSL;
    }
    else
        if (lpwd->pnshhkA)
        {
            //
            // The object is ready to be saved to a file.
            //

            if (FAILED(lpwd->pnshhkA->lpVtbl->QueryInterface(lpwd->pnshhkA, &IID_IPersistFile, &ppf)))
                goto ExitFreePSL;
        }
        else
    {
#endif
        //
        //    We didn't do a simple copy.  Now do the full-blown create.
        //
        if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (void **)&psl)))
        {
            TraceMsg(TF_ERROR, "%s", "Could not create instance of IShellLink");
            goto ExitNoFree;
        }

        if (FAILED(psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, &ppf)))
        {
            goto ExitFreePSL;
        }

        psl->lpVtbl->SetPath(psl, lpwd->szExeName);

        psl->lpVtbl->SetArguments(psl, lpwd->szParams);

        psl->lpVtbl->SetWorkingDirectory(psl, lpwd->szWorkingDir);

        if (lpwd->dwFlags & WDFLAG_DOSAPP)
        {
            MultiByteToWideChar(CP_ACP, 0, lpwd->PropPrg.achIconFile, -1, wszPath, ARRAYSIZE(wszPath));

            psl->lpVtbl->SetIconLocation(psl, wszPath, (int)(lpwd->PropPrg.wIconIndex));
        }
#ifndef NO_NEW_SHORTCUT_HOOK
    }
#endif

    bWorked = SUCCEEDED(ppf->lpVtbl->Save(ppf, szLinkName, TRUE));

    ppf->lpVtbl->Release(ppf);

ExitFreePSL:

#ifndef NO_NEW_SHORTCUT_HOOK
    if (lpwd->pnshhk)
    {
        lpwd->pnshhk->lpVtbl->Release(lpwd->pnshhk);
        lpwd->pnshhk = NULL;
    }
    else
          if (lpwd->pnshhkA)
          {
              lpwd->pnshhkA->lpVtbl->Release(lpwd->pnshhkA);
              lpwd->pnshhkA = NULL;
          }
          else
#endif
        psl->lpVtbl->Release(psl);

ExitNoFree:

    if (bWorked)
    {
        SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_FLUSH | SHCNF_PATH,
                       szLinkName, NULL);
    }
    SetCursor(hcurOld);

    if (bWorked)
    {
        if (!(lpwd->dwFlags & WDFLAG_DONTOPENFLDR))
        {
            OpenLinkFolder(lpwd, szLinkName);
        }
    }
    else
    {
        ShellMessageBox(g_hinst, lpwd->hwnd, MAKEINTRESOURCE(IDS_NOSHORTCUT),
                        0, MB_OK | MB_ICONEXCLAMATION);
    }

    return(bWorked);
}
