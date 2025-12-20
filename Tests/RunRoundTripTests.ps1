param(
    [string]$UE4EditorCmd = "C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",
    [string]$ProjectFile = "$PSScriptRoot\..\..\..\..\A_MiniFootball.uproject"
)

$ErrorActionPreference = "Stop"

Write-Host "Starting MWCS Round-Trip Tests..." -ForegroundColor Cyan

# This script assumes that a Spec Provider is configured to load the test JSONs from P_MWCS/Tests/TestSpecs/
# or that they have been integrated into a specific test provider class.

# 1. Run ForceRecreate (Builds widgets from specs)
Write-Host "Running MWCS_CreateWidgets -Mode=ForceRecreate..." -ForegroundColor Yellow
# Note: Adjust arguments as needed for your specific environment
& $UE4EditorCmd "$ProjectFile" -run=MWCS_CreateWidgets -Mode=ForceRecreate -unattended -nopause -headless -log
if ($LASTEXITCODE -ne 0) { 
    Write-Error "MWCS_CreateWidgets failed with code $LASTEXITCODE" 
    exit 1 
}

# 2. Run Validation
Write-Host "Running MWCS_ValidateWidgets..." -ForegroundColor Yellow
& $UE4EditorCmd "$ProjectFile" -run=MWCS_ValidateWidgets -unattended -nopause -headless -log
if ($LASTEXITCODE -ne 0) { 
    Write-Error "MWCS_ValidateWidgets failed with code $LASTEXITCODE" 
    exit 1 
}

Write-Host "All tests passed successfully!" -ForegroundColor Green
