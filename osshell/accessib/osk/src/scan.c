// Copyright (c) 1997-1999 Microsoft Corporation
// Updated :1999 Anil Kumar and Maria Jose
// 

#include <windows.h>
#include <mmsystem.h>
#include "kbmain.h"
#include "kbus.h"
#include "resource.h"
#include "msswch.h"
#include "w95trace.h"

#define SCANTIMER	123    //timer identifier
#define NUMOFTIME   2      //after scan this many time of each key, 
                           //back to scan row : a-anilk

#define ROW1	1
#define ROW2	12
#define ROW3	21
#define ROW4	30
#define ROW5	39
#define ROW6	48
static int aiRows[] = {-1/*dummy*/, ROW1, ROW2, ROW3, ROW4, ROW5, ROW6};

#define COL1	  1
#define COL2	  52
#define COL3	  103
#define COL4	  153
#define COL4_END  202
#define COL4S     103   //Small KB
#define COL4S_END 143   //Small KB

int count=0;

UINT_PTR TimerS1;       //Timer for scanning
int g_iCurRowYPos = 1;  //current row which is scanning
int CurKey=0;       //current key which is scanning
int LastKey=0;      //Last scanned key in Small keyboard
int ScanState=0;    //State : Row scan / Key scan
int CurCol= COL1;

extern HWND g_hBitmapLockHwnd;

__inline ChangeBGColor(HWND hwnd)
{
    SetWindowLong(hwnd, 0, 4);
    SetBackgroundColor(hwnd, COLOR_HOTLIGHT);
    InvalidateRect(hwnd, NULL, TRUE);
}

/***********************************************/
//			Functions in this file
/***********************************************/

#include "scan.h"

void ScanningSound(int what);


/***********************************************/
//			Functions in other file
/***********************************************/
#include "ms32dll.h"
extern BOOL RedrawNumLock();
extern BOOL RedrawScrollLock();

/***************************************************************/
void Scanning(int from)
{	
	count = 0;   //reset this counter of key scan

	//Play some sound
	ScanningSound(3);

	if(kbPref->Actual)
		Scanning_Actual(from);
	else
		Scanning_Block(from);
}

/***************************************************************/
// Actual layout
/***************************************************************/
void Scanning_Actual(int from)
{	
	ScanState = ScanState + from;
	
	count = 0;   //reset this counter of key scan

	switch (ScanState)
	{
		case 0:
			KillScanTimer(TRUE);
			break;
	
		case 1:		//Row scanning
			KillScanTimer(FALSE);
			CurKey = 0;  //reset to 0 anyway
			TimerS1 = SetTimer(g_hwndOSK, SCANTIMER, PrefScanTime, (TIMERPROC)LineScanProc);
			break;
	
		case 2:		//key scanning
			KillScanTimer(FALSE);
			switch (kbPref->KBLayout)
			{
				case 101:
					TimerS1 = SetTimer(g_hwndOSK, SCANTIMER, PrefScanTime, (TIMERPROC)KeyScanProc_Actual_101);
					break;
				case 106:
					TimerS1 = SetTimer(g_hwndOSK, SCANTIMER, PrefScanTime, (TIMERPROC)KeyScanProc_Actual_106);
					break;
				case 102:
					TimerS1 = SetTimer(g_hwndOSK, SCANTIMER, PrefScanTime, (TIMERPROC)KeyScanProc_Actual_102);
					break;
				default:
					break;	// internal error!
			}
			break;
	
		default:     //stop scanning and send char
			KillScanTimer(FALSE);
			swchCheckForScanChar(FALSE); // flag msswch dll to not check for scan char

			if (smallKb && (LastKey != 0))
			{
				SendChar(lpkeyhwnd[LastKey]);
			}
			else
			{
				SendChar(lpkeyhwnd[CurKey-1]);
			}
			
			ScanState = 0;
			CurKey = 0;

			//Post a message to call Scanning again to avoid recursive call
			PostMessage(g_hwndOSK, WM_USER + 1, 0L, 0L);
			break;
	}
}

