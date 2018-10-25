/*****************************************************************************/
/* hdclient.c
 *
 * HelpDesk for OS/2 - v1.00
 * Copyright 2018 OS/2 VOICE      - all rights reserved
 * Copyright 2018 Richard L Walsh - all rights reserved
 *
 */
/*****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define INCL_DOS
#define INCL_WIN
#include <os2.h>

#define INCL_REXXSAA
#include <rexxsaa.h>

#include "hdclient.h"

/*****************************************************************************/

BOOL    IsEmbedded(void);
ULONG   ReadEmbeddedScript(void);
ULONG   ErrMsg(char *fmt, ...);
int _System ClientGetSize(PSZ Name, LONG Argc, RXSTRING Argv[],
                          PSZ Queuename, PRXSTRING Retstr);
int _System ClientDlgBox(PSZ Name, LONG Argc, RXSTRING Argv[],
                         PSZ Queuename, PRXSTRING Retstr);
MRESULT EXPENTRY MainDlgProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
char *  GetMsg(char *stem, ULONG ndx);
int _System ClientUnlock(PSZ Name, LONG Argc, RXSTRING Argv[],
                         PSZ Queuename, PRXSTRING Retstr);
int _System ClientSwitchTo(PSZ Name, LONG Argc, RXSTRING Argv[],
                           PSZ Queuename, PRXSTRING Retstr);

/*****************************************************************************/
/* 'hdclient.exe' *must* know its size on disk. To accomplish                */
/* this, build the exe, plug in its length here, then rebuild.               */
/*****************************************************************************/

ULONG	exeLth = 8497;

/*****************************************************************************/

BOOL    fFalse = FALSE;
PPIB    ppib = 0;
PTIB    ptib = 0;
ULONG   scriptLth = 0;
ULONG   fileLth = 0;

ULONG   dlgIDs[] =
{
    0, MAIN_DLG, ABOUT_LBL, ABOUT_TXT,
    ALERT_LBL, ALERT_TXT, CANCEL_BTN, OK_BTN, PROJECT_TXT, 0
};

char    exePath[CCHMAXPATH] = "";
char    arg[256] = "";
char    retValue[256] = "";
char    msgBuf[1024] = "";

/*****************************************************************************/

/* IBM C Set/2 creates a nearly empty segment named DDE4_DATA32;
 * the following puts the buffer used to store the REXX script
 * into that segment.
 */

#pragma data_seg(DDE4_DATA32)

char    script[65536];

#pragma data_seg()

/*****************************************************************************/

int     main(int argc, char* argv[])
{
    LONG        rc = 0;
    ULONG       ctr;
    ULONG       cnt;
    char       *ptr;
    RXSTRING    argstr;
    RXSTRING    retstr;
    RXSTRING    instore[2];
    SHORT       retCode = 0;

    DosGetInfoBlocks(&ptib, &ppib);

    /* ensure there's a usable script attached
     * and that this exe has been renamed
     */
    if (!IsEmbedded())
        return -1;

    /* concatenate any args into a single string */
    for (ptr = arg, ctr = 1; ctr < argc; ctr++) {
        cnt = strlen(argv[ctr]);
        if (&ptr[cnt+1] >= &arg[sizeof(arg)])
            break;
        if (ptr > arg)
            *ptr++ = ' ';
        strcpy(ptr, argv[ctr]);
        ptr += cnt;
    }

    argstr.strptr = arg;
    argstr.strlength = strlen(arg);

    retstr.strptr = retValue;
    retstr.strlength = sizeof(retValue);

    /* read the script appended to this exe */
    ctr = ReadEmbeddedScript();
    if (!ctr)
        return -1;

    instore[0].strptr = script;
    instore[0].strlength = ctr;
    instore[1].strptr = 0;
    instore[1].strlength = 0;

    /* register REXX functions the script will use */
    rc = (LONG)RexxRegisterFunctionExe("ClientGetSize", &ClientGetSize);
    if (!rc)
        rc = (LONG)RexxRegisterFunctionExe("ClientDlgBox", &ClientDlgBox);
    if (!rc)
        rc = (LONG)RexxRegisterFunctionExe("ClientUnlock", &ClientUnlock);
    if (!rc)
        rc = (LONG)RexxRegisterFunctionExe("ClientSwitchTo", &ClientSwitchTo);
    if (rc)
        ErrMsg("RexxRegisterFunctionExe - rc= %d\r\n", rc);

    /* run the script */
    rc = RexxStart(1, &argstr, exePath, instore, "CMD", RXCOMMAND,
                   0, &retCode, &retstr);

    return (rc ? rc : retCode);
}

/*****************************************************************************/

/* is there a script appended to the exe? */

