/*****************************************************************************/
/* hdhost.c
 *
 * HelpDesk for OS/2 - v1.00
 * Copyright 2018 OS/2 VOICE      - all rights reserved
 * Copyright 2018 Richard L Walsh - all rights reserved
 *
 */
/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define INCL_DOS
#define INCL_WIN
#include <os2.h>

#define INCL_REXXSAA
#include <rexxsaa.h>

#include "hdhost.h"

/*****************************************************************************/

BOOL    SetCWD(void);
int _System HostDlgBox(PSZ Name, LONG Argc, RXSTRING Argv[],
                       PSZ Queuename, PRXSTRING Retstr);
MRESULT EXPENTRY MainDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL    InitMainDlg(HWND hwnd, char** args);
BOOL    Save(HWND hwnd);
BOOL    ShowErr(HWND hwnd, ULONG msg);
char *  GetStem(char *stem, ULONG ndx);
BOOL    SetStem(char *stem, ULONG ndx, char * value, ULONG size);
BOOL    RunIPScript(char *arg);

/*****************************************************************************/

#define MP              MPARAM
#define ERR_TIMER       (TID_USERMAX - 27)

#define ErrGetHostIP    1
#define ErrClientName   2
#define ErrHostName     3
#define ErrHostIP       4
#define ErrInvalidIP    5

/*****************************************************************************/

/* dialog globals (oh no!) */

char    *dlgStem = 0;
char    *msgStem = 0;
char    *dataStem = 0;

char    clientName[32] = "";
char    hostName[32]   = "";
char    hostAddr[128] = "";

char    *data[] = {0, clientName, hostName, hostAddr};
size_t  sizes[] = {0, sizeof(clientName), sizeof(hostName), sizeof(hostAddr)};
ULONG   ids[]   = {0, CLIENT_EF, NAME_EF, ADDR_EF};
ULONG   dlgIDs[] =
{
    0, 0, ABOUT_LBL, ABOUT_TXT, CLIENT_LBL, CLIENT_TXT, NAME_LBL, NAME_TXT,
    ADDR_LBL, ADDR_TXT, GETIP_BTN, CANCEL_BTN, OK_BTN, PROJECT_TXT, 0
};

/* miscellanea */

PPIB    ppib;
PTIB    ptib;
char    hostUiFile[] = "host.scr";
char    hostIPFile[] = "hostip.scr";
char    msgBuf[1024] = "";

/*****************************************************************************/
/* C Startup                                                                 */
/*****************************************************************************/

int main(int argc, char **argv)
{
    int         ctr;
    LONG        rc;
    SHORT       retCode = 0;
    char        retValue[256] = "";
    RXSTRING    argstr[3] = { {0,0}, {0,0}, {0,0} };
    RXSTRING    retstr = {sizeof(retValue), retValue};

    if (!SetCWD())
        return 1;

    argc = argc > 4 ? 4 : argc;
    for (ctr = 1; ctr < argc; ctr++) {
        argstr[ctr-1].strlength = strlen(argv[ctr]);
        argstr[ctr-1].strptr = argv[ctr];
    }

    rc = (LONG)RexxRegisterFunctionExe("HostDlgBox", &HostDlgBox);
    if (rc) {
        printf("RexxRegisterFunctionExe - rc= %d\r\n", rc);
        return 1;
    }

    rc = RexxStart(3, argstr, hostUiFile, 0, "CMD", RXCOMMAND,
                   0, &retCode, &retstr);

    return rc ? rc : retCode;
}

/*****************************************************************************/

/* confirm 'tools\host.scr' can be found relative to
 * the exe (vs the CWD); if so, change the CWD to 'tools\'
 */

BOOL    SetCWD(void)
{
    ULONG       rc;
    char        *ptr;
    FILESTATUS3 fs3;

    DosGetInfoBlocks(&ptib, &ppib);
    rc = DosQueryModuleName(ppib->pib_hmte, sizeof(msgBuf), msgBuf);
    if (rc) {
        printf("DosQueryModuleName - rc= %d\r\n", rc);
        return FALSE;
    }

    ptr = strrchr(msgBuf, '\\');
    if (!ptr)
        ptr = msgBuf - 1;
    ptr++;
    sprintf(ptr, "TOOLS\\%s", hostUiFile);

    rc = DosQueryPathInfo(msgBuf, FIL_STANDARD, &fs3, sizeof(fs3));
    if (rc) {
        printf("DosQueryPathInfo - rc= %d\r\n", rc);
        return FALSE;
    }

    strcpy(ptr, "TOOLS");
    rc = DosSetDefaultDisk((ULONG)((msgBuf[0] & ~0x20) - 0x40));
    if (!rc)
        rc = DosSetCurrentDir(msgBuf);
    if (rc) {
        printf("DosSet Disk/Dir - rc= %d\r\n", rc);
        return FALSE;
    }

    return TRUE;
}

