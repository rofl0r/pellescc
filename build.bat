@echo off
IF "%~1"=="" GOTO usage
SET P=%1

REM left as an exercise for the reader: add /DPELLESROOT="%P%" to both pocc.exe
REM calls to have the right location set inside the binaries

%P%\Bin\pocc.exe /std:C99 /Os /I%P%\Include\Win /I%P%\Include /Tx86-coff /fp:precise /W0 /Zd /Ze /Go pellescc.c /Fopellescc.o
%P%\Bin\polink.exe /libpath:%P%\Lib\Win /libpath:%P%\Lib /subsystem:console /machine:x86 kernel32.lib advapi32.lib delayimp.lib pellescc.o /out:pellescc.exe
%P%\Bin\pocc.exe /std:C99 /Os /I%P%\Include\Win /I%P%\Include /Tx86-coff /fp:precise /W0 /Zd /Ze /Go pellesar.c /Fopellesar.o
%P%\Bin\polink.exe /libpath:%P%\Lib\Win /libpath:%P%\Lib /subsystem:console /machine:x86 kernel32.lib advapi32.lib delayimp.lib pellesar.o /out:pellesar.exe

exit 0
:usage
echo usage: %0 BASEDIR
echo BASEDIR: base directory of PellesC install (containing Bin/ Lib/ etc)
exit 1
