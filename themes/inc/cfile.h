//---------------------------------------------------------------------------
//    cfile.h - file read/write (ansi/unicode) helper class
//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
#ifndef CFILE_H
#define CFILE_H
//---------------------------------------------------------------------------
class CSimpleFile
{
public:
    HANDLE _hFile;
    BOOL _fAnsi;

    CSimpleFile()
    {
        _hFile = INVALID_HANDLE_VALUE;
        _fAnsi = FALSE;
    }

    BOOL IsOpen()
    {
        return _hFile != INVALID_HANDLE_VALUE;
    }

    HRESULT Create(LPCWSTR lpsz, BOOL fAnsi=FALSE)
    {
        _fAnsi = fAnsi;

        _hFile = CreateFile(lpsz, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (_hFile == INVALID_HANDLE_VALUE)
            return MakeErrorLast();
        
        return S_OK;
    }

    HRESULT Open(LPCWSTR lpsz, BOOL fAnsi=FALSE, BOOL fExclusive=FALSE)
    {
        _fAnsi = fAnsi;

        DWORD dwShare = 0;
        if (! fExclusive)
            dwShare = FILE_SHARE_READ;

        _hFile = CreateFile(lpsz, GENERIC_READ, dwShare, NULL, OPEN_EXISTING, 0, NULL);
        if (_hFile == INVALID_HANDLE_VALUE)
            return MakeErrorLast();
        
        return S_OK;
    }

    HRESULT Append(LPCWSTR lpsz, BOOL fAnsi=FALSE)
    {
        _fAnsi = fAnsi;

        _hFile = CreateFile(lpsz, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL);
        if (_hFile == INVALID_HANDLE_VALUE)
            return MakeErrorLast();

        // cannot rely on retval from SetFilePointer() so must use GetLastError()
        SetLastError(0);  
        SetFilePointer(_hFile, 0, NULL, FILE_END);
        if (GetLastError() != NO_ERROR)
            return MakeErrorLast();

        return S_OK;
    }

    DWORD GetFileSize(DWORD* pdw = NULL)
    {
        return ::GetFileSize(_hFile, pdw);
    }

    HRESULT Printf(LPCWSTR fmtstr, ...)
    {
        va_list args;
        va_start(args, fmtstr);

        WCHAR msgbuff[2048];

        //---- format caller's string ----
        StringCchVPrintfW(msgbuff, ARRAYSIZE(msgbuff), fmtstr, args);
        va_end(args);

        return Write((void*)msgbuff, lstrlen(msgbuff)*sizeof(WCHAR));
    }

    HRESULT OutLine(LPCWSTR fmtstr, ...)
    {
        va_list args;
        va_start(args, fmtstr);

        WCHAR msgbuff[2048];

        //---- format caller's string ----
        StringCchVPrintfW(msgbuff, ARRAYSIZE(msgbuff), fmtstr, args);
        va_end(args);

        //---- add a CR/LF at end ----
        StringCchCatW(msgbuff, ARRAYSIZE(msgbuff), L"\r\n");

        return Write((void*)msgbuff, lstrlen(msgbuff)*sizeof(WCHAR));
    }

    ~CSimpleFile()
    {
        Close();
    }

    void Close()
    {
        if (_hFile != INVALID_HANDLE_VALUE)
            CloseHandle(_hFile);

        _hFile = INVALID_HANDLE_VALUE;
    }

    HRESULT Write(void* pv, DWORD dwBytes)
    {
        HRESULT hr = S_OK;

        DWORD dw;
        if (_fAnsi)
        {
            USES_CONVERSION;
            LPSTR p = W2A((LPCWSTR)pv);
            if (! WriteFile(_hFile, p, dwBytes/sizeof(WCHAR), &dw, NULL))
                hr = MakeErrorLast();
        }
        else
        {
            if (! WriteFile(_hFile, pv, dwBytes, &dw, NULL))
                hr = MakeErrorLast();
        }

        return hr;
    }

    HRESULT Read(void* lpBuffer, DWORD n, DWORD* lpcb)
    {
        HRESULT hr = S_OK;

        if (_fAnsi)
        {
            ASSERT(FALSE); // you don't want to be here.  Here's why (exercise for the reader):

#if 0
            LPSTR pszAnsiBuff = new CHAR[n+1];

            if (pszAnsiBuff)
            {
                if (ReadFile(_hFile, pszAnsiBuff, n, lpcb, NULL))
                {
                    //  Copy out as unicode.
                    USES_CONVERSION;
                    pszAnsiBuff[*lpcb] = 0;      // zero terminate

                    // THIS IS UNACCEPTABLE:
                    #error I'm an idiot.
                    LPCWSTR pwsz = A2W(pszAnsiBuff);
                    
                    // CALLER GETS SCREWED HERE (half of the contents of the file):
                    CopyMemory(lpBuffer, pwsz, lstrlen(pwsz));  // no room for NULL
                }
                else
                {
                    hr = MakeErrorLast();
                }

                delete [] pszAnsiBuff;
            }
            else
            {
                hr = MakeError32(E_OUTOFMEMORY);
            }
#endif 0

        }
        else
        {
            if (! ReadFile(_hFile, lpBuffer, n, lpcb, NULL))
                hr = MakeErrorLast();
        }

        return hr;
    }
};
//---------------------------------------------------------------------------
#endif      // CFILE_H
