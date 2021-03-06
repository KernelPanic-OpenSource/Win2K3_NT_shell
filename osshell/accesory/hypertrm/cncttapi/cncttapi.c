/*  File: D:\WACKER\cncttapi\cncttapi.c (Created: 08-Feb-1994)
 *
 *  Copyright 1994 by Hilgraeve Inc. -- Monroe, MI
 *  All rights reserved
 *
 *      $Revision: 53 $
 *      $Date: 7/12/02 9:06a $
 */

#define TAPI_CURRENT_VERSION 0x00010004     // cab:11/14/96 - required!

#include <tapi.h>
#include <unimodem.h>
#pragma hdrstop

//#define DEBUGSTR

#include <time.h>

#include <tdll\stdtyp.h>
#include <tdll\session.h>
#include <tdll\statusbr.h>
#include <tdll\tdll.h>
#include <tdll\misc.h>
#include <tdll\mc.h>
#include <tdll\assert.h>
#include <tdll\errorbox.h>
#include <tdll\cnct.h>
#include <tdll\globals.h>
#include <tdll\sf.h>
#include <tdll\sess_ids.h>
#include <tdll\com.h>
#include <tdll\comdev.h>
#include <tdll\com.hh>
#include <tdll\htchar.h>
#include <tdll\cloop.h>
#include <emu\emu.h>
#include <term\res.h>
#include "cncttapi.h"
#include "cncttapi.hh"
#include <tdll\XFER_MSC.HH>     // XD_TYPE
#include <tdll\XFER_MSC.H>      // xfrGetDisplayWindow(), xfrDoTransfer()
#include "tdll\XFDSPDLG.H"      // XFR_SHUTDOWN

static int DoNewModemWizard(HWND hWnd, int iTimeout);
static int tapiReinit(const HHDRIVER hhDriver);
static int tapiReinitMessage(const HHDRIVER hhDriver);
static int DoDelayedCall(const HHDRIVER hhDriver);

const TCHAR *g_achApp = TEXT("HyperTerminal");

static HHDRIVER gbl_hhDriver;	// see LINEDEVSTATE for explaination.

#if 0
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvEntry
 *
 * DESCRIPTION:
 *  Currently, just initializes the C-Runtime library but may be used
 *  for other things later.
 *
 * ARGUMENTS:
 *  hInstDll    - Instance of this DLL
 *  fdwReason   - Why this entry point is called
 *  lpReserved  - reserved
 *
 * RETURNS:
 *  BOOL
 *
 */
BOOL WINAPI cnctdrvEntry(HINSTANCE hInstDll, DWORD fdwReason, LPVOID lpReserved)
	{
	hInstance = hInstDll;
	return _CRT_INIT(hInstDll, fdwReason, lpReserved);
	}
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvCreate
 *
 * DESCRIPTION:
 *  Initializes the connection driver and returns a handle to the driver
 *  if successful.
 *
 * ARGUMENTS:
 *  hCnct   - public connection handle
 *
 * RETURNS:
 *  Handle to driver if successful, else 0.
 *
 */
