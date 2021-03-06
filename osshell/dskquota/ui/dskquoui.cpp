#include "pch.h" // PCH
#pragma hdrstop

#define INITGUIDS
#include "dskquota.h"
#include <gpedit.h>    // Pick up IID_IGPEInformation
#include <shfusion.h>

#include "guidsp.h"    // Private GUIDs.
#include "factory.h"   // Class factory declarations.
#include "registry.h"

//
// Verify that build is UNICODE.
//
#if !defined(UNICODE)
#   error This module must be compiled UNICODE.
#endif


HINSTANCE     g_hInstDll     = NULL;    // DLL instance handle.
LONG          g_cRefThisDll  = 0;       // DLL reference count.
LONG          g_cLockThisDll = 0;       // DLL lock count.


///////////////////////////////////////////////////////////////////////////////
/*  Function: DllGetClassObject

    Description: Creates instance of DiskQuotaControlClassFactory.

    Arguments:
        rclsid - Reference to class ID that identifies the type of object that the
            class factory will be asked to create.

        riid - Reference to interface ID on the class factory object.

        ppvOut - Destination location for class factory object pointer after 
            instantiation.

    Returns:
        NOERROR                   - Success.
        E_OUTOFMEMORY             - Can't create class factory object.
        E_NOINTERFACE             - Interface not supported.
        E_INVALIDARG              - ppvOut arg is NULL.
        CLASS_E_CLASSNOTAVAILABLE - Class factory not available.

    Revision History:

    Date        Description                                          Programmer
    --------    ---------------------------------------------------  ----------
    05/22/96    Initial creation.                                    BrianAu
*/
///////////////////////////////////////////////////////////////////////////////
STDAPI 
DllGetClassObject(
    REFCLSID rclsid, 
    REFIID riid, 
    LPVOID *ppvOut
    )
{
    DBGTRACE((DM_COM, DL_HIGH, TEXT("DllGetClassObject")));
    HRESULT hResult = CLASS_E_CLASSNOTAVAILABLE;
    
    if (NULL == ppvOut)
        return E_INVALIDARG;

    *ppvOut = NULL;

    try
    {
        if (IsEqualIID(rclsid, CLSID_DiskQuotaUI))
        {
            DiskQuotaUIClassFactory *pClassFactory = NULL;

            pClassFactory = new DiskQuotaUIClassFactory;
            hResult = pClassFactory->QueryInterface(riid, ppvOut);
            if (FAILED(hResult))
            {
                delete pClassFactory;
            }
        }
    }
    catch(CAllocException& e)
    {
        hResult = E_OUTOFMEMORY;
    }
    
    return hResult;
}


///////////////////////////////////////////////////////////////////////////////
/*  Function: DllCanUnloadNow

    Description: Called by OLE to determine if DLL can be unloaded.

    Arguments: None.

    Returns:
        S_FALSE     - Can't unload.  Ref count or lock count are > 0.
        S_OK        - OK to unload. Ref count and lock count are 0.

    Revision History:

    Date        Description                                          Programmer
    --------    ---------------------------------------------------  ----------
    05/22/96    Initial creation.                                    BrianAu
*/
///////////////////////////////////////////////////////////////////////////////
STDAPI 
DllCanUnloadNow(
    VOID
    )
{
    DBGPRINT((DM_COM, DL_HIGH, TEXT("DllCanUnloadNow (dskquoui.dll) RefCnt = %d  LockCnt = %d"),
         g_cRefThisDll, g_cLockThisDll));

    return (0 == g_cRefThisDll && 0 == g_cLockThisDll) ? S_OK : S_FALSE;
}


///////////////////////////////////////////////////////////////////////////////
/*  Function: DllRegisterServer

    Description: Create the necessary registry entries for dskquoui.dll
        to operate properly.  This is typically called by REGSVR32.EXE.

    Arguments: None.

    Returns:
        S_OK            - Succeeded.
        SELFREG_E_CLASS - Failed to create one of the registry entries.

    Revision History:

    Date        Description                                          Programmer
    --------    ---------------------------------------------------  ----------
    08/18/97    Initial creation.                                    BrianAu
*/
///////////////////////////////////////////////////////////////////////////////
HRESULT
DllRegisterServer(
    VOID
    )
{
    HRESULT hResult = CallRegInstall(g_hInstDll, "RegDll");

    if (FAILED(hResult))
    {
        hResult = SELFREG_E_CLASS;
    }

    return hResult;
}


