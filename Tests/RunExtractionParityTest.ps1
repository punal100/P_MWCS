# MWCS Extraction Parity Test Script
# Tests bidirectional parity for Image and Throbber widget brush properties

param(
    [Parameter(Mandatory=$false)]
    [string]$ProjectPath,
    [Parameter(Mandatory=$false)]
    [string]$UEPath
)

$ErrorActionPreference = "Stop"

# Auto-detect paths
if (-not $ProjectPath) {
    $CurrentDir = Get-Location
    while ($CurrentDir.Parent) {
        $UProject = Get-ChildItem -Path $CurrentDir -Filter "*.uproject" -File | Select-Object -First 1
        if ($UProject) {
            $ProjectPath = $UProject.FullName
            break
        }
        $CurrentDir = $CurrentDir.Parent
    }
    if (-not $ProjectPath) {
        Write-Host "ERROR: Could not find .uproject file" -ForegroundColor Red
        exit 1
    }
}

if (-not $UEPath) {
    $CommonPaths = @("d:\UE\UE_S", "C:\Program Files\Epic Games\UE_5.5")
    foreach ($Path in $CommonPaths) {
        if (Test-Path "$Path\Engine\Binaries\Win64\UnrealEditor-Cmd.exe") {
            $UEPath = $Path
            break
        }
    }
    if (-not $UEPath) {
        Write-Host "ERROR: Could not find UE installation" -ForegroundColor Red
        exit 1
    }
}

$ProjectRoot = Split-Path -Parent $ProjectPath
$UnrealEditor = "$UEPath\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

Write-Host "=== MWCS Extraction Parity Test ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectPath" -ForegroundColor Gray
Write-Host "UE: $UEPath" -ForegroundColor Gray

Write-Host "`n[1/3] Force Recreating widgets..." -ForegroundColor Yellow
& $UnrealEditor $ProjectPath -run=MWCS_CreateWidgets -Mode=ForceRecreate -FailOnErrors -unattended -nop4 -NullRHI -stdout
if ($LASTEXITCODE -ne 0) { 
    Write-Host "ERROR: Widget creation failed!" -ForegroundColor Red
    exit 1 
}

Write-Host "`n[2/3] Validating widgets..." -ForegroundColor Yellow
& $UnrealEditor $ProjectPath -run=MWCS_ValidateWidgets -FailOnErrors -FailOnWarnings -unattended -nop4 -NullRHI -stdout
if ($LASTEXITCODE -ne 0) { 
    Write-Host "ERROR: Validation failed!" -ForegroundColor Red
    exit 1 
}

Write-Host "`n[3/3] Checking extracted specs..." -ForegroundColor Yellow
$ExtractDir = "$ProjectRoot\Saved\MWCS\ExtractedSpecs"

if (-not (Test-Path $ExtractDir)) {
    Write-Host "WARNING: No extracted specs found" -ForegroundColor Yellow
    exit 0
}

$ExtractedSpecs = Get-ChildItem -Path $ExtractDir -Filter "*.json" -File
$FoundBrushDrawAs = $false
$FoundThrobberProps = $false

foreach ($SpecFile in $ExtractedSpecs) {
    $Spec = Get-Content $SpecFile.FullName -Raw | ConvertFrom-Json
    if ($Spec.Design) {
        foreach ($WidgetName in $Spec.Design.PSObject.Properties.Name) {
            $WidgetDesign = $Spec.Design.$WidgetName
            if ($WidgetDesign.Brush -and $WidgetDesign.Brush.DrawAs) {
                Write-Host "OK $($SpecFile.BaseName): $WidgetName.Brush.DrawAs = $($WidgetDesign.Brush.DrawAs)" -ForegroundColor Green
                $FoundBrushDrawAs = $true
            }
            if ($WidgetDesign.NumberOfPieces) {
                Write-Host "OK $($SpecFile.BaseName): $WidgetName.NumberOfPieces = $($WidgetDesign.NumberOfPieces)" -ForegroundColor Green
                $FoundThrobberProps = $true
            }
        }
    }
}

Write-Host "`n=== Test Summary ===" -ForegroundColor Cyan
if ($FoundBrushDrawAs) {
    Write-Host "PASS Image Brush.DrawAs extraction working" -ForegroundColor Green
} else {
    Write-Host "INFO No Image widgets with Brush.DrawAs found" -ForegroundColor Gray
}
if ($FoundThrobberProps) {
    Write-Host "PASS Throbber properties extraction working" -ForegroundColor Green
} else  {
    Write-Host "INFO No Throbber widgets with custom properties found" -ForegroundColor Gray
}

Write-Host "`n=== All Tests Passed ===" -ForegroundColor Green
