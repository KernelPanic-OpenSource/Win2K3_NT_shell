//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  Copyright (C) Microsoft Corporation, 1997 - 1999
//
//  File:       uuid.h
//
//--------------------------------------------------------------------------

// Uncomment this if code in cscst.cpp monitors device changes.
//#include <pnpmgr.h>    // For GUID_DEVNODE_CHANGE

// {750fdf0e-2a26-11d1-a3ea-080036587f03}
DEFINE_GUID(CLSID_CscShellExt, 0x750fdf0e, 0x2a26, 0x11d1, 0xa3, 0xea, 0x08, 0x00, 0x36, 0x58, 0x7f, 0x03);

// {750fdf10-2a26-11d1-a3ea-080036587f03}
DEFINE_GUID(CLSID_CscUpdateHandler, 0x750fdf10, 0x2a26, 0x11d1, 0xa3, 0xea, 0x08, 0x00, 0x36, 0x58, 0x7f, 0x03);

// {750fdf0f-2a26-11d1-a3ea-080036587f03}
DEFINE_GUID(CLSID_CscVolumeCleaner, 0x750fdf0f, 0x2a26, 0x11d1, 0xa3, 0xea, 0x08, 0x00, 0x36, 0x58, 0x7f, 0x03);
// {effc2928-37b1-11d2-a3c1-00c04fb1782a}
DEFINE_GUID(CLSID_CscVolumeCleaner2, 0xeffc2928, 0x37b1, 0x11d2, 0xa3, 0xc1, 0x00, 0xc0, 0x4f, 0xb1, 0x78, 0x2a);

// {AFDB1F70-2A4C-11d2-9039-00C04F8EEB3E}
DEFINE_GUID(CLSID_OfflineFilesFolder, 0xafdb1f70, 0x2a4c, 0x11d2, 0x90, 0x39, 0x0, 0xc0, 0x4f, 0x8e, 0xeb, 0x3e);

// {10CFC467-4392-11d2-8DB4-00C04FA31A66}
DEFINE_GUID(CLSID_OfflineFilesOptions, 0x10cfc467, 0x4392, 0x11d2, 0x8d, 0xb4, 0x0, 0xc0, 0x4f, 0xa3, 0x1a, 0x66);

// {A8A5A263-A58C-11d2-A7C2-00C04FA31A66}
DEFINE_GUID(GUID_CscNullSyncItem, 
0xa8a5a263, 0xa58c, 0x11d2, 0xa7, 0xc2, 0x0, 0xc0, 0x4f, 0xa3, 0x1a, 0x66);