/*****************************************************************************/
/* REXX HostDlgBox() function                                                */
/*****************************************************************************/

int _System HostDlgBox(PSZ Name, LONG Argc, RXSTRING Argv[],
                       PSZ Queuename, PRXSTRING Retstr)
{
    LONG    ctr;
    int     ret = 3;
    HAB     hab = 0;
    HMQ     hmq = 0;
    char    **args[3] = {&dlgStem, &msgStem, &dataStem};

    if (Argc != 3)
        return 40;

    /* the args are the names of 3 stem variables
     * containing NLS text and data
     */
    for (ctr = 0; ctr < Argc; ctr++) {
        if (!Argv[ctr].strptr || !Argv[ctr].strlength)
            return 40;
        *args[ctr] = Argv[ctr].strptr;
    }

    /* change the process type to PM (it should be VIO on entry) */
    DosGetInfoBlocks(&ptib, &ppib);
    ppib->pib_ultype = 3;

    if ((hab = WinInitialize(0)) && (hmq = WinCreateMsgQueue(hab, 0)))
        ret = (int)WinDlgBox(HWND_DESKTOP, 0, MainDlgProc, 0, MAIN_DLG, args);

    if (hmq)
        WinDestroyMsgQueue(hmq);
    if (hab)
        WinTerminate(hab);

    _ltoa(ret, Retstr->strptr, 10);
    Retstr->strlength = strlen(Retstr->strptr);

    return 0;
}

/*****************************************************************************/

/* this function knows as little as possible about the dialog;
 * the strings it displays are provided by the REXX script
 * in a stem variable
 */

MRESULT EXPENTRY MainDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    switch (msg)
    {
        case WM_INITDLG:
            InitMainDlg(hwnd, (char**)mp2);
            return 0;

        case WM_CLOSE:
            WinDismissDlg(hwnd, 2);
            return 0;

        case WM_COMMAND:
            /* the values passed to WinDismissDlg() are the same as those
             * used by RxMsgBox() to identify which button was pressed
             */
            switch ((ULONG)mp1)
            {
                case GETIP_BTN:
                    WinEnableControl(hwnd, GETIP_BTN, FALSE);
                    if (!RunIPScript(0))
                        ShowErr(hwnd, ErrGetHostIP);
                    WinSetDlgItemText(hwnd, ADDR_EF, hostAddr);
                    WinEnableControl(hwnd, GETIP_BTN, TRUE);
                    break;

                case CANCEL_BTN:
                    WinDismissDlg(hwnd, 2);
                    break;

                case OK_BTN:
                    if (Save(hwnd))
                        WinDismissDlg(hwnd, 1);
                    break;
            }
            return 0;

        case WM_TIMER:
            if ((ULONG)mp1 == ERR_TIMER) {
                WinStopTimer(0, hwnd, ERR_TIMER);
                ShowErr(hwnd, 0);
                return 0;
            }
            break;
    }

    return (WinDefDlgProc(hwnd, msg, mp1, mp2));
}

/*****************************************************************************/

BOOL    InitMainDlg(HWND hwnd, char** args)
{
    ULONG   ctr;
    LONG    cx, cy;
    char    *ptr;
    RECTL   rclDlg;

    /* set the window title */
    ctr = 1;
    ptr = GetStem(dlgStem, ctr++);
    if (ptr)
        WinSetWindowText(hwnd, ptr);

    /* set the dialog items' text */
    for (; dlgIDs[ctr]; ctr++) {
        ptr = GetStem(dlgStem, ctr);
        if (ptr)
            WinSetDlgItemText(hwnd, dlgIDs[ctr], ptr);
    }

    /* set up the entryfields */
    for (ctr = 1; ctr < 4; ctr++)
    {
        ptr = GetStem(dataStem, ctr);
        ptr[sizes[ctr]-1] = 0;
        strcpy(data[ctr], ptr);
        WinSendDlgItemMsg(hwnd, ids[ctr], EM_SETTEXTLIMIT, (MP)(sizes[ctr]-1), 0);
        WinSetDlgItemText(hwnd, ids[ctr], data[ctr]);
    }

    /* center and show the dialog */
    cx = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
    cy = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
    WinQueryWindowRect(hwnd, &rclDlg);

    WinSetWindowPos(hwnd, 0, (cx - rclDlg.xRight) / 2, (cy - rclDlg.yTop) / 2,
                    0, 0, SWP_MOVE | SWP_ACTIVATE | SWP_SHOW);

    return TRUE;
}

