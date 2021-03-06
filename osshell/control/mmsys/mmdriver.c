/*----------------------------------------------------------------------------
 | mmdriver.c - Install Multimedia Drivers
 |
 | Copyright (C) Microsoft, 1989, 1990.  All Rights Reserved
 |
 |  History:
 |      09/11/90    davidle     created
 |          Install Multimedia Drivers
 |
 |      Tue Jan 29 1991 -by- MichaelE
 |          Redesigned installing installable drivers so additional drivers
 |        can be installed by adding them to setup.inf's [installable.drivers]
 |
 |      Wed Mar 20 1991 -by- MichaelE
 |          Changed mmAddInstallableDriver to accept multiple VxDs.
 |          Changed and WriteNextPrivateProfileString to check if the profile
 |          being concatenated is already there.
 |
 |      Sun Apr 14 1991 -by- MichaelE
 |          WriteNextPrivateProfileString -> Next386EnhDevice.
 |
 |      Sun Apr 14 1991 -by- JohnYG
 |          Taken from setup for drivers applet.
 |
 |      Wed Jun 05 1991 -by- MichaelE
 |          Added FileCopy of associated file list to windows system dir.
 |
 *----------------------------------------------------------------------------*/

#include <windows.h>
#include <mmsystem.h>
#include <winsvc.h>
#include <string.h>
#include <stdlib.h>
#include "drivers.h"
#include "sulib.h"

/*
 *  Local functions
 */

 static BOOL mmAddInstallableDriver         (PINF, LPTSTR, LPTSTR, PIDRIVER );
 static BOOL GetDrivers                     (PINF, LPTSTR, LPTSTR);

/**************************************************************************
 *
 *  AccessServiceController()
 *
 *  Check we will be able to access the service controller to install
 *  a driver
 *
 *  returns FALSE if we can't get access - otherwise TRUE
 *
 **************************************************************************/
 BOOL AccessServiceController(void)
 {

     SC_HANDLE SCManagerHandle;

     SCManagerHandle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
     if (SCManagerHandle == NULL) {
         return FALSE;
     }

     CloseServiceHandle(SCManagerHandle);

     return TRUE;
 }


/**************************************************************************
 *
 *  mmDisplayInfFileErrMsg()
 *
 *  This function displays an error message when the inf file contains
 *  corrupt data.
 *
 **************************************************************************/

 void mmDisplayInfFileErrMsg( void )
 {
     TCHAR strBuf[MAXSTR];

    /*
     *  We do not want to report any errors that occur while installing
     *  related drivers to the user
     */

     if( bCopyingRelated )
	 {
          return;
	 }

	 LoadString(myInstance, IDS_INVALIDINF, strBuf, MAXSTR);
     MessageBox(hMesgBoxParent,
                strBuf,
                szFileError,
                MB_OK | MB_ICONEXCLAMATION  | MB_TASKMODAL);
 }


/**************************************************************************
 *
 *  mmAddNewDriver() - only exported function in this file.
 *
 *  This function installs (copies) a driver
 *
 *  returns FALSE if no drivers could be installed.
 *          TRUE if at least one driver installation was sucessful.
 *          All added types in lpszNewTypes buffer.
 *
 **************************************************************************/

 BOOL mmAddNewDriver( LPTSTR lpstrDriver, LPTSTR lpstrNewTypes, PIDRIVER pIDriver )
 {
     PINF pinf;

     if ((pinf = FindInstallableDriversSection(NULL)) == NULL)
         return FALSE;

     return mmAddInstallableDriver(pinf, lpstrDriver, lpstrNewTypes, pIDriver);
 }


/**************************************************************************
 * mmAddInstallableDriver() - Do the dirty work looking for VxD's copying them
 *     looking for drivers, copying them, and returning the best type names.
 *
 *
 **************************************************************************/

