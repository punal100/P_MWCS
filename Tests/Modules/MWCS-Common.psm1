<#
.SYNOPSIS
MWCS-Common - Shared utilities for MWCS testing framework.

.DESCRIPTION
Provides common functions for project/UE detection, logging, and file utilities.
This module is project-agnostic and works with any UE project using P_MWCS.
#>

# ============================================
# LOGGING UTILITIES
# ============================================

function Write-TestLog {
    <#
    .SYNOPSIS
    Write formatted test log output.
    
    .PARAMETER Message
    The message to log.
    
    .PARAMETER Level
    Log level: Info, Success, Warning, Error
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,
        
        [ValidateSet('Info', 'Success', 'Warning', 'Error')]
        [string]$Level = 'Info'
    )
    
    $timestamp = Get-Date -Format "HH:mm:ss"
    
    switch ($Level) {
        'Info' {
            Write-Host "[$timestamp] $Message" -ForegroundColor Cyan
        }
        'Success' {
            Write-Host "[$timestamp] ✓ $Message" -ForegroundColor Green
        }
        'Warning' {
            Write-Host "[$timestamp] ⚠ $Message" -ForegroundColor Yellow
        }
        'Error' {
            Write-Host "[$timestamp] ✗ $Message" -ForegroundColor Red
        }
    }
}

# ============================================
# PROJECT DETECTION
# ============================================

function Find-ProjectRoot {
    <#
    .SYNOPSIS
    Auto-detect the UE project root by searching for .uproject files.
    
    .DESCRIPTION
    Searches current directory and parent directories for a .uproject file.
    
    .OUTPUTS
    PSObject with ProjectPath and ProjectFile properties, or $null if not found.
    #>
    param(
        [string]$StartPath = (Get-Location).Path,
        [int]$MaxDepth = 10
    )
    
    $currentPath = $StartPath
    $depth = 0
    
    while ($depth -lt $MaxDepth) {
        $uprojFiles = Get-ChildItem -Path $currentPath -Filter "*.uproject" -ErrorAction SilentlyContinue
        
        if ($uprojFiles -and $uprojFiles.Count -gt 0) {
            return [PSCustomObject]@{
                ProjectPath = $currentPath
                ProjectFile = $uprojFiles[0].FullName
                ProjectName = $uprojFiles[0].BaseName
            }
        }
        
        $parentPath = Split-Path -Parent $currentPath
        if ([string]::IsNullOrEmpty($parentPath) -or $parentPath -eq $currentPath) {
            break
        }
        
        $currentPath = $parentPath
        $depth++
    }
    
    return $null
}

function Find-UnrealEngine {
    <#
    .SYNOPSIS
    Auto-detect Unreal Engine installation path.
    
    .DESCRIPTION
    Searches common installation locations and registry for UE installation.
    
    .OUTPUTS
    Path to UE installation root, or $null if not found.
    #>
    
    # Common installation paths (Windows)
    $commonPaths = @(
        "C:\Program Files\Epic Games\UE_5.5",
        "C:\Program Files\Epic Games\UE_5.4",
        "C:\Program Files\Epic Games\UE_5.3",
        "D:\UE\UE_S",
        "D:\EpicGames\UE_5.5",
        "E:\EpicGames\UE_5.5"
    )
    
    foreach ($path in $commonPaths) {
        if (Test-Path $path) {
            $editorPath = Join-Path $path "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
            if (Test-Path $editorPath) {
                return $path
            }
        }
    }
    
    # Try to detect from .uproject EngineAssociation
    $projectInfo = Find-ProjectRoot
    if ($projectInfo) {
        try {
            $projectJson = Get-Content -Path $projectInfo.ProjectFile -Raw | ConvertFrom-Json
            if ($projectJson.EngineAssociation) {
                # Handle GUID-based association (installed from launcher)
                # For source builds, this might be a path or version string
                $engineAssoc = $projectJson.EngineAssociation
                
                # Check if it looks like a path
                if (Test-Path $engineAssoc) {
                    return $engineAssoc
                }
                
                # Try Epic Games Launcher registry
                $regPath = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$engineAssoc"
                if (Test-Path $regPath) {
                    $installedDir = Get-ItemProperty -Path $regPath -Name "InstalledDirectory" -ErrorAction SilentlyContinue
                    if ($installedDir -and $installedDir.InstalledDirectory) {
                        return $installedDir.InstalledDirectory
                    }
                }
            }
        }
        catch {
            Write-TestLog "Could not parse project file for engine association" -Level Warning
        }
    }
    
    return $null
}

# ============================================
# PLUGIN VERIFICATION
# ============================================

function Test-MWCSPluginEnabled {
    <#
    .SYNOPSIS
    Check if P_MWCS plugin is enabled in the project.
    
    .PARAMETER ProjectFile
    Path to the .uproject file.
    
    .OUTPUTS
    $true if P_MWCS is enabled, $false otherwise.
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectFile
    )
    
    if (!(Test-Path $ProjectFile)) {
        return $false
    }
    
    try {
        $projectJson = Get-Content -Path $ProjectFile -Raw | ConvertFrom-Json
        
        if ($projectJson.Plugins) {
            foreach ($plugin in $projectJson.Plugins) {
                if ($plugin.Name -eq "P_MWCS" -and $plugin.Enabled -eq $true) {
                    return $true
                }
            }
        }
        
        # Also check if plugin exists in Plugins directory (auto-enabled)
        $projectDir = Split-Path -Parent $ProjectFile
        $pluginPath = Join-Path $projectDir "Plugins\P_MWCS\P_MWCS.uplugin"
        if (Test-Path $pluginPath) {
            return $true
        }
    }
    catch {
        Write-TestLog "Error parsing project file: $_" -Level Error
    }
    
    return $false
}

# ============================================
# FILE UTILITIES
# ============================================

function Get-MWCSTestSpecs {
    <#
    .SYNOPSIS
    Get all test specification files from the TestSpecs directory.
    
    .PARAMETER TestsPath
    Path to the Tests directory.
    
    .OUTPUTS
    Array of FileInfo objects for .json test specs.
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$TestsPath
    )
    
    $specsPath = Join-Path $TestsPath "TestSpecs"
    if (!(Test-Path $specsPath)) {
        return @()
    }
    
    return Get-ChildItem -Path $specsPath -Filter "*.json" -ErrorAction SilentlyContinue
}

function Get-UnrealEditorCmd {
    <#
    .SYNOPSIS
    Get path to UnrealEditor-Cmd.exe.
    
    .PARAMETER UEPath
    Path to UE installation root.
    
    .OUTPUTS
    Path to editor command line executable.
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$UEPath
    )
    
    return Join-Path $UEPath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
}

# ============================================
# EXPORTS
# ============================================

Export-ModuleMember -Function @(
    'Write-TestLog',
    'Find-ProjectRoot',
    'Find-UnrealEngine',
    'Test-MWCSPluginEnabled',
    'Get-MWCSTestSpecs',
    'Get-UnrealEditorCmd'
)
