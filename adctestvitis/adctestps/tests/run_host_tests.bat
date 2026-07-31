@echo off
setlocal
cd /d "%~dp0"

set "SRC=..\src"
set "PL_SAMPLES=..\..\..\adc_easy_test\sim_4msps\pl_samples.txt"
set "GCC=F:\AMDDesignTools\2025.2\Vivado\tps\mingw\6.2.0\win64.o\nt\bin\gcc.exe"

"%GCC%" -std=c11 -O2 -Wall -Wextra -I "%SRC%" ^
  host_signal_analysis_test.c ^
  "%SRC%\signal_analysis.c" "%SRC%\calibration.c" ^
  -lm -o host_signal_analysis_test.exe
if errorlevel 1 exit /b 1

host_signal_analysis_test.exe "%PL_SAMPLES%"
if errorlevel 1 exit /b 1

"%GCC%" -std=c11 -O2 -Wall -Wextra -I "%SRC%" ^
  host_requirement_sweep.c ^
  "%SRC%\signal_analysis.c" "%SRC%\calibration.c" ^
  -lm -o host_requirement_sweep.exe
if errorlevel 1 exit /b 1

host_requirement_sweep.exe
if errorlevel 1 exit /b 1

"%GCC%" -std=c11 -O2 -Wall -Wextra -I "%SRC%" ^
  host_interference_test.c ^
  "%SRC%\signal_analysis.c" "%SRC%\calibration.c" ^
  -lm -o host_interference_test.exe
if errorlevel 1 exit /b 1

host_interference_test.exe
if errorlevel 1 exit /b 1

endlocal