///////////////////////////////////////////////////////////////////////////////
/*  Function: DllUnregisterServer

    Description: Remove the necessary registry entries for dskquoui.dll.
        This is typically called by REGSVR32.EXE.

    Arguments: None.

    Returns:
        S_OK            - Succeeded.
        SELFREG_E_CLASS - Failed to remove the CLSID entry.

    Revision History:

    Date        Description                                          Programmer
    --------    ---------------------------------------------------  ----------
    08/18/97    Initial creation.                                    BrianAu
*/
///////////////////////////////////////////////////////////////////////////////
HRESULT
DllUnregisterServer(
    VOID
    )
{
    HRESULT hResult = CallRegInstall(g_hInstDll, "UnregDll");

    if (FAILED(hResult))
    {
        hResult = SELFREG_E_CLASS;
    }
    return hResult;
}

///////////////////////////////////////////////////////////////////////////////
/*  Function: OnProcessAttach

    Description: Handles all tasks associated with a process attaching to 
        the DLL.

        Try to keep processing time to a minimum.

    Arguments:
        hInstDll - The DLL instance handle passed to DllMain.

    Returns:
        NO_ERROR    - Success.
        E_FAIL      - Something failed.

    Revision History:

    Date        Description                                          Programmer
    --------    ---------------------------------------------------  ----------
    08/09/96    Initial creation.                                    BrianAu
*/
///////////////////////////////////////////////////////////////////////////////
HRESULT
OnProcessAttach(
    HINSTANCE hInstDll
    )
{
    HRESULT hResult = NOERROR;

    //
    // Be specific - you want to use this dll's manifest (in resources at ID 1.)
    //
    SHFusionInitializeFromModuleID(hInstDll, 1);

    //
    // Start IceCAP profiling.
    //
    ICAP_START_ALL;

#if DBG
    DBGMODULE(TEXT("DSKQUOUI"));  // Name of module displayed with messages.
    RegKey key(HKEY_CURRENT_USER, REGSTR_KEY_DISKQUOTA);
    if (SUCCEEDED(key.Open(KEY_READ)))
    {
        DebugRegParams dp;
        if (SUCCEEDED(key.GetValue(REGSTR_VAL_DEBUGPARAMS, (LPBYTE)&dp, sizeof(dp))))
        {
            DBGPRINTMASK(dp.PrintMask);
            DBGPRINTLEVEL(dp.PrintLevel);
            DBGPRINTVERBOSE(dp.PrintVerbose);
            DBGTRACEMASK(dp.TraceMask);
            DBGTRACELEVEL(dp.TraceLevel);
            DBGTRACEVERBOSE(dp.TraceVerbose);
            DBGTRACEONEXIT(dp.TraceOnExit);
        }
    }
#endif // DBG


    g_hInstDll = hInstDll;
    DisableThreadLibraryCalls(hInstDll);

    return hResult;
}



///////////////////////////////////////////////////////////////////////////////
/*  Function: OnProcessDetach

    Description: Handles all tasks associated with a process detaching from 
        the DLL.

    Arguments: None.

    Returns:
        NO_ERROR    - Success.
        E_FAIL      - Something failed.

    Revision History:

    Date        Description                                          Programmer
    --------    ---------------------------------------------------  ----------
    08/09/96    Initial creation.                                    BrianAu
*/
///////////////////////////////////////////////////////////////////////////////
HRESULT
OnProcessDetach(
    VOID
    )
{
    HRESULT hResult = NO_ERROR;

    SHFusionUninitialize();

    //
    // Stop IceCAP profiling.
    //
    ICAP_STOP_ALL;

    return hResult;
}



///////////////////////////////////////////////////////////////////////////////
/*  Function: DllMain

    Description: Main entry point for OLE component server.

    Arguments:
        hInstDll - Instance handle of DLL

        fdwReason - Reason DllMain is being called.  Can be at Process attach/
            detach or Thread attach/detach.

        lpdwReserved - Reserved.

    Returns:
        TRUE    - Successful initialization.
        FALSE   - Failed initialization.

    Revision History:

    Date        Description                                          Programmer
    --------    ---------------------------------------------------  ----------
    05/22/96    Initial creation.                                    BrianAu
    08/09/96    Moved code associated with process attach and        BrianAu
                detach out to separate functions.
*/
///////////////////////////////////////////////////////////////////////////////
BOOL WINAPI 
DllMain(
    HINSTANCE hInstDll, 
    DWORD fdwReason, 
    LPVOID lpvReserved
    )
{
    BOOL bResult = FALSE;

    switch(fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DBGPRINT((DM_COM, DL_HIGH, TEXT("DSKQUOUI - DLL_PROCESS_ATTACH")));
            bResult = SUCCEEDED(OnProcessAttach(hInstDll));
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            bResult = TRUE;
            break;

        case DLL_PROCESS_DETACH:
            DBGPRINT((DM_COM, DL_HIGH, TEXT("DSKQUOUI - DLL_PROCESS_DETACH")));
            bResult = SUCCEEDED(OnProcessDetach());
            break;
    }

    return bResult;
}