/****************************************************************/
// Scan each row. Both Actual and Block
/****************************************************************/
void CALLBACK LineScanProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{	
    int i, iPrevRowIndex;
    static int s_iRowIndex = 1;

    // quit scanning if we've cycled NUMOFTIME times w/no scan key

	if (count == NUMOFTIME)
	{	
		Scanning(-1);
		return;
	}

    // get the previous row to be restored and restore it

    iPrevRowIndex = s_iRowIndex - 1;
    if (iPrevRowIndex < 1)
        iPrevRowIndex = 6;

    RestoreRowColor(aiRows[iPrevRowIndex]);
    g_iCurRowYPos = aiRows[s_iRowIndex];

    // change the background of each key in the current row

    for (i=1; i < lenKBkey; i++)
    {	
        if (KBkey[i].posY == g_iCurRowYPos)
        {
            ChangeBGColor(lpkeyhwnd[i]);
        }
    }
	
	ScanningSound(1);	// play sound?
	
	s_iRowIndex++;        // update the current row index

	if (s_iRowIndex == 7) // last row, reset to first row 
	{	
        s_iRowIndex = 1;
		count++;
	}
}

/******************************************************************************/
// Scan each key in Actual 101 kb
/******************************************************************************/
void CALLBACK KeyScanProc_Actual_101(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{	
    register int j=0;

	if(CurKey == 0)
	{	
		//move to the correct row
		while ((KBkey[j].posY != g_iCurRowYPos) && (j < lenKBkey))
			j++;
		CurKey = j;
	}

	//Skip all the dummy key
	for(CurKey;KBkey[CurKey].smallKb == NOTSHOW;CurKey++);

	//Scan each key
	if (KBkey[CurKey].posY == g_iCurRowYPos)
	{	
		//Just reach the end last time (For Small KB only)
		if(LastKey != 0)
		{	
			RestoreKeyColor(LastKey);
			
		    //	
            //if 97 (LALT) just pass the space in SMALL KB, so don't increment the counter
            //
			if(LastKey != 97)  
            {
				count++;   //else increment the counter
            }
			
			LastKey = 0;
			
			if (count == NUMOFTIME)
			{	count = 0;
				Scanning_Actual(-1);
				return;
			}
		}
		else
		{
			RestoreKeyColor(CurKey - 1);
		}

        //Change the key color to black
		ChangeBGColor(lpkeyhwnd[CurKey]);
		CurKey++;   //jump to next key

		//Reach at the VERY END in Large KB
		//Note: It should be 111, but I let it to scan one more so I don't need to write extra code to redraw the last key 
		if(!smallKb && CurKey == 112)  
		{	
			count++;		//increment the counter
			CurKey = 95;	//set it back to first key (LCTRL) in last row of the kb
		}

		//reach at the VERY END in Small KB (KB has 117 keys)
		else if(smallKb && CurKey == 118)
		{			
			CurKey = 95;  // first key (LCTRL) in last row
			LastKey = 117;
		}

		//After left Alt, jump to 111 (Space) for Small KB
		else if(smallKb && CurKey == 98)
		{	
            CurKey = 111;   //Space in small KB
			LastKey = 97;   //LALT
		}
			
		//reach at the end of each row
		else if(smallKb) 
		{	
  			// the number is one advance than the actual key 
			// because it increment one after it scan the key
			switch(CurKey)
			{
				case 14:		//f12
				CurKey = 1;
				LastKey = 13;
				break;

				case 32:		//BS
				CurKey = 17;	// ~
				LastKey = 31;	//BS
				break;

				case 53:		// |
				CurKey = 39;	//TAB
				LastKey = 52;	// |
				break;

				case 74:		//enter
				CurKey = 60;	// Cap
				LastKey = 73;	//ENTER
				break;

				case 90:		//right shift
				CurKey = 77;   //LSHIFT
				LastKey = 89;  //RSHIFT
				break;
			}
		}
	}
	//End of the row (Large KB).  Reset to beginning of the row
	else if (KBkey[CurKey].posY > g_iCurRowYPos && !smallKb)
	{	
		RestoreKeyColor(CurKey - 1);
		
		count++;   //increment the counter

		switch (g_iCurRowYPos)
		{
			case ROW1:
			CurKey = 1;  //esc
			break;

			case ROW2:
			CurKey = 17;  // ~
			break;

			case ROW3:
			CurKey = 39;  //TAB
			break;

			case ROW4:
			CurKey = 60;  // CAP
			break;

			case ROW5:
			CurKey = 77;  // LSHIFT
			break;

			case ROW6:
			CurKey = 95;  //LCRL
			break;
		}
	}

	//Play some sound
	ScanningSound(1);
	
	// We have scan NUMOFTIME for each key in this row, and 
	// the user hasn't made a choice. Now go back to scan ROW
	if (count == NUMOFTIME)
	{	
		count = 0;
		Scanning_Actual(-1);
	}
}

/******************************************************************************/
// Scan each key in Actual 102 kb
/******************************************************************************/
void CALLBACK KeyScanProc_Actual_102(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{	
    register int j=0;

	if(CurKey == 0)
	{	//move to the correct row
		while ((KBkey[j].posY != g_iCurRowYPos) && (j < lenKBkey))
			j++;
		CurKey = j;
	}

	//Skip all the dummy key
	for(CurKey;KBkey[CurKey].smallKb == NOTSHOW;CurKey++);

	//Scan each key
	if (KBkey[CurKey].posY == g_iCurRowYPos)
	{	
		//Just reach the end last time (For Small KB only)
		if(LastKey != 0)
		{	
			RestoreKeyColor(LastKey);
			
		    //	
            //if 97 (LALT) just pass the space in SMALL KB, so don't increment the counter
            //
			if(LastKey != 97)  
            {
				count++;   //else increment the counter
            }
			
			LastKey = 0;
			
			if (count == NUMOFTIME)
			{	count = 0;
				Scanning_Actual(-1);
				return;
			}
		}
		else
		{
			RestoreKeyColor(CurKey - 1);
		}

        //Change the key color to black
		ChangeBGColor(lpkeyhwnd[CurKey]);
		CurKey++;   //jump to next key

		//Reach at the VERY END in Large KB
		//Note: It should be 111, but I let it to scan one more so I don't need to write extra code to redraw the last key 
		if(!smallKb && CurKey == 112)  
		{	
			count++;   //increment the counter
			CurKey = 95;  //set it back to first key (LCTRL) in last row of the kb
		}
		//reach at the VERY END in Small KB (KB has 117 keys)
		else if(smallKb && CurKey == 118)
		{			
			CurKey = 95;  // first key (LCTRL) in last row
			LastKey = 117;
		}
		//After left Alt, jump to 111 (Space) for Small KB
		else if(smallKb && CurKey == 98)
		{	
            CurKey = 111;   //Space in small KB
			LastKey = 97;   //LALT
		}
		//reach at the end of each row
		else if(smallKb) 
		{	
  			// the number is one advance than the actual key 
			// because it increment one after it scan the key
			switch(CurKey)
			{
				case 14:  //f12
				CurKey = 1;
				LastKey = 13;
				break;

				case 32:   //BS
				CurKey = 17;  // ~
				LastKey = 31;  //BS
				break;

				case 53:    // |
				CurKey = 39;  //TAB
				LastKey = 52;  // |
				break;

				case 74:   //enter
				CurKey = 60;    // Cap
				LastKey = 73;   //ENTER
				break;

				case 90:   //right shift
				CurKey = 77;   //LSHIFT
				LastKey = 89;  //RSHIFT
				break;
			}
		}
	}
	//End of the row (Large KB).  Reset to beginning of the row
	else if (KBkey[CurKey].posY > g_iCurRowYPos && !smallKb)
	{	
		RestoreKeyColor(CurKey - 1);
		
		count++;   //increment the counter

		switch (g_iCurRowYPos)
		{
			case ROW1:
			CurKey = 1;  //esc
			break;

			case ROW2:
			CurKey = 17;  // ~
			break;

			case ROW3:
			CurKey = 39;  //TAB
			break;

			case ROW4:
			CurKey = 60;  // CAP
			break;

			case ROW5:
			CurKey = 77;  // LSHIFT
			break;

			case ROW6:
			CurKey = 95;  //LCRL
			break;
		}
	}

	//Play some sound
	ScanningSound(1);
	
	// We have scan NUMOFTIME for each key in this row, and the 
	// user hasn't made a choice. Now go back to scan ROW
	if (count == NUMOFTIME)
	{	
		count = 0;
		Scanning_Actual(-1);
	}
}

/******************************************************************************/
// Scan each key in Actual 106 kb
/******************************************************************************/
void CALLBACK KeyScanProc_Actual_106(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{	
    register int j=0;

	if(CurKey == 0)
	{	//move to the correct row
		while ((KBkey[j].posY != g_iCurRowYPos) && (j < lenKBkey))
			j++;
		CurKey = j;
	}

	//Skip all the dummy key
	for(CurKey;KBkey[CurKey].smallKb == NOTSHOW;CurKey++);

	//Scan each key 
	if(KBkey[CurKey].posY == g_iCurRowYPos)
	{	
		//Just reach the end last time (For Small KB only)
		if(LastKey != 0)
		{	
			RestoreKeyColor(LastKey);
			
		    //	
            //These are the keys as exception in SMALL KB.
			//They are not reach the end of the road, 
		    //so don't increment the counter
            //
			if(LastKey != 98 && LastKey != 101 && LastKey != 111)  
            {
				count++;   //else increment the counter
            }
			
			LastKey = 0;
			
			if (count == NUMOFTIME)
			{	count = 0;
				Scanning_Actual(-1);
				return;
			}
		}
		else
		{
			RestoreKeyColor(CurKey - 1);
		}

        //Change the key color to black
		ChangeBGColor(lpkeyhwnd[CurKey]);
		CurKey++;   //jump to next key

		//Reach at the VERY END in Large KB
		//Note: It should be 111, but I let it to scan one more so I don't need to write extra code to redraw the last key 
		if(!smallKb && CurKey == 112)  
		{	
			count++;   //increment the counter
			
			CurKey = 95;  //set it back to first key (LCTRL) in last row of the kb
		}
		//reach at the VERY END in Small KB (KB has 117 keys)
		else if(smallKb && CurKey == 118)
		{			
			CurKey = 95;  // first key (LCTRL) in last row
			LastKey = 117;
		}
		//After the NO CONVERT Key, skip the Space in Large KB
		else if(smallKb && CurKey == 99)
		{	
            CurKey = 111;   //CONVERT key
			LastKey = 98;   //Japanses
		}
		//After Space jump back to CONVERT key
		else if(smallKb && CurKey == 112)
		{	
            CurKey = 100;   //CONVERT
			LastKey = 111;   // SPACE
		}
		//After THE Japanese key, jump to 102 (RALT) for Small KB
		else if(smallKb && CurKey == 102)
		{	
            CurKey = 112;   //RALT in small KB
			LastKey = 101;   //Japanese Key
		}
		//reach at the end of each row
		else if(smallKb) 
		{	
  			// the number is one advance than the actual key 
			// because it increment one after it scan the key
			switch(CurKey)
			{
				case 14:  //f12
				CurKey = 1;
				LastKey = 13;
				break;

				case 32:   //BS
				CurKey = 17;  // ~
				LastKey = 31;  //BS
				break;

				case 54:    // Enter
				CurKey = 39;  //TAB
				LastKey = 53;  // |
				break;

				case 74:   // '\'
				CurKey = 61;    // Cap
				LastKey = 73;   //ENTER
				break;

				case 90:   //right shift
				CurKey = 77;   //LSHIFT
				LastKey = 89;  //RSHIFT
				break;
			}
		}
	}
	//End of the row (Large KB).  Reset to beginning of the row
	else if (KBkey[CurKey].posY > g_iCurRowYPos && !smallKb)
	{	
		RestoreKeyColor(CurKey - 1);
		
		count++;   //increment the counter

		switch (g_iCurRowYPos)
		{
			case ROW1:
			CurKey = 1;  //esc
			break;

			case ROW2:
			CurKey = 17;  // ~
			break;

			case ROW3:
			CurKey = 39;  //TAB
			break;

			case ROW4:
			CurKey = 61;  // CAP
			break;

			case ROW5:
			CurKey = 77;  // LSHIFT
			break;

			case ROW6:
			CurKey = 95;  //LCRL
			break;
		}
	}

	//Play some sound
	ScanningSound(1);

	// We have scan NUMOFTIME for each key in this row, and the
	// user hasn't made  a choice. Now go back to scan ROW
	if (count == NUMOFTIME)
	{	
		count = 0;
		Scanning_Actual(-1);
	}
}

/******************************************************************************/
// Restore the whole row's color
/******************************************************************************/
void RestoreRowColor(int Row)
{	
    register int i;

    //Reset previous row color
    for(i=1; i < lenKBkey; i++)
    {
        if(KBkey[i].posY == Row)
        {
            RestoreKeyColor(i);
        }
	}
}

/****************************************************************************/
// Restore one key color
/****************************************************************************/
void RestoreKeyColor(int i)
{
	// index > 0
	if(i<=0)
		return;
	
	//Skip all the dummy key
	for(i; KBkey[i].smallKb == NOTSHOW; i--);

	if (lpkeyhwnd[i] != g_hBitmapLockHwnd)
	{
		// Do not change the key color if Caplock bitmap
		SetWindowLong(lpkeyhwnd[i], 0, 0);	
	}

	switch (KBkey[i].ktype)
	{
		case KNORMAL_TYPE:
		SetBackgroundColor(lpkeyhwnd[i], COLOR_MENU);
	    break;

		case SCROLLOCK_TYPE:
        RedrawScrollLock();
        break;

		case NUMLOCK_TYPE:
        RedrawNumLock();
	    break;
			
		case KMODIFIER_TYPE:
		case KDEAD_TYPE:
        {
            int iColor = (IsModifierPressed(lpkeyhwnd[i]))?COLOR_HOTLIGHT:COLOR_INACTIVECAPTION;
            SetBackgroundColor(lpkeyhwnd[i], iColor);
        }
	    break;

        case LED_NUMLOCK_TYPE:
		case LED_SCROLLLOCK_TYPE:
		case LED_CAPSLOCK_TYPE:
		SetBackgroundColor(lpkeyhwnd[i], COLOR_BTNSHADOW);
	    break;	
	}

	InvalidateRect(lpkeyhwnd[i], NULL, TRUE);
}

/****************************************************************/
// Pass FALSE - Pause
// Pass TRUE  - Reset (start scanning from row 1) 
/****************************************************************/
void KillScanTimer(BOOL reset)
{
	KillTimer(g_hwndOSK, SCANTIMER);
	RestoreRowColor(g_iCurRowYPos);

	//if calling from dialog init, reset these vars to start at the beginning 
	if (reset)
	{	
		ScanState = 0;
		CurKey = 0;
		g_iCurRowYPos = ROW1;
	}
}

/**********************************************************************/
// Scan in block kb
/**********************************************************************/
void Scanning_Block(int from)
{	

	ScanState = ScanState + from;
	
	count = 0;   //reset this counter of key scan


	switch (ScanState)
	{
		case 0:
			KillScanTimer(TRUE);
			break;
	
		case 1:   //Row scanning
			KillScanTimer(FALSE);
			CurCol = COL1;    //reset Col to COL1 for next round
			TimerS1 = SetTimer(g_hwndOSK, SCANTIMER, PrefScanTime, (TIMERPROC)LineScanProc);
			break;

		case 2:
			KillScanTimer(FALSE);
			TimerS1 = SetTimer(g_hwndOSK, SCANTIMER, PrefScanTime, (TIMERPROC)BlockScanProc);
			break;

		case 3:    //key scanning
			KillScanTimer(FALSE);

			//Set the coloum back 1 because in Block scan I set it 1 advance
			switch(CurCol)
			{
				case COL1: CurCol = COL4; break;
				case COL2: CurCol = COL1; break;
				case COL3: CurCol = COL2; break;
				case COL4: CurCol = COL3; break;
			}

			//Small KB doesn't has Col4, Set to Col3
			if(smallKb && (CurCol == COL4))
				CurCol = COL3;

			TimerS1 = SetTimer(g_hwndOSK, SCANTIMER, PrefScanTime, (TIMERPROC)KeyScanProc_Block);
			break;
	
		default:     //stop scanning and send char
			KillScanTimer(FALSE);

			//a special for SPACE in Large KB and Block layout
			if((!kbPref->Actual) && (!smallKb) && (CurKey == 99))
				CurKey++;

			// tell msswch dll to not check this char to see if it is the scanning char

			swchCheckForScanChar(FALSE);

			SendChar(lpkeyhwnd[CurKey-1]);
			
			//re-set some vars
			ScanState = 0;
			CurKey = 0;

			//Post a message to call Scanning again to avoid recursive call
			PostMessage(g_hwndOSK, WM_USER + 1, 0L, 0L);
			break;
	}
}

/*********************************************************************/
// Restore the whole block color in block kb
/*********************************************************************/
void RestoreBlockColor(int ColStart, int ColEnd)
{	register int i;

	//Find the first key in current row and current col
	for(i=1; i < lenKBkey; i++)
		if((KBkey[i].posY == g_iCurRowYPos) && (KBkey[i].posX == ColStart))
			break;

	while(((KBkey[i].posX < ColEnd) && (KBkey[i].posY == g_iCurRowYPos)) || (KBkey[i].smallKb == NOTSHOW))
	{
		if(KBkey[i].smallKb == NOTSHOW)
		{	
			i++;
			continue;
		}

		if (lpkeyhwnd[i] != g_hBitmapLockHwnd)
		{
			// Do no change the color key if Caplock bitmap
			SetWindowLong(lpkeyhwnd[i], 0, 0);	
		}

		switch (KBkey[i].ktype)
		{
			case KNORMAL_TYPE:
			SetBackgroundColor(lpkeyhwnd[i], COLOR_MENU);
			break;

			case SCROLLOCK_TYPE:
            RedrawScrollLock();
            break;

			case NUMLOCK_TYPE:
            RedrawNumLock();
            break;
		
			case KMODIFIER_TYPE:
			case KDEAD_TYPE:
			SetBackgroundColor(lpkeyhwnd[i], COLOR_INACTIVECAPTION);
			break;
		
		
			case LED_NUMLOCK_TYPE:
			case LED_CAPSLOCK_TYPE:
			case LED_SCROLLLOCK_TYPE:
			SetBackgroundColor(lpkeyhwnd[i], COLOR_BTNSHADOW);
			break;
		}
		InvalidateRect(lpkeyhwnd[i], NULL, TRUE);
		i++;
	}
}


/*********************************************************************/
// Scan each block in block kb
/*********************************************************************/
void CALLBACK BlockScanProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{	
	register int i;

	//We have scan NUMOFTIME for each block in this row, and the user hasn't make 
	//any choice. Now go back to scan ROW
	if (count == NUMOFTIME)
	{	
		count = 0;    //reset the counter
		Scanning_Block(-1);
		return;
	}

	//Find the first key in current row and current col
	for(i=1; i < lenKBkey; i++)
		if((KBkey[i].posY == g_iCurRowYPos) && (KBkey[i].posX == CurCol))
			break;

        switch (CurCol)
		{
			case COL1:
				CurKey = i;    //set the CurKey to the beginning of this block 
				if (smallKb)	//Small KB
				{
					int j;
					RestoreBlockColor(COL3,COL4);
					for(j=113; j <= 117; j++)  //Hardcoded it!
						RestoreKeyColor(j); 
				}
				else   // Large KB
				{
					RestoreBlockColor(COL4, COL4_END);
				}

				//Paint all keys within the Block
				while(((KBkey[i].posX < COL2) && (KBkey[i].posY == g_iCurRowYPos)) || (KBkey[i].smallKb == NOTSHOW))		
				{	
					if(KBkey[i].smallKb == NOTSHOW)
					{	i++;
						continue;
					}

					ChangeBGColor(lpkeyhwnd[i]);	
					i++;
				}
				CurCol = COL2;  //Jump to next col
				break;

			case COL2:
				CurKey = i;    //set the CurKey to the beginning of this block
				RestoreBlockColor(COL1, COL2);

				//In Small KB, skip all the large kb keys
				if (smallKb)
				{
					while((KBkey[i].smallKb == LARGE) || (KBkey[i].smallKb == NOTSHOW))
						i++;
				}

				//Paint all keys within the Block
				while(((KBkey[i].posX < COL3) && (KBkey[i].posY == g_iCurRowYPos)) || (KBkey[i].smallKb == NOTSHOW))		
				{	
					if(KBkey[i].smallKb == NOTSHOW)
					{	i++;
						continue;
					}
									
					ChangeBGColor(lpkeyhwnd[i]);	
					i++;
				}
				CurCol = COL3;   //Jump to next col
				break;

			case COL3:
				//For small KB, skip all the keys only for LARGE kb
				if(smallKb)
				{
					while(KBkey[i].smallKb == LARGE || KBkey[i].smallKb == NOTSHOW)
						i++;
				}

				CurKey = i;    //set the CurKey to the beginning of this block

				//Small kb
				if (smallKb && CurKey == 111)   // CurKey == SPACE
				{
					RestoreKeyColor(111);   // SPACE
					RestoreKeyColor(112);   // RALT

					CurKey = i = 113;   //APP KEY
				}
				//Large KB
				else
				{
					RestoreBlockColor(COL2, COL3);
				}

				//Paint all keys within the Block
				while(((KBkey[i].posX < COL4) && (KBkey[i].posY == g_iCurRowYPos)) || (KBkey[i].smallKb == NOTSHOW))		
				{	
					if(KBkey[i].smallKb == NOTSHOW)
					{	i++;
						continue;
					}
					
					ChangeBGColor(lpkeyhwnd[i]);	
					i++;
				}

				if (smallKb)   //Small KB only has 3 columns
				{
					CurCol = COL1;
					count++;        // increment the counter
				}
				else    //Large KB
				{
					CurCol = COL4;  //Jump to next col
				}
				break;

			case COL4:
				CurKey = i;    //set the CurKey to the beginning of this block
				RestoreBlockColor(COL3, COL4);

				//Paint all keys within the Block
				while(((KBkey[i].posX < COL4_END) && (KBkey[i].posY == g_iCurRowYPos)) || (KBkey[i].smallKb == NOTSHOW))
				{	
					if(KBkey[i].smallKb == NOTSHOW)
					{	i++;
						continue;
					}
					
					ChangeBGColor(lpkeyhwnd[i]);	
					i++;
				}

				CurCol = COL1;   //Jump to next col
				count++;   //increment the counter
				break;
		}

	//Play some sound
	ScanningSound(1);
}

/*********************************************************************/
// Scan each key in a block in block kb
/*********************************************************************/
void CALLBACK KeyScanProc_Block(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{	
	static int last=0;	
	register int i;

	switch (CurCol)
	{
		case COL1:   //This is actually COL1
			//Scanning each key in the Block
			if((KBkey[CurKey].posY == g_iCurRowYPos) && (KBkey[CurKey].posX < COL2))
			{	
				if(last != 0)
				{	RestoreKeyColor(last);
					last = 0;
				}
				else if(CurKey > 1)
				{
					RestoreKeyColor(CurKey - 1);
				}
				
				ChangeBGColor(lpkeyhwnd[CurKey]);
				CurKey++;   //jump to next key
			}
				//Reach last key, reset to first key in the col
			else if(KBkey[CurKey].posX >= COL2)
			{	for(i=1; i < lenKBkey; i++)
				{
					if((KBkey[i].posY == g_iCurRowYPos) && (KBkey[i].posX == COL1))
					{	last = CurKey -1;   //save the last key for next restore color
						CurKey = i;
						break;
					}
				}
				count++;
			}
			break;

		case COL2:   //Actually COL2
			//In small KB, skip all the Large KB keys
			if (smallKb)
			{
				while(KBkey[CurKey].smallKb == LARGE || KBkey[CurKey].smallKb == NOTSHOW)
					CurKey++;
			}

			//Only for the last row (both Small and Large KB)
			if(KBkey[CurKey].name == KB_SPACE)
			{	
				if (smallKb)    //Small KB
					RestoreKeyColor(112);   //106

				ChangeBGColor(lpkeyhwnd[CurKey]);
				CurKey++;

				if (!smallKb)   //Large KB
				{				
					count++;

					if(count < NUMOFTIME)
						CurKey--;
				}
				else    //Small Kb
				{
					last = 111;      //SPACE (small)
				}
				break;
			}

			//Scanning each key in the Column
			if((KBkey[CurKey].posY == g_iCurRowYPos) && (KBkey[CurKey].posX < COL3))
			{
				if(last != 0)
				{	
					RestoreKeyColor(last);
					last = 0;
				}
				else
				{
					RestoreKeyColor(CurKey - 1);
				}

				ChangeBGColor(lpkeyhwnd[CurKey]);
			
				CurKey++;   //jump to next key
			}
			//Reach last key, reset to first key in the col
			else if(KBkey[CurKey].posX >= COL3)
			{	
				if (smallKb && CurKey == 113)    //App Key (small)
				{
					CurKey = 111;       //SPACE (small)
					last = 112;         //RALT (small)
				}
				else
				{
					for(i=1; i < lenKBkey; i++)
					{
						if((KBkey[i].posY == g_iCurRowYPos) && (KBkey[i].posX == COL2))
						{	
							//save the last key for next restore color
							last = CurKey - 1;
							CurKey = i;
							break;
						}
					}
				}
				count++;
			}
			break;

		case COL3:  //Actually COL3
			//Last Col for SMALL KB
			//Special case!!  Last scan reach the very end in Samll KB (118)
			if (CurKey == 118)  
			{   
				CurKey = 113;   //Reset to the first one in this col
				count++;
			}

			//Skip all Dummy key
			while(KBkey[CurKey].smallKb == NOTSHOW)
				CurKey++;

			//Scanning each key in the Block
			if((KBkey[CurKey].posY == g_iCurRowYPos) && (KBkey[CurKey].posX < COL4) && 
			   (CurKey <= 117))
			{
				//In small KB, skip all the Large KB keys
				if(smallKb)
				{
					while(KBkey[CurKey].smallKb == LARGE || KBkey[CurKey].smallKb == NOTSHOW)
						CurKey++;
				}

				if(last != 0)
				{	
					RestoreKeyColor(last);

					//Special case, reach the end of the small KB
					if (last == 118)    //The end in Small KB
						CurKey = 113;   //reset to first one in this col

					last = 0;
				}
				else
				{
					RestoreKeyColor(CurKey - 1);
				}

				//Set the key to black
				ChangeBGColor(lpkeyhwnd[CurKey]);
			
				CurKey++;   //jump to next key
			}
			//Reach last key, reset to first key in the col
			else if( (KBkey[CurKey].posX >= COL4 || CurKey > 109)  && (CurKey <= 117) )  
			{	
				//reach the very end

				for(i=1; i < lenKBkey; i++)
				{
					if((KBkey[i].posY == g_iCurRowYPos) && (KBkey[i].posX == COL3))
					{	last = CurKey - 1;   //save the last key for next restore color
						CurKey = i;
						break;
					}
				}
				count++;
			}

			//Small KB reach the last key (117)
			if (smallKb && (CurKey == 118))
				last = CurKey -1;
			break;

		case COL4:   //Actual COL4
			//Only LARGE KB has COL4
			//Scanning each key in the Block
																					   //large KB and not reach the end
			if((KBkey[CurKey].posY == g_iCurRowYPos) && (KBkey[CurKey].posX < COL4_END) && !smallKb && (CurKey <= 110))
			{
				if(last != 0)
				{	RestoreKeyColor(last);
					last = 0;
				}
				else
					RestoreKeyColor(CurKey - 1);


				ChangeBGColor(lpkeyhwnd[CurKey]);
			
				CurKey++;   //jump to next key
							
			}
			//Reach last key, reset to first key in the col
			else // if(KBkey[CurKey - 1].posX >= COL4_END)
			{	
				for(i=1; i < lenKBkey; i++)
				{
					if((KBkey[i].posY == g_iCurRowYPos) && (KBkey[i].posX == COL4))
					{	last = CurKey - 1;   //save the last key for next restore color
						CurKey = i;
						break;
					}
				}
				count++;
			}
			break;
	}

	//Play some sound
	ScanningSound(1);
		
	// We have scan NUMOFTIME for each key in this Block, and the 
	// user hasn't made a choice. Now go back to scan BLOCK
	if (count == NUMOFTIME)
	{	
		count = 0;    //reset the counter
		Scanning_Block(-1);
	}
}
/*****************************************************************************/
void ScanningSound(int what)
{	
	// don't want sound, then exit
	if(!Prefusesound)
		return;

	switch(what)
	{
		case 2:     // scanning
		PlaySound(MAKEINTRESOURCE(WAV_CLICKDN), hInst, SND_ASYNC|SND_RESOURCE);
		break;

		case 1:      //one level up
		PlaySound(MAKEINTRESOURCE(WAV_CLICKUP), hInst, SND_ASYNC|SND_RESOURCE);
		break;

		case 3:      //switch click
		PlaySound(MAKEINTRESOURCE(WAV_SWITCH_CLICK), hInst, SND_ASYNC|SND_RESOURCE);
		break;
	}
}