HDRIVER WINAPI cnctdrvCreate(const HCNCT hCnct, const HSESSION hSession)
	{
	HHDRIVER hhDriver;

	if (hCnct == 0)
		{
		assert(FALSE);
		return 0;
		}

	hhDriver = malloc(sizeof(*hhDriver));

	if (hhDriver == 0)
		{
		assert(FALSE);
		return 0;
		}

	gbl_hhDriver = hhDriver;
	memset(hhDriver, 0, sizeof(*hhDriver));

	InitializeCriticalSection(&hhDriver->cs);

	hhDriver->hCnct = hCnct;
	hhDriver->hSession = hSession;
	hhDriver->iStatus  = CNCT_STATUS_FALSE;
	hhDriver->dwLine   = (DWORD)-1;

	cnctdrvInit(hhDriver);
	return (HDRIVER)hhDriver;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvDestroy
 *
 * DESCRIPTION:
 *  Destroys a connection driver handle.
 *
 * ARGUMENTS:
 *  hhDriver - private driver handle.
 *
 * RETURNS:
 *  0 or error code
 *
 */
int WINAPI cnctdrvDestroy(const HHDRIVER hhDriver)
	{
	if (hhDriver == 0)
		{
		assert(FALSE);
		return CNCT_BAD_HANDLE;
		}

	// Disconnect if we're connected or in the process.
	// Note: cnctdrvDisconnect should terminate the thread.

	cnctdrvDisconnect(hhDriver, DISCNCT_NOBEEP);

	if (hhDriver->hLine)
		{
		lineClose(hhDriver->hLine);
		memset(&hhDriver->stCallPar, 0, sizeof(hhDriver->stCallPar));
		hhDriver->stCallPar.dwTotalSize = sizeof(hhDriver->stCallPar);
		hhDriver->stCallPar.dwMediaMode = LINEMEDIAMODE_DATAMODEM;
		hhDriver->stCallPar.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
		hhDriver->stCallPar.dwBearerMode = 0;

		hhDriver->hLine = 0;
		}

	if (hhDriver->hLineApp)
		{
		LONG lLineShutdown = lineShutdown(hhDriver->hLineApp);

		if (lLineShutdown == LINEERR_NOMEM)
			{
			//
			// We are in a low memory state, so wait for a while,
			// then try to shutdown the line again. REV: 5/1/2002
			//
			Sleep(500);
			lLineShutdown = lineShutdown(hhDriver->hLineApp);
			}

		if (lLineShutdown != 0)
			{
			assert(FALSE);
			}

		hhDriver->hLineApp = 0;
		}

	if (IsWindow(hhDriver->hwndCnctDlg))
		EndModelessDialog(hhDriver->hwndCnctDlg);

	if (IsWindow(hhDriver->hwndTAPIWindow))
		{
		DestroyWindow(hhDriver->hwndTAPIWindow);
		}

	/* --- Cleanup --- */

	DeleteCriticalSection(&hhDriver->cs);
	free(hhDriver);
	return 0;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvLock
 *
 * DESCRIPTION:
 *  Locks the connection driver's critical section semaphore.
 *
 * ARGUMENTS:
 *  hhDriver    - private driver handle
 *
 * RETURNS:
 *  void
 *
 */
void cnctdrvLock(const HHDRIVER hhDriver)
	{
	EnterCriticalSection(&hhDriver->cs);
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvUnlock
 *
 * DESCRIPTION:
 *  Unlocks the connection driver's critical section semaphore.
 *
 * ARGUMENTS:
 *  hhDriver    - private driver handle
 *
 * RETURNS:
 *  void
 *
 */
void cnctdrvUnlock(const HHDRIVER hhDriver)
	{
	LeaveCriticalSection(&hhDriver->cs);
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *	cnctdrvInit
 *
 * DESCRIPTION:
 *	Initializes the connection handle.	Can be called to reinitialize
 *	the handle.  Does an implicit disconnect.
 *
 * ARGUMENTS:
 *	hhDriver	- private driver handle
 *
 * RETURNS:
 *	0
 *
 */
int WINAPI cnctdrvInit(const HHDRIVER hhDriver)
	{
	long  lRet;
	int   id = 0;
    int   iReturn = 0;

	// Make sure we're disconnected.
	//
	cnctdrvDisconnect(hhDriver, DISCNCT_NOBEEP);

   	// ----------------------------------------------------------------
	// Need to shut down hLineApp and reinitialize everytime we read
	// new data file so that TAPI starts out in a clean and initialized
	// state.  Otherwise, we might inherit values from the previous
	// session.
	// ----------------------------------------------------------------
	if (hhDriver->hLineApp)
		{
		LONG lLineShutdown = lineShutdown(hhDriver->hLineApp);

		if (lLineShutdown == LINEERR_NOMEM)
			{
			//
			// We are in a low memory state, so wait for a while,
			// then try to shutdown the line again. REV: 5/1/2002
			//
			Sleep(500);
			lLineShutdown = lineShutdown(hhDriver->hLineApp);
			}

		if (lLineShutdown != 0)
			{
			assert(FALSE);
			hhDriver->hLineApp = 0;
			return -2;
			}
		}

	hhDriver->hLineApp = 0;

	// Try to get a new LineApp handle now.
	//
	lRet = lineInitialize(&hhDriver->hLineApp, glblQueryDllHinst(),
			              lineCallbackFunc, g_achApp, &hhDriver->dwLineCnt);

	if (lRet != 0)
		{
        iReturn = -3;
		switch (lRet)
			{
		case LINEERR_INIFILECORRUPT:
			id = IDS_ER_TAPI_INIFILE;
			break;

		case LINEERR_NODRIVER:
			id = IDS_ER_TAPI_NODRIVER;
			break;

		case LINEERR_NOMULTIPLEINSTANCE:
			id = IDS_ER_TAPI_NOMULTI;
			break;

#if 0   // rev:08/05/99 We are now printing the lineInitialize() error.
        // rev:08/26/98 We need to make sure there was no error reported.
        //
        case LINEERR_INVALAPPNAME:
        case LINEERR_OPERATIONFAILED:
        case LINEERR_RESOURCEUNAVAIL:
        case LINEERR_INVALPOINTER:
        case LINEERR_REINIT:
        case LINEERR_NODEVICE:
        case LINEERR_NOMEM:
            id = IDS_ER_CNCT_TAPIFAILED;
            break;
#endif

        case LINEERR_OPERATIONUNAVAIL:
            //rev: 08-05-99 If TAPI has not been installed, then return a
            //              unique error code (since it will be handled
            //              differently than other TAPI errors).
            //
            iReturn = -4;

		#if ((NT_EDITION && !NDEBUG) || !NT_EDITION)
            // Run the new Modem wizard if we have not prompted before.
            //
            DoNewModemWizard(sessQueryHwnd(hhDriver->hSession),
                             sessQueryTimeout(hhDriver->hSession));
		#endif // ((NT_EDITION && !NDEBUG) || !NT_EDITION)

            break;

        default:
			id = IDS_ER_TAPI_UNKNOWN;
			break;
			}

		//
		// Only display these errors if in Debug mode in NT_EDITION.
		//
		#if ((NT_EDITION && !NDEBUG) || !NT_EDITION)
		if ( id )
			{
			TCHAR ach[256];
			TCHAR achMessage[256];

			LoadString(glblQueryDllHinst(), id, ach, sizeof(ach) / sizeof(TCHAR));
            if (id == IDS_ER_TAPI_UNKNOWN)
                {
                wsprintf(achMessage, ach, lRet);
                }
            else
                {
                lstrcpy(achMessage, ach);
                }

			TimedMessageBox(sessQueryHwnd(hhDriver->hSession),
							achMessage, NULL, MB_OK | MB_ICONSTOP,
							sessQueryTimeout(hhDriver->hSession));
			}
		#endif //((NT_EDITION && !NDEBUG) || !NT_EDITION)

        return iReturn;
		}

	hhDriver->iStatus			= CNCT_STATUS_FALSE;
	hhDriver->dwLine			= (DWORD)-1;
	hhDriver->dwCountryID		= (DWORD)-1;
	hhDriver->dwPermanentLineId = (DWORD)-1;
	hhDriver->achDest[0]		= TEXT('\0');
	hhDriver->achAreaCode[0]	= TEXT('\0');
	hhDriver->achLineName[0]	= TEXT('\0');
	hhDriver->fUseCCAC			= TRUE;

	/* --- This guy will set defaults --- */

	EnumerateTapiLocations(hhDriver, 0, 0);
	
#if defined(INCL_WINSOCK)
	hhDriver->iPort = 23;
	hhDriver->achDestAddr[0] = TEXT('\0');
#endif

#ifdef INCL_CALL_ANSWERING	
    hhDriver->fAnswering = FALSE;
    hhDriver->fRestoreSettings = FALSE;
    hhDriver->nSendCRLF = 0;
    hhDriver->nLocalEcho = 0;
    hhDriver->nAddLF = 0;
    hhDriver->nEchoplex = 0;
    hhDriver->pvUnregister = 0;
#endif

	return iReturn;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *	cnctdrvLoad
 *
 * DESCRIPTION:
 *  Reads the session file to get stuff connection driver needs.
 *
 * ARGUMENTS:
 *  hhDriver    - private driver handle
 *
 * RETURNS:
 *  0=OK, else error
 *
 */
int WINAPI cnctdrvLoad(const HHDRIVER hhDriver)
	{
    LPVARSTRING pvs;
	unsigned long ul;
	const SF_HANDLE sfhdl = sessQuerySysFileHdl(hhDriver->hSession);

	hhDriver->dwCountryID = (DWORD)-1;
	ul = sizeof(hhDriver->dwCountryID);
	sfGetSessionItem(sfhdl, SFID_CNCT_CC, &ul, &hhDriver->dwCountryID);

	hhDriver->achAreaCode[0] = TEXT('\0');
	ul = sizeof(hhDriver->achAreaCode);
	sfGetSessionItem(sfhdl, SFID_CNCT_AREA, &ul, hhDriver->achAreaCode);

	hhDriver->achDest[0] = TEXT('\0');
	ul = sizeof(hhDriver->achDest);
	sfGetSessionItem(sfhdl, SFID_CNCT_DEST, &ul, hhDriver->achDest);

	hhDriver->dwPermanentLineId = 0;
	ul = sizeof(hhDriver->dwPermanentLineId);
	sfGetSessionItem(sfhdl, SFID_CNCT_LINE, &ul, &hhDriver->dwPermanentLineId);

	hhDriver->fUseCCAC = 1;
	ul = sizeof(hhDriver->fUseCCAC);
	sfGetSessionItem(sfhdl, SFID_CNCT_USECCAC, &ul, &hhDriver->fUseCCAC);

    hhDriver->fRedialOnBusy = 1;
    ul = sizeof(hhDriver->fRedialOnBusy);
    sfGetSessionItem(sfhdl, SFID_CNCT_REDIAL, &ul, &hhDriver->fRedialOnBusy);

#if defined (INCL_WINSOCK)
	hhDriver->iPort = 23;
    ul = sizeof(hhDriver->iPort);
    sfGetSessionItem(sfhdl, SFID_CNCT_IPPORT, &ul, &hhDriver->iPort);

	hhDriver->achDestAddr[0] = TEXT('\0');
	ul = sizeof(hhDriver->achDestAddr);
	sfGetSessionItem(sfhdl, SFID_CNCT_IPDEST, &ul, hhDriver->achDestAddr);
#endif

    hhDriver->fCarrierDetect = FALSE;
    ul = sizeof(hhDriver->fCarrierDetect);
    sfGetSessionItem(sfhdl, SFID_CNCT_CARRIERDETECT, &ul, &hhDriver->fCarrierDetect);


	if ( IsNT() )
		{
		hhDriver->achComDeviceName[0] = TEXT('\0');
		ul = sizeof(hhDriver->achComDeviceName);
		sfGetSessionItem(sfhdl, SFID_CNCT_COMDEVICE, &ul, hhDriver->achComDeviceName);
		}

   	// ----------------------------------------------------------------
	// Need to shut down hLineApp and reinitialize everytime we read
	// new data file so that TAPI starts out in a clean and initialized
	// state.  Otherwise, we might inherit values from the previous
	// session.
	// ----------------------------------------------------------------

	if (hhDriver->hLineApp)
		{
		LONG lLineShutdown = lineShutdown(hhDriver->hLineApp);

		if (lLineShutdown == LINEERR_NOMEM)
			{
			//
			// We are in a low memory state, so wait for a while,
			// then try to shutdown the line again. REV: 5/1/2002
			//
			Sleep(500);
			lLineShutdown = lineShutdown(hhDriver->hLineApp);
			}

		if (lLineShutdown != 0)
			{
			assert(FALSE);
			hhDriver->hLineApp = 0;
			return -2;
			}

		hhDriver->hLineApp = 0;

		if (lineInitialize(&hhDriver->hLineApp, glblQueryDllHinst(),
				lineCallbackFunc, g_achApp, &hhDriver->dwLineCnt))
			{
			assert(FALSE);
			return -3;
			}
		}

	// EnumerateLines() will set the hhDriver->fMatchedPermanentLineID
	// guy if it finds a match for our saved dwPermanentLineId guy
	//
	if ( IsNT() )
		{
		EnumerateLinesNT(hhDriver, 0);
		}
	else
		{
		EnumerateLines(hhDriver, 0);
		}
	
	/* --- If we saved a tapi configuration, restore it. --- */

	if (sfGetSessionItem(sfhdl, SFID_CNCT_TAPICONFIG, &ul, 0) != 0)
		return 0; // Ok, might not be there.

	if ((pvs = malloc(ul)) == 0)
		{
		assert(FALSE);
		return -4;
		}

	if (sfGetSessionItem(sfhdl, SFID_CNCT_TAPICONFIG, &ul, pvs) == 0)
		{
		if (hhDriver->fMatchedPermanentLineID)
			{
			LPVOID pv = (BYTE *)pvs + pvs->dwStringOffset;

			if (lineSetDevConfig(hhDriver->dwLine, pv,
			        pvs->dwStringSize, DEVCLASS) != 0)
				{
                // This error prevented a user from even opening a session
                // file if the file contained TAPI info and the user had
                // never installed a modem. We modified the error that appears
                // when you actually try to USE a non-existant modem so that
                // we could suppress the display of this error  jkh 8/3/98
#if 0
                TCHAR ach[FNAME_LEN];

                LoadString(glblQueryDllHinst(), IDS_OPEN_FAILED, ach,
				    sizeof(ach) / sizeof(TCHAR));

				TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
					            MB_OK | MB_ICONINFORMATION,
								sessQueryTimeout(hhDriver->hSession));

                                free(pvs);
                                pvs = NULL;
                                return -5;
#endif
                                }
			}
		}

        free(pvs);
        pvs = NULL;
	return 0;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvSave
 *
 * DESCRIPTION:
 *  Saves connection settings to the session file
 *
 * ARGUMENTS:
 *  hhDriver    - private driver handle
 *
 * RETURNS:
 *  0=OK, else error
 *
 */
int WINAPI cnctdrvSave(const HHDRIVER hhDriver)
	{
	DWORD dwSize;
	unsigned long ul;
	LPVARSTRING pvs = NULL;
	const SF_HANDLE sfhdl = sessQuerySysFileHdl(hhDriver->hSession);

	sfPutSessionItem(sfhdl, SFID_CNCT_CC, sizeof(hhDriver->dwCountryID),
		&hhDriver->dwCountryID);

	sfPutSessionItem(sfhdl, SFID_CNCT_AREA,
		(lstrlen(hhDriver->achAreaCode) + 1) * sizeof(TCHAR),
			hhDriver->achAreaCode);

	sfPutSessionItem(sfhdl, SFID_CNCT_DEST,
		(lstrlen(hhDriver->achDest) + 1) * sizeof(TCHAR), hhDriver->achDest);

	sfPutSessionItem(sfhdl, SFID_CNCT_LINE, sizeof(hhDriver->dwPermanentLineId),
		&hhDriver->dwPermanentLineId);

	sfPutSessionItem(sfhdl, SFID_CNCT_USECCAC, sizeof(hhDriver->fUseCCAC),
		&hhDriver->fUseCCAC);

	sfPutSessionItem(sfhdl, SFID_CNCT_REDIAL, sizeof(hhDriver->fRedialOnBusy),
		&hhDriver->fRedialOnBusy);

#if defined (INCL_WINSOCK)
	sfPutSessionItem(sfhdl, SFID_CNCT_IPPORT, sizeof(hhDriver->iPort),
		&hhDriver->iPort);

	sfPutSessionItem(sfhdl, SFID_CNCT_IPDEST,
		(lstrlen(hhDriver->achDestAddr) + 1) * sizeof(TCHAR),
			hhDriver->achDestAddr);
#endif

	/* --- Usual lines of code to use TAPI --- */

	if (hhDriver->hLineApp && hhDriver->dwLine != (DWORD)-1 &&
			!IN_RANGE(hhDriver->dwPermanentLineId, DIRECT_COM1, DIRECT_COM4) &&
            hhDriver->dwPermanentLineId != DIRECT_COM_DEVICE &&
            hhDriver->dwPermanentLineId != DIRECT_COMWINSOCK)
		{
		if ((pvs = malloc(sizeof(VARSTRING))) == 0)
			{
			assert(FALSE);
			return 0;
			}
		
		memset( pvs, 0, sizeof(VARSTRING) );
		pvs->dwTotalSize = sizeof(VARSTRING);

		if (lineGetDevConfig(hhDriver->dwLine, pvs, DEVCLASS) != 0)
			{
			assert(FALSE);
            free(pvs);
            pvs = NULL;
			return 0;
			}

		if (pvs->dwNeededSize > pvs->dwTotalSize)
			{
			dwSize = pvs->dwNeededSize;
			free(pvs);
                        pvs = NULL;

			if ((pvs = malloc(dwSize)) == 0)
				{
				assert(FALSE);
				return 0;
				}

			memset( pvs, 0, dwSize );
			pvs->dwTotalSize = dwSize;

			if (lineGetDevConfig(hhDriver->dwLine, pvs, DEVCLASS) != 0)
				{
				assert(FALSE);
                free(pvs);
                pvs = NULL;
				return 0;
				}
			}

		/* --- Store the whole structure --- */

		ul = pvs->dwTotalSize;
		sfPutSessionItem(sfhdl, SFID_CNCT_TAPICONFIG, ul, pvs);
                free(pvs);
                pvs = NULL;
		}

	if ( IsNT() && hhDriver->dwPermanentLineId == DIRECT_COM_DEVICE)
		{
		ul = sizeof(hhDriver->achComDeviceName);

		sfPutSessionItem(sfhdl, SFID_CNCT_COMDEVICE, ul,
			hhDriver->achComDeviceName);
		}

	sfPutSessionItem(sfhdl, SFID_CNCT_CARRIERDETECT, sizeof(hhDriver->fCarrierDetect),
	&hhDriver->fCarrierDetect);

	return 0;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvQueryStatus
 *
 * DESCRIPTION:
 *  Returns the current connection status as defined in <tdll\cnct.h>
 *
 * ARGUMENTS:
 *  hhDriver - private driver handle
 *
 * RETURNS:
 *  connection status or error code
 *
 */
int WINAPI cnctdrvQueryStatus(const HHDRIVER hhDriver)
	{
	int iStatus = CNCT_STATUS_FALSE;

	if (hhDriver == 0)
		{
		assert(FALSE);
		iStatus = CNCT_BAD_HANDLE;
		}
	else
		{
		cnctdrvLock(hhDriver);
		iStatus = hhDriver->iStatus;   //* hard-code for now.
		cnctdrvUnlock(hhDriver);
		}

	return iStatus;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  SetStatus
 *
 * DESCRIPTION:
 *  There's actually more to setting the connection status than just
 *  setting the status variable as the code below indicates.  Dumb
 *  question:  Why aren't there any locks in this code.  Dumb Answer:
 *  This function is only called from the ConnectLoop thread context
 *  which has already locked things down.
 *
 * ARGUMENTS:
 *  hhDriver    - private driver handle
 *  iStatus     - new status
 *
 * RETURNS:
 *  void
 *
 */
void SetStatus(const HHDRIVER hhDriver, const int iStatus)
	{
	HCLOOP	hCLoop;
	/* --- Don't do things twice --- */

	const HWND hwndToolbar = sessQueryHwndToolbar(hhDriver->hSession);

	cnctdrvLock(hhDriver);

	if (iStatus == hhDriver->iStatus)
		{
		if (iStatus == CNCT_STATUS_TRUE || iStatus == CNCT_STATUS_FALSE)
			{
			hCLoop = sessQueryCLoopHdl(hhDriver->hSession);
			if (hCLoop)
				CLoopSndControl(hCLoop, CLOOP_RESUME, CLOOP_SB_CNCTDRV);
            }

		cnctdrvUnlock(hhDriver);
		return;
		}

	/* --- Set the status, an exciting new adventure game --- */

	switch (iStatus)
		{
	case CNCT_STATUS_TRUE:
            hCLoop = sessQueryCLoopHdl(hhDriver->hSession);
          #ifdef INCL_CALL_ANSWERING
            // If we are going from answering to connected, that means
            // we have answered a call. So tweak the ASCII settings so
            // that they make chatting possible. - cab:11/20/96
            //
            if (hhDriver->fAnswering)
                {
                // Store old ASCII settings, and set the new ones.
                //
                hhDriver->nSendCRLF = CLoopGetSendCRLF(hCLoop);
                hhDriver->nLocalEcho = CLoopGetLocalEcho(hCLoop);
                hhDriver->nAddLF = CLoopGetAddLF(hCLoop);
                hhDriver->nEchoplex = CLoopGetEchoplex(hCLoop);

                CLoopSetSendCRLF(hCLoop, TRUE);
                CLoopSetLocalEcho(hCLoop, TRUE);
                CLoopSetAddLF(hCLoop, TRUE);
                CLoopSetEchoplex(hCLoop, TRUE);

                hhDriver->fRestoreSettings = TRUE;
                }
          #endif
            hhDriver->iStatus = CNCT_STATUS_TRUE;
            assert(hCLoop);
            if (hCLoop)
                {
                CLoopRcvControl(hCLoop, CLOOP_RESUME, CLOOP_RB_CNCTDRV);
                CLoopSndControl(hCLoop, CLOOP_RESUME, CLOOP_SB_CNCTDRV);
                }

            NotifyClient(hhDriver->hSession, EVENT_CONNECTION_OPENED, 0);
            sessBeeper(hhDriver->hSession);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_DIAL, FALSE);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_HANGUP, TRUE);
            break;

	case CNCT_STATUS_CONNECTING:
            hhDriver->iStatus = CNCT_STATUS_CONNECTING;
            DialingMessage(hhDriver, IDS_DIAL_OFFERING); // temp
            NotifyClient(hhDriver->hSession, EVENT_CONNECTION_INPROGRESS, 0);
            EnableDialNow(hhDriver->hwndCnctDlg, FALSE);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_DIAL, FALSE);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_HANGUP, TRUE);
            break;

	case CNCT_STATUS_DISCONNECTING:
            hhDriver->iStatus = CNCT_STATUS_DISCONNECTING;
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_DIAL, FALSE);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_HANGUP, FALSE);
            break;

	case CNCT_STATUS_FALSE:
		hCLoop = sessQueryCLoopHdl(hhDriver->hSession);
          #ifdef INCL_CALL_ANSWERING
            // Since this is called when we disconnect we need to restore
            // any ASCII Settings here. - cab:11/20/96
            //
            if ( hhDriver->fRestoreSettings && hCLoop ) //mpt: so that we don't reference a null pointer
                {
                CLoopSetSendCRLF(hCLoop, hhDriver->nSendCRLF);
                CLoopSetLocalEcho(hCLoop, hhDriver->nLocalEcho);
                CLoopSetAddLF(hCLoop, hhDriver->nAddLF);
                CLoopSetEchoplex(hCLoop, hhDriver->nEchoplex);
                hhDriver->fRestoreSettings = FALSE;
                }
            hhDriver->fAnswering = FALSE;
          #endif
            hhDriver->iStatus = CNCT_STATUS_FALSE;
            if (hCLoop)
                {
                CLoopRcvControl(hCLoop, CLOOP_RESUME, CLOOP_RB_CNCTDRV);
                CLoopSndControl(hCLoop, CLOOP_RESUME, CLOOP_SB_CNCTDRV);
                }
            NotifyClient(hhDriver->hSession, EVENT_CONNECTION_CLOSED, 0);
            EnableDialNow(hhDriver->hwndCnctDlg, TRUE);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_DIAL, TRUE);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_HANGUP, FALSE);
            break;

        case CNCT_STATUS_ANSWERING:
          #ifdef INCL_CALL_ANSWERING
            hhDriver->fAnswering = TRUE;
            hhDriver->iStatus = CNCT_STATUS_ANSWERING;
            NotifyClient(hhDriver->hSession, EVENT_CONNECTION_INPROGRESS, 0);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_DIAL, FALSE);
			ToolbarEnableButton(hwndToolbar, IDM_ACTIONS_HANGUP, TRUE);
          #endif
            break;

	default:
		assert(FALSE);
		break;
		}

	cnctdrvUnlock(hhDriver);

	/* --- Notify status bar so it can update it's display --- */

	PostMessage(sessQueryHwndStatusbar(hhDriver->hSession), SBR_NTFY_REFRESH,
		(WPARAM)SBR_CNCT_PART_NO, 0);

	return;
	}

