/**************************************************************************\
* Module Name: ntreg.cpp
*
* CRegistrySettings class
*
*  This class handles getting registry information for display driver
*  information.
*
* Copyright (c) Microsoft Corp.  1992-1998 All Rights Reserved
*
\**************************************************************************/

#include "priv.h"
#include <tchar.h>
#include "ntreg.hxx"
#include <devguid.h>

//
// CRegistrySettings constructor
//

CRegistrySettings::CRegistrySettings(LPTSTR pstrDeviceKey)
    : _hkVideoReg(NULL)
    , _pszDrvName(NULL)
    , _pszKeyName(NULL)
    , _pszDeviceInstanceId(NULL)
    , _dwDevInst(0)
{
    TCHAR szBuffer[MAX_PATH];
    LPTSTR pszPath;
    HKEY hkeyCommon, hkeyDriver;
    DWORD cb;
    LPTSTR pszName = NULL;
    LPTSTR pszEnd;

    ASSERT(lstrlen(pstrDeviceKey) < MAX_PATH);
    
    //
    // Copy the data to local buffer.
    //

    StringCchCopy(szBuffer, ARRAYSIZE(szBuffer), pstrDeviceKey);

    //
    // Initialize the device instance id
    // 
    
    InitDeviceInstanceID(szBuffer);

    //
    // At this point, szBuffer has something like:
    //  \REGISTRY\Machine\System\ControlSet001\...
    //
    // To use the Win32 registry calls, we have to strip off the \REGISTRY
    // and convert \Machine to HKEY_LOCAL_MACHINE
    //

    pszPath = SubStrEnd(SZ_REGISTRYMACHINE, szBuffer);

    //
    // Try to open the registry key
    //

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     pszPath,
                     0,
                     KEY_READ,
                     &_hkVideoReg) != ERROR_SUCCESS) {

        _hkVideoReg = 0;
    }

    //
    // Go to the video subkey
    //

    pszEnd = pszPath + lstrlen(pszPath);

    while (pszEnd != pszPath && *pszEnd != TEXT('\\')) {

        pszEnd--;
    }

    *pszEnd = UNICODE_NULL;

    StringCchCat(pszPath, ARRAYSIZE(szBuffer) - (pszEnd - szBuffer), SZ_COMMON_SUBKEY);

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     pszPath,
                     0,
                     KEY_READ,
                     &hkeyCommon) == ERROR_SUCCESS) {

        cb = sizeof(szBuffer);
        ZeroMemory(szBuffer, cb);

        if (RegQueryValueEx(hkeyCommon,
                            SZ_SERVICE,
                            NULL,
                            NULL,
                            (LPBYTE)szBuffer,
                            &cb) == ERROR_SUCCESS) {

            //
            // Save the key name
            //

            DWORD cchKeyName = (lstrlen(szBuffer) + 1);
            _pszKeyName = (LPTSTR)LocalAlloc(LPTR, cchKeyName * sizeof(TCHAR));

            if (_pszKeyName != NULL) {

                StringCchCopy(_pszKeyName, cchKeyName, szBuffer);
            
                StringCchPrintf(szBuffer, ARRAYSIZE(szBuffer), TEXT("%s%s"), SZ_SERVICES_PATH, _pszKeyName);

                if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                 szBuffer,
                                 0,
                                 KEY_READ,
                                 &hkeyDriver) == ERROR_SUCCESS) {
        
                    cb = sizeof(szBuffer);
                    ZeroMemory(szBuffer, cb);

                    if (RegQueryValueEx(hkeyDriver,
                                        L"ImagePath",
                                        NULL,
                                        NULL,
                                        (LPBYTE)szBuffer,
                                        &cb) == ERROR_SUCCESS) {
        
                        //
                        // This is a binary.
                        // Extract the name, which will be of the form ...\driver.sys
                        //
        
                        LPTSTR pszDriver, pszDriverEnd;
    
                        pszDriver = szBuffer;
                        pszDriverEnd = pszDriver + lstrlen(pszDriver);
    
                        while(pszDriverEnd != pszDriver &&
                              *pszDriverEnd != TEXT('.')) {
                            pszDriverEnd--;
                        }
    
                        *pszDriverEnd = UNICODE_NULL;
    
                        while(pszDriverEnd != pszDriver &&
                              *pszDriverEnd != TEXT('\\')) {
                            pszDriverEnd--;
                        }
    
                        //
                        // If pszDriver and pszDriverEnd are different, we now
                        // have the driver name.
                        //
    
                        if (pszDriverEnd > pszDriver) {
                            
                            pszDriverEnd++;
                            pszName = pszDriverEnd;
    
                        }
                    }
        
                    RegCloseKey(hkeyDriver);
                }
            
                if (!pszName) {

                    //
                    // Something failed trying to get the binary name.just get the device name
                    //

                    _pszDrvName = _pszKeyName;

                } else {

                    DWORD cchDrvName = lstrlen(pszName) + 1;
                    _pszDrvName = (LPTSTR)LocalAlloc(LPTR, cchDrvName * sizeof(TCHAR));

                    if (_pszDrvName != NULL) {

                        StringCchCopy(_pszDrvName, cchDrvName, pszName);

                    }
                }
            }
        }

        RegCloseKey(hkeyCommon);
    }
}

