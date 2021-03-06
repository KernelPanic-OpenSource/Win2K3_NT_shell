// key.cpp : implementation file
//
// This is a part of the Microsoft Foundation Classes C++ library.
// Copyright (C) 1992-1995 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.

#include "stdafx.h"
#include "key.h"
#include <winreg.h>

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKey

void CKey::Close()
{
    if (m_hKey != NULL)
    {

#if defined(_DEBUG)
        LONG lRes = RegCloseKey(m_hKey);
        ASSERT(lRes == ERROR_SUCCESS);
#else
        RegCloseKey(m_hKey);
#endif

        m_hKey = NULL;
    }
}

BOOL CKey::Create(HKEY hKey, LPCTSTR lpszKeyName, REGSAM samDesired)
{
    ASSERT(hKey != NULL);
    return (RegCreateKeyEx(hKey, lpszKeyName, 0, NULL, 0, samDesired, NULL, &m_hKey, NULL) == ERROR_SUCCESS);
}

BOOL CKey::Open(HKEY hKey, LPCTSTR lpszKeyName, REGSAM samDesired)
{
    ASSERT(hKey != NULL);
    return (RegOpenKeyEx(hKey, lpszKeyName, 0, samDesired, &m_hKey) == ERROR_SUCCESS);
}

BOOL CKey::SetStringValue(LPCTSTR lpszValue, LPCTSTR lpszValueName)
{
    ASSERT(m_hKey != NULL);
    return (RegSetValueEx(m_hKey, lpszValueName, NULL, REG_SZ,
        (BYTE * const)lpszValue, (lstrlen(lpszValue)+1)*sizeof(TCHAR)) == ERROR_SUCCESS);
}

BOOL CKey::GetStringValue(CString& str, LPCTSTR lpszValueName)
{
    ASSERT(m_hKey != NULL);
    str.Empty();
    DWORD dw = 0;
    DWORD dwType = 0;
    LONG lRes = RegQueryValueEx(m_hKey, (LPTSTR)lpszValueName, NULL, &dwType,
        NULL, &dw);
    if (lRes == ERROR_SUCCESS)
    {
        ASSERT(dwType == REG_SZ);
        LPTSTR lpsz = str.GetBufferSetLength(dw);
        lRes = RegQueryValueEx(m_hKey, (LPTSTR)lpszValueName, NULL, &dwType, (BYTE*)lpsz, &dw);
        ASSERT(lRes == ERROR_SUCCESS);
        str.ReleaseBuffer();
        return TRUE;
    }
    return FALSE;
}
