[CmdletBinding()]
param(
    [string]$VivadoRoot = "F:/AMDDesignTools/2025.2/Vivado",
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function ConvertTo-ToolPath([string]$Path) {
    return $Path -replace '\\', '/'
}

$bootRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$bif = Join-Path $bootRoot "boot.bif"
$output = Join-Path $bootRoot "BOOT.BIN"
$bootgen = Join-Path $VivadoRoot "bin/unwrapped/win64.o/bootgen.exe"
$productRoot = Split-Path -Parent $VivadoRoot
$toolRoot = Split-Path -Parent $productRoot
$jre = Join-Path $VivadoRoot "tps/win64/jre21.0.5_11"
$javaFx = Join-Path $VivadoRoot "tps/win64/javafx-sdk-21.0.5"
$javaCef = Join-Path $VivadoRoot "tps/win64/java-cef-95.0.4638.69"
$python = Join-Path $VivadoRoot "tps/win64/python-3.13.0"
$tclLibrary = Join-Path $VivadoRoot "tps/tcl/tcl8.6"

foreach ($required in @($bootgen, $bif, (Join-Path $jre "bin/java.exe"), (Join-Path $tclLibrary "init.tcl"))) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required bootgen component: $required"
    }
}
if ((Test-Path -LiteralPath $output) -and -not $Force) {
    throw "BOOT.BIN already exists; pass -Force to replace it: $output"
}

$env:XILINX_VIVADO = ConvertTo-ToolPath $VivadoRoot
$env:XILINX_PLANAHEAD = ConvertTo-ToolPath $VivadoRoot
$env:XILINX_LOCAL_USER_DATA = "no"
$env:RDI_BINROOT = ConvertTo-ToolPath (Join-Path $VivadoRoot "bin")
$env:RDI_APPROOT = ConvertTo-ToolPath $VivadoRoot
$env:HDI_APPROOT = ConvertTo-ToolPath $VivadoRoot
$env:RDI_BASEROOT = ConvertTo-ToolPath $productRoot
$env:RDI_INSTALLROOT = ConvertTo-ToolPath $toolRoot
$env:RDI_INSTALLVER = Split-Path -Leaf $VivadoRoot
$env:RDI_INSTALLVERSION = Split-Path -Leaf $productRoot
$env:RDI_SHARED_DATA = ConvertTo-ToolPath (Join-Path $toolRoot "SharedData/$($env:RDI_INSTALLVERSION)/data")
$env:RDI_PLATFORM = "win64"
$env:RDI_OPT_EXT = ".o"
$env:RDI_DATADIR = ConvertTo-ToolPath (Join-Path $VivadoRoot "data")
$env:RDI_LIBDIR = ConvertTo-ToolPath (Join-Path $VivadoRoot "lib/win64.o")
$env:RDI_BINDIR = ConvertTo-ToolPath (Join-Path $VivadoRoot "bin")
$env:RDI_JAVAROOT = ConvertTo-ToolPath $jre
$env:RDI_JAVAFXROOT = ConvertTo-ToolPath $javaFx
$env:RDI_JAVACEFROOT = ConvertTo-ToolPath $javaCef
$env:TCL_LIBRARY = ConvertTo-ToolPath $tclLibrary
$env:ISL_IOSTREAMS_RSA = ConvertTo-ToolPath (Join-Path $VivadoRoot "tps/isl")
$env:RDI_BUILD = "yes"
$env:RDI_MINGW_LIB = "$(ConvertTo-ToolPath (Join-Path $VivadoRoot 'tps/mingw/6.2.0/win64.o/nt/bin'));$(ConvertTo-ToolPath (Join-Path $VivadoRoot 'tps/mingw/6.2.0/win64.o/nt/libexec/gcc/x86_64-w64-mingw32/6.2.0'))"
$env:RDI_PYTHONHOME = ConvertTo-ToolPath $python
$env:RDI_PYTHONPATH = "$(ConvertTo-ToolPath $python);$(ConvertTo-ToolPath (Join-Path $python 'bin'));$(ConvertTo-ToolPath (Join-Path $python 'lib'));$(ConvertTo-ToolPath (Join-Path $python 'lib/site-packages'))"
$env:PYTHON = ConvertTo-ToolPath $python
$env:PYTHONHOME = ConvertTo-ToolPath $python
$env:PYTHONPATH = "$($env:RDI_PYTHONPATH);$($env:RDI_LIBDIR);$env:PYTHONPATH"
$env:RT_LIBPATH = ConvertTo-ToolPath (Join-Path $VivadoRoot "scripts/rt/data")
$env:RT_TCL_PATH = ConvertTo-ToolPath (Join-Path $VivadoRoot "scripts/rt/base_tcl/tcl")

$runtimePaths = @(
    $env:RDI_PYTHONPATH,
    (Join-Path $VivadoRoot "lib/win64.o"),
    (Join-Path $VivadoRoot "bin/unwrapped/win64.o"),
    (Join-Path $jre "bin/server"),
    (Join-Path $jre "bin"),
    (Join-Path $javaFx "lib"),
    (Join-Path $javaFx "bin"),
    (Join-Path $javaCef "bin/lib/win64"),
    (Join-Path $VivadoRoot "tps/win64"),
    (Join-Path $VivadoRoot "bin"),
    (Join-Path $VivadoRoot "gnu/microblaze/nt/bin"),
    (Join-Path $VivadoRoot "gnuwin/bin"),
    $env:RDI_MINGW_LIB
)
$env:PATH = ($runtimePaths -join ";") + ";$env:PATH"

Push-Location $bootRoot
try {
    & $bootgen -arch zynq -image $bif -w on -o $output
    $bootgenExit = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($bootgenExit -ne 0) {
    throw "bootgen failed with exit code $bootgenExit"
}
if (-not (Test-Path -LiteralPath $output)) {
    throw "bootgen returned success without creating $output"
}

Get-Item -LiteralPath $output
Get-FileHash -Algorithm SHA256 -LiteralPath $output
