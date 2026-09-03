@echo off
setlocal
rem Build the FlexVoice discovery probe as a 32-bit console app.
rem The 2002 SDK needs C++14 (std::auto_ptr in EngineFactory::createEngine) and RTTI.

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 (echo vcvars32 failed & exit /b 1)

set ROOT=%~dp0..
set SDK=%ROOT%\bin\mmfvapi
set OUT=%ROOT%\build_probe

if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /EHsc /GR /MT /W3 /O2 /std:c++14 ^
   /D_CRT_SECURE_NO_WARNINGS /D_HAS_AUTO_PTR_ETC=1 /DWIN32_LEAN_AND_MEAN /DNOMINMAX ^
   /I"%SDK%\include" ^
   /Fo"%OUT%\\" /Fe"%OUT%\fv_render.exe" ^
   "%~dp0fv_render.cpp" ^
   /link /LIBPATH:"%SDK%\lib" FlexVoice_3_01_001.lib

if errorlevel 1 (echo BUILD FAILED & exit /b 1)
echo BUILD OK: %OUT%\fv_render.exe
endlocal
