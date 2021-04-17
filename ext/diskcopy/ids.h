#define DLG_DISKCOPYPROGRESS    1086

#define IDI_DISKCOPY            1

#define IDX_DOSBOOTDISK         1

#define IDC_STATIC              -1

// CopyDisk text strings
#define IDS_DISKCOPYMENU        0x10C0
#define IDS_INSERTDEST          0x10C1
#define IDS_INSERTSRC           0x10C2
#define IDS_INSERTSRCDEST       0x10C3
#define IDS_FORMATTINGDEST      0x10C4
#define IDS_COPYSRCDESTINCOMPAT 0x10C5
#define IDS_SRCDISKBAD          0x10C6
#define IDS_DSTDISKBAD          0x10C7
#define IDS_READING             0x10C8
#define IDS_WRITING             0x10c9
#define IDS_SRCDISKDMF          0x10ca
#if defined(DBCS) && !defined(NEC_98)
#define IDS_SRCDISK1024         0x10cb
#endif

#define IDD_STATUS              100
#define IDD_PROBAR              104
#define IDD_FROM                0x3202
#define IDD_TO                  0x3203

#define IDS_ERROR_READ          0x4000
#define IDS_ERROR_WRITE         0x4001
#define IDS_ERROR_FORMAT        0x4002
#define IDS_ERROR_GENERAL       0x4003

#define IDS_COPYCOMPLETED       0x4010
#define IDS_COPYABORTED         0x4011
#define IDS_COPYFAILED          0x4012


#define IDS_CANCEL              0x4020
#define IDS_CLOSE               0x4021

#define IDS_HELPSTRING          0x5001
#define IDS_VERBSTRING          0x5002

// disk image strings

#define IDS_DOSBOOTDISK_FIRST                       0x6000
#define IDS_DOSBOOTDISK_AUTOEXEC_FNAME              (IDS_DOSBOOTDISK_FIRST + 0)
#define IDS_DOSBOOTDISK_CONFIG_FNAME                (IDS_DOSBOOTDISK_FIRST + 1)
#define IDS_DOSBOOTDISK_AUTOEXEC_TEMPLATE           (IDS_DOSBOOTDISK_FIRST + 2)
#define IDS_DOSBOOTDISK_AUTOEXEC_TEMPLATE_WITH_CODE (IDS_DOSBOOTDISK_FIRST + 3)
#define IDS_DOSBOOTDISK_CONFIG_TEMPLATE             (IDS_DOSBOOTDISK_FIRST + 4)

#define IDS_DOSBOOTDISK_CONFIG_EGA_1          (IDS_DOSBOOTDISK_FIRST + 10)
#define IDS_DOSBOOTDISK_CONFIG_EGA_2          (IDS_DOSBOOTDISK_FIRST + 11)
#define IDS_DOSBOOTDISK_CONFIG_EGA_3          (IDS_DOSBOOTDISK_FIRST + 12)
#define IDS_DOSBOOTDISK_CONFIG_EGA_4          (IDS_DOSBOOTDISK_FIRST + 13)
#define IDS_DOSBOOTDISK_CONFIG_EGA_H          (IDS_DOSBOOTDISK_FIRST + 14)

#define IDS_DOSBOOTDISK_AUTOEXEC_EGA_1        (IDS_DOSBOOTDISK_FIRST + 20)
#define IDS_DOSBOOTDISK_AUTOEXEC_EGA_2        (IDS_DOSBOOTDISK_FIRST + 21)
#define IDS_DOSBOOTDISK_AUTOEXEC_EGA_3        (IDS_DOSBOOTDISK_FIRST + 22)
#define IDS_DOSBOOTDISK_AUTOEXEC_EGA_4        (IDS_DOSBOOTDISK_FIRST + 23)
#define IDS_DOSBOOTDISK_AUTOEXEC_EGA_H        (IDS_DOSBOOTDISK_FIRST + 24)

#define IDS_DOSBOOTDISK_KEYBOARD_CODE_US      (IDS_DOSBOOTDISK_FIRST + 100)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_GR      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 1)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_HE      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 2)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_FR      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 3)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_SP      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 4)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_IT      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 5)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_SV      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 6)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_NL      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 7)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_BR      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 8)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_NO      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 9)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_DK      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 10)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_SU      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 11)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_RU      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 12)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_CZ      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 13)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_PL      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 14)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_HU      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 15)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_PO      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 16)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_TR      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 17)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_GK      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 18)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_BL      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 19)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_BG      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 20)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_YU      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 21)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_BE      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 22)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_CF      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 23)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_UK      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 24)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_ET      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 25)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_SF      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 26)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_SG      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 27)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_IS      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 28)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_IME     (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 29)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_RO      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 30)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_YC      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 31)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_LA      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 32)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_UR      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 33)
#define IDS_DOSBOOTDISK_KEYBOARD_CODE_SL      (IDS_DOSBOOTDISK_KEYBOARD_CODE_US + 34)

#define IDS_DOSBOOTDISK_KEYBOARD_FNAME_1      (IDS_DOSBOOTDISK_FIRST + 200)
#define IDS_DOSBOOTDISK_KEYBOARD_FNAME_2      (IDS_DOSBOOTDISK_KEYBOARD_FNAME_1 + 1)
#define IDS_DOSBOOTDISK_KEYBOARD_FNAME_3      (IDS_DOSBOOTDISK_KEYBOARD_FNAME_1 + 2)
#define IDS_DOSBOOTDISK_KEYBOARD_FNAME_4      (IDS_DOSBOOTDISK_KEYBOARD_FNAME_1 + 3)