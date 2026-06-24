@echo off
rem **************************************************************************
rem  Clean Script: M3DE Test
rem
rem  Description:
rem
rem    Removes all build artifacts (object files, executables, and logs).
rem
rem  Usage:
rem
rem    clean
rem
rem **************************************************************************

echo Cleaning build artifacts ...

if exist gfx13.obj del gfx13.obj
if exist m3de.obj  del m3de.obj
if exist test.obj  del test.obj
if exist test.exe  del test.exe
if exist build.log del build.log

echo Done.