#ifdef INCL_CALL_ANSWERING
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  WaitForCRcallback
 *
 * DESCRIPTION:
 *  This function gets registered as a callback with the cloop.  Every
 *  character that the cloop gets, is passed back to this function.  When
 *  this function finds a CR, it indicates that a connection has been
 *  established.  Note that this applies only to an answer mode connection,
 *  in the direct connect driver.
 *
 * ARGUMENTS:
 *  ECHAR   ech  -   The character returned from cloop.
 *  void    *p   -   A void pointer passed back from cloop.  This is
 *                   the enternal connection driver handle.
 *
 * RETURNS:
 *  CLOOP_DISCARD unless the character is a CR where is returns CLOOP_KEEP.
 *
 * AUTHOR: C. Baumgartner, 11/20/96 (ported from HAWin32)
 */
int WaitForCRcallback(ECHAR ech, void *p)
    {
    int   iRet = CLOOP_DISCARD; // Discard all characters except the CR.
    TCHAR chC = (TCHAR) ech;
    const HHDRIVER hhDriver = (HHDRIVER)p;

    if (chC == TEXT('\r'))
        {
        CLoopUnregisterRmtInputChain(hhDriver->pvUnregister);
        hhDriver->pvUnregister = 0;

        // Okay, we are connected now.
        //
        SetStatus(hhDriver, CNCT_STATUS_TRUE);

        iRet = CLOOP_KEEP;
        }

    return iRet;
    }

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  WaitForCRinit
 *
 * DESCRIPTION:
 *  This function is called to register a new string match function
 *  with the cloop.  It unregisters a previously registered function
 *  if necessary. It basically will cause us to wait for a carriage
 *  return.
 *
 * ARGUMENTS:
 *  HHDRIVER    hhDriver    -   The internal connection handle.
 *
 * RETURNS:
 *  0 if successful, otherwise -1.
 *
 * AUTHOR: C. Baumgartner, 11/20/96 (ported from HAWin32)
 */
static int WaitForCRinit(const HHDRIVER hhDriver)
    {
    const HCLOOP hCLoop = sessQueryCLoopHdl(hhDriver->hSession);

    if (!hCLoop)
        {
        return -1;
        }

    // If we are already registered, unregister.
    //
    if (hhDriver->pvUnregister != 0)
        {
        CLoopUnregisterRmtInputChain(hhDriver->pvUnregister);
        hhDriver->pvUnregister = 0;
        }

    // We need to un-block CLoop so we can look at the
    // characters as they come in.
    //
	CLoopRcvControl(hCLoop, CLOOP_RESUME, CLOOP_RB_CNCTDRV);

    // Register the match function with the cloop.
    //
    hhDriver->pvUnregister = CLoopRegisterRmtInputChain(hCLoop,
        WaitForCRcallback, hhDriver);

    if (hhDriver->pvUnregister == 0)
        {
        return -1;
        }

    return 0;
    }
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvComEvent
 *
 * DESCRIPTION:
 *  Com routines call the this to notify connection routines that some
 *  significant event has happened (ie. carrier lost).  The connetion
 *  driver decides what it is interested in knowing however by
 *  querying the com drivers for the specific data.
 *
 * ARGUMENTS:
 *  hhDriver - private connection driver handle
 *  event    - the com event we are being notified of
 *
 * RETURNS:
 *  0
 *
 */
int WINAPI cnctdrvComEvent(const HHDRIVER hhDriver, const enum COM_EVENTS event)
	{
	int		iRet;
	TCHAR 	ach[MAX_PATH];
#if defined(INCL_WINSOCK)
	char	achMsg[512];
#endif
	HCOM	hCom;

	if (hhDriver == 0)
		{
		assert(FALSE);
		return CNCT_BAD_HANDLE;
		}

    if (event == CONNECT)
        {
		#if defined (INCL_WINSOCK)
	    // If we are connected via Winsock, there will be some ComEvents
	    // that we have to handle
	    if (hhDriver->dwPermanentLineId == DIRECT_COMWINSOCK)
		    {
		    hCom = sessQueryComHdl(hhDriver->hSession);
		    iRet = ComDriverSpecial(hCom, "Query ISCONNECTED", ach, MAX_PATH);

		    if (iRet == COM_OK)
			    {
				int iPortOpen = atoi(ach);
			    // Do we want to initiate a disconnect?  Only if we're
			    // connected.
			    if (iPortOpen == COM_PORT_NOT_OPEN)
				    {
				    if (hhDriver->iStatus == CNCT_STATUS_TRUE)
					    {
                        // If we are already connected, then beep when
                        // we disconnect. - cab:12/06/96
                        //
						//mpt:10-28-97 added exit upon disconnect feature
						NotifyClient(hhDriver->hSession, EVENT_LOST_CONNECTION,
							         CNCT_LOSTCARRIER | (sessQueryExit(hhDriver->hSession) ? DISCNCT_EXIT :  0 ));
					    }
				    else if (hhDriver->iStatus == CNCT_STATUS_CONNECTING)
					    {
					    NotifyClient(hhDriver->hSession, EVENT_LOST_CONNECTION,
							         CNCT_LOSTCARRIER | DISCNCT_NOBEEP);

					    LoadString(glblQueryDllHinst(), IDS_ER_TCPIP_BADADDR, ach, MAX_PATH);
					    wsprintf(achMsg, ach, hhDriver->achDestAddr, hhDriver->iPort);
					    TimedMessageBox(sessQueryHwnd(hhDriver->hSession),
                                        achMsg, NULL, MB_OK | MB_ICONINFORMATION,
										sessQueryTimeout(hhDriver->hSession));
					    }
				    }
			    else if (iPortOpen == COM_PORT_OPEN)
				    {
				    SetStatus(hhDriver, CNCT_STATUS_TRUE);
				    }
			    }
		    }
		else
		#endif // defined (INCL_WINSOCK)
        if (IN_RANGE(hhDriver->dwPermanentLineId, DIRECT_COM1, DIRECT_COM4) ||
            hhDriver->dwPermanentLineId == DIRECT_COM_DEVICE)
            {
			// Checking the status of DCD before disconnecting
			// is a good idea, prevents us hanging up whenever we
			// get any event while connected
			// - mpt:08-26-97
			hCom = sessQueryComHdl(hhDriver->hSession);
			iRet = ComDriverSpecial(hCom, "Query DCD_STATUS", ach, MAX_PATH);

		    if (iRet == COM_OK)
			    {
				int iPortOpen = atoi(ach);
			    // Do we want to initiate a disconnect?  Only if we're
			    // connected.
			    if (iPortOpen == COM_PORT_NOT_OPEN)
				    {
				    if (hhDriver->iStatus == CNCT_STATUS_TRUE)
					    {
						// If we are direct cabled, and we're connected, then
						// the other end just disconnected, so disconnect now.
						// - cab:11/20/96
						//
						// Note: We must disconnect by posting a message to
						// thread one. This is because if we get here, we were
						// called from the context of the com thread, which
						// will not exit properly if cnctdrvDisconnect is called.
						// - cab:11/21/96
						//
						//
                        // If we are already connected, then beep when
                        // we disconnect. - cab:12/06/96
                        //
						//mpt:10-28-97 added exit upon disconnect feature
						NotifyClient(hhDriver->hSession, EVENT_LOST_CONNECTION,
							         CNCT_LOSTCARRIER | (sessQueryExit(hhDriver->hSession) ? DISCNCT_EXIT :  0 ));
					    }
					#if defined(INCL_CALL_ANSWERING)
				    else if (hhDriver->iStatus == CNCT_STATUS_CONNECTING ||
						     hhDriver->iStatus == CNCT_STATUS_ANSWERING)
					#else // defined(INCL_CALL_ANSWERING)
				    else if (hhDriver->iStatus == CNCT_STATUS_CONNECTING)
					#endif // defined(INCL_CALL_ANSWERING)
					    {
					    NotifyClient(hhDriver->hSession, EVENT_LOST_CONNECTION,
							         CNCT_LOSTCARRIER | DISCNCT_NOBEEP);

						LoadString(glblQueryDllHinst(), IDS_ER_CNCT_PORTFAILED, ach, MAX_PATH);
					    wsprintf(achMsg, ach, hhDriver->achComDeviceName);
					    TimedMessageBox(sessQueryHwnd(hhDriver->hSession),
                                        achMsg, NULL, MB_OK | MB_ICONINFORMATION,
										sessQueryTimeout(hhDriver->hSession));
					    }
				    }
				else if (iPortOpen == COM_PORT_OPEN &&
					     hhDriver->iStatus != CNCT_STATUS_ANSWERING)
					{
					SetStatus(hhDriver, CNCT_STATUS_TRUE);
					}
			    }
			#if defined(INCL_CALL_ANSWERING)
            if (hhDriver->iStatus == CNCT_STATUS_ANSWERING)
                {
                // If we are a direct cabled connection, and we are waiting
                // for a call, connect when we see a carriage return.
                //
                WaitForCRinit(hhDriver);
                }
			#endif // defined(INCL_CALL_ANSWERING)
            }
        }
	return 0;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  DoAnswerCall
 *
 * DESCRIPTION:
 *  Sets up TAPI to answer the next data modem call.
 *
 * ARGUMENTS:
 *  hhDriver - private driver handle
 *
 * RETURNS:
 *  0 or error
 *
 * AUTHOR:  C. Baumgartner, 11/25/96 (ported from HAWin32)
 */
int DoAnswerCall(const HHDRIVER hhDriver)
    {
	TCHAR ach[256];

    // Believe it or not, this is all one has to do to setup and
    // answer a call.  Quite a contrast to placing a call.
    //
    if (TRAP(lineOpen(hhDriver->hLineApp, hhDriver->dwLine, &hhDriver->hLine,
        hhDriver->dwAPIVersion, 0, (DWORD_PTR)hhDriver, LINECALLPRIVILEGE_OWNER,
        LINEMEDIAMODE_DATAMODEM, 0)) != 0)
        {
        assert(0);
	    LoadString(glblQueryDllHinst(), IDS_ER_CNCT_TAPIFAILED, ach, sizeof(ach) / sizeof(TCHAR));
	    TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
			            MB_OK | MB_ICONINFORMATION | MB_TASKMODAL,
						sessQueryTimeout(hhDriver->hSession));
        return -1;
        }

    // The the line app priority for compliance with TAPI specifications.
    // mrw:9/18/96
    //
	LoadString(glblQueryDllHinst(), IDS_GNRL_APPNAME, ach, sizeof(ach) / sizeof(TCHAR));
    TRAP(lineSetAppPriority(ach, LINEMEDIAMODE_DATAMODEM, 0, 0, 0, 1));

    // Set line notifications we want to receive
    //
    TRAP(lineSetStatusMessages(hhDriver->hLine, LINEDEVSTATE_RINGING, 0));

    SetStatus(hhDriver, CNCT_STATUS_ANSWERING);
    return 0;
    }

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  DoMakeCall
 *
 * DESCRIPTION:
 *  Performs the neccessary TAPI rituals to place an outbound call.
 *
 * ARGUMENTS:
 *  hhDriver - private driver handle
 *  uFlags   - connection flags
 *
 * RETURNS:
 *  0 or error
 *
 * AUTHOR:  C. Baumgartner, 11/25/96 (ported from cnctdrvConnect)
 */