BOOL mmAddInstallableDriver( PINF pInfIDrivers,
                             LPTSTR pstrDriver,
                             LPTSTR lpstrNewTypes,
                             PIDRIVER pIDriver)
{
    LPTSTR pstr, pstrSection;
    static TCHAR szTemp[10];
    PINF pInfSection= pInfIDrivers;
	LONG lResult;
    int  i;
    TCHAR szBuffer[MAX_INF_LINE_LEN],
         szFilename[MAXSTR],
         szType[MAX_SECT_NAME_LEN];

   /*
    *  format of a line in [installable.drivers] of setup.inf:
    *  driver profile =                            [0]
    *                   filename,                  [1]
    *                   "type(s)",                 [2]
    *                   "description",             [3]
    *                   "VxD and .sys filename(s)",[4]
    *                   "default config params"    [5]
    *                   "Related drivers"          [6]
    *
    *  find the driver profile line in szMDrivers we are installing
    */

    while ( TRUE )
    {
        lResult = infParseField( pInfIDrivers, 0, szBuffer, SIZEOF(szBuffer) );
		if( INF_PARSE_FAILED(lResult) )
		{
			mmDisplayInfFileErrMsg();
			return FALSE;
		}
        if ( lstrcmpi( szBuffer, pstrDriver ) == 0 )
            break;
        else if ( ! (pInfIDrivers = infNextLine( pInfIDrivers )) )
            return FALSE;
    }

   /*
    *  copy the driver file and add driver type(s) to the installable
    *  driver section
    */

    if ( infParseField(pInfIDrivers, 1, szFilename, SIZEOF(szFilename)) != ERROR_SUCCESS )
	{
		mmDisplayInfFileErrMsg();
        return FALSE;
	}


   /*
    *  Ignore the disk number
    */

    wcscpy(szDrv, RemoveDiskId(szFilename));

   /*
    *  Cache whether it's a kernel driver
    */

    pIDriver->KernelDriver = IsFileKernelDriver(szFilename);

   /*
    *  Can't install kernel drivers if don't have privilege
    */

    if (pIDriver->KernelDriver && !AccessServiceController()) {

        TCHAR szMesg[MAXSTR];
        TCHAR szMesg2[MAXSTR];
        TCHAR szTitle[50];

        LoadString(myInstance, IDS_INSUFFICIENT_PRIVILEGE, szMesg, sizeof(szMesg)/sizeof(TCHAR));
        LoadString(myInstance, IDS_CONFIGURE_DRIVER, szTitle, sizeof(szTitle)/sizeof(TCHAR));
        wsprintf(szMesg2, szMesg, szDrv);
        MessageBox(hMesgBoxParent, szMesg2, szTitle, MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);

        return FALSE;
    }

   /*
    *  Do the file copying
    */

    if (FileCopy( szFilename,
                  szSystem,
                  (FPFNCOPY)wsCopySingleStatus,
                  FC_FILE ) != NO_ERROR) {
        return FALSE;
    }

   /*
    *  Add options
    */

    lResult = infParseField (pInfIDrivers,5,szBuffer+1, SIZEOF(szBuffer)-1);
	if( INF_PARSE_FAILED(lResult) )
	{
		mmDisplayInfFileErrMsg();
		return FALSE;
	}
	else if( lResult == ERROR_SUCCESS )
	{
		szBuffer[0]=TEXT(' ');
		lstrcat(szFilename,szBuffer);
	}

   /*
    *  copy filename and options
    */

    wcsncpy(pIDriver->szFile, FileName(szFilename), sizeof(pIDriver->szFile)/sizeof(TCHAR));
    pIDriver->szFile[sizeof(pIDriver->szFile)/sizeof(TCHAR) - 1] = 0;

   /*
    *  copy description
    */

    lResult = infParseField( pInfIDrivers, 3, pIDriver->szDesc, SIZEOF(pIDriver->szDesc) );
	if( INF_PARSE_FAILED(lResult) )
	{
		mmDisplayInfFileErrMsg();
		return FALSE;
	}

   /*
    *  determine the section from the description.  A kernel driver
    *  will appear as a driver of type 'KERNEL' in system.ini
    *
    *  If the description contains [MCI] then it's MCI.
    */

    if (wcsstr(pIDriver->szDesc, TEXT("MCI")))
        pstrSection = szMCI;
    else
        pstrSection = szDrivers;

   /*
    *  Copy name plus parameters to our driver data
    */

    wcsncpy(pIDriver->szSection, pstrSection, sizeof(pIDriver->szSection)/sizeof(TCHAR));
    pIDriver->szSection[sizeof(pIDriver->szSection)/sizeof(TCHAR) - 1] = TEXT('\0');
    wcscpy(pIDriver->wszSection, pIDriver->szSection);

   /*
    *  We return all types in a parseable, contcatentated string
    */
	lResult = infParseField( pInfIDrivers, 2, szBuffer, SIZEOF(szBuffer) );
	if( INF_PARSE_FAILED(lResult) )
	{
		mmDisplayInfFileErrMsg();
		return FALSE;
	}
    for ( i = 1; ; i++ )
    {
		lResult = infParseField( szBuffer, i, szType, SIZEOF(szType) );
		if( INF_PARSE_FAILED(lResult) )
		{
			mmDisplayInfFileErrMsg();
			return FALSE;
		}
		else if( lResult != ERROR_SUCCESS )
		{
			break;
		}

        pstr = &(szType[lstrlen(szType)]);
        *pstr++ = TEXT(',');
        *pstr = 0;
        lstrcat(lpstrNewTypes, szType );
    }

    if (!*lpstrNewTypes)

      /*
       *  We weren't able to return any types.
       */
       return FALSE;

   /*
    *  copy an associated file list (if it exists) to windows system dir
    */

    if (FileCopy(pstrDriver,
                 szSystem,
                 (FPFNCOPY)wsCopySingleStatus,
                 FC_SECTION) != ERROR_SUCCESS)

        return(FALSE);


   /*
    *  if there are system driver files copy them to the system
    *  drivers directory.
    *
    *  NOTE that it is assumed here that any installation and
    *  configuration for these drivers is performed by the main
    *  (.drv) driver being installed.
    *
    */

    lResult = infParseField( pInfIDrivers, 4, szBuffer, SIZEOF(szBuffer) );
	if( INF_PARSE_FAILED(lResult) )
	{
		mmDisplayInfFileErrMsg();
		return FALSE;
	}
    if ( (lResult == ERROR_SUCCESS) && szBuffer[0] )
    {
        for ( i = 1; ; i++ )
        {
			lResult = infParseField( szBuffer, i, szFilename, SIZEOF(szFilename) );
			if( INF_PARSE_FAILED(lResult) )
			{
				mmDisplayInfFileErrMsg();
				return FALSE;
			}
			else if( lResult != ERROR_SUCCESS )
			{
				break;
			}

            wcscpy(szDrv, RemoveDiskId(szFilename));

           /*
            *  FileCopy will adjust the 'system' directory to
            *  system\drivers.  It's done this way because FileCopy
            *  must anyway look for old files in the same directory.
            */

            if (FileCopy(szFilename,
                         szSystem,
                         (FPFNCOPY)wsCopySingleStatus,
                         FC_FILE )
                != ERROR_SUCCESS)
            {
                return FALSE;
            }
        }
    }

#ifdef DOBOOT // Don't do boot section on NT

    lResult = infParseField(pInfIDrivers, 7, szTemp, SIZEOF(szTemp));
	if( INF_PARSE_FAILED(lResult) )
	{
		mmDisplayInfFileErrMsg();
		return FALSE;
	}

    if (!_strcmpi(szTemp, szBoot))
        bInstallBootLine = TRUE;

#endif // DOBOOT


   /*
    *  Read the related drivers list (drivers which must/can also be
    *  be installed).
    */

    if (bRelated == FALSE)
    {
       lResult = infParseField(pInfIDrivers, 6, pIDriver->szRelated, SIZEOF(pIDriver->szRelated));
	   if( INF_PARSE_FAILED(lResult) )
	   {
		   mmDisplayInfFileErrMsg();
		   return FALSE;
	   }
       if (wcslen(pIDriver->szRelated))
       {
          if( !GetDrivers(pInfSection, pIDriver->szRelated, pIDriver->szRemove) )
		  {
			   mmDisplayInfFileErrMsg();
			   return FALSE;
		  }
          pIDriver->bRelated = TRUE;
          bRelated = TRUE;
       }
    }
    return TRUE;
}

