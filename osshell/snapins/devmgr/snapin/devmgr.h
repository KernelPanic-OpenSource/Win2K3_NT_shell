//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation
//
//  File:       devmgr.h
//
//--------------------------------------------------------------------------

#ifndef __DEVMGR_H_
#define __DEVMGR_H_

#pragma warning( disable : 4201 ) // nonstandard extension used : nameless struct/union

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <new.h>
#include <stdio.h>
#include <stdlib.h>
#include <prsht.h>
#include <prshtp.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlapip.h>
#include <ole2.h>
#include <mmc.h>
#include <objsel.h>
#include <htmlhelp.h>
#include <winioctl.h>
#include <strsafe.h>

#include <shfusion.h>

extern "C" {
#include <commdlg.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <spapip.h>
#include <sputils.h>
#include <regstr.h>
#include <shimdb.h>
}

#pragma warning( default : 4201 )


#define ARRAYLEN(array)     (sizeof(array) / sizeof(array[0]))


typedef enum tagCookieType
{
    COOKIE_TYPE_SCOPEITEM_DEVMGR = 0,
    COOKIE_TYPE_RESULTITEM_RESOURCE_IRQ,
    COOKIE_TYPE_RESULTITEM_RESOURCE_DMA,
    COOKIE_TYPE_RESULTITEM_RESOURCE_IO,
    COOKIE_TYPE_RESULTITEM_RESOURCE_MEMORY,
    COOKIE_TYPE_RESULTITEM_COMPUTER,
    COOKIE_TYPE_RESULTITEM_DEVICE,
    COOKIE_TYPE_RESULTITEM_CLASS,
    COOKIE_TYPE_RESULTITEM_RESTYPE,
    COOKIE_TYPE_UNKNOWN
} COOKIE_TYPE, *PCOOKIE_TYPE;


#define COOKIE_FLAGS_EXPANDED           0x00000001

const int TOTAL_COOKIE_TYPES = COOKIE_TYPE_RESULTITEM_RESTYPE - COOKIE_TYPE_SCOPEITEM_DEVMGR + 1;

const int NODETYPE_FIRST =      (int)COOKIE_TYPE_SCOPEITEM_DEVMGR;
const int NODETYPE_LAST  =      (int)COOKIE_TYPE_RESULTITEM_RESTYPE;


typedef struct tagNodeInfo {
    COOKIE_TYPE     ct;
    int             idsName;
    int             idsFormat;
    GUID            Guid;
    TCHAR*          GuidString;
} NODEINFO, *PNODEINFO;


//
// Device manager needs to keep track of the largest problem code that it 
// currently knows about.  Using the NUM_CM_PROB defined in cfg.h is a bad
// idea because if more problem codes are added that device manager doesn't
// know about it will cause us to overrun the end of the CMPROBLEM_INFO array.
//
// Note that the +2 are for
//  1) the working case (0 index in the array)
//  2) the unknown problem case (last problem + 1 index in the array)
//                                                                  

#define DEVMGR_NUM_CM_PROB      0x34

#if DEVMGR_NUM_CM_PROB != (NUM_CM_PROB + 1)
#error Update DEVMGR_NUM_CM_PROB and update tswizard.cpp and globals.cpp.
#endif

#define PIF_CODE_EMBEDDED       0x01

typedef struct tagProblemInfo {
    int     StringId;
    DWORD   Flags;
} PROBLEMINFO, *PPROBLEMINFO;

typedef struct tagInternalData {
    DATA_OBJECT_TYPES   dot;
    COOKIE_TYPE ct;
    MMC_COOKIE  cookie;
} INTERNAL_DATA, *PINTERNAL_DATA;

typedef enum tagPropertyChangeType
{
    PCT_STARTUP_INFODATA = 0,
    PCT_DEVICE,
    PCT_CLASS
} PROPERTY_CHANGE_TYPE, *PPROPERTY_CHANGE_TYPE;

typedef struct tagPropertyChangeInfo
{
    PROPERTY_CHANGE_TYPE Type;
    BYTE                 InfoData[1];
} PROPERTY_CHANGE_INFO, *PPROPERTY_CHANGE_INFO;

