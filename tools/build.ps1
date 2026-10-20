#requires -Version 5.1
<#
Usage:
  build.bat [x64|x86|ARM64EC] [lite|full] [Release|Debug] [-NoUpdate] [-WarningsAsErrors]
Default: x64 lite Release. Run on Windows 10/11 with internet access.
An elevated UAC prompt may appear to install/update VS 2022 Build Tools.
#>
param(
    [string]$Platform = "x64",
    [string]$Flavor = "lite",
    [string]$Configuration = "Release",
    [switch]$NoUpdate,
    [switch]$WarningsAsErrors
)
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
function Fail([string]$Message) { throw $Message }
try {
    if ($env:OS -ne "Windows_NT") { Fail "This build requires Windows and the Microsoft MFC toolchain." }
    if ($Platform -notin @("x64", "x86", "ARM64EC")) { Fail "Unknown architecture '$Platform'. Use x64, x86 or ARM64EC." }
    if ($Flavor -notin @("lite", "full")) { Fail "Unknown flavor '$Flavor'. Use lite or full." }
    if ($Configuration -notin @("Release", "Debug")) { Fail "Unknown configuration '$Configuration'." }
    $base = Split-Path -Parent $PSScriptRoot
    $solution = if ($Flavor -eq "lite") { Join-Path $base "TrafficMonitor_Lite.sln" } else { Join-Path $base "TrafficMonitor.sln" }
    $configurationName = if ($Flavor -eq "lite") { "$Configuration (lite)" } else { $Configuration }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $installer = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\setup.exe"
    $requirements = @(
        "Microsoft.VisualStudio.Workload.VCTools",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "Microsoft.VisualStudio.Component.VC.ATLMFC",
        "Microsoft.VisualStudio.Component.Windows11SDK.26100"
    )
    if ($Platform -eq "ARM64EC") {
        $requirements += @(
            "Microsoft.VisualStudio.Component.VC.Tools.ARM64EC",
            "Microsoft.VisualStudio.Component.VC.MFC.ARM64"
        )
    }
    if ($Flavor -eq "full") {
        # OpenHardwareMonitorApi.vcxproj is a C++/CLI .NET Framework 4.7.2 DLL.
        $requirements += @(
            "Microsoft.VisualStudio.Component.VC.CLI.Support",
            "Microsoft.Net.Component.4.7.2.TargetingPack",
            "Microsoft.Net.Component.4.7.2.SDK"
        )
    }
    function FindVS([bool]$RequireAll) {
        if (!(Test-Path $vswhere)) { return $null }
        $args = @("-latest", "-products", "*", "-version", "[17.0,18.0)", "-property", "installationPath")
        if ($RequireAll) { $args += @("-requires"); $args += $requirements }
        $path = & $vswhere @args 2>$null | Select-Object -First 1
        if ($path -and (Test-Path $path)) { return $path }
        return $null
    }
    $vsPath = FindVS $false
    if (!$vsPath) {
        Write-Host "[Setup] Visual Studio 2022 with C++ MFC tools not found; installing Build Tools..."
        $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
        if ($winget) {
            $override = "--passive --wait --norestart " + (($requirements | ForEach-Object { "--add $_" }) -join " ")
            & winget.exe install --id Microsoft.VisualStudio.2022.BuildTools -e --source winget --accept-package-agreements --accept-source-agreements --override $override
            if ($LASTEXITCODE -ne 0) { Fail "winget installation failed ($LASTEXITCODE)." }
        } else {
            # Official Microsoft bootstrapper, downloaded only if winget is unavailable.
            $bootstrapper = Join-Path $env:TEMP "TrafficMonitor-vs_BuildTools.exe"
            Invoke-WebRequest "https://aka.ms/vs/17/release/vs_BuildTools.exe" -OutFile $bootstrapper -UseBasicParsing
            $arguments = @("--passive", "--wait", "--norestart")
            foreach ($component in $requirements) { $arguments += @("--add", $component) }
            $process = Start-Process $bootstrapper -ArgumentList $arguments -Wait -PassThru
            if ($process.ExitCode -notin @(0, 3010)) { Fail "Visual Studio bootstrapper failed: $($process.ExitCode)" }
        }
        $vsPath = FindVS $false
        if (!$vsPath) { Fail "VS 2022 installation not detected. A reboot may be necessary." }
    } elseif (!$NoUpdate) {
        # Query winget for updates rather than downloading every build.
        $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
        if ($winget) {
            $raw = & $vswhere -latest -products "*" -version "[17.0,18.0)" -format json | ConvertFrom-Json
            $productId = @($raw)[0].productId
            $package = switch ($productId) {
                "Microsoft.VisualStudio.Product.BuildTools" { "Microsoft.VisualStudio.2022.BuildTools"; break }
                "Microsoft.VisualStudio.Product.Community" { "Microsoft.VisualStudio.2022.Community"; break }
                "Microsoft.VisualStudio.Product.Professional" { "Microsoft.VisualStudio.2022.Professional"; break }
                "Microsoft.VisualStudio.Product.Enterprise" { "Microsoft.VisualStudio.2022.Enterprise"; break }
                default { $null }
            }
            if ($package) {
                Write-Host "[Setup] Checking VS 2022 updates: $package"
                & winget.exe upgrade --id $package -e --source winget --accept-package-agreements --accept-source-agreements --silent
                # winget returns nonzero when no upgrade is found on some versions.
                if ($LASTEXITCODE -ne 0) { Write-Warning "Update unavailable or not managed by winget; continuing with installed VS." }
            }
        } else {
            Write-Warning "winget unavailable: cannot check existing Visual Studio for newer releases."
        }
    }

    # Adding missing workloads/components to an existing VS edition is necessary:
    # MSBuild alone does not imply the MFC headers and libraries are installed.
    if (!(FindVS $true)) {
        if (!(Test-Path $installer)) { Fail "VS Installer not found: $installer" }
        Write-Host "[Setup] Installing missing C++ / MFC / SDK components in $vsPath"
        $args = @("modify", "--installPath", ('"' + $vsPath + '"'), "--passive", "--norestart")
        foreach ($component in $requirements) { $args += @("--add", $component) }
        $proc = Start-Process $installer -ArgumentList $args -Wait -PassThru
        if ($proc.ExitCode -notin @(0, 3010)) { Fail "Component installation failed: $($proc.ExitCode)" }
        if (!(FindVS $true)) { Fail "Required MSVC/MFC components not detected after install. A reboot may be needed." }
    }
    $msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
    if (!(Test-Path $msbuild)) { Fail "MSBuild.exe missing from $vsPath" }

    Write-Host "[Build] $configurationName / $Platform"
    $logsDir = Join-Path $base "build-logs"
    New-Item -Path $logsDir -ItemType Directory -Force | Out-Null
    $binlog = Join-Path $logsDir ("TrafficMonitor-$Platform-$Flavor-$Configuration.binlog")
    $buildArgs = @($solution, "/t:Build", "/m", "/v:minimal",
        "/p:Configuration=$configurationName", "/p:Platform=$Platform",
        "/p:PlatformToolset=v143", "/bl:$binlog")
    if ($WarningsAsErrors) {
        Write-Host "[Build] Strict mode: treating warnings as errors."
        $buildArgs += "/warnaserror"
    }
    & $msbuild @buildArgs
    Write-Host "[Build] Diagnostic log: $binlog"
    if ($LASTEXITCODE -ne 0) { Fail "MSBuild failed with exit code $LASTEXITCODE." }
    $out = if ($Platform -eq "x86") {
        Join-Path $base "Bin\$configurationName"
    } else {
        Join-Path $base "Bin\$Platform\$configurationName"
    }
    Write-Host "[OK] Build succeeded. Output: $out"
    exit 0
} catch {
    Write-Host "[ERROR] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
