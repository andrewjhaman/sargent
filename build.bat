@echo off

set CommonFlags=-Z7 -FeDx12_Renderer.exe -EHsc Winmm.lib

set OptimizationLevel=-Od

set IS_RELEASE=false
if "%1"=="Release" set IS_RELEASE=true
if "%1"=="-Release" set IS_RELEASE=true
if "%1"=="release" set IS_RELEASE=true
if "%1"=="-release" set IS_RELEASE=true

if %IS_RELEASE%==true ( 
	set OptimizationLevel=-O2 -GL -GA
	echo Building Release.
) else (
	set OptimizationLevel=-Od -D_DEBUG
	echo Building Debug.
)

cl.exe -nologo dx_window.cpp %OptimizationLevel% %CommonFlags%