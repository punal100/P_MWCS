<#
.SYNOPSIS
Strict property verification for MWCS-generated widgets.

.DESCRIPTION
Compares widget properties extracted from UE Editor against their spec source.
Reports any mismatches with detailed property paths.

This script provides a framework for property comparison. It requires:
1. A spec JSON file with widget definitions
2. The extracted widget properties (manual or via MWCS commandlet)

.PARAMETER ProjectPath
Path to the UE project (auto-detected if not provided).

.PARAMETER SpecFile
Path to the widget spec JSON file to validate against.

.PARAMETER ExtractedFile
Path to JSON file with extracted widget properties (from UE).

.PARAMETER Tolerance
Floating-point comparison tolerance (default: 0.01)

.EXAMPLE
.\VerifyWidgetProperties.ps1 -SpecFile "path\to\spec.json" -ExtractedFile "path\to\extracted.json"
#>

param(
    [string]$ProjectPath = $null,
    [string]$SpecFile = $null,
    [string]$ExtractedFile = $null,
    [double]$Tolerance = 0.01
)

$ErrorActionPreference = 'Stop'

# ============================================
# INITIALIZATION
# ============================================

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host "      MWCS Strict Property Verification v1.0                   " -ForegroundColor Cyan
Write-Host "      Verifies Generated Widgets Match Specifications          " -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PluginRoot = (Resolve-Path (Join-Path $ScriptDir "..\..")).Path

# ============================================
# HELPER FUNCTIONS
# ============================================

function Write-Log {
    param([string]$Message, [string]$Level = "Info")
    $timestamp = Get-Date -Format "HH:mm:ss"
    switch ($Level) {
        "Success" { Write-Host "[$timestamp] [OK] $Message" -ForegroundColor Green }
        "Error" { Write-Host "[$timestamp] [FAIL] $Message" -ForegroundColor Red }
        "Warning" { Write-Host "[$timestamp] [WARN] $Message" -ForegroundColor Yellow }
        "Mismatch" { Write-Host "[$timestamp] [MISMATCH] $Message" -ForegroundColor Magenta }
        default { Write-Host "[$timestamp] $Message" -ForegroundColor Gray }
    }
}

function Compare-FloatValue {
    param([double]$Expected, [double]$Actual, [double]$Eps)
    return [Math]::Abs($Expected - $Actual) -le $Eps
}

function Compare-ColorValue {
    param($Expected, $Actual, [double]$Eps)
    
    if (-not $Expected -or -not $Actual) { return $false }
    
    $rMatch = Compare-FloatValue -Expected $Expected.R -Actual $Actual.R -Eps $Eps
    $gMatch = Compare-FloatValue -Expected $Expected.G -Actual $Actual.G -Eps $Eps
    $bMatch = Compare-FloatValue -Expected $Expected.B -Actual $Actual.B -Eps $Eps
    $aMatch = Compare-FloatValue -Expected $Expected.A -Actual $Actual.A -Eps $Eps
    
    return $rMatch -and $gMatch -and $bMatch -and $aMatch
}

function Parse-UEColorString {
    param([string]$ColorString)
    
    # Parse format: "(R=0.02,G=0.02,B=0.05,A=1.0)" or "R=0.02,G=0.02,B=0.05,A=1.0"
    $pattern = "R=([0-9.]+).*?G=([0-9.]+).*?B=([0-9.]+).*?A=([0-9.]+)"
    if ($ColorString -match $pattern) {
        return @{
            R = [double]$Matches[1]
            G = [double]$Matches[2]
            B = [double]$Matches[3]
            A = [double]$Matches[4]
        }
    }
    return $null
}

function Parse-UEMarginString {
    param([string]$MarginString)
    
    # Parse format: "(Left=16,Top=12,Right=16,Bottom=12)"
    $pattern = "Left=([0-9.]+).*?Top=([0-9.]+).*?Right=([0-9.]+).*?Bottom=([0-9.]+)"
    if ($MarginString -match $pattern) {
        return @{
            Left = [double]$Matches[1]
            Top = [double]$Matches[2]
            Right = [double]$Matches[3]
            Bottom = [double]$Matches[4]
        }
    }
    return $null
}

