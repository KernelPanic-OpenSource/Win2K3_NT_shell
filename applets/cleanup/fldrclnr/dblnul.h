#ifndef _INC_FLDRCLNR_DBLNUL_H
#define _INC_FLDRCLNR_DBLNUL_H

//
// For iterating over items in a double-nul terminated list of
// text strings.
//
class DblNulTermListIter
{
    public:
        explicit DblNulTermListIter(LPCTSTR pszList)
            : m_pszList(pszList),
              m_pszCurrent(pszList) { }

        ~DblNulTermListIter(void) { }

        BOOL Next(LPCTSTR *ppszItem);
        void Reset(void)
            { m_pszCurrent = m_pszList; }

    private:
        LPCTSTR m_pszList;
        LPCTSTR m_pszCurrent;
};


class DblNulTermList
{
    public:
        explicit DblNulTermList(int cchGrow = MAX_PATH)
            : m_psz(NULL),
              m_cchAlloc(0),
              m_cchUsed(0),
              m_cStrings(0),
              m_cchGrow(cchGrow) {  }

        ~DblNulTermList(void)
            { LocalFree(m_psz); }

        BOOL AddString(LPCTSTR psz)
            { return AddString(psz, psz ? lstrlen(psz) : 0); }

        int Count(void) const
            { return m_cStrings; }

        operator LPCTSTR ()
            { return m_psz; }

        DblNulTermListIter CreateIterator(void) const
            { return DblNulTermListIter(m_psz); }

#if DBG
        void Dump(void) const;
#endif

    private:
        LPTSTR m_psz;       // The text buffer.
        int    m_cchAlloc;  // Total allocation in chars.
        int    m_cchUsed;   // Total used excluding FINAL nul term.
        int    m_cchGrow;   // How much to grow each expansion.
        int    m_cStrings;  // Count of strings in list.

        BOOL AddString(LPCTSTR psz, int cch);
        BOOL Grow(void);

        //
        // Prevent copy.
        //
        DblNulTermList(const DblNulTermList& rhs);
        DblNulTermList& operator = (const DblNulTermList& rhs);
};


#endif // INC_FLDRCLNR_DBLNUL_H

