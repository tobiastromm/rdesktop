# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceRoot,

    [Parameter(Mandatory = $true)]
    [string]$BinDir,

    [Parameter(Mandatory = $true)]
    [string]$PackageDir,

    [Parameter(Mandatory = $true)]
    [string]$PackageName
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $BinDir -PathType Container)) {
    throw "Win32 bin directory does not exist: $BinDir"
}

$binRoot = (Resolve-Path -LiteralPath $BinDir).Path.TrimEnd('\', '/')
$sourceRootPath = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\', '/')
$sourceKeymaps = Join-Path $sourceRootPath 'keymaps'
$sourceLanguages = Join-Path $sourceRootPath 'resources\lang'
if (-not (Test-Path -LiteralPath $sourceKeymaps -PathType Container)) {
    throw "Upstream keymaps directory does not exist: $sourceKeymaps"
}

$binKeymaps = Join-Path $binRoot 'keymaps'
if (Test-Path -LiteralPath $binKeymaps) {
    Remove-Item -LiteralPath $binKeymaps -Recurse -Force
}
Copy-Item -LiteralPath $sourceKeymaps -Destination $binKeymaps -Recurse

$binLanguages = Join-Path $binRoot 'resources\lang'
if (Test-Path -LiteralPath $binLanguages) {
    Remove-Item -LiteralPath $binLanguages -Recurse -Force
}
New-Item -ItemType Directory -Path $binLanguages -Force | Out-Null
Copy-Item -Path (Join-Path $sourceLanguages '*.lang') -Destination $binLanguages

$files = @(Get-ChildItem -LiteralPath $binRoot -File -Recurse)
if ($files.Count -eq 0) {
    throw "No built runtime files found in $BinDir"
}
if (-not ($files | Where-Object { $_.Name -eq 'rdesktop.exe' })) {
    throw "rdesktop.exe has not been built; packaging cannot continue"
}
if (-not ($files | Where-Object { $_.Name -eq 'rdesktop-gui.exe' })) {
    throw "rdesktop-gui.exe has not been built; packaging cannot continue"
}

if (Test-Path -LiteralPath $PackageDir) {
    Get-ChildItem -LiteralPath $PackageDir -Force | Remove-Item -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $PackageDir | Out-Null
}

$zipPath = Join-Path $PackageDir $PackageName
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::Open($zipPath, 'Create')
try {
    foreach ($file in $files) {
        $entryName = $file.FullName.Substring($binRoot.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive,
            $file.FullName,
            $entryName,
            [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally {
    $archive.Dispose()
}