/*
 *  Used to get the list of the related driver filenames
 *
 *  pInfIDrivers - Pointer to the [installable.drivers] section or equivalent
 *  szAliasList  - List of driver aliases (ie key values - eg msalib).
 *  szDriverList - List of drivers file names found
 */

BOOL GetDrivers(PINF pInfIDrivers, LPTSTR szAliasList, LPTSTR szDriverList)
{
    TCHAR szBuffer[50];
    TCHAR szAlias[50];
    TCHAR szFileName[50];
    PINF pInfILocal;
	LONG lResult;
    BOOL bEnd;
    int i;

    for ( i = 1; ; i++ )
    {
		lResult = infParseField(szAliasList, i, szAlias, SIZEOF(szAlias));
		if( INF_PARSE_FAILED(lResult) )
		{
			return FALSE;
		}
		else if( lResult != ERROR_SUCCESS )
		{
			break;
		}

        pInfILocal = pInfIDrivers;
        bEnd = FALSE;
        while (!bEnd)
        {
            lResult = infParseField( pInfILocal, 0, szBuffer, SIZEOF(szBuffer));
		    if( INF_PARSE_FAILED(lResult) )
			{
			    return FALSE;
			}
            if (lstrcmpi( szBuffer, szAlias) == 0 )
            {
                lResult = infParseField(pInfILocal, 1, szFileName, SIZEOF(szFileName));
				if( INF_PARSE_FAILED(lResult) )
				{
					return FALSE;
				}
				else if( lResult == ERROR_SUCCESS )
                {
                    lstrcat(szDriverList, RemoveDiskId(szFileName));
                    lstrcat(szDriverList, TEXT(","));
                }
                break;
            }
            else
                if ( ! (pInfILocal = infNextLine( pInfILocal )) )
                    bEnd = TRUE;
        }
    }

	return TRUE;
}