BOOL    IsEmbedded(void)
{
    char        *ptr;
    FILESTATUS3 fs3;

    if (exeLth == 0 ||
        DosQueryModuleName(ppib->pib_hmte, sizeof(exePath), exePath) ||
        DosQueryPathInfo(exePath, FIL_STANDARD, &fs3, sizeof(fs3)) ||
        fs3.cbFile <= exeLth + 4) {
        ErrMsg("Embedded Rexx command file not found.\r\n");
        return FALSE;
    }
    fileLth = fs3.cbFile;

    ptr = strrchr(exePath, '\\');
    if (!ptr)
        ptr = exePath - 1;
    ptr++;
    if (!strcmp(ptr, "CLIENTUI.EXE")) {
        ErrMsg("Please give ClientUI.exe a different name.\r\n");
        return FALSE;
    }

    return TRUE;
}

/*****************************************************************************/

/* like the name says... */

ULONG   ReadEmbeddedScript(void)
{
    ULONG       rtn = 0;
    ULONG       rc = 0;
    ULONG       ul;
    ULONG       max;
    HFILE       hf = 0;
    char        *ptr;

do {
    rc = DosOpen(exePath, &hf, &ul, 0, 0, OPEN_ACTION_OPEN_IF_EXISTS,
                 OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, 0);
    if (rc)
        break;

    rc = DosSetFilePtr(hf, (LONG)exeLth, FILE_BEGIN, &ul);
    if (rc || ul != exeLth)
        break;

    rc = DosRead(hf, script, 4, &ul);
    if (rc || ul != 4)
        break;

    if (script[0] != '/' || script[1] != '*' ) {
        rc = 87;
        break;
    }

    max = fileLth - exeLth - 4;
    if (max > sizeof(script) - 2)
        max = sizeof(script) - 2;

    rc = DosRead(hf, &script[4], max, &ul);
    if (rc || ul != max)
        break;

    max += 4;

    ptr = memchr(script, 0x1A, max);
    if (!ptr) {
        ptr = &script[max];
        *ptr = (UCHAR)0x1A;
    }
    scriptLth = (ULONG)(ptr - script);
    *++ptr = 0;

    rtn = scriptLth;

} while (fFalse);

    if (hf)
        DosClose(hf);

    if (!rtn)
        ErrMsg("Unable to read embedded script: rc= %d\r\n", rc);

    return rtn;
}

/*****************************************************************************/

ULONG   ErrMsg(char *fmt, ...)
{
    ULONG   ul;
    va_list arg_ptr;
    char    buf[512];

    va_start(arg_ptr, fmt);
    vsprintf(buf, fmt, arg_ptr);
    va_end(arg_ptr);

    DosWrite(2, buf, strlen(buf), &ul);
    return 0;
}

/*****************************************************************************/
/* REXX Functions                                                            */
/*****************************************************************************/

/* REXX function ClientGetSize(requestType)
 * args are: 'E' - exe length; 'F' - total file length
 *           'S' - script length; 'C' - combined exe & script length
 */

int _System ClientGetSize(PSZ Name, LONG Argc, RXSTRING Argv[],
                          PSZ Queuename, PRXSTRING Retstr)
{
    ULONG   size = 0;
    char    opt;

    if (Argc != 1 || !Argv[0].strptr || !Argv[0].strlength)
        return 40;

    opt = (char)(*(Argv[0].strptr) & ~0x20);

    switch (opt) {
        case 'E':
            size = exeLth;
            break;

        case 'S':
            size = scriptLth;
            break;

        case 'C':
            size = exeLth + scriptLth;
            break;

        case 'F':
            size = fileLth;
            break;
    }

    _ultoa(size, Retstr->strptr, 10);
    Retstr->strlength = strlen(Retstr->strptr);

    return 0;
}

/*****************************************************************************/

/* REXX function ClientDlgBox(stemName) */