typedef struct tagStartupInfoData
{
    DWORD           Size;
    COOKIE_TYPE     ct;
    TCHAR           MachineName[MAX_PATH + 3];
} STARTUP_INFODATA, *PSTARTUP_INFODATA;

typedef enum tagdmQuerySiblingCode
{
    QSC_TO_FOREGROUND = 0,
    QSC_PROPERTY_CHANGED,
} DMQUERYSIBLINGCODE, *PDMQUERYSIBLINGCODE;

//
// private header files
//
#include "..\inc\tvintf.h"
#include "resource.h"
#include "prndlg.h"
#include "globals.h"
#include "utils.h"
#include "ccookie.h"
#include "machine.h"
#include "compdata.h"
#include "componet.h"
#include "cnode.h"
#include "cfolder.h"
#include "dataobj.h"

BOOL InitGlobals(HINSTANCE hInstance);

extern LPCTSTR DEVMGR_DEVICEID_SWITCH;
extern LPCTSTR DEVMGR_MACHINENAME_SWITCH;
extern LPCTSTR DEVMGR_COMMAND_SWITCH;

void * __cdecl operator new(size_t size);
void __cdecl operator delete(void *ptr);
__cdecl _purecall(void);


STDAPI
DllRegisterServer();

STDAPI
DllUnregisterServer();

STDAPI
DllCanUnloadNow();

STDAPI
DllGetClassObject(REFCLSID rclsid, REFIID iid, void** ppv);


//
// CDMCommandLine class defination
//
class CDMCommandLine : public CCommandLine
{
public:
    CDMCommandLine() : m_WaitForDeviceId(FALSE),
                       m_WaitForMachineName(FALSE),
                       m_WaitForCommand(FALSE)
    {}
    virtual void ParseParam(LPCTSTR lpszParam, BOOL bFlag)
    {
        if (bFlag)
        {
            if (!lstrcmpi(DEVMGR_DEVICEID_SWITCH, lpszParam)) {
                m_WaitForDeviceId = TRUE;
            
            } else if (!lstrcmpi(DEVMGR_MACHINENAME_SWITCH, lpszParam)) {
                m_WaitForMachineName = TRUE;
            
            } else if (!lstrcmpi(DEVMGR_COMMAND_SWITCH, lpszParam)) {
                m_WaitForCommand = TRUE;
            }
        } else {
            if (m_WaitForDeviceId) {
                m_strDeviceId = lpszParam;
                m_WaitForDeviceId = FALSE;
            
            } else if (m_WaitForMachineName) {
                m_strMachineName = lpszParam;
                m_WaitForMachineName = FALSE;
            
            } else if (m_WaitForCommand) {
                m_strCommand = lpszParam;
                m_WaitForCommand = FALSE;
            }
        }
    }
    LPCTSTR GetDeviceId()
    {
        return m_strDeviceId.IsEmpty() ? NULL : (LPCTSTR)m_strDeviceId;
    }
    LPCTSTR GetMachineName()
    {
        return m_strMachineName.IsEmpty() ? NULL : (LPCTSTR)m_strMachineName;
    }
    LPCTSTR GetCommand()
    {
        return m_strCommand.IsEmpty() ? NULL : (LPCTSTR)m_strCommand;
    }

private:
    String      m_strDeviceId;
    String      m_strMachineName;
    String      m_strCommand;
    BOOL        m_WaitForDeviceId;
    BOOL        m_WaitForMachineName;
    BOOL        m_WaitForCommand;
};

class CNotifyRebootRequest
{
public:
    CNotifyRebootRequest() : Ref(1), m_hWnd(NULL), m_RestartFlags(0), m_StringId(0)
    {}
    CNotifyRebootRequest(HWND hWnd, DWORD RestartFlags, UINT StringId) : Ref(1)
    {
        m_hWnd = hWnd;
        m_RestartFlags = RestartFlags;
        m_StringId = StringId;
    }
    ~CNotifyRebootRequest()
    {
    }
    long AddRef()
    {
        Ref++;
        return Ref;
    }
    long Release()
    {
        ASSERT(Ref);
        if (!(--Ref))
        {
            delete this;
            return 0;
        }
        return Ref;
    }

    HWND    m_hWnd;
    DWORD   m_RestartFlags;
    UINT    m_StringId;

private:
    long    Ref;
};


#endif