int DoMakeCall(const HHDRIVER hhDriver, const unsigned int uFlags)
    {
    unsigned int  uidErr = 0;
    int           iRet = 0;
    LINEDEVSTATUS stLnDevStat;
    TCHAR         ach[256];
    BOOL          msgFlag = FALSE;
    
	tapiReinit(hhDriver);

    //
    // Set the line settings.
    //
    if (cncttapiSetLineConfig(hhDriver->dwLine, sessQueryComHdl(hhDriver->hSession)) != 0)
        {
        assert(0);
        uidErr = IDS_ER_CNCT_TAPIFAILED;
        iRet = -1;
        msgFlag = TRUE;
        goto ERROR_EXIT;
        }

	/* --- Open the line, pass driver handle for data reference --- */

	if (TRAP(lineOpen(hhDriver->hLineApp, hhDriver->dwLine,
	        &hhDriver->hLine, hhDriver->dwAPIVersion, 0, (DWORD_PTR)hhDriver,
				LINECALLPRIVILEGE_NONE, 0, 0)) != 0)
            {
            assert(0);
            uidErr = IDS_ER_CNCT_TAPIFAILED;
            iRet = -1;
            msgFlag = TRUE;
            goto ERROR_EXIT;
            }

	/* --- Set line notifications we want to receive, mrw,2/28/95 --- */

	TRAP(lineSetStatusMessages(hhDriver->hLine,
		LINEDEVSTATE_INSERVICE | LINEDEVSTATE_OUTOFSERVICE, 0));

	/* --- Check if our device is in service, mrw,2/28/95 --- */

	stLnDevStat.dwTotalSize = sizeof(stLnDevStat);
	TRAP(lineGetLineDevStatus(hhDriver->hLine, &stLnDevStat));

	if ((stLnDevStat.dwDevStatusFlags & LINEDEVSTATUSFLAGS_INSERVICE) == 0)
            {
            if (DialogBoxParam(glblQueryDllHinst(),
			MAKEINTRESOURCE(IDD_CNCT_PCMCIA),
                        sessQueryHwnd(hhDriver->hSession), PCMCIADlg,
                        (LPARAM)hhDriver) == FALSE)
                {
                iRet = -2;
                goto ERROR_EXIT;
                }
            }

	/* --- Launch the dialing dialog, or go right into passthrough mode. --- */

	if ((uFlags & CNCT_PORTONLY) == 0)
            {
            if (!IsWindow(hhDriver->hwndCnctDlg))
                {
                hhDriver->hwndCnctDlg = DoModelessDialog(glblQueryDllHinst(),
				MAKEINTRESOURCE(IDD_DIALING), sessQueryHwnd(hhDriver->hSession),
                                DialingDlg, (LPARAM)hhDriver);
                }
            }

	/* --- Make the call (oooh, how exciting!) --- */

	memset(&hhDriver->stCallPar, 0, sizeof(hhDriver->stCallPar));
	hhDriver->stCallPar.dwTotalSize = sizeof(hhDriver->stCallPar);
	hhDriver->stCallPar.dwMediaMode = LINEMEDIAMODE_DATAMODEM;
	hhDriver->stCallPar.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;

	if (uFlags & CNCT_PORTONLY)
		hhDriver->stCallPar.dwBearerMode = LINEBEARERMODE_PASSTHROUGH;

	if ((hhDriver->lMakeCallId = lineMakeCall(hhDriver->hLine,
			&hhDriver->hCall, hhDriver->achDialableDest,
                        hhDriver->dwCountryCode, &hhDriver->stCallPar)) < 0)
            {
            #if defined(_DEBUG)
            char ach[50];
            wsprintf(ach, "lineMakeCall returned %x", hhDriver->lMakeCallId);
            MessageBox (0, ach, "debug", MB_OK);
            #endif

            switch (hhDriver->lMakeCallId)
                {
				case LINEERR_BEARERMODEUNAVAIL:
				case LINEERR_INVALBEARERMODE:
                    uidErr = IDS_ER_CNCT_PASSTHROUGH;
                    iRet   = -6;
                    msgFlag = TRUE;
					goto ERROR_EXIT;

                case LINEERR_RESOURCEUNAVAIL:
                case LINEERR_CALLUNAVAIL:
                    uidErr = IDS_ER_CNCT_CALLUNAVAIL;
                    iRet   = -3;
                    msgFlag = TRUE;
                    goto ERROR_EXIT;

                case LINEERR_DIALDIALTONE:
                case LINEERR_DIALPROMPT:
                    if (DoDelayedCall(hhDriver) != 0)
                        {
                        iRet = -4;
                        msgFlag = TRUE;
                        goto ERROR_EXIT;
                        }

                    break;

                default:
                    iRet = -5;
                    msgFlag = TRUE;
                    goto ERROR_EXIT;
                    }
                }

	SetStatus(hhDriver, CNCT_STATUS_CONNECTING);
	return 0;

	/* --- Error exit --- */

ERROR_EXIT:

        // Change this so that the dialog is destroyed before the
        // error message is displayed. Otherwise, the timer that
        // handled redials continued to pump a redial message once
        // every second, causing HT to go into a very nasty loop. mpt 02SEP98

        if (IsWindow(hhDriver->hwndCnctDlg))
            {
            EndModelessDialog(hhDriver->hwndCnctDlg);
            hhDriver->hwndCnctDlg = 0;
            }

        if ( msgFlag )
            {
            LoadString(glblQueryDllHinst(), uidErr, ach, sizeof(ach) / sizeof(TCHAR));

            TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
				            MB_OK | MB_ICONINFORMATION | MB_TASKMODAL,
            sessQueryTimeout(hhDriver->hSession));
            }

	return iRet;
    }

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvConnect
 *
 * DESCRIPTION:
 *  Attempts to dial the modem.
 *
 * ARGUMENTS:
 *  hhDriver - private driver handle
 *  uFlags   - connection flags
 *
 * RETURNS:
 *  0 or error
 *
 */
