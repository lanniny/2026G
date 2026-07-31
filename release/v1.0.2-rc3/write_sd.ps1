[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z]:?$')]
    [string]$DriveLetter,

    [Parameter(Mandatory = $true)]
    [ValidateRange(0, 255)]
    [int]$ExpectedDiskIndex,

    [switch]$ConfirmWrite
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$deviceId = $DriveLetter.Substring(0, 1).ToUpperInvariant() + ":"
$root = $deviceId + "\"
$source = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "BOOT.BIN"
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "Missing source image: $source"
}

$drive = [IO.DriveInfo]::new($root)
if (-not $drive.IsReady) {
    throw "Drive is not ready: $root"
}
if ($drive.DriveType -ne [IO.DriveType]::Removable) {
    throw "Refusing non-removable drive $root (type: $($drive.DriveType))"
}
if ($drive.DriveFormat -notin @("FAT", "FAT32")) {
    throw "Zynq SD boot requires FAT/FAT32; $root is $($drive.DriveFormat)"
}

$logical = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$deviceId'"
if (-not $logical) {
    throw "Cannot resolve logical disk $deviceId through CIM"
}
$partitions = @(Get-CimAssociatedInstance -InputObject $logical -ResultClassName Win32_DiskPartition)
$disks = @(
    $partitions |
        ForEach-Object { Get-CimAssociatedInstance -InputObject $_ -ResultClassName Win32_DiskDrive } |
        Sort-Object Index -Unique
)
if ($disks.Count -ne 1) {
    throw "Expected one physical disk for $deviceId, found $($disks.Count)"
}
$disk = $disks[0]
if ([int]$disk.Index -ne $ExpectedDiskIndex) {
    throw "Disk index changed: expected $ExpectedDiskIndex, found $($disk.Index)"
}

$sourceInfo = Get-Item -LiteralPath $source
$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
if ($drive.AvailableFreeSpace -lt $sourceInfo.Length) {
    throw "Insufficient free space on $root"
}

$summary = [pscustomobject]@{
    Drive = $deviceId
    Label = $drive.VolumeLabel
    FileSystem = $drive.DriveFormat
    VolumeSizeGiB = [math]::Round($drive.TotalSize / 1GB, 2)
    DiskIndex = [int]$disk.Index
    Model = $disk.Model
    Serial = $disk.SerialNumber
    Interface = $disk.InterfaceType
    MediaType = $disk.MediaType
    SourceLength = $sourceInfo.Length
    SourceSHA256 = $sourceHash
}
$summary | Format-List

if (-not $ConfirmWrite) {
    Write-Output "READY_FOR_CONFIRMATION"
    return
}

$target = Join-Path $root "BOOT.BIN"
if (Test-Path -LiteralPath $target -PathType Leaf) {
    $targetHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $target).Hash
    if ($targetHash -eq $sourceHash) {
        Write-Output "ALREADY_CURRENT"
        return
    }

    $backupDir = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "sd_backups"
    [IO.Directory]::CreateDirectory($backupDir) | Out-Null
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $driveTag = $deviceId.TrimEnd(":")
    $backup = Join-Path $backupDir "disk$($disk.Index)-$driveTag-$stamp-BOOT.BIN"
    Copy-Item -LiteralPath $target -Destination $backup
    $backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backup).Hash
    if ($backupHash -ne $targetHash) {
        throw "Existing BOOT.BIN backup hash mismatch: $backup"
    }
}

$tempTarget = Join-Path $root "BOOT.CDX"
if (Test-Path -LiteralPath $tempTarget) {
    throw "Temporary target already exists; inspect it before retrying: $tempTarget"
}

$input = [IO.File]::OpenRead($source)
$output = [IO.FileStream]::new(
    $tempTarget,
    [IO.FileMode]::CreateNew,
    [IO.FileAccess]::Write,
    [IO.FileShare]::None,
    1MB,
    [IO.FileOptions]::WriteThrough
)
try {
    $input.CopyTo($output)
    $output.Flush($true)
} finally {
    $output.Dispose()
    $input.Dispose()
}

$tempHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $tempTarget).Hash
if ($tempHash -ne $sourceHash) {
    throw "SD temporary-file hash mismatch: $tempTarget"
}
if (Test-Path -LiteralPath $target) {
    [IO.File]::Delete($target)
}
[IO.File]::Move($tempTarget, $target)

$readbackHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $target).Hash
if ($readbackHash -ne $sourceHash) {
    throw "SD readback hash mismatch: $target"
}

Get-Item -LiteralPath $target | Select-Object FullName, Length, LastWriteTime
Write-Output "SD_WRITE_VERIFIED_SHA256=$readbackHash"