/*****************************************************************************/

/* validate values in the entryfields,
 * then set them back into the stem variable
 */

BOOL    Save(HWND hwnd)
{
    char    buf[128];

    if (!WinQueryDlgItemText(hwnd, CLIENT_EF, sizeof(clientName)-1, clientName))
        return ShowErr(hwnd, ErrClientName);

    if (!WinQueryDlgItemText(hwnd, NAME_EF, sizeof(hostName)-1, hostName))
        return ShowErr(hwnd, ErrHostName);

    if (!WinQueryDlgItemText(hwnd, ADDR_EF, sizeof(buf)-1, buf))
        return ShowErr(hwnd, ErrHostIP);

    if (!RunIPScript(buf))
        return ShowErr(hwnd, ErrInvalidIP);

    SetStem(dataStem, 1, clientName, strlen(clientName));
    SetStem(dataStem, 2, hostName, strlen(hostName));
    SetStem(dataStem, 3, hostAddr, strlen(hostAddr));

    return TRUE;
}

/*****************************************************************************/

/* show a message in red for 5 seconds */

BOOL    ShowErr(HWND hwnd, ULONG msg)
{
    char    *err = "";

    if (msg) {
        err = GetStem(msgStem, msg);
        if (!err)
            err = "Error!";
    }
    WinSetDlgItemText(hwnd, ERROR_TXT, err);

    if (*err)
        WinStartTimer(0, hwnd, ERR_TIMER, 5000);

    return FALSE;
}

/*****************************************************************************/

/* fetch a stem variable from the REXX variable pool */

char *  GetStem(char *stem, ULONG ndx)
{
    ULONG       cnt;
    SHVBLOCK    block;
    COUNTRYCODE cc = {0,0};
    char        name[256];

    block.shvnext = 0;
    block.shvcode = RXSHV_FETCH;
    block.shvret = 0;

    cnt = (ULONG)sprintf(name, "%s.%d", stem, ndx);
    DosMapCase(cnt, &cc, name);
    MAKERXSTRING(block.shvname, name, cnt);

    block.shvvaluelen = sizeof(msgBuf);
    MAKERXSTRING(block.shvvalue, msgBuf, block.shvvaluelen);

    msgBuf[0] = 0;
    RexxVariablePool(&block);

    if (block.shvret != 0)
        return "Error!";

    return msgBuf;
}

/*****************************************************************************/

/* set a stem variable in the REXX variable pool */

BOOL    SetStem(char *stem, ULONG ndx, char * value, ULONG size)
{
    ULONG       cnt;
    SHVBLOCK    block;
    COUNTRYCODE cc = {0,0};
    char        name[256];

    block.shvnext = 0;
    block.shvcode = RXSHV_SET;
    block.shvret = 0;

    cnt = (ULONG)sprintf(name, "%s.%d", stem, ndx);
    DosMapCase(cnt, &cc, name);
    MAKERXSTRING(block.shvname, name, cnt);

    block.shvvaluelen = size;
    MAKERXSTRING(block.shvvalue, value, size);

    RexxVariablePool(&block);

    return (BOOL)(block.shvret < 4);
}

/*****************************************************************************/

/* run 'hostip.scr' - with an arg, it validates the arg as an IP or domain;
 * without an arg, it runs getmyip.exe then validates the result; it returns
 * either a valid IP or domain name, or a null string for invalid input
 */

BOOL    RunIPScript(char *arg)
{
    LONG        rc = 0;
    SHORT       retCode = 0;
    RXSTRING    argstr;
    RXSTRING    retstr;
    char        retValue[256] = "";

    if (!arg || !*arg) {
        argstr.strptr = "";
        argstr.strlength = 0;
    } else {
        argstr.strptr = arg;
        argstr.strlength = strlen(arg);
    }

    retstr.strptr = retValue;
    retstr.strlength = sizeof(retValue);

    rc = RexxStart(1, &argstr, hostIPFile, 0, "CMD", RXCOMMAND,
                   0, &retCode, &retstr);

    if (!rc && retstr.strlength > 0) {
        strcpy(hostAddr, retstr.strptr);
        return TRUE;
    }
    *hostAddr = 0;

    return FALSE;
}

/*****************************************************************************/