int WINAPI cnctdrvConnect(const HHDRIVER hhDriver, const unsigned int uFlags)
	{
	TCHAR 	ach[FNAME_LEN];
	#if defined(INCL_WINSOCK)
	//
	// MAX_IP_ADDR_LEN+11+1 = buffer size of hhDriver->achDestAddr +
	// settings string "SET IPADDR=" + 1 for the terminating NULL
	// character.  REV 09/20/2000
	//
	TCHAR	szInstruct[MAX_IP_ADDR_LEN+11+1]; // Used only for WinSock
	TCHAR   szResult[MAX_IP_ADDR_LEN+11+1];   // Used only for WinSock
	int     iNumChars;
	#endif //defined (INCL_WINSOCK)
	TCHAR	achNewCnct[FNAME_LEN];
	TCHAR	achCom[MAX_PATH];
	BOOL	fGetNewName = FALSE;
	HICON	hIcon;
	HCOM	hCom;
	int 	hIconId;
	int 	fFlag;
	unsigned int uidErr = IDS_ER_CNCT_TAPIFAILED;

	if (hhDriver == 0)
		{
		assert(FALSE);
		return CNCT_BAD_HANDLE;
		}

	/* --- Makes for easier referencing --- */

	hCom = sessQueryComHdl(hhDriver->hSession);

	/* --- Check to see we're not already connected --- */

	if (cnctdrvQueryStatus(hhDriver) != CNCT_STATUS_FALSE)
		return CNCT_ERROR;

	// JMH 05-29-96 This is needed to prevent CLoop from processing
	// activity on the terminal window while TAPI is connecting.
	//
	CLoopRcvControl(sessQueryCLoopHdl(hhDriver->hSession),
				    CLOOP_SUSPEND,
					CLOOP_RB_CNCTDRV);
	CLoopSndControl(sessQueryCLoopHdl(hhDriver->hSession),
					CLOOP_SUSPEND,
					CLOOP_SB_CNCTDRV);

	/* --- Just on the off chance we still have an open line --- */

	if (hhDriver->hLineApp && hhDriver->hLine)
		{
		lineClose(hhDriver->hLine);
		memset(&hhDriver->stCallPar, 0, sizeof(hhDriver->stCallPar));
		hhDriver->stCallPar.dwTotalSize = sizeof(hhDriver->stCallPar);
		hhDriver->stCallPar.dwMediaMode = LINEMEDIAMODE_DATAMODEM;
		hhDriver->stCallPar.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
		hhDriver->stCallPar.dwBearerMode = 0;
		hhDriver->hLine = 0;
		}

	if (hhDriver->hLineApp && hhDriver->dwLineCnt == 0 &&
            (uFlags & CNCT_PORTONLY) == 0)
		{
    	DoNewModemWizard(sessQueryHwnd(hhDriver->hSession),
                         sessQueryTimeout(hhDriver->hSession));
		}

	/* --- Ask for new session name only if needed  --- */

	sessQueryName(hhDriver->hSession, ach, sizeof(ach));

	achNewCnct[0] = TEXT('\0');
	LoadString(glblQueryDllHinst(),	IDS_GNRL_NEW_CNCT, achNewCnct,
		sizeof(achNewCnct) / sizeof(TCHAR));

	if (ach[0] == TEXT('\0') || lstrcmp(achNewCnct, ach) == 0)
		{
		// This can only happen if the user double-clicks on the term.exe or
		// there is no session name given on the command line.
		// In this case give the "New Connection" name to the session.
		//
		sessSetName(hhDriver->hSession, achNewCnct);

		if (!(uFlags & CNCT_PORTONLY))
            fGetNewName = TRUE;
		}
	else if (uFlags & CNCT_NEW)
		{
		// This can only happen if the user selects 'File | New Connection'
		// from the menus.
		//
		sessSetName(hhDriver->hSession, achNewCnct);
		sessSetIsNewSession(hhDriver->hSession, TRUE);
		}
	#if defined (INCL_WINSOCK)
	else if (uFlags & CNCT_WINSOCK)
		{
		//
		// Make sure we don't overwrite the buffer. If the string
		// is too long, then truncate to the hhDriver->achDestAddr
		// size of MAX_AP_ADDR_LEN.  REV 09/20/2000 
		//
		StrCharCopyN(hhDriver->achDestAddr, ach, MAX_IP_ADDR_LEN);
		hhDriver->achDestAddr[MAX_IP_ADDR_LEN - 1] = TEXT('\0');
		hhDriver->dwPermanentLineId = DIRECT_COMWINSOCK;
		}
	#endif // defined (INCL_WINSOCK)

	if (fGetNewName || (uFlags & CNCT_NEW))
		{
		if (DialogBoxParam(glblQueryDllHinst(), MAKEINTRESOURCE(IDD_NEWCONNECTION),
			sessQueryHwnd(hhDriver->hSession), NewConnectionDlg,
				(LPARAM)hhDriver->hSession) == FALSE)
			{
			if (uFlags & CNCT_NEW)
				{
				sessQueryOldName(hhDriver->hSession, ach, sizeof(ach));
				sessSetName(hhDriver->hSession, ach);
				sessSetIsNewSession(hhDriver->hSession, FALSE);
				}
			goto ERROR_EXIT;
			}
		else
			{
			if (uFlags & CNCT_NEW)
				{
				sessQueryName(hhDriver->hSession, ach, sizeof(ach));
				hIcon = sessQueryIcon(hhDriver->hSession);
				hIconId = sessQueryIconID(hhDriver->hSession);

				ReinitializeSessionHandle(hhDriver->hSession, FALSE);
				CLoopSndControl(sessQueryCLoopHdl(hhDriver->hSession),
                                CLOOP_SUSPEND,
                                CLOOP_SB_CNCTDRV);

				sessSetName(hhDriver->hSession, ach);
				sessSetIconID(hhDriver->hSession, hIconId);
				}
			}
		}

	/* --- Load the Standard Com  drivers --- */

	ComLoadStdcomDriver(hCom);

	// There are a bunch of conditions that can trigger the
	// phone dialog.
	//
	fFlag = FALSE;

	// If no phone number, bring up new phone dialog here
	// Unless we have a direct to com port selected
    //
    // Don't display the dialog if we are answering, because
    // we don't need a phone number. - cab:11/19/96
    //
	
	if (!IN_RANGE(hhDriver->dwPermanentLineId, DIRECT_COM1, DIRECT_COM4) &&
            hhDriver->dwPermanentLineId != DIRECT_COM_DEVICE &&
			!(uFlags & (CNCT_PORTONLY | CNCT_ANSWER)))
		{
		#ifdef INCL_WINSOCK
        // If the driver is WinSock, then check for a
        // destination IP address. - cab:11/19/96
        //
        if (hhDriver->dwPermanentLineId == DIRECT_COMWINSOCK &&
                hhDriver->achDestAddr[0] == TEXT('\0'))
            {
            fFlag = TRUE;
            }
		#endif // defined (INCL_WINSOCK)
        // If the driver isn't WinSock, then we must be using
        // TAPI, so check for a destination phone number. - cab:11/19/96
        //
        if (hhDriver->dwPermanentLineId != DIRECT_COMWINSOCK &&
                (hhDriver->achDest[0] == TEXT('\0') || 
				 hhDriver->achDialableDest[0] == TEXT('\0') ||
				 hhDriver->achCanonicalDest[0] == TEXT('\0')))
            {
            fFlag = TRUE;
            }
		}

	// New connections trigger this dialog
	//
	if (uFlags & CNCT_NEW)
		fFlag = TRUE;

	// If the modem/port we saved no longer exists
	//
	//if (!hhDriver->fMatchedPermanentLineID)
	//	fFlag = TRUE;

	// Note:  Passing the property sheet page here because property
	//        sheets use same code and don't have access directly
	//        to the private driver handle.  Upper wacker will have
	//        to address the problem differently - mrw.

	if (fFlag)
		{
		PROPSHEETPAGE psp;

		// Before you go and critize this goto target come talk to
		// me.	There are enough things going on here that a goto
		// is warranted in my humble opinion. - mrw

NEWPHONEDLG:

		psp.lParam = (LPARAM)hhDriver->hSession;

		if (DialogBoxParam(glblQueryDllHinst(),
			MAKEINTRESOURCE(IDD_CNCT_NEWPHONE),
				sessQueryHwnd(hhDriver->hSession), NewPhoneDlg,
					(LPARAM)&psp) == FALSE)
			{
			goto ERROR_EXIT;
			}
		else if (IN_RANGE(hhDriver->dwPermanentLineId, DIRECT_COM1, DIRECT_COM4) ||
				(IsNT() && hhDriver->dwPermanentLineId == DIRECT_COM_DEVICE))
			{
			//
			// See if the "Configure..." button has already been clicked
			// and the ComDeviceDialog() function has already been called
			// for this COM device.
			//
			TCHAR szPortName[MAX_PATH];

			ComGetPortName(hCom, szPortName, MAX_PATH);

			if (StrCharCmp(szPortName, hhDriver->achComDeviceName) != 0 )
				{
				/* --- Bring up the port configure dialog --- */
				if (hhDriver->dwPermanentLineId == DIRECT_COM_DEVICE)
					{
					ComSetPortName(hCom, hhDriver->achComDeviceName);
					}
				else
					{
					wsprintf(ach, TEXT("COM%d"),
							 hhDriver->dwPermanentLineId - DIRECT_COM1 + 1);
					ComSetPortName(hCom, ach);
					}

				//
				// Get the current defaults for the serial port.
				//
				if (ComDriverSpecial(hCom, "GET Defaults", NULL, 0) != COM_OK)
					{
					if (ComDeviceDialog(hCom, sessQueryHwnd(hhDriver->hSession))
							!= COM_OK)
						{
						goto ERROR_EXIT;
						}
					}

				if (ComDeviceDialog(hCom, sessQueryHwnd(hhDriver->hSession))
						!= COM_OK)
					{
					// User canceled
					//  --jcm 3-2-95
					//return CNCT_BAD_HANDLE;
					}
				}
			}
		#if defined(INCL_WINSOCK) // mrw:3/5/96
        else if (hhDriver->dwPermanentLineId == DIRECT_COMWINSOCK)
            {
            if (hhDriver->achDestAddr[0] == TEXT('\0'))
                {
				LoadString(glblQueryDllHinst(), IDS_ER_TCPIP_MISSING_ADDR,
					ach, sizeof(ach) / sizeof(TCHAR));

        		TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
		        	MB_OK | MB_ICONINFORMATION,
                        sessQueryTimeout(hhDriver->hSession));

                goto NEWPHONEDLG;
                }
            }
		#endif
        else
            {
            // mrw: Check that we have valid data.
            //
            if (hhDriver->achDest[0] == TEXT('\0'))
                {
		        LoadString(glblQueryDllHinst(), IDS_ER_CNCT_BADADDRESS, ach,
    			    sizeof(ach) / sizeof(TCHAR));

        		TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
		        	MB_OK | MB_ICONINFORMATION,
                        sessQueryTimeout(hhDriver->hSession));

                // goto ERROR_EXIT; // mrw:3/5/96
                goto NEWPHONEDLG;   // mrw:3/5/96
                }
            }
		}

	/* --- Enumerate lines, picks default (set in hhDriver->dwLine) --- */

	if ( IsNT() )
		{
		if (EnumerateLinesNT(hhDriver, 0) != 0)
			{
			assert(FALSE);
			goto MSG_EXIT;
			}
		}
	else
		{
		if (EnumerateLines(hhDriver, 0) != 0)
			{
			assert(FALSE);
			goto MSG_EXIT;
			}
		}


	/* --- If we don't match any TAPI lines, go back to new phone --- */

	if (hhDriver->dwLine == (DWORD)-1)
		{
		//
		// If this is not a modem (it is a COM port), then display the modem
		// wizard, otherwise just go back to the new phone. REV: 11/1/2001
		//
		if (!IN_RANGE(hhDriver->dwPermanentLineId, DIRECT_COM1, DIRECT_COM4) &&
		    hhDriver->dwPermanentLineId != DIRECT_COM_DEVICE )
			{
			DoNewModemWizard(sessQueryHwnd(hhDriver->hSession),
							 sessQueryTimeout(hhDriver->hSession));
			}

		goto NEWPHONEDLG;
		}

	/* --- Redraw window now so dialogs don't overlap ---- */

	UpdateWindow(sessQueryHwnd(hhDriver->hSession));

 	/* --- Check if we're doing a direct connect or using passthrough mode --- */

	if (IsNT() && hhDriver->dwPermanentLineId == DIRECT_COM_DEVICE)
		{
        int iActivatePortReturn = IDS_ER_CNCT_PORTFAILED;
		if (TRAP(ComSetPortName(hCom, hhDriver->achComDeviceName)) != COM_OK ||
			(iActivatePortReturn = TRAP(ComActivatePort(hCom, 0))) != COM_OK)
			{
            if (iActivatePortReturn == COM_PORT_IN_USE)
                {
				LoadString(glblQueryDllHinst(), IDS_ER_CNCT_CALLUNAVAIL,
					ach, sizeof(ach) / sizeof(TCHAR));
                }
            else
                {
				LoadString(glblQueryDllHinst(), IDS_ER_CNCT_PORTFAILED,
					achNewCnct, sizeof(achNewCnct) / sizeof(TCHAR));

				wsprintf(ach, achNewCnct, hhDriver->achComDeviceName);
                }

			TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
				MB_OK | MB_ICONINFORMATION,
					sessQueryTimeout(hhDriver->hSession));

			return -1;
			}
		else
			{
            if (uFlags & CNCT_ANSWER)
                {
                SetStatus(hhDriver, CNCT_STATUS_ANSWERING);
                }
            else
                {
			    SetStatus(hhDriver, CNCT_STATUS_CONNECTING);
                }
			}

		//
		// Allow PASSTHROUGH on serial ports as well so that the session
		// will not be disconnected with loss of carrier. REV: 11/6/2001
		//
		if (uFlags & CNCT_PORTONLY)
			{
			hhDriver->stCallPar.dwBearerMode = LINEBEARERMODE_PASSTHROUGH;
			}

		cnctdrvComEvent(hhDriver, CONNECT);

		return COM_OK;
		}
	else if (IN_RANGE(hhDriver->dwPermanentLineId, DIRECT_COM1, DIRECT_COM4))
		{
        int iActivatePortReturn = IDS_ER_CNCT_PORTFAILED;
		wsprintf(achCom, TEXT("COM%d"), hhDriver->dwPermanentLineId -
			DIRECT_COM1 + 1);

        if (TRAP(ComSetPortName(hCom, achCom)) != COM_OK ||
			(iActivatePortReturn = TRAP(ComActivatePort(hCom, 0))) != COM_OK)
			{
            if (iActivatePortReturn == COM_PORT_IN_USE)
                {
				LoadString(glblQueryDllHinst(), IDS_ER_CNCT_CALLUNAVAIL,
					ach, sizeof(ach) / sizeof(TCHAR));
                }
            else
                {
				LoadString(glblQueryDllHinst(), IDS_ER_CNCT_PORTFAILED,
					achNewCnct, sizeof(achNewCnct) / sizeof(TCHAR));

			    wsprintf(ach, achNewCnct, achCom);
                }

			TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
				MB_OK | MB_ICONINFORMATION,
					sessQueryTimeout(hhDriver->hSession));

			return -1;
			}

		else
			{
            if (uFlags & CNCT_ANSWER)
                {
                SetStatus(hhDriver, CNCT_STATUS_ANSWERING);
                }
            else
                {
			    SetStatus(hhDriver, CNCT_STATUS_CONNECTING);
                }
			}

		//
		// Allow PASSTHROUGH on serial ports as well so that the session
		// will not be disconnected with loss of carrier. REV: 11/6/2001
		//
		if (uFlags & CNCT_PORTONLY)
			{
			hhDriver->stCallPar.dwBearerMode = LINEBEARERMODE_PASSTHROUGH;
			}

		cnctdrvComEvent(hhDriver, CONNECT);

		return COM_OK;
		}
	#if defined(INCL_WINSOCK)
	else if (hhDriver->dwPermanentLineId == DIRECT_COMWINSOCK)
		{
		int iPort;

		/* --- Load the Winsock Com  drivers --- */
		ComLoadWinsockDriver(hCom);

        // Baud rate, etc. are meaningless for TCP/IP connections
        //
        ComSetAutoDetect(hCom, FALSE);
		iPort = sessQueryTelnetPort(hhDriver->hSession);
		if (iPort != 0)
			hhDriver->iPort = iPort;
        PostMessage(sessQueryHwndStatusbar(hhDriver->hSession),
            SBR_NTFY_REFRESH, (WPARAM)SBR_COM_PART_NO, 0);

#if 0   //DEADWOOD:jmh 3/24/97 Yes, we really want auto-detection, even in telnet!
        // Auto-detection of emulator type is redundant, since we tell
        // the telnet host what type we want. Seems like ANSI is the
        // most likely choice.
        hEmu = sessQueryEmuHdl(hhDriver->hSession);
        if (emuQueryEmulatorId(hEmu) == EMU_AUTO)
            {
            emuLoad(hEmu, EMU_VT100);
			#if defined(INCL_USER_DEFINED_BACKSPACE_AND_TELNET_TERMINAL_ID)
            // Make sure the telnet terminal id is correct. - cab:11/18/96
            //
            emuLoadDefaultTelnetId(hEmu);
			#endif // defined(INCL_USER_DEFINED_BACKSPACE_AND_TELNET_TERMINAL_ID)
            PostMessage(sessQueryHwndStatusbar(hhDriver->hSession),
                SBR_NTFY_REFRESH, (WPARAM)SBR_EMU_PART_NO, 0);
            }
#endif  // 0

		#if defined(INCL_CALL_ANSWERING)
        if (uFlags & CNCT_ANSWER)
            {
            wsprintf(szInstruct, "SET ANSWER=1");
            }
        else
            {
            wsprintf(szInstruct, "SET ANSWER=0");
            }
        ComDriverSpecial(hCom, szInstruct, szResult, sizeof(szResult) / sizeof(TCHAR));
		#endif // defined(INCL_CALL_ANSWERING)
		/* --- Do ComDriverSpecial calls to send the IP address &  port number
			   to the comm driver */

		//
		// Make sure we don't overwrite the buffer. If the string
		// is too long, then truncate to the hhDriver->achDestAddr
		// size of MAX_AP_ADDR_LEN.  REV 09/20/2000 
		//
		StrCharCopyN(szInstruct, TEXT("SET IPADDR="), sizeof(szInstruct) / sizeof(TCHAR));
		iNumChars = StrCharGetStrLength(szInstruct);
		StrCharCopyN(&szInstruct[iNumChars], hhDriver->achDestAddr,
			         sizeof(szInstruct)/sizeof(TCHAR) - iNumChars);

		//
		// Make sure the string is null terminated.
		//
		szInstruct[sizeof(szInstruct)/sizeof(TCHAR) - 1]=TEXT('\0');
		ComDriverSpecial(hCom, szInstruct, szResult,
					  sizeof(szResult) / sizeof(TCHAR));

		wsprintf(szInstruct, "SET PORTNUM=%ld", hhDriver->iPort);
		ComDriverSpecial(hCom, szInstruct,
					  szResult, sizeof(szResult) / sizeof(TCHAR));

		#if defined(INCL_CALL_ANSWERING)
        if (uFlags & CNCT_ANSWER)
            {
            SetStatus(hhDriver, CNCT_STATUS_ANSWERING);
            }
        else
            {
		    SetStatus(hhDriver, CNCT_STATUS_CONNECTING);
            }
		#else // defined(INCL_CALL_ANSWERING)
		SetStatus(hhDriver, CNCT_STATUS_CONNECTING);
		#endif // defined(INCL_CALL_ANSWERING)

		/* --- Activate the port  ---*/
		if (ComActivatePort(hCom, 0) != COM_OK)
			{
			LoadString(glblQueryDllHinst(), IDS_ER_TCPIP_FAILURE,
			achNewCnct, sizeof(achNewCnct) / sizeof(TCHAR));

			TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
				MB_OK | MB_ICONINFORMATION,
				sessQueryTimeout(hhDriver->hSession));

			return -1;
			}

		else
			{
			return COM_OK;
			}
		}
	#endif // defined(INCL_WINSOCK)

	/* --- Display confimation dialog if requested --- */

	if ((uFlags & (CNCT_PORTONLY | CNCT_DIALNOW | CNCT_ANSWER)) == 0)
		{
		if (DialogBoxParam(glblQueryDllHinst(),
			MAKEINTRESOURCE(IDD_CNCT_CONFIRM),
				sessQueryHwnd(hhDriver->hSession), ConfirmDlg,
					(LPARAM)hhDriver) == FALSE)
			{
			goto ERROR_EXIT;
			}
		}

    // Either make the call or wait for a call.
    //
    if (uFlags & CNCT_ANSWER)
        {
        if (DoAnswerCall(hhDriver) != 0)
            {
            goto ERROR_EXIT;
            }
        }
    else
        {
        if (DoMakeCall(hhDriver, uFlags) != 0)
            {
            goto ERROR_EXIT;
            }
        }

    ComSetAutoDetect(hCom, FALSE);
    PostMessage(sessQueryHwndStatusbar(hhDriver->hSession),
        SBR_NTFY_REFRESH, (WPARAM)SBR_COM_PART_NO, 0);

    return 0;

	/* --- Message exit --- */