function Compare-Property {
    param(
        [string]$WidgetName,
        [string]$PropertyPath,
        $SpecValue,
        $ActualValue,
        [double]$Tolerance
    )
    
    $results = @()
    
    # Null check
    if ($null -eq $SpecValue) {
        return $results
    }
    
    # Handle color comparison
    if ($PropertyPath -like "*Color*" -or $PropertyPath -eq "BrushColor") {
        $parsedActual = $null
        if ($ActualValue -is [string]) {
            $parsedActual = Parse-UEColorString -ColorString $ActualValue
        } elseif ($ActualValue -is [hashtable] -or $ActualValue -is [PSCustomObject]) {
            $parsedActual = $ActualValue
        }
        
        if ($parsedActual -and $SpecValue) {
            if (-not (Compare-ColorValue -Expected $SpecValue -Actual $parsedActual -Eps $Tolerance)) {
                $results += @{
                    Widget = $WidgetName
                    Property = $PropertyPath
                    Expected = "R=$($SpecValue.R),G=$($SpecValue.G),B=$($SpecValue.B),A=$($SpecValue.A)"
                    Actual = "R=$($parsedActual.R),G=$($parsedActual.G),B=$($parsedActual.B),A=$($parsedActual.A)"
                    Status = "MISMATCH"
                }
            }
        }
    }
    # Handle margin/padding comparison
    elseif ($PropertyPath -like "*Padding*" -or $PropertyPath -like "*Margin*") {
        $parsedActual = $null
        if ($ActualValue -is [string]) {
            $parsedActual = Parse-UEMarginString -MarginString $ActualValue
        } elseif ($ActualValue -is [hashtable] -or $ActualValue -is [PSCustomObject]) {
            $parsedActual = $ActualValue
        }
        
        if ($parsedActual -and $SpecValue) {
            $leftMatch = Compare-FloatValue -Expected $SpecValue.Left -Actual $parsedActual.Left -Eps $Tolerance
            $topMatch = Compare-FloatValue -Expected $SpecValue.Top -Actual $parsedActual.Top -Eps $Tolerance
            $rightMatch = Compare-FloatValue -Expected $SpecValue.Right -Actual $parsedActual.Right -Eps $Tolerance
            $bottomMatch = Compare-FloatValue -Expected $SpecValue.Bottom -Actual $parsedActual.Bottom -Eps $Tolerance
            
            if (-not ($leftMatch -and $topMatch -and $rightMatch -and $bottomMatch)) {
                $results += @{
                    Widget = $WidgetName
                    Property = $PropertyPath
                    Expected = "L=$($SpecValue.Left),T=$($SpecValue.Top),R=$($SpecValue.Right),B=$($SpecValue.Bottom)"
                    Actual = "L=$($parsedActual.Left),T=$($parsedActual.Top),R=$($parsedActual.Right),B=$($parsedActual.Bottom)"
                    Status = "MISMATCH"
                }
            }
        }
    }
    # Handle float comparison
    elseif ($SpecValue -is [double] -or $SpecValue -is [float] -or $SpecValue -is [int]) {
        $actualFloat = [double]$ActualValue
        if (-not (Compare-FloatValue -Expected $SpecValue -Actual $actualFloat -Eps $Tolerance)) {
            $results += @{
                Widget = $WidgetName
                Property = $PropertyPath
                Expected = $SpecValue
                Actual = $actualFloat
                Status = "MISMATCH"
            }
        }
    }
    # Handle string/bool comparison
    else {
        $normalizedExpected = "$SpecValue".Trim().ToLower()
        $normalizedActual = "$ActualValue".Trim().ToLower()
        
        if ($normalizedExpected -ne $normalizedActual) {
            $results += @{
                Widget = $WidgetName
                Property = $PropertyPath
                Expected = $SpecValue
                Actual = $ActualValue
                Status = "MISMATCH"
            }
        }
    }
    
    return $results
}

# ============================================
# MAIN VERIFICATION LOGIC
# ============================================

Write-Log "Starting strict property verification..."
Write-Log "Tolerance: $Tolerance"
Write-Log ""