//
// CRegistrySettings destructor
//

CRegistrySettings::~CRegistrySettings() 
{
    //
    // Close the registry
    //

    if (_hkVideoReg) {
        RegCloseKey(_hkVideoReg);
    }

    //
    // Free the strings
    //
    if (_pszKeyName) {
        LocalFree(_pszKeyName);
    }

    if ((_pszKeyName != _pszDrvName) && _pszDrvName) {
        LocalFree(_pszDrvName);
    }

    if(_pszDeviceInstanceId) {
        LocalFree(_pszDeviceInstanceId);
    }
}


//
// Method to get the hardware information fields.
//

VOID
CRegistrySettings::GetHardwareInformation(
    PDISPLAY_REGISTRY_HARDWARE_INFO pInfo)
{

    DWORD cb, dwType;
    DWORD i;
    LONG lRet;

    LPWSTR pKeyNames[5] = {
        L"HardwareInformation.MemorySize",
        L"HardwareInformation.ChipType",
        L"HardwareInformation.DacType",
        L"HardwareInformation.AdapterString",
        L"HardwareInformation.BiosString"
    };

    ZeroMemory(pInfo, sizeof(*pInfo));

    //
    // Query each entry one after the other.
    //

    for (i = 0; i < 5; i++) {

        //
        // query the size of the string
        //

        cb = sizeof(pInfo->MemSize);
        lRet = RegQueryValueExW(_hkVideoReg,
                                pKeyNames[i],
                                NULL,
                                &dwType,
                                NULL,
                                &cb);

        if (lRet == ERROR_SUCCESS) {

            if (i == 0) {

                ULONG mem;

                cb = sizeof(mem);

                if (RegQueryValueExW(_hkVideoReg,
                                 pKeyNames[i],
                                 NULL,
                                 &dwType,
                                 (PUCHAR) (&mem),
                                 &cb) == ERROR_SUCCESS)
                {

                    //
                    // If we queried the memory size, we actually have
                    // a DWORD.  Transform the DWORD to a string
                    //

                    // Divide down to Ks

                    mem =  mem >> 10;

                    // if a MB multiple, divide again.

                    if ((mem & 0x3FF) != 0) {

                        StringCchPrintf((LPWSTR)pInfo, ARRAYSIZE(pInfo->MemSize), L"%d KB", mem );

                    } else {

                        StringCchPrintf((LPWSTR)pInfo, ARRAYSIZE(pInfo->MemSize), L"%d MB", mem >> 10 );

                    }
                }
                else
                {
                    goto Default;
                }

            } else {

                cb = sizeof(pInfo->MemSize);

                //
                // get the string
                //

                if (RegQueryValueExW(_hkVideoReg,
                                 pKeyNames[i],
                                 NULL,
                                 &dwType,
                                 (LPBYTE) pInfo,
                                 &cb) != ERROR_SUCCESS)
                {
                    goto Default;
                }
            }
        }
        else
        {
            //
            // Put in the default string
            //
Default:
            LoadString(HINST_THISDLL,
                       IDS_UNAVAILABLE,
                       (LPWSTR)pInfo,
                       ARRAYSIZE(pInfo->MemSize));
        }

        pInfo = (PDISPLAY_REGISTRY_HARDWARE_INFO)((PUCHAR)pInfo + sizeof(pInfo->MemSize));
    }
}


VOID CRegistrySettings::InitDeviceInstanceID(
    LPTSTR pstrDeviceKey
    ) 
{
    HDEVINFO hDevInfo = INVALID_HANDLE_VALUE ;
    SP_DEVINFO_DATA DevInfoData;
    ULONG InstanceIDSize = 0;
    BOOL bSuccess = FALSE;
    LPWSTR pwInterfaceName = NULL;
    LPWSTR pwInstanceID = NULL;

    ASSERT (pstrDeviceKey != NULL);
    ASSERT (_pszDeviceInstanceId == NULL);

    if (AllocAndReadInterfaceName(pstrDeviceKey, &pwInterfaceName)) {

        bSuccess = GetDevInfoDataFromInterfaceName(pwInterfaceName,
                                                   &hDevInfo,
                                                   &DevInfoData);
        if (bSuccess) {

            InstanceIDSize = 0;

            bSuccess = 

                ((CM_Get_Device_ID_Size(&InstanceIDSize, 
                                        DevInfoData.DevInst, 
                                        0) == CR_SUCCESS) &&

                 ((_pszDeviceInstanceId = (LPTSTR)LocalAlloc(LPTR, 
                     (InstanceIDSize + 1) * sizeof(TCHAR))) != NULL) &&

                 (CM_Get_Device_ID(DevInfoData.DevInst, 
                                   _pszDeviceInstanceId,
                                   InstanceIDSize,
                                   0) == CR_SUCCESS));

            if (bSuccess) {

                _dwDevInst = DevInfoData.DevInst;
            
            } else {

                //
                // Clean-up
                //

                if (NULL != _pszDeviceInstanceId) {
                    LocalFree(_pszDeviceInstanceId);
                    _pszDeviceInstanceId = NULL;
                }
            }

            SetupDiDestroyDeviceInfoList(hDevInfo);
        }

        LocalFree(pwInterfaceName);
    }

    if ((!bSuccess) &&
        AllocAndReadInstanceID(pstrDeviceKey, &pwInstanceID)) {

        _pszDeviceInstanceId = pwInstanceID;
    }

} // InitDeviceInstanceID