MSG_EXIT:
	LoadString(glblQueryDllHinst(), uidErr, ach, sizeof(ach) / sizeof(TCHAR));

	TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach,
		            NULL, MB_OK | MB_ICONINFORMATION | MB_TASKMODAL,
					sessQueryTimeout(hhDriver->hSession));

	/* --- Error exit --- */

ERROR_EXIT:
	if (hhDriver->hLineApp && hhDriver->hLine)
		{
		lineClose(hhDriver->hLine);
		memset(&hhDriver->stCallPar, 0, sizeof(hhDriver->stCallPar));
		hhDriver->stCallPar.dwTotalSize = sizeof(hhDriver->stCallPar);
		hhDriver->stCallPar.dwMediaMode = LINEMEDIAMODE_DATAMODEM;
		hhDriver->stCallPar.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
		hhDriver->stCallPar.dwBearerMode = 0;
		hhDriver->hLine = 0;
		}

	SetStatus(hhDriver, CNCT_STATUS_FALSE);
    return CNCT_ERROR;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *	DoDelayedCall
 *
 * DESCRIPTION:
 *	Check the section under Delayed Dialing in the programmers guide to
 *	TAPI.  Basicly, if the service provider does not provide dialtone
 *	support, then we have to break of the dialable string format into
 *	pieces and prompt the user.
 *
 * ARGUMENTS:
 *	hhDriver	- private driver handle.
 *
 * RETURNS:
 *	0=OK, else error.
 *
 * AUTHOR: Mike Ward, 20-Apr-1995
 */
static int DoDelayedCall(const HHDRIVER hhDriver)
	{
	TCHAR ach[256];
	TCHAR ach2[256];
	TCHAR *pach;
	long  lDialRet;

	#define DIAL_DELIMITERS "Ww@$?"

	hhDriver->lMakeCallId = -1;
	lstrcpy(ach, hhDriver->achDialableDest);

	if ((pach = strtok(ach, DIAL_DELIMITERS)) == 0)
		return -1;

	while (pach)
		{
		lstrcpy(ach2, pach);

		// If this is the last segment of the string, don't append the
		// semicolon.
		//
		if ((pach = strtok(NULL, DIAL_DELIMITERS)) != 0)
			lstrcat(ach2, ";");

		if (hhDriver->lMakeCallId < 0)
			{
			// By appending a semicolon to the dialable string, we're
			// telling lineMakeCall that more is on the way.
			//
			if ((hhDriver->lMakeCallId = lineMakeCall(hhDriver->hLine,
				&hhDriver->hCall, ach2, hhDriver->dwCountryCode,
					&hhDriver->stCallPar)) < 0)
				{
				#if defined(_DEBUG)
				char ach[50];
				wsprintf(ach, "DoDelayedCall returned %x", hhDriver->lMakeCallId);
				MessageBox(GetFocus(), ach, "debug", MB_OK);
				#endif

				return -3;
				}
			}

		else
			{
			// Once we have a call handle we have to use lineDial to complete
			// the call.
			//
			if ((lDialRet = lineDial(hhDriver->hCall, ach2,
				hhDriver->dwCountryCode)) < 0)
				{
				#if defined(_DEBUG)
				char ach[50];
				wsprintf(ach, "lineDial returned %x", lDialRet);
				MessageBox(GetFocus(), ach, "debug", MB_OK);
				#endif

				return -4;
				}
			}

		// The user has to tell us when the we can continue dialing
		//
		if (pach != 0)
			{
			LoadString(glblQueryDllHinst(), IDS_CNCT_DELAYEDDIAL, ach2,
			sizeof(ach2) / sizeof(TCHAR));

			if (TimedMessageBox(hhDriver->hwndCnctDlg, ach2, NULL,
				                MB_OKCANCEL | MB_ICONINFORMATION | MB_TASKMODAL,
								sessQueryTimeout(hhDriver->hSession)) != IDOK)
				{
				return -4;
				}
			}
		}

	return 0;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvDisconnect
 *
 * DESCRIPTION:
 *  Signals a disconnect
 *
 * ARGUMENTS:
 *  hhDriver - private driver handle
 *  uFlags   - disconnect flags
 *
 * RETURNS:
 *  0 or error
 *
 */
int WINAPI cnctdrvDisconnect(const HHDRIVER hhDriver, const unsigned int uFlags)
	{
	LONG      lLineDropId;
    #if defined(INCL_REDIAL_ON_BUSY)
    HKEY      hKey;
    DWORD     dwSize;
    BYTE      ab[20];
    #endif
    XD_TYPE*  pX;
    int       nReturnVal = 0;
    TCHAR     ach[256];

	if (hhDriver == 0)
		{
		assert(FALSE);
		return CNCT_BAD_HANDLE;
		}

    //
    // Cancel any active file transfers that are currently executing.
    // REV: 02/01/2001
    //
	pX = (XD_TYPE*)sessQueryXferHdl(hhDriver->hSession);

    if (pX != NULL && pX->hwndXfrDisplay != NULL &&
        IsWindow(pX->hwndXfrDisplay) && pX->nDirection != XFER_NONE)
        {
        int nCancelTransfer = IDYES;


        if (uFlags & CNCT_XFERABORTCONFIRM)
            {
            //
            // Prompt to cancel the file transfer. REV: 02/16/2001
            //
            LoadString(glblQueryDllHinst(), IDS_ER_CNCT_ACTIVETRANSFER, ach, sizeof(ach) / sizeof(TCHAR));

            nCancelTransfer = TimedMessageBox(pX->hwndXfrDisplay, ach, NULL,
                                              MB_YESNO | MB_ICONEXCLAMATION | MB_TASKMODAL,
			                                  sessQueryTimeout(hhDriver->hSession));

            }

        if (nCancelTransfer == IDYES || nCancelTransfer == -1)
            {
            unsigned int uNewFlags = uFlags;

            if (uFlags & CNCT_LOSTCARRIER)
                {
                //
                // NOTE:  We should only have to tell the XFER to abort here.
                //        It should not be dependent on a message to a dialog.
                //
                PostMessage(pX->hwndXfrDisplay, WM_COMMAND, XFER_LOST_CARRIER, 0L);
                }
            else if (uFlags & CNCT_XFERABORTCONFIRM)
                {
                //
                // NOTE:  We should only have to tell the XFER to abort here.
                //        It should not be dependent on a message to a dialog.
                //
                PostMessage(pX->hwndXfrDisplay, WM_COMMAND, XFR_SHUTDOWN, 0L);
                }

            //
            // We can't exit until the file transfer exits, so post a
            // message to try to disconnect again.  Make sure to turn
            // of the CNCT_XFERABORTCONFIRM flag as we don't want to
            // prompt the kill the transfer again.
            //
            uNewFlags &= ~CNCT_XFERABORTCONFIRM;

            //
            // We must post a message to disconnect because we are
            // waiting for the file transfer to cancel.  We have to
            // post a message otherwise we will get into a deadlock
            // situation.  This is not the best way to accomplish
            // this as we may be posting a lot of messages to the
            // session window and there is a potential for the
            // file transfer to not respond quickly causing the
            // disconnect to loop.  Eventually, the file transfer
            // will cancel, or will timeout and cancel, so we will
            // not get into an endless loop. REV: 06/22/2001
            //

			//
			// Wait half a second before posting this message so we don't
			// flood ourselves with disconnect messages. REV: 4/25/2002
			//
			Sleep(500);
            PostDisconnect(hhDriver, uNewFlags);
            }

        //
        // Return an status that the current file transfer must be
        // canceled (or is in the process of being canceled).  We
        // cannot disconnect until the transfer is complete.
        //
        return XFR_SHUTDOWN;
        }

#ifdef INCL_CALL_ANSWERING
    // Unregister our cloop callback.
    //
    if (hhDriver->pvUnregister)
        {
        CLoopUnregisterRmtInputChain(hhDriver->pvUnregister);
        hhDriver->pvUnregister = 0;
        }
#endif

	ComDeactivatePort(sessQueryComHdl(hhDriver->hSession));

	if (hhDriver->hCall)
		{
		SetStatus(hhDriver, CNCT_STATUS_DISCONNECTING);

		if ((lLineDropId = lineDrop(hhDriver->hCall, 0, 0)) < 0)
			assert(FALSE);

		hhDriver->hCall = 0;

		// If the drop is completing asychronously, save the flags and
		// wait for the call status to go idle.
		//
		if (lLineDropId > 0)
			{
			hhDriver->uDiscnctFlags = uFlags;
			return 0;
			}
		}

	SetStatus(hhDriver, CNCT_STATUS_FALSE);

	if ((uFlags & DISCNCT_NOBEEP) == 0)
		sessBeeper(hhDriver->hSession);

	//mpt:10-28-97 added exit upon disconnect feature
	if ((uFlags & DISCNCT_EXIT))
		PostMessage(sessQueryHwnd(hhDriver->hSession), WM_CLOSE, 0, 0);

	if (hhDriver->hLine)
		{
		lineClose(hhDriver->hLine);
		memset(&hhDriver->stCallPar, 0, sizeof(hhDriver->stCallPar));
		hhDriver->stCallPar.dwTotalSize = sizeof(hhDriver->stCallPar);
		hhDriver->stCallPar.dwMediaMode = LINEMEDIAMODE_DATAMODEM;
		hhDriver->stCallPar.dwCallParamFlags = LINECALLPARAMFLAGS_IDLE;
		hhDriver->stCallPar.dwBearerMode = 0;
		hhDriver->hLine = 0;
		}

	if (uFlags & CNCT_DIALNOW)
		{
        #if defined(INCL_REDIAL_ON_BUSY)
        if (hhDriver->fRedialOnBusy && hhDriver->iRedialCnt > 0)
            {
			hhDriver->uDiscnctFlags = uFlags;
            hhDriver->iRedialSecsRemaining = 2;

            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Microsoft\\HyperTerminal\\TimeToRedial", 0, KEY_READ,
                    &hKey) == ERROR_SUCCESS)
                {
                dwSize = sizeof(ab);

                if (RegQueryValueEx(hKey, "", 0, 0, ab, &dwSize) == ERROR_SUCCESS)
                    hhDriver->iRedialSecsRemaining = atoi(ab);

                RegCloseKey(hKey);
                }

            SetTimer(hhDriver->hwndCnctDlg, 1, 1000, 0);
            }

        else
            {
		    PostMessage(sessQueryHwnd(hhDriver->hSession), WM_CNCT_DIALNOW,
			    uFlags, 0);
            }

        #else
		PostMessage(sessQueryHwnd(hhDriver->hSession), WM_CNCT_DIALNOW,
			uFlags, 0);
        #endif
		}

    else
        {
        // If we're not auto redialing, reset the dial count. - mrw:10/10/95
        //
        hhDriver->iRedialCnt = 0;
        }

	return nReturnVal;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  lineCallbackFunc
 *
 * DESCRIPTION:
 *  Function TAPI calls to handle asynchronous events
 *
 * ARGUMENTS:
 *  see TAPI.H
 *
 * RETURNS:
 *  void
 *
 */