int _System ClientDlgBox(PSZ Name, LONG Argc, RXSTRING Argv[],
                         PSZ Queuename, PRXSTRING Retstr)
{
    int     ret = 3;
    ULONG   errID;
    HAB     hab = 0;
    HMQ     hmq = 0;

    if (Argc != 1 || !Argv[0].strptr || !Argv[0].strlength)
        return 40;

do {
    /* change the process type to PM if it isn't already PM */
    ppib->pib_ultype = 3;

    /* WinInitialize() can be called multiple times without failing */
    hab = WinInitialize(0);

    /* under Object REXX, a message queue already exists, so this may fail;
     * under Classic REXX, we have to create it
     */
    WinGetLastError(0);
    hmq = WinCreateMsgQueue(hab, 0);
    if (!hmq) {
        errID = WinGetLastError(0);
        if (LOUSHORT(errID) != 0x1052) {
            ErrMsg("WinCreateMsgQueue failed - err= %x\r\n", LOUSHORT(errID));
            break;
        }
    }

    /* show the dialog */
    ret = (int)WinDlgBox(HWND_DESKTOP, 0, MainDlgProc, 0, MAIN_DLG, Argv[0].strptr);

} while (fFalse);

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
        {
            ULONG   ctr;
            LONG    cx;
            LONG    cy;
            HWND    hCtl;
            RECTL   rclDlg;
            char    *stem = (char*)mp2;
            char    *ptr;

            cx = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
            cy = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);

            /* at high-res (1280x1024 & above), change to a larger font */
            if (cx * cy > 1200000) {
                hCtl = WinWindowFromID(hwnd, ABOUT_TXT);
                WinSetPresParam(hCtl, PP_FONTNAMESIZE, 12, "9.Helvetica");
                hCtl = WinWindowFromID(hwnd, ALERT_TXT);
                WinSetPresParam(hCtl, PP_FONTNAMESIZE, 12, "9.Helvetica");
            }

            /* set the window title */
            ctr = 1;
            ptr = GetMsg(stem, ctr++);
            if (ptr)
                WinSetWindowText(hwnd, ptr);
            
            /* set the text for all dlgitems */
            for (; dlgIDs[ctr]; ctr++) {
                ptr = GetMsg(stem, ctr);
                if (ptr)
                    WinSetDlgItemText(hwnd, dlgIDs[ctr], ptr);
            }

            /* center and show the dialog */
            WinQueryWindowRect(hwnd, &rclDlg);
            WinSetWindowPos(hwnd, 0,
                            (cx - rclDlg.xRight) / 2, (cy - rclDlg.yTop) / 2,
                            0, 0, SWP_MOVE | SWP_ACTIVATE | SWP_SHOW);

            return 0;
        }

        case WM_CLOSE:
            WinDismissDlg(hwnd, 2);
            return 0;

        case WM_COMMAND:
            /* the values passed to WinDismissDlg() are the same as those
             * used by RxMsgBox() to identify which button was pressed
             */
            switch ((ULONG)mp1)
            {
                case CANCEL_BTN:
                    WinDismissDlg(hwnd, 2);
                    break;

                case OK_BTN:
                    WinDismissDlg(hwnd, 1);
                    break;
            }
            return 0;
    }

    return (WinDefDlgProc(hwnd, msg, mp1, mp2));
}

/*****************************************************************************/

/* fetch a stem variable from the REXX variable pool */

char *  GetMsg(char *stem, ULONG ndx)
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

/* REXX function ClientUnlock(exeName) */

int _System ClientUnlock(PSZ Name, LONG Argc, RXSTRING Argv[],
                         PSZ Queuename, PRXSTRING Retstr)
{
    ULONG   rc;

    if (Argc != 1 || !Argv[0].strptr || !Argv[0].strlength)
        return 40;

    rc = DosReplaceModule(Argv[0].strptr, 0, 0);

    _ultoa(rc, Retstr->strptr, 10);
    Retstr->strlength = strlen(Retstr->strptr);

    return 0;
}

/*****************************************************************************/

/* REXX function ClientSwitchTo(sessionTitle) */

int _System ClientSwitchTo(PSZ Name, LONG Argc, RXSTRING Argv[],
                           PSZ Queuename, PRXSTRING Retstr)
{
    ULONG   rc = 1;
    ULONG   ctr;
    ULONG   cnt;
    HAB     hab = 0;
    HWND    hMenu = 0;
    void    *buf = 0;
    PSWENTRY psw;

    if (Argc != 1 || !Argv[0].strptr || !Argv[0].strlength)
        return 40;

    /* change the process type to PM (it should be VIO on entry) */
    ppib->pib_ultype = 3;

    /* init PM then get the switch list */
    if ((hab = WinInitialize(0)) != 0 &&
        !DosAllocMem(&buf, 0xF000, PAG_COMMIT | PAG_READ | PAG_WRITE) &&
        (cnt = WinQuerySwitchList(hab, (PSWBLOCK)buf, 0xF000)) != 0) {

        /* locate the requested window in the switch list;
         * if found, remove the Close entry from its sysmenu,
         * then bring the window to the top
         */
        psw = ((PSWBLOCK)buf)->aswentry;
        for (ctr = 0; ctr < cnt; ctr++, psw++) {
            if (!strcmp(psw->swctl.szSwtitle, Argv[0].strptr)) {
                hMenu = WinWindowFromID(psw->swctl.hwnd, FID_SYSMENU);
                if (hMenu)
                    WinPostMsg(hMenu, MM_DELETEITEM, MPFROM2SHORT(0x8004, TRUE), 0);
                rc = WinSwitchToProgram(psw->hswitch);
                break;
            }
        }
    }

    if (buf)
        DosFreeMem(buf);
    if (hab)
        WinTerminate(hab);

    _ultoa(rc, Retstr->strptr, 10);
    Retstr->strlength = strlen(Retstr->strptr);

    return 0;
}

/*****************************************************************************/

