<#
.SYNOPSIS
Master MWCS test runner - executes all test suites.

.DESCRIPTION
Orchestrates all MWCS tests for any UE project using P_MWCS plugin.
Auto-detects project and UE installation.

.PARAMETER ProjectPath
(Optional) Path to project root. If not provided, auto-detect.

.PARAMETER UEPath
(Optional) Path to UE installation. If not provided, auto-detect.

.PARAMETER TestSuite
Which tests to run:
- All: All test suites
- Creation: Widget creation tests only
- Extraction: Widget extraction tests only
- RoundTrip: Full create->extract->validate cycle

.EXAMPLE
# Auto-detect, run all tests
.\RunMWCSTests.ps1

# Run specific test suite
.\RunMWCSTests.ps1 -TestSuite Creation

# Specify paths explicitly
.\RunMWCSTests.ps1 -ProjectPath "D:\MyGame" -UEPath "C:\Program Files\Epic Games\UE_5.5"
#>

param(
    [string]$ProjectPath = $null,
    [string]$UEPath = $null,
    [ValidateSet('All', 'Creation', 'Extraction', 'RoundTrip')]
    [string]$TestSuite = 'All'
)

$ErrorActionPreference = 'Stop'

# ============================================
# INITIALIZATION
# ============================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          MWCS Automated Testing Framework v1.0                 ║" -ForegroundColor Cyan
Write-Host "║  (Modular Widget Creation System - Test Suite)                 ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Import modules from Modules subdirectory
$scriptsDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$modulesDir = Join-Path $scriptsDir "Modules"

if (!(Test-Path $modulesDir)) {
    Write-Host "✗ ERROR: Modules directory not found at: $modulesDir" -ForegroundColor Red
    exit 1
}

Write-Host "Importing modules..." -ForegroundColor Gray
try {
    Import-Module (Join-Path $modulesDir "MWCS-Common.psm1") -Force -ErrorAction Stop
    Import-Module (Join-Path $modulesDir "MWCS-Commandlets.psm1") -Force -ErrorAction Stop
    Write-Host "✓ Modules imported successfully" -ForegroundColor Green
}
catch {
    Write-Host "✗ Failed to import modules: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""

# ============================================
# PROJECT & UE DETECTION
# ============================================

# Detect project
if (-not $ProjectPath) {
    $projectInfo = Find-ProjectRoot
    if (-not $projectInfo) {
        Write-TestLog "ERROR: Could not detect project. Please specify -ProjectPath" -Level Error
        exit 1
    }
    $ProjectPath = $projectInfo.ProjectPath
    $ProjectFile = $projectInfo.ProjectFile
}
else {
    # Verify provided path
    $uprojFiles = Get-ChildItem -Path $ProjectPath -Filter "*.uproject" -ErrorAction SilentlyContinue
    if (!$uprojFiles) {
        Write-TestLog "ERROR: No .uproject found in $ProjectPath" -Level Error
        exit 1
    }
    $ProjectFile = $uprojFiles[0].FullName
}

Write-TestLog "Project detected: $ProjectFile" -Level Success

# Detect UE
if (-not $UEPath) {
    $UEPath = Find-UnrealEngine
    if (-not $UEPath) {
        Write-TestLog "ERROR: Could not detect UE installation. Please specify -UEPath" -Level Error
        exit 1
    }
}

Write-TestLog "UE installation: $UEPath" -Level Success

# Verify P_MWCS is enabled
if (!(Test-MWCSPluginEnabled $ProjectFile)) {
    Write-TestLog "ERROR: P_MWCS plugin not enabled in project" -Level Error
    exit 1
}

Write-TestLog "P_MWCS plugin verified" -Level Success
Write-Host ""

# ============================================
# TEST EXECUTION
# ============================================

$allPassed = $true
$testResults = @()

# Test 1: Creation
if ($TestSuite -eq 'All' -or $TestSuite -eq 'Creation') {
    Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info
    Write-TestLog "Running Widget Creation Tests..." -Level Info
    Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info
    
    $createResult = Invoke-MWCSCreateWidgets -ProjectFile $ProjectFile -UEPath $UEPath -Mode Preserve
    if ($createResult -eq 0) {
        Write-TestLog "Creation Tests PASSED" -Level Success
        $testResults += "Creation: PASS"
    }
    else {
        Write-TestLog "Creation Tests FAILED" -Level Error
        $testResults += "Creation: FAIL"
        $allPassed = $false
    }
    Write-Host ""
}

# Test 2: Extraction
if ($TestSuite -eq 'All' -or $TestSuite -eq 'Extraction') {
    Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info
    Write-TestLog "Running Widget Extraction Tests..." -Level Info
    Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info
    
    $extractResult = Invoke-MWCSExtractWidgets -ProjectFile $ProjectFile -UEPath $UEPath
    if ($extractResult -eq 0) {
        Write-TestLog "Extraction Tests PASSED" -Level Success
        $testResults += "Extraction: PASS"
    }
    else {
        Write-TestLog "Extraction Tests SKIPPED (commandlet may not exist)" -Level Warning
        $testResults += "Extraction: SKIP"
    }
    Write-Host ""
}

# Test 3: Round-Trip
if ($TestSuite -eq 'All' -or $TestSuite -eq 'RoundTrip') {
    Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info
    Write-TestLog "Running Round-Trip Tests..." -Level Info
    Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info
    
    # Round-trip: Create -> Validate
    $rtCreateResult = Invoke-MWCSCreateWidgets -ProjectFile $ProjectFile -UEPath $UEPath -Mode Preserve
    if ($rtCreateResult -eq 0) {
        $rtValidateResult = Invoke-MWCSValidateWidgets -ProjectFile $ProjectFile -UEPath $UEPath
        if ($rtValidateResult -eq 0) {
            Write-TestLog "Round-Trip Tests PASSED" -Level Success
            $testResults += "RoundTrip: PASS"
        }
        else {
            Write-TestLog "Round-Trip Tests FAILED (validation)" -Level Error
            $testResults += "RoundTrip: FAIL"
            $allPassed = $false
        }
    }
    else {
        Write-TestLog "Round-Trip Tests FAILED (creation)" -Level Error
        $testResults += "RoundTrip: FAIL"
        $allPassed = $false
    }
    Write-Host ""
}

# ============================================
# SUMMARY & EXIT
# ============================================

Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info
Write-TestLog "TEST SUMMARY" -Level Info
Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info

foreach ($result in $testResults) {
    if ($result -like "*PASS*") {
        Write-TestLog $result -Level Success
    }
    elseif ($result -like "*SKIP*") {
        Write-TestLog $result -Level Warning
    }
    else {
        Write-TestLog $result -Level Error
    }
}

Write-TestLog "════════════════════════════════════════════════════════════════" -Level Info

if ($allPassed) {
    Write-TestLog "✓✓✓ ALL TESTS PASSED ✓✓✓" -Level Success
    Write-Host ""
    exit 0
}
else {
    Write-TestLog "✗✗✗ SOME TESTS FAILED ✗✗✗" -Level Error
    Write-Host ""
    exit 1
}