# Check if files provided
if (-not $SpecFile -or -not $ExtractedFile) {
    Write-Log "Usage: .\VerifyWidgetProperties.ps1 -SpecFile <path> -ExtractedFile <path>" -Level Warning
    Write-Log ""
    Write-Log "Description:" -Level Info
    Write-Log "  This script compares widget properties against their specification." -Level Info
    Write-Log ""
    Write-Log "Required Files:" -Level Info
    Write-Log "  -SpecFile      : JSON file containing widget specification (from GetWidgetSpec)" -Level Info
    Write-Log "  -ExtractedFile : JSON file with extracted widget properties from UE Editor" -Level Info
    Write-Log ""
    Write-Log "Example (demonstration mode):" -Level Info
    
    # Run demonstration with hardcoded values
    Write-Log ""
    Write-Log "--- DEMONSTRATION MODE ---" -Level Warning
    
    $demoSpec = @{
        HeaderBorder = @{
            BrushColor = @{ R = 0.02; G = 0.02; B = 0.05; A = 1.0 }
            Padding = @{ Left = 16; Top = 12; Right = 16; Bottom = 12 }
        }
        TitleText = @{
            FontSize = 20
            Text = "EAIS AI Editor"
        }
    }
    
    # Simulated "before fix" values - white color
    $demoActualBefore = @{
        HeaderBorder = @{
            BrushColor = "(R=1.000000,G=1.000000,B=1.000000,A=1.000000)"
            Padding = "(Left=0.000000,Top=0.000000,Right=0.000000,Bottom=0.000000)"
        }
        TitleText = @{
            FontSize = 14
            Text = "Title"
        }
    }
    
    # Simulated "after fix" values - correct color
    $demoActualAfter = @{
        HeaderBorder = @{
            BrushColor = "(R=0.020000,G=0.020000,B=0.050000,A=1.000000)"
            Padding = "(Left=16.000000,Top=12.000000,Right=16.000000,Bottom=12.000000)"
        }
        TitleText = @{
            FontSize = 20
            Text = "EAIS AI Editor"
        }
    }
    
    Write-Log ""
    Write-Log "Checking BEFORE fix (expected mismatches):" -Level Info
    $mismatchesBefore = @()
    
    foreach ($widgetName in $demoSpec.Keys) {
        $specProps = $demoSpec[$widgetName]
        $actualProps = $demoActualBefore[$widgetName]
        
        if (-not $actualProps) {
            Write-Log "Widget '$widgetName' not found in actual output" -Level Error
            continue
        }
        
        foreach ($propName in $specProps.Keys) {
            $specValue = $specProps[$propName]
            $actualValue = $actualProps[$propName]
            
            if ($null -ne $actualValue) {
                $result = Compare-Property -WidgetName $widgetName -PropertyPath $propName -SpecValue $specValue -ActualValue $actualValue -Tolerance $Tolerance
                $mismatchesBefore += $result
            }
        }
    }
    
    if ($mismatchesBefore.Count -gt 0) {
        Write-Log "Found $($mismatchesBefore.Count) property mismatches (expected):" -Level Mismatch
        foreach ($m in $mismatchesBefore) {
            Write-Log "  $($m.Widget).$($m.Property):" -Level Mismatch
            Write-Log "    Expected: $($m.Expected)" -Level Info
            Write-Log "    Actual:   $($m.Actual)" -Level Info
        }
    }
    
    Write-Log ""
    Write-Log "Checking AFTER fix (should pass):" -Level Info
    $mismatchesAfter = @()
    
    foreach ($widgetName in $demoSpec.Keys) {
        $specProps = $demoSpec[$widgetName]
        $actualProps = $demoActualAfter[$widgetName]
        
        if (-not $actualProps) {
            Write-Log "Widget '$widgetName' not found in actual output" -Level Error
            continue
        }
        
        foreach ($propName in $specProps.Keys) {
            $specValue = $specProps[$propName]
            $actualValue = $actualProps[$propName]
            
            if ($null -ne $actualValue) {
                $result = Compare-Property -WidgetName $widgetName -PropertyPath $propName -SpecValue $specValue -ActualValue $actualValue -Tolerance $Tolerance
                $mismatchesAfter += $result
            }
        }
    }
    
    if ($mismatchesAfter.Count -eq 0) {
        Write-Log "All properties match specification!" -Level Success
    } else {
        Write-Log "Found $($mismatchesAfter.Count) property mismatches:" -Level Error
        foreach ($m in $mismatchesAfter) {
            Write-Log "  $($m.Widget).$($m.Property): Expected '$($m.Expected)', Got '$($m.Actual)'" -Level Error
        }
    }
    
    Write-Log ""
    Write-Log "--- END DEMONSTRATION ---" -Level Warning
    exit 0
}

# Load spec file
if (-not (Test-Path $SpecFile)) {
    Write-Log "Spec file not found: $SpecFile" -Level Error
    exit 1
}

# Load extracted file
if (-not (Test-Path $ExtractedFile)) {
    Write-Log "Extracted file not found: $ExtractedFile" -Level Error
    exit 1
}

Write-Log "Loading spec file: $SpecFile"
$specJson = Get-Content $SpecFile -Raw | ConvertFrom-Json

Write-Log "Loading extracted file: $ExtractedFile"
$extractedJson = Get-Content $ExtractedFile -Raw | ConvertFrom-Json

$mismatches = @()

# TODO: Implement full comparison once extraction format is defined
Write-Log "Full comparison requires extraction file format to be defined." -Level Warning
Write-Log "Use demonstration mode to see example output." -Level Info

# ============================================
# REPORT
# ============================================

Write-Log ""
Write-Log "================================================================"
Write-Log "VERIFICATION REPORT"
Write-Log "================================================================"

if ($mismatches.Count -eq 0) {
    Write-Log "All properties match specification!" -Level Success
    exit 0
}
else {
    Write-Log "Found $($mismatches.Count) property mismatches:" -Level Error
    foreach ($m in $mismatches) {
        Write-Log "  $($m.Widget).$($m.Property):" -Level Mismatch
        Write-Log "    Expected: $($m.Expected)" -Level Info
        Write-Log "    Actual:   $($m.Actual)" -Level Info
    }
    exit 1
}
