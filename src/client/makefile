#*****************************************************************************/
#* makefile for hdclient
#*
#* HelpDesk for OS/2 - v1.00
#* Copyright 2018 OS/2 VOICE      - all rights reserved
#* Copyright 2018 Richard L Walsh - all rights reserved
#*
#*****************************************************************************/
#*
#* built using: nmake v4.0, C Set/2 v1.0, rc v4.0, ilink v5.0, lxlite
#*
#* Notes:
#* 1. This was built with the ancient IBM C Set/2 compiler using its
#*    "no runtime environment" library ('/Rn') option to produce the
#*    smallest possible executable. Using VACPP with this option will
#*    produce a somewhat larger binary. Do not even consider building
#*    this using a compiler that doesn't support this option.
#*
#* 2. The executable **MUST** know its size to function properly.
#*    Build 'hdclient.exe', determine its size on disk, plug this
#*    number into the "exeLth" variable in the source, then rebuild.
#*
#* 3. LxLite is used here to strip the DOS "MZ" header. This is part
#*    of a strategy to get a renamed 'helpdesk.exe' past most ISP's
#*    email virus scanners.
#*
#*****************************************************************************/

.SUFFIXES:

.SUFFIXES: .c .h .rc .obj .exe .res

all: \
  hdclient.exe \
  makefile

hdclient.exe:  \
  hdclient.obj \
  hdclient.res \
  makefile
  ilink @<<cliui.lrf
    /EXE /EXEPACK:2 /NOI /MAP /NOL /ST:0xe000 /PM:PM
    /OUT:hdclient.exe
    hdclient.obj
    hdclient.def
    rexx.lib
<<
  rc -n -x2 hdclient.res hdclient.exe
  lxlite hdclient.exe /anp:0 /d+ /f+ /ydl /yxl /mrn /mln /mfn /u- /zs

hdclient.obj: \
  hdclient.c \
  hdclient.h \
  makefile

hdclient.res: \
  hdclient.rc \
  hdclient.h \
  makefile

.c.obj:
  icc /Ss /Q /Kbga /O /Rn /C $<

.rc.res:
  rc -n -r $<

clean:
  @echo Performing cleanup...
  -@del *.obj 2> nul
  -@del *.map 2> nul
  -@del *.res 2> nul
  -@del hdclient.exe 2> nul

