@echo off
setlocal

set "VIVADO_ROOT=F:\AMDDesignTools\2025.2\Vivado"
set "PRODUCT_ROOT=F:\AMDDesignTools\2025.2"
set "TOOL_ROOT=F:\AMDDesignTools"
set "VIVADO_BIN=%VIVADO_ROOT%\bin\unwrapped\win64.o"
set "VIVADO_JRE=%VIVADO_ROOT%\tps\win64\jre21.0.5_11"
set "VIVADO_PYTHON=%VIVADO_ROOT%\tps\win64\python-3.13.0"
set "RTL=..\adc_easy_test.srcs\sources_1\new\ad9226test.v"
set "GLBL=%VIVADO_ROOT%\data\verilog\src\glbl.v"
set "RDI_DATADIR=%VIVADO_ROOT%\data"
set "RDI_LIBDIR=%VIVADO_ROOT%\lib\win64.o"
set "RDI_JAVAROOT=%VIVADO_JRE%"
set "RDI_BINROOT=%VIVADO_ROOT%\bin"
set "RDI_BINDIR=%VIVADO_ROOT%\bin"
set "RDI_APPROOT=%VIVADO_ROOT%"
set "HDI_APPROOT=%VIVADO_ROOT%"
set "RDI_BASEROOT=%PRODUCT_ROOT%"
set "RDI_INSTALLROOT=%TOOL_ROOT%"
set "RDI_INSTALLVER=Vivado"
set "RDI_INSTALLVERSION=2025.2"
set "RDI_SHARED_DATA=%TOOL_ROOT%\SharedData\2025.2\data"
set "RDI_PLATFORM=win64"
set "RDI_OPT_EXT=.o"
set "RDI_BUILD=yes"
set "ISL_IOSTREAMS_RSA=%VIVADO_ROOT%\tps\isl"
set "RT_LIBPATH=%VIVADO_ROOT%\scripts\rt\data"
set "RT_TCL_PATH=%VIVADO_ROOT%\scripts\rt\base_tcl\tcl"
set "PYTHONHOME=%VIVADO_PYTHON%"
set "TCL_LIBRARY=%VIVADO_ROOT%\tps\tcl\tcl8.6"
if not defined PROCESSOR_ARCHITECTURE set "PROCESSOR_ARCHITECTURE=AMD64"

call "%VIVADO_ROOT%\settings64.bat"
if errorlevel 1 exit /b 1
set "PATH=%VIVADO_ROOT%\lib\win64.o;%VIVADO_BIN%;%VIVADO_JRE%\bin\server;%VIVADO_JRE%\bin;%VIVADO_PYTHON%;%VIVADO_ROOT%\tps\win64;%PATH%"

"%VIVADO_BIN%\xvlog.exe" -sv ad9226_sim_models.sv tb_ad9226_4msps.sv "%RTL%" "%GLBL%"
if errorlevel 1 exit /b 1

"%VIVADO_BIN%\xelab.exe" -L xpm tb_ad9226_4msps glbl -s tb_ad9226_4msps_sim
if errorlevel 1 exit /b 1

"%VIVADO_BIN%\xsim.exe" tb_ad9226_4msps_sim -runall > xsim_run.log 2>&1
set "XSIM_EXIT=%ERRORLEVEL%"
type xsim_run.log
if not "%XSIM_EXIT%"=="0" exit /b %XSIM_EXIT%
findstr /c:"PL_SIM_PASS samples=8192" xsim_run.log >nul
if errorlevel 1 exit /b 1

endlocal
