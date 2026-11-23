#Requires -Version 5.1
<#
.SYNOPSIS
Checks a Windows NeneEngine release ZIP and runs its smoke test.

.EXAMPLE
.\tools\Test-ReleasePackage.ps1 -PackagePath .\out\package\NeneEngine-1.1.1-win64.zip
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateNotNullOrEmpty()]
    [string]$PackagePath,

    [ValidateRange(1, 3600)]
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'

$archive = Get-Item -LiteralPath $PackagePath
if ($archive -isnot [System.IO.FileInfo] -or $archive.Extension -ne '.zip') {
    throw "Expected a ZIP file at '$PackagePath'."
}

$tempRoot = (Resolve-Path -LiteralPath ([System.IO.Path]::GetTempPath())).ProviderPath
$tempDirectory = Join-Path $tempRoot ('ne-' + [guid]::NewGuid().ToString('N').Substring(0, 16))
$null = New-Item -ItemType Directory -Path $tempDirectory
$engine = $null
try {
    Expand-Archive -LiteralPath $archive.FullName -DestinationPath $tempDirectory

    $executables = @(Get-ChildItem -LiteralPath $tempDirectory -Filter 'NeneEngine.exe' -File -Recurse)
    if ($executables.Count -ne 1) {
        throw "Expected one NeneEngine.exe in the ZIP, found $($executables.Count)."
    }

    $executable = $executables[0]
    $packageRoot = $executable.DirectoryName
    $requiredFiles = @(
        'README.txt'
        'LICENSE.txt'
        'THIRD_PARTY_NOTICES.txt'
        'assets/config/engine.json'
        'assets/scenes/demo_scene.json'
        'assets/engine/shaders/textured_mesh.shader'
        'assets/scripts/spin_cube.lua'
    )
    $missingFiles = @($requiredFiles | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $packageRoot $_) -PathType Leaf)
    })
    if ($missingFiles.Count -gt 0) {
        throw "Package is missing required files: $($missingFiles -join ', ')."
    }

    $localAppData = Join-Path $tempDirectory 'profile'
    $null = New-Item -ItemType Directory -Path $localAppData
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $executable.FullName
    $startInfo.WorkingDirectory = $packageRoot
    $startInfo.Arguments = '--smoke-test'
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = 'Hidden'
    $startInfo.RedirectStandardError = $true
    $startInfo.EnvironmentVariables['LOCALAPPDATA'] = $localAppData
    $engine = [System.Diagnostics.Process]::Start($startInfo)
    $stderr = $engine.StandardError.ReadToEndAsync()

    if (-not $engine.WaitForExit($TimeoutSeconds * 1000)) {
        if (-not $engine.HasExited) {
            $engine.Kill()
            $engine.WaitForExit()
        }
        throw "NeneEngine smoke test timed out after $TimeoutSeconds seconds."
    }
    if ($engine.ExitCode -ne 0) {
        $details = $stderr.GetAwaiter().GetResult().Trim()
        throw "NeneEngine smoke test failed with exit code $($engine.ExitCode).`n$details"
    }

    Write-Output "Package smoke test passed: $($archive.Name)"
}
finally {
    if ($null -ne $engine) {
        $engine.Dispose()
    }

    $cleanupPath = (Resolve-Path -LiteralPath $tempDirectory).ProviderPath
    if ($cleanupPath -eq [System.IO.Path]::GetFullPath($tempDirectory)) {
        Remove-Item -LiteralPath $cleanupPath -Recurse -Force
    }
}