void CALLBACK lineCallbackFunc(DWORD hDevice, DWORD dwMsg, DWORD_PTR dwCallback,
							   DWORD_PTR dwParm1, DWORD_PTR dwParm2, DWORD_PTR dwParm3)
	{
	const HHDRIVER hhDriver = (HHDRIVER)dwCallback;
	int id;
    unsigned int uFlags;

	#if 0
	{
	char ach[256];
	wsprintf(ach,"%x %x", dwMsg, dwParm1);
	MessageBox(NULL, ach, "debug", MB_OK);
	}
	#endif

	switch (dwMsg)
		{
	case LINE_REPLY:
		if ((LONG)dwParm1 == hhDriver->lMakeCallId)
			{
			hhDriver->lMakeCallId = 0;

			if ((LONG)dwParm2 != 0) // zero indicates success
				{
				switch (dwParm2)
					{
				case LINEERR_CALLUNAVAIL:
					id = IDS_DIAL_NODIALTONE;
					break;

				default:
					id = IDS_DIAL_DISCONNECTED;
					break;
					}

				cnctdrvDisconnect(hhDriver, 0);
				DialingMessage(hhDriver, id);
				}
			}
		break;

	case LINE_LINEDEVSTATE:
		DbgOutStr("LINEDEVSTATE_DISCONNECTED 0x%x\r\n", dwParm1, 0, 0, 0, 0);

		switch (dwParm1)
			{
        case PHONESTATE_CAPSCHANGE:
            //
            // If we are currently disconnected, then reset.
            //
            if (hhDriver != NULL && hhDriver->iStatus != CNCT_STATUS_FALSE)
                break;

		case LINEDEVSTATE_REINIT:
        case PHONESTATE_REINIT:
			if (hhDriver == 0)
				{
				// Until we open a line, we don't have a driver handle
				// since we can't pass one during lineInitialize().
				// This turns out to be a good time to reinit if we get
				// notified to do so however, so it has use.
				//
				if (tapiReinit(gbl_hhDriver) != 0)
					tapiReinitMessage(gbl_hhDriver);
				}

			else
				{
				tapiReinitMessage(hhDriver);
				}
			break;

		case LINEDEVSTATE_INSERVICE:
			// If we are showing our PCMCIA dialog prompting the user
			// to insert the card, we post a message to dismiss the
			// dialog once they insert it. - mrw,2/28/95
			//
			if (IsWindow(hhDriver->hwndPCMCIA))
				{
				PostMessage(hhDriver->hwndPCMCIA, WM_COMMAND,
					MAKEWPARAM(IDOK, 0), (LPARAM)hhDriver->hwndPCMCIA);
				}
			break;

		case LINEDEVSTATE_OUTOFSERVICE:
			// Means they yanked the PCMCIA card - mrw,2/28/95
			//
			cnctdrvDisconnect(hhDriver, 0);
			break;

        case LINEDEVSTATE_RINGING:
            // When the current ring count (as told by dwParam3) equals
            // or exceeds the rings to answer on then we'll do the answer
            // using the hhdriver->hCall handle we cached during the
            // LINECALLSTATE_BURNTOFFERING notification. - rjk. 07-31-96
            //
            if ((hhDriver->lMakeCallId = lineAnswer(hhDriver->hCall,0,0)) >= 0)
                {
                SetStatus(hhDriver, CNCT_STATUS_CONNECTING);
                }
            break;

        case LINEDEVSTATE_CLOSE:
        case PHONESTATE_DISCONNECTED:
            //
            // Another application has disconnected this device. REV: 04/27/2001
            //
            uFlags = CNCT_DIALNOW | CNCT_NOCONFIRM;
            id = IDS_DIAL_DISCONNECTED;
			PostDisconnect(hhDriver, uFlags);
			DialingMessage(hhDriver, id);
            break;

		default:
			break;
			}
		break; // case LINE_LINEDEVSTATE

	case LINE_CREATE:	// Sent when new modem is added
		assert(0);		// So I know it happened

		// A remote possibilility exists that if two modems were created
		// back to back, that the LINE_CREATE's would come out of order.
		// T. Nixon suggests that we bump the line count by the dwParm1
		// parameter plus one only when it is greater than or equal to
		// the current line count. - mrw
		//
		if (dwParm1 >= gbl_hhDriver->dwLineCnt)
			gbl_hhDriver->dwLineCnt = (DWORD)(dwParm1 + 1);

		break;

	case LINE_CALLSTATE:
		DbgOutStr("LINECALLSTATE 0x%x\r\n", dwParm1, 0, 0, 0, 0);
		switch ((LONG)dwParm1)
			{
		case LINECALLSTATE_OFFERING:
			DialingMessage(hhDriver, IDS_DIAL_OFFERING);
            // Windows sends us this message only one time while receiving
            // a call and that is on the very first ring.  See the code
            // that responds to the LINEDEVSTATE_RINGING to see how the call
            // gets answered. - rjk. 07-31-96
            //
            hhDriver->hCall = (HCALL)hDevice;
			break;

		case LINECALLSTATE_DIALTONE:
			DialingMessage(hhDriver, IDS_DIAL_DIALTONE);
			break;

		case LINECALLSTATE_DIALING:
			DialingMessage(hhDriver, IDS_DIAL_DIALING);
			break;

		case LINECALLSTATE_RINGBACK:
			DialingMessage(hhDriver, IDS_DIAL_RINGBACK);
			break;

		case LINECALLSTATE_BUSY:
			DialingMessage(hhDriver, IDS_DIAL_BUSY);
			EnableDialNow(hhDriver->hwndCnctDlg, TRUE);
            uFlags = DISCNCT_NOBEEP;

            #if defined(INCL_REDIAL_ON_BUSY)
            if (hhDriver->fRedialOnBusy && hhDriver->iRedialCnt++ < REDIAL_MAX)
                uFlags = CNCT_DIALNOW | CNCT_NOCONFIRM | DISCNCT_NOBEEP;
            #endif

			PostDisconnect(hhDriver, uFlags);
			break;

		case LINECALLSTATE_CONNECTED:
			DialingMessage(hhDriver, IDS_DIAL_CONNECTED);

			if (Handoff(hhDriver) != 0)
				{
				PostDisconnect(hhDriver, 0);
				}
			else
				{
                if (IsWindow(hhDriver->hwndCnctDlg))
                    {
                    // Closes the dialing dialog
				    PostMessage(hhDriver->hwndCnctDlg, WM_USER+0x100, 0, 0);
                    }
				SetStatus(hhDriver, CNCT_STATUS_TRUE);
				}

			break;

		case LINECALLSTATE_DISCONNECTED:
			DbgOutStr("LINECALLSTATE_DISCONNECTED 0x%x\r\n", dwParm2, 0, 0, 0, 0);
			uFlags = 0;

			if (dwParm2 & LINEDISCONNECTMODE_BUSY)
                {
				id = IDS_DIAL_BUSY;

                #if defined(INCL_REDIAL_ON_BUSY)
                if (hhDriver->fRedialOnBusy &&
                    hhDriver->iRedialCnt++ < REDIAL_MAX)
                    {
                    // Wait to let slower phone systems catchup - mrw 2/29/96
                    //
                    uFlags |= CNCT_DIALNOW|CNCT_NOCONFIRM|DISCNCT_NOBEEP;
                    }
                #endif
                }

			else if (dwParm2 & LINEDISCONNECTMODE_NOANSWER)
				id = IDS_DIAL_NOANSWER;

			else if (dwParm2 & LINEDISCONNECTMODE_NODIALTONE)
				id = IDS_DIAL_NODIALTONE;

			else
				{
				id = IDS_DIAL_DISCONNECTED;
				//mpt:10-28-97 added exit upon disconnect feature
				uFlags |= ( sessQueryExit(hhDriver->hSession) ? DISCNCT_EXIT : 0 );
				}

			PostDisconnect(hhDriver, uFlags);
			DialingMessage(hhDriver, id);
			break;

		case LINECALLSTATE_IDLE:
			cnctdrvDisconnect(hhDriver, hhDriver->uDiscnctFlags);
			break;

		default:
			break;
			}

	default:
		break;
		}

	return;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *	Handoff
 *
 * DESCRIPTION:
 *	Hands TAPI's com handle to the Wacker's com routines.
 *
 * ARGUMENTS:
 *	hhDriver	- private driver handle
 *
 * RETURNS:
 *	0=OK
 *
 */
int Handoff(const HHDRIVER hhDriver)
	{
	LPVARSTRING pVarstr;
	HANDLE hdl;
	DWORD dwSize;
	int i;

	pVarstr = malloc(sizeof(VARSTRING));

	if (pVarstr == 0)
		{
		assert(FALSE);
		return 1;
		}

    memset( pVarstr, 0, sizeof(VARSTRING) );
	pVarstr->dwTotalSize = sizeof(VARSTRING);

	if (lineGetID(hhDriver->hLine, hhDriver->dwLine, hhDriver->hCall,
                        LINECALLSELECT_CALL, pVarstr, DEVCLASS) != 0)
		{
		assert(FALSE);
        free(pVarstr);
        pVarstr = NULL;
		return 2;
		}

	if (pVarstr->dwNeededSize > pVarstr->dwTotalSize)
		{
		dwSize = pVarstr->dwNeededSize;
		free(pVarstr);
        pVarstr = NULL;
        pVarstr = malloc(dwSize);

		if (pVarstr == 0)
			{
			assert(FALSE);
			return 3;
			}

        memset( pVarstr, 0, dwSize );
		pVarstr->dwTotalSize = dwSize;

		if (TRAP(lineGetID(hhDriver->hLine, hhDriver->dwLine, hhDriver->hCall,
                                LINECALLSELECT_CALL, pVarstr, DEVCLASS)) != 0)
			{
			assert(FALSE);
			free(pVarstr);
            pVarstr = NULL;
            return 4;
			}
		}

	if (pVarstr->dwStringSize == 0)
		{
		assert(FALSE);
		free(pVarstr);
        pVarstr = NULL;
        return 5;
		}

	hdl = *(HANDLE *)((BYTE *)pVarstr + pVarstr->dwStringOffset);

	// Set comm buffers to 32K
	//
	if (SetupComm(hdl, 32768, 32768) == FALSE)
		{
		DWORD dwLastError = GetLastError();
		assert(0);
		}

	if ((i = ComActivatePort(sessQueryComHdl(hhDriver->hSession),
			(DWORD_PTR)hdl)) != COM_OK)
		{
        #if !defined(NDEBUG)
		char ach[256];
		wsprintf(ach, "hdl=%x, i=%d", hdl, i);
		MessageBox(NULL, ach, "debug", MB_OK);
        #endif

		assert(FALSE);
		free(pVarstr);
        pVarstr = NULL;
        return 6;
		}

    if(pVarstr)
        {
        free(pVarstr);
        pVarstr = NULL;
        }

	return 0;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *	PostDisconnect
 *
 * DESCRIPTION:
 *	Work around to TAPI bug that does not allow us to call lineShutDown()
 *	from with the TAPI callback
 *
 * ARGUMENTS:
 *	hhDriver	- private driver handle
 *
 * RETURNS:
 *	void
 *
 */
void PostDisconnect(const HHDRIVER hhDriver, const unsigned int uFlags)
	{
	PostMessage(sessQueryHwnd(hhDriver->hSession), WM_DISCONNECT,
		uFlags, 0);

	return;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *	tapiReinitMessage
 *
 * DESCRIPTION:
 *	Displays a messagebox showing TAPI needs to be reinitialized.
 *
 * ARGUMENTS:
 *	hhDriver    - private driver handle
 *
 * RETURNS:
 *  0=OK, <0=error
 *
 */
static int tapiReinitMessage(const HHDRIVER hhDriver)
    {
	TCHAR ach[512], achTitle[256];

	if (hhDriver == 0)
        {
        assert(FALSE);
        return -1;
		}

	LoadString(glblQueryDllHinst(), IDS_ER_TAPI_REINIT, ach, sizeof(ach) / sizeof(TCHAR));

	LoadString(glblQueryDllHinst(), IDS_ER_TAPI_REINIT2, achTitle,
		sizeof(achTitle) / sizeof(TCHAR));

	lstrcat(ach, achTitle);

	TimedMessageBox(sessQueryHwnd(hhDriver->hSession), ach, NULL,
		            MB_OK | MB_ICONINFORMATION | MB_TASKMODAL,
					sessQueryTimeout(hhDriver->hSession));

	return 0;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *	tapiReinit
 *
 * DESCRIPTION:
 *	Attempts to reinit tapi.
 *
 * ARGUMENTS:
 *	hhDriver	- private driver handle
 *
 * RETURNS:
 *	0=OK,else error
 *
 */
static int tapiReinit(const HHDRIVER hhDriver)
	{
	int i;
	LPVARSTRING pvs = 0;
    DWORD dwSize;
	const SF_HANDLE sfhdl = sessQuerySysFileHdl(hhDriver->hSession);

    if (hhDriver == 0)
        {
        assert(FALSE);
        return -1;
        }

    if (hhDriver->hLineApp)
        {
        /* --- Get current config so we can restore it --- */

        if (hhDriver->dwLine != (DWORD)-1)
            {
            if ((pvs = malloc(sizeof(VARSTRING))) == 0)
                {
                assert(FALSE);
                goto SHUTDOWN;
                }

			memset( pvs, 0, sizeof(VARSTRING) );
            pvs->dwTotalSize = sizeof(VARSTRING);

            if (lineGetDevConfig(hhDriver->dwLine, pvs, DEVCLASS) != 0)
                {
                assert(FALSE);
                free(pvs);
                pvs = NULL;
                hhDriver->dwLine = (DWORD)-1;
                goto SHUTDOWN;
                }

            if (pvs->dwNeededSize > pvs->dwTotalSize)
                {
                dwSize = pvs->dwNeededSize;
                free(pvs);
                pvs = NULL;

                if ((pvs = malloc(dwSize)) == 0)
                    {
                    assert(FALSE);
                    hhDriver->dwLine = (DWORD)-1;
                    goto SHUTDOWN;
                    }

				memset( pvs, 0, dwSize );
                pvs->dwTotalSize = dwSize;

                if (lineGetDevConfig(hhDriver->dwLine, pvs, DEVCLASS) != 0)
                    {
                    assert(FALSE);
                    free(pvs);
                    pvs = NULL;
                    hhDriver->dwLine = (DWORD)-1;
                    goto SHUTDOWN;
                    }
                }
            }

        SHUTDOWN:

		{
		LONG lLineShutdown = lineShutdown(hhDriver->hLineApp);

		if (lLineShutdown == LINEERR_NOMEM)
			{
			//
			// We are in a low memory state, so wait for a while,
			// then try to shutdown the line again. REV: 5/1/2002
			//
			Sleep(500);
			lLineShutdown = lineShutdown(hhDriver->hLineApp);
			}

		if (lLineShutdown != 0)
            {
            assert(FALSE);
            return -6;
            }
		}

        hhDriver->hLineApp = 0;

        // Wait for 10 seconds, if nothing happens, return an error
        //
        for (i=0 ;; ++i)
            {
            if (lineInitialize(&hhDriver->hLineApp, glblQueryDllHinst(),
                            lineCallbackFunc, g_achApp, &hhDriver->dwLineCnt) != 0)
                {
                if (i > 10)
                    {
                    assert(0);
                    return -7;
                    }

                Sleep(1000);    // sleep 1 second
                continue;
                }

            break;
            }
        }

    /* --- Ok, we've reintialized, put settings back now --- */

    if (pvs)
        {
    	LPVOID pv = (BYTE *)pvs + pvs->dwStringOffset;

        if (lineSetDevConfig(hhDriver->dwLine, pv, pvs->dwStringSize,
                             DEVCLASS) != 0)
            {
            assert(FALSE);
            free(pvs);
            pvs = NULL;
            return -8;
            }

        free(pvs);
        pvs = NULL;
        }

    return 0;
    }

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *  cnctdrvSetDestination
 *
 * DESCRIPTION:
 *	Sets the destination (in this case phone number).
 *
 * ARGUMENTS:
 *	hhDriver    - private driver handle
 *	ach			- string to set
 *	cb			- number of chars in ach
 *
 * RETURNS:
 *  0=OK, <0=error
 *
 */
int WINAPI cnctdrvSetDestination(const HHDRIVER hhDriver, TCHAR * const ach,
								 const size_t cb)
	{
	int len;

	if (hhDriver == 0 || ach == 0 || cb == 0)
		{
		assert(FALSE);
		return -1;
		}

	len = (int) min(cb, sizeof(hhDriver->achDest));
	strncpy(hhDriver->achDest, ach, len);
	hhDriver->achDest[len-1];

	return 0;
	}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 * FUNCTION:
 *	DoNewModemWizard
 *
 * DESCRIPTION:
 *	Calls up the new modem wizard
 *
 * ARGUMENTS:
 *	hhDriver    - private driver handle
 *  iTimeout    - The timeout length
 *
 * RETURNS:
 *	0=OK,else error
 *
 */
static int DoNewModemWizard(HWND hWnd, int iTimeout)
	{
	PROCESS_INFORMATION stPI;
	STARTUPINFO 		stSI;
    TCHAR               ach[256];
    int                 returnVal = 0;

	// Initialize the PROCESS_INFORMATION structure for CreateProcess
	//
	memset( &stPI, 0, sizeof( PROCESS_INFORMATION ) );

	// Initialize the STARTUPINFO structure for CreateProcess
	//
	memset(&stSI, 0, sizeof(stSI));
	stSI.cb = sizeof(stSI);
	stSI.dwFlags = STARTF_USESHOWWINDOW;
	stSI.wShowWindow = SW_SHOW;

    // See if we should run the New modem wizard.
    //
    if(mscAskWizardQuestionAgain())
        {
		LoadString(glblQueryDllHinst(), IDS_ER_CNCT_BADLINE, ach, sizeof(ach) / sizeof(TCHAR));

		if (TimedMessageBox(hWnd, ach, NULL, MB_YESNO | MB_ICONEXCLAMATION, iTimeout) == IDYES)
			{
            TCHAR  systemDir[MAX_PATH];
            TCHAR  executeString[MAX_PATH * 3];
            TCHAR *pParams = TEXT("\\control.exe\" modem.cpl,,Add");
            UINT   numChars = 0;

            TCHAR_Fill(systemDir, TEXT('\0'), MAX_PATH);
            TCHAR_Fill(executeString, TEXT('\0'), MAX_PATH * 3);
            numChars = GetSystemDirectory(systemDir, MAX_PATH);
            
            if (numChars == 0 || StrCharGetStrLength(systemDir) == 0)
                {
                returnVal = -3;
                }
            else
                {
                if (StrCharGetStrLength(systemDir) + StrCharGetStrLength(pParams) + sizeof(TEXT("\"")) / sizeof(TCHAR) >
                    sizeof(executeString) / sizeof(TCHAR))
                    {
                    returnVal = -2;
                    }
                else
                    {
                    StrCharCopyN(executeString, TEXT("\""), sizeof(executeString) / sizeof(TCHAR));
                    StrCharCat(executeString, systemDir);
                    StrCharCat(executeString, pParams);

                    //
	                // Launch the new modem wizard with the command below.
	                //

	                //if (CreateProcess(0, "rundll sysdm.cpl,InstallDevice_Rundll modem,,",
	                //		  0, 0, 0, 0, 0, 0, &stSI, &stPI) == FALSE)
	                //if (CreateProcess(0, "control.exe modem.cpl,,Add",
                    //    0, 0, 0, 0, 0, 0, &stSI, &stPI) == FALSE)
                    if (CreateProcess(NULL, executeString,
 			            NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS,
                        NULL, systemDir, &stSI, &stPI) == FALSE)
		                {
		                #if defined(_DEBUG)
			                {
			                char ach[100];
			                DWORD dw = GetLastError();

			                wsprintf(ach,"CreateProcess (%s, %d) : %x",__FILE__,__LINE__,dw);
			                MessageBox(NULL, ach, "Debug", MB_OK);
			                }
		                #endif

		                returnVal = -1;
		                }
		            else
			            {
			            mscUpdateRegistryValue();

						//
						// Close the handles.
						//
						CloseHandle(stPI.hProcess);
						CloseHandle(stPI.hThread);
			            }
                    }
			    }
            }
        }
	return returnVal;
	}

int cncttapiGetLineConfig( const DWORD dwLineId, VOID ** ppvs )
    {
    DWORD dwSize;
    LPVARSTRING pvs = (LPVARSTRING)*ppvs;

    if (pvs != NULL)
        {
        assert(FALSE);
        free(pvs);
        pvs = NULL;
        }

    if ((pvs = malloc(sizeof(VARSTRING))) == 0)
    	{
    	assert(FALSE);
    	return -3;
    	}

	memset(pvs, 0, sizeof(VARSTRING));
    pvs->dwTotalSize = sizeof(VARSTRING);
	pvs->dwNeededSize = 0;

    if (lineGetDevConfig(dwLineId, pvs, DEVCLASS) != 0)
    	{
    	assert(FALSE);
    	free(pvs);
  		pvs = NULL;
    	return -4;
    	}

    if (pvs->dwNeededSize > pvs->dwTotalSize)
    	{
    	dwSize = pvs->dwNeededSize;
    	free(pvs);
  		pvs = NULL;

    	if ((pvs = malloc(dwSize)) == 0)
    		{
    		assert(FALSE);
    		return -5;
    		}

		memset(pvs, 0, dwSize);
    	pvs->dwTotalSize = dwSize;

    	if (lineGetDevConfig(dwLineId, pvs, DEVCLASS) != 0)
    		{
    		assert(FALSE);
    		free(pvs);
  			pvs = NULL;
    		return -6;
    		}
    	}

    *ppvs = (VOID *)pvs;
    return 0;
    }

int cncttapiSetLineConfig(const DWORD dwLineId, const HCOM hCom)
    {
    int         retValue = 0;
    LPVARSTRING pvs = NULL;
    PUMDEVCFG   pDevCfg = NULL;
    int         iBaudRate;
    int         iDataBits;
    int         iParity;
    int         iStopBits;
    LONG        lLineReturn;

    retValue = cncttapiGetLineConfig( dwLineId, (VOID **) &pvs);

    if (retValue != 0)
        {
        retValue = retValue;
        }

    if (retValue == 0 && pvs == NULL)
        {
        retValue = -7;
        }

    // The structure of the DevConfig block is as follows
    //
    //	VARSTRING
    //	UMDEVCFGHDR
    //	COMMCONFIG
    //	MODEMSETTINGS
    //
    // The UMDEVCFG structure used below is defined in the
    // UNIMODEM.H provided in the platform SDK (in the nih
    // directory for HTPE). REV: 12/01/2000 
    //
    if (retValue == 0)
        {
        pDevCfg = (UMDEVCFG *)((BYTE *)pvs + pvs->dwStringOffset);
        if (pDevCfg == NULL)
            {
            retValue = -8;
            }
        }

    if (retValue == 0 && (hCom == NULL || ComValidHandle(hCom) == FALSE))
        {
        retValue = -9;
        }

    //
    // commconfig struct has a DCB structure we dereference for the
    // com settings.
    //

    //
    // The baud rate should be stored with the COM settings for
    // TAPI devices, but we may want to use the current TAPI device
    // baud rate instead.  We should find a better solution for this.
    // TODO:REV 05/01/2001
    //
    if (retValue == 0 && ComGetBaud(hCom, &iBaudRate) != COM_OK)
        {
		#if defined(TODO)
        retValue = -10;
		#endif // TODO
        }
	else if (retValue == 0)
		{
		ComSetBaud(hCom, pDevCfg->commconfig.dcb.BaudRate);
		}

    if (retValue == 0 && ComGetDataBits(hCom, &iDataBits) != COM_OK)
        {
        retValue = -11;
        }

    if (retValue == 0 && ComGetParity(hCom, &iParity) != COM_OK)
        {
        retValue = -12;
        }

    if (retValue == 0 && ComGetStopBits(hCom, &iStopBits) != COM_OK)
        {
        retValue = -13;
        }

    if (retValue != 0)
        {
        free(pvs);
        pvs = NULL;
        return retValue;
        }

    #if defined(TODO)
    pDevCfg->commconfig.dcb.BaudRate = iBaudRate;
    #endif // TODO
	pDevCfg->commconfig.dcb.ByteSize = (BYTE)iDataBits;
	pDevCfg->commconfig.dcb.Parity = (BYTE)iParity;
	pDevCfg->commconfig.dcb.StopBits = (BYTE)iStopBits;

    if (iDataBits != 8 && iParity != NOPARITY && iStopBits != ONESTOPBIT)
        {
        ComSetAutoDetect(hCom, FALSE);
        }

    //
    // Actually set the TAPI device's COM settings.
    //
    lLineReturn = lineSetDevConfig(dwLineId, pDevCfg, pvs->dwStringSize, DEVCLASS);

    free(pvs);
    pvs = NULL;

    if (lLineReturn < 0)
        {
		assert(FALSE);
		return lLineReturn;
        }

    retValue = cncttapiGetLineConfig( dwLineId, (VOID **) &pvs);

    if (retValue != 0)
        {
        retValue = retValue - 100;
        }

    //
    // Make sure the port settings get updated.
    //
    retValue = ComConfigurePort(hCom);

    //
    // Make sure the status bar contains the correct settings.
    //
    PostMessage(sessQueryHwndStatusbar(hCom->hSession),
                SBR_NTFY_REFRESH, (WPARAM)SBR_COM_PART_NO, 0);

    if (pvs == NULL)
        {
        return -14;
        }

    // The structure of the DevConfig block is as follows
    //
    //	VARSTRING
    //	UMDEVCFGHDR
    //	COMMCONFIG
    //	MODEMSETTINGS
    //
    // The UMDEVCFG structure used below is defined in the
    // UNIMODEM.H provided in the platform SDK (in the nih
    // directory for HTPE). REV: 12/01/2000 
    //
    if (retValue == 0)
        {
        pDevCfg = (UMDEVCFG *)((BYTE *)pvs + pvs->dwStringOffset);

        if (pDevCfg == NULL)
            {
            retValue = -15;
            }
        }

    if (retValue == 0 && (
        #if defined(TODO)
        pDevCfg->commconfig.dcb.BaudRate != iBaudRate ||
        #endif // TODO
	    pDevCfg->commconfig.dcb.ByteSize != iDataBits ||
	    pDevCfg->commconfig.dcb.Parity != iParity ||
	    pDevCfg->commconfig.dcb.StopBits != iStopBits))
        {

        //
        // If this is NT and we are currently connected with
        // a modem, we must disconnect and attempt to redial
        // so that the COM settings are set properly for the
        // modem since this can not be done once a connection
        // has been made. REV: 06/05/2001
        //
        if (IsNT())
            {
            HCNCT hCnct = sessQueryCnctHdl(hCom->hSession);
            if (hCnct)
                {
                int iStatus = cnctQueryStatus(hCnct);

                if (iStatus != CNCT_STATUS_FALSE &&
                    iStatus != CNCT_BAD_HANDLE &&
                    cnctIsModemConnection(hCnct) == 1)
                    {
                    int nDisconnect = IDYES;

                    //
                    // Don't prompt if this is NT_EDITION, just do the
                    // disconnection quietly and attempt to reconnect.
                    //
                    #if !defined(NT_EDITION)
                    TCHAR ach[256];

                    TCHAR_Fill(ach, TEXT('\0'), sizeof(ach) / sizeof(TCHAR));

                    //
                    // Prompt to disconnect current connection due to TAPI
                    // device needing to be reset. REV: 05/31/2001
                    //
                    LoadString(glblQueryDllHinst(), IDS_ER_TAPI_NEEDS_RESET, ach, sizeof(ach) / sizeof(TCHAR));

                    nDisconnect =
                        TimedMessageBox(sessQueryHwnd(hCom->hSession), ach, NULL,
                                        MB_YESNO | MB_ICONEXCLAMATION | MB_TASKMODAL,
                                        sessQueryTimeout(hCom->hSession));
                    #endif //NT_EDITION

                    if (nDisconnect == IDYES || nDisconnect == -1)
                        {
                        retValue = -16;
                        }
                    }
                } // hCnct
            } // IsNT()
        }

    free(pvs);
    pvs = NULL;

    return retValue;
    }

