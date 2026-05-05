param(
    [switch]$SkipPathUpdate
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..")
$KaguyaDir = Join-Path $RepoRoot "kaguya"
$CargoBin = Join-Path $HOME ".cargo\bin"
$KaguyaExe = Join-Path $CargoBin "kaguya.exe"

if (-not (Test-Path (Join-Path $KaguyaDir "Cargo.toml"))) {
    throw "kaguya Cargo.toml was not found at $KaguyaDir"
}

Write-Host "Installing kaguya from $KaguyaDir"
cargo install --path $KaguyaDir --locked --force
if ($LASTEXITCODE -ne 0) {
    throw "cargo install failed with exit code $LASTEXITCODE. Close any running kaguya.exe process and retry."
}

Write-Host "Writing user-level kaguya config"
& $KaguyaExe --repo-root $RepoRoot --config (Join-Path $KaguyaDir "kaguya.toml") install-config
if ($LASTEXITCODE -ne 0) {
    throw "kaguya install-config failed with exit code $LASTEXITCODE"
}

if (-not $SkipPathUpdate) {
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $pathParts = @()
    if (-not [string]::IsNullOrWhiteSpace($userPath)) {
        $pathParts = $userPath -split ";" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    }

    $alreadyPresent = $false
    foreach ($part in $pathParts) {
        if ([string]::Equals(
            (Resolve-Path $part -ErrorAction SilentlyContinue),
            (Resolve-Path $CargoBin),
            [StringComparison]::OrdinalIgnoreCase
        )) {
            $alreadyPresent = $true
            break
        }
    }

    if (-not $alreadyPresent) {
        New-Item -ItemType Directory -Force -Path $CargoBin | Out-Null
        $newPath = if ([string]::IsNullOrWhiteSpace($userPath)) {
            $CargoBin
        } else {
            "$userPath;$CargoBin"
        }
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Write-Host "Added $CargoBin to the user PATH. Open a new terminal to inherit it."
    } else {
        Write-Host "$CargoBin is already on the user PATH."
    }
}

Write-Host "kaguya installed: $KaguyaExe"
Write-Host "Try: kaguya doctor"
