[CmdletBinding()]
param(
    [string]$Script = "run_dual_uart_target.tcl",
    [string]$VivadoRoot = "F:/AMDDesignTools/2025.2/Vivado"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$runtimeTemp = Join-Path $projectRoot "tmp/xsdb-runtime"
$xsdb = Join-Path $VivadoRoot "bin/unwrapped/win64.o/xsdb.exe"
$tclLibrary = Join-Path $VivadoRoot "tps/tcl/tcl8.6"
$runtimeLibrary = Join-Path $VivadoRoot "lib/win64.o"

foreach ($required in @($xsdb, (Join-Path $tclLibrary "init.tcl"), $runtimeLibrary)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing XSDB runtime component: $required"
    }
}

if (-not [IO.Path]::IsPathRooted($Script)) {
    $Script = Join-Path $projectRoot $Script
}
$Script = (Resolve-Path -LiteralPath $Script).Path
[IO.Directory]::CreateDirectory($runtimeTemp) | Out-Null

$env:PATH = "$runtimeLibrary;$(Join-Path $VivadoRoot 'bin');$env:PATH"
$env:TCL_LIBRARY = $tclLibrary
$tclPackagePaths = @(
    (Join-Path $VivadoRoot "scripts/xsdb/xsdb")
    (Join-Path $VivadoRoot "scripts/xsdb/tcf")
    (Join-Path $VivadoRoot "scripts/xsdb/hsi")
    (Join-Path $VivadoRoot "tps/tcl/tcllib2.0")
    (Join-Path $VivadoRoot "tps/tcl/tdom")
)
$env:TCLLIBPATH = ($tclPackagePaths | ForEach-Object { $_ -replace '\\', '/' }) -join " "
$env:XILINX_VIVADO = $VivadoRoot
$env:TEMP = $runtimeTemp
$env:TMP = $runtimeTemp

& $xsdb $Script
$xsdbExit = $LASTEXITCODE
if ($xsdbExit -ne 0) {
    throw "XSDB failed with exit code $xsdbExit"
}
