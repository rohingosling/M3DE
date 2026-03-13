@echo off
rem **************************************************************************
rem  Build Script: M3DE Test
rem  Compiler:     Borland Turbo C++ 3.1
rem  Environment:  DOS / DOSBox
rem
rem  Description:
rem
rem    Compiles GFX-13 (C with inline assembly), M3DE (C++), and the test
rem    program (C++), then links them into TEST.EXE.
rem
rem    Uses the large memory model (-ml) for far pointer and farmalloc
rem    support. Floating point emulation is included automatically.
rem
rem    All compiler and linker output is logged to BUILD.LOG.
rem
rem  Usage:
rem
rem    build          Build TEST.EXE
rem    build clean    Delete intermediate and output files
rem
rem **************************************************************************

if "%1"=="clean" goto clean

echo Build started > build.log

rem --------------------------------------------------------------------------
rem  Compile
rem --------------------------------------------------------------------------

echo Compiling GFX13.C ...
echo ---- GFX13.C ---- >> build.log
bcc -c -ml -2 gfx13.c >> build.log
if errorlevel 1 goto fail

echo Compiling M3DE.CPP ...
echo ---- M3DE.CPP ---- >> build.log
bcc -c -ml m3de.cpp >> build.log
if errorlevel 1 goto fail

echo Compiling TEST.CPP ...
echo ---- TEST.CPP ---- >> build.log
bcc -c -ml test.cpp >> build.log
if errorlevel 1 goto fail

rem --------------------------------------------------------------------------
rem  Link
rem --------------------------------------------------------------------------

echo Linking TEST.EXE ...
echo ---- LINK TEST.EXE ---- >> build.log
bcc -ml -etest.exe gfx13.obj m3de.obj test.obj >> build.log
if errorlevel 1 goto fail

echo.
echo Build successful: TEST.EXE
echo Build successful >> build.log
goto end

rem --------------------------------------------------------------------------
rem  Clean
rem --------------------------------------------------------------------------

:clean
echo Cleaning ...
if exist gfx13.obj del gfx13.obj
if exist m3de.obj  del m3de.obj
if exist test.obj  del test.obj
if exist test.exe  del test.exe
if exist build.log del build.log
echo Done.
goto end

rem --------------------------------------------------------------------------
rem  Error
rem --------------------------------------------------------------------------

:fail
echo.
echo Build failed! See BUILD.LOG for details.
echo Build failed >> build.log

:end
