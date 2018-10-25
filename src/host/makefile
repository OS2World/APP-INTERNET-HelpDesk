#*****************************************************************************/
#* makefile for hdhost
#*
#* HelpDesk for OS/2
#* Copyright 2018 OS/2 VOICE      - all rights reserved
#* Copyright 2018 Richard L Walsh - all rights reserved
#*
#*****************************************************************************/
#*
#* built using: nmake v4.0, C Set/2 v1.0, rc v4.0, ilink v5.0
#*
#* Notes:
#*    This was built with the ancient IBM C Set/2 compiler using its
#*    "no runtime environment" library ('/Rn') option to produce the
#*    smallest possible executable. Using VACPP with this option will
#*    produce a somewhat larger binary. Do not even consider building
#*    this using a compiler that doesn't support this option.
#*
#*****************************************************************************/

.SUFFIXES:

.SUFFIXES: .c .h .rc .obj .exe .res

all: \
  hdhost.exe \
  makefile

hdhost.exe:  \
  hdhost.obj \
  hdhost.res \
  makefile
  ilink @<<hdhost.lrf
    /EXE /EXEPACK:2 /NOI /MAP /NOL /ST:0xe000 /PM:VIO
    /OUT:hdhost.exe
    hdhost.obj
    hdhost.def
    rexx.lib
<<
  rc -n -x2 hdhost.res hdhost.exe
  ren hdhost.exe newClient.exe

hdhost.obj: \
  hdhost.c \
  hdhost.h \
  makefile

hdhost.res: \
  hdhost.rc \
  hdhost.h \
  makefile

.c.obj:
  icc /Ss /Q /Kbga /Rn /C $<

.rc.res:
  rc -n -r $<

clean:
  @echo Performing cleanup...
  -@del hdhost.exe 2> nul
  -@del newClient.exe 2> nul
  -@del hdhost.map 2> nul
  -@del hdhost.obj 2> nul
  -@del hdhost.res 2> nul
  -@del hdhost.msg 2> nul

