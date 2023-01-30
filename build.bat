@echo off

set CommonFlags=-Z7 -FeDx12_Renderer.exe -EHsc -D_DEBUG

set OptimizationLevel=-Od

if "%1"=="Release" ( 
	set OptimizationLevel=-O2
	echo Building Release.
) else (
	set OptimizationLevel=-Od
	echo Building Debug.
)

cl.exe -nologo dx_window.cpp %OptimizationLevel% %CommonFlags%