BOOL
CRegistrySettings::GetDevInfoDataFromInterfaceName(
    IN  LPWSTR pwInterfaceName,
    OUT HDEVINFO* phDevInfo,
    OUT PSP_DEVINFO_DATA pDevInfoData
    )

/*

    Note: If this function retuns success, the caller is responsible
          to destroy the device info list returned in phDevInfo

*/

{
    LPWSTR pwDevicePath = NULL;
    HDEVINFO hDevInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DevInfoData;
    SP_DEVICE_INTERFACE_DATA InterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA pInterfaceDetailData = NULL;
    DWORD InterfaceIndex = 0;
    DWORD InterfaceSize = 0;
    BOOL bMatch = FALSE;

    ASSERT (pwInterfaceName != NULL);
    ASSERT (phDevInfo != NULL);
    ASSERT (pDevInfoData != NULL);

    //
    // Enumerate all display adapter interfaces
    //

    hDevInfo = SetupDiGetClassDevs(&GUID_DISPLAY_ADAPTER_INTERFACE,
                                   NULL,
                                   NULL,
                                   DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        goto Cleanup;
    }

    InterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    while (SetupDiEnumDeviceInterfaces(hDevInfo,
                                       NULL,
                                       &GUID_DISPLAY_ADAPTER_INTERFACE,
                                       InterfaceIndex,
                                       &InterfaceData)) {

        //
        // Get the required size for the interface
        //

        InterfaceSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo,
                                        &InterfaceData,
                                        NULL,
                                        0,
                                        &InterfaceSize,
                                        NULL);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            goto Cleanup;
        }

        //
        // Alloc memory for the interface
        //

        pInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)
            LocalAlloc(LPTR, InterfaceSize);
        if (pInterfaceDetailData == NULL)
            goto Cleanup;

        //
        // Get the interface
        //

        pInterfaceDetailData->cbSize =
            sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        DevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetail(hDevInfo,
                                            &InterfaceData,
                                            pInterfaceDetailData,
                                            InterfaceSize,
                                            &InterfaceSize,
                                            &DevInfoData)) {

            //
            // Is the InterfaceName the same as the DevicePath?
            //

            pwDevicePath = pInterfaceDetailData->DevicePath;

            //
            // The first 4 characters of the interface name are different
            // between user mode and kernel mode (e.g. "\\?\" vs "\\.\")
            // Therefore, ignore them.
            //

            bMatch = (_wcsnicmp(pwInterfaceName + 4,
                                pwDevicePath + 4,
                                wcslen(pwInterfaceName + 4)) == 0);

            if (bMatch) {

                //
                // We found the device
                //

                *phDevInfo = hDevInfo;
                CopyMemory(pDevInfoData, &DevInfoData, sizeof(*pDevInfoData));

                break;
            }
        }

        //
        // Clean-up
        //

        LocalFree(pInterfaceDetailData);
        pInterfaceDetailData = NULL;

        //
        // Next interface ...
        //

        InterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        ++InterfaceIndex;
    }

Cleanup:

    if (pInterfaceDetailData != NULL) {
        LocalFree(pInterfaceDetailData);
    }

    //
    // Upon success, the caller is responsible to destroy the list
    //

    if (!bMatch && (hDevInfo != INVALID_HANDLE_VALUE)) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    return bMatch;
}


HKEY
CRegistrySettings::OpenDrvRegKey()
{
    HKEY hkDriver = (HKEY)INVALID_HANDLE_VALUE;
    HDEVINFO hDevInfo = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA did;
    DWORD index = 0;

    if (_dwDevInst == 0) {
        goto Fallout;
    }

    hDevInfo = SetupDiGetClassDevs((LPGUID) &GUID_DEVCLASS_DISPLAY,
                                   NULL,
                                   NULL,
                                   0);
    
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        goto Fallout;
    }

    ZeroMemory(&did, sizeof(did));
    did.cbSize = sizeof(did);

    while (SetupDiEnumDeviceInfo(hDevInfo, index, &did)) {
    
        if (did.DevInst == _dwDevInst) {

            hkDriver = SetupDiOpenDevRegKey(hDevInfo,
                                            &did,
                                            DICS_FLAG_GLOBAL,
                                            0,
                                            DIREG_DRV ,
                                            KEY_READ);
            break;
        }

        did.cbSize = sizeof(SP_DEVINFO_DATA);
        index++;
    }

Fallout:
    
    if (hDevInfo != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    return hkDriver;
}

