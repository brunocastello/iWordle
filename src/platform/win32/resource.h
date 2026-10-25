/*
 * iWordle - Win32 resource IDs, shared by main.c and iWordle.rc.
 *
 * Only #defines here (the resource compiler reads this file too).
 * IDOK / IDCANCEL come from <windows.h>.
 */
#ifndef IWORDLE_RESOURCE_H
#define IWORDLE_RESOURCE_H

/* Older headers may lack these; the values are the Win32 constants. */
#ifndef DS_3DLOOK
#define DS_3DLOOK 0x0004L
#endif
#ifndef VK_F2
#define VK_F2 0x71
#endif
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

/* Icon, menu, accelerator table */
#define IDI_APP             1
#define IDM_MAIN            100
#define IDA_MAIN            101

/* Dialogs */
#define IDD_NAME            200
#define IDD_RANKING         201
#define IDD_ABOUT           202

/* Dialog controls */
#define IDC_NAME            300
#define IDC_LIST            301
#define IDC_CLEAR           302
#define IDC_ABOUT_TITLE     303
#define IDC_ABOUT_AUTHOR    304

/* Menu commands */
#define IDM_NEWGAME         400
#define IDM_ENDGAME         401
#define IDM_RANKING         402
#define IDM_EXIT            403
#define IDM_ABOUT           404

#endif /* IWORDLE_RESOURCE_H */
