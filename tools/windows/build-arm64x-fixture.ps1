# SPDX-License-Identifier: Apache-2.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$LockFile
)
$ErrorActionPreference = 'Stop'
function Fail([string]$Message) { throw "ARM64X fixture build: $Message" }
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
  & $Program @Arguments
  if ($LASTEXITCODE -ne 0) { Fail "$Program exited with $LASTEXITCODE" }
}
function Hash([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }

$sourcePath = [IO.Path]::GetFullPath($SourceRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
$buildPath = [IO.Path]::GetFullPath($BuildDir).TrimEnd([IO.Path]::DirectorySeparatorChar)
if ($buildPath.StartsWith($sourcePath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or $buildPath -eq $sourcePath) {
  Fail 'BuildDir must remain outside the source tree'
}
$lock = Get-Content -Raw -LiteralPath $LockFile | ConvertFrom-Json
if ($lock.status -ne 'reviewed' -or $null -eq $lock.pin) { Fail 'producer provenance lock is not reviewed' }

$fixtureSource = Join-Path $sourcePath 'tests\fixtures\arm64x_linked'
if (-not (Test-Path -LiteralPath (Join-Path $fixtureSource 'CMakeLists.txt'))) { Fail 'fixture source is absent' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -format json | ConvertFrom-Json | Select-Object -First 1)
if ($null -eq $installation) { Fail 'Visual Studio ARM64 tools are absent' }
$vcRoot = Join-Path $installation.installationPath 'VC\Tools\MSVC'
$vcVersion = (Get-ChildItem -LiteralPath $vcRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
if ($vcVersion -ne $lock.pin.vcToolsVersion) { Fail 'selected VC tools differ from the reviewed lock' }
$x64Cl = Join-Path $vcRoot "$vcVersion\bin\Hostarm64\x64\cl.exe"
if (-not (Test-Path -LiteralPath $x64Cl)) { Fail 'pinned x64 compiler is absent' }
if ((Hash $x64Cl) -ne $lock.pin.tools.x64.cl.sha256) { Fail 'x64 compiler hash differs from the reviewed lock' }

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
$arm64Build = Join-Path $buildPath 'arm64'
$arm64ecBuild = Join-Path $buildPath 'arm64ec'
$reproDir = Join-Path $buildPath 'repro'

Invoke-Checked cmake @('-S', $fixtureSource, '-B', $arm64Build, '-G', 'Visual Studio 17 2022', '-A', 'ARM64', '-DBUILD_AS_ARM64X=ARM64', "-DMSWR_ARM64_REPRO_DIR=$reproDir")
Invoke-Checked cmake @('--build', $arm64Build, '--config', 'Release', '--target', 'arm64x_fixture', '--', '/m:1')
$reproFile = Join-Path $reproDir 'arm64x_fixture.rsp'
if (-not (Test-Path -LiteralPath $reproFile)) { Fail 'ARM64 linker response file was not produced' }

Invoke-Checked cmake @('-S', $fixtureSource, '-B', $arm64ecBuild, '-G', 'Visual Studio 17 2022', '-A', 'ARM64EC', '-DBUILD_AS_ARM64X=ARM64EC', "-DMSWR_ARM64_REPRO_DIR=$reproDir", "-DMSWR_X64_CL=$x64Cl")
Invoke-Checked cmake @('--build', $arm64ecBuild, '--config', 'Release', '--target', 'arm64x_fixture', 'arm64x_fixture_host', '--', '/m:1')

$dll = Join-Path $arm64ecBuild 'Release\arm64x_fixture.dll'
$host = Join-Path $arm64ecBuild 'Release\arm64x_fixture_host.exe'
foreach ($output in @($dll, $host)) { if (-not (Test-Path -LiteralPath $output)) { Fail "missing output $output" } }
$manifest = [ordered]@{
  schemaVersion = 1
  producerLock = (Hash $LockFile)
  source = @{
    fixture = (Hash (Join-Path $fixtureSource 'fixture.c'))
    x64 = (Hash (Join-Path $fixtureSource 'fixture_x64.c'))
    host = (Hash (Join-Path $fixtureSource 'host.c'))
    definition = (Hash (Join-Path $fixtureSource 'fixture.def'))
  }
  outputs = @{
    dll = @{ path = $dll; sha256 = Hash $dll }
    host = @{ path = $host; sha256 = Hash $host }
  }
  distribution = 'build-tree-only'
}
$manifestPath = Join-Path $buildPath 'arm64x-fixture-build.manifest.json'
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 $manifestPath
Write-Output $manifestPath
