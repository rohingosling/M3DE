@echo off
rem **************************************************************************
rem  Build Script: M3DE Test (GFX-13 v1)
rem  Assembler:    Borland Turbo Assembler (TASM)
rem  Compiler:     Borland Turbo C++ 3.1
rem  Environment:  DOS / DOSBox
rem
rem  Description:
rem
rem    Assembles GFX-13 v1 (pure assembly via TASM), compiles M3DE (C++) and
rem    the test program (C++), then links them into TEST.EXE.
rem
rem    Uses the large memory model (-ml) for far pointer and farmalloc
rem    support. Floating point emulation is included automatically.
rem
rem    All assembler, compiler, and linker output is logged to BUILD.LOG.
rem
rem  Usage:
rem
rem    buildv1          Build TEST.EXE
rem    buildv1 clean    Delete intermediate and output files
rem
rem **************************************************************************

if "%1"=="clean" goto clean

echo Build started > build.log

rem --------------------------------------------------------------------------
rem  Assemble GFX-13 v1
rem --------------------------------------------------------------------------

echo Assembling GFX13.ASM ...
echo ---- GFX13.ASM ---- >> build.log
tasm /ml ..\GFX13\v1\gfx13.asm >> build.log
if errorlevel 1 goto fail
copy ..\GFX13\v1\gfx13.obj . > nul

rem --------------------------------------------------------------------------
rem  Compile
rem --------------------------------------------------------------------------

echo Compiling M3DE.CPP ...
echo ---- M3DE.CPP ---- >> build.log
bcc -c -ml -I..\GFX13\v1 -I..\M3DE ..\M3DE\m3de.cpp >> build.log
if errorlevel 1 goto fail

echo Compiling TEST.CPP ...
echo ---- TEST.CPP ---- >> build.log
bcc -c -ml -I..\GFX13\v1 -I..\M3DE test.cpp >> build.log
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
if exist gfx13.obj            del gfx13.obj
if exist ..\GFX13\v1\gfx13.obj del ..\GFX13\v1\gfx13.obj
if exist m3de.obj              del m3de.obj
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
