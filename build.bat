@ECHO ON
SET HOME=%~dp0
SET WORK=%HOME%\build
SET OLD_CD=%CD%
SET OLD_PATH=%PATH%
PATH %PATH%;%1;%2
DEL %WORK% /S/Q
MKDIR %WORK%
CD /D %WORK%
qmake ..
mingw32-make
release\ClipboardShare %3 %4 %5 %6 %7 %8 %9
PATH %OLD_PATH%
CD /D %OLD_CD%