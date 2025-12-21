<#
.SYNOPSIS
MWCS-Commandlets - Wrapper functions for MWCS UE commandlets.

.DESCRIPTION
Provides PowerShell wrapper functions for executing MWCS commandlets:
- MWCS_CreateWidgets
- MWCS_ValidateWidgets
- MWCS_ExtractWidgets
#>

# ============================================
# COMMANDLET WRAPPERS
# ============================================

function Invoke-MWCSCreateWidgets {
    <#
    .SYNOPSIS
    Execute MWCS_CreateWidgets commandlet to generate widget blueprints.
    
    .PARAMETER ProjectFile
    Path to the .uproject file.
    
    .PARAMETER UEPath
    Path to UE installation root.
    
    .PARAMETER Mode
    Creation mode: Preserve (keep existing), Overwrite (replace all)
    
    .OUTPUTS
    Exit code (0 = success, non-zero = failure)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectFile,
        
        [Parameter(Mandatory = $true)]
        [string]$UEPath,
        
        [ValidateSet('Preserve', 'Overwrite')]
        [string]$Mode = 'Preserve'
    )
    
    $editorCmdExe = Join-Path $UEPath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    
    if (!(Test-Path $editorCmdExe)) {
        Write-TestLog "UnrealEditor-Cmd.exe not found at: $editorCmdExe" -Level Error
        return 1
    }
    
    $arguments = @(
        "`"$ProjectFile`"",
        "-run=MWCS_CreateWidgets",
        "-Mode=$Mode",
        "-unattended",
        "-nopause"
    )
    
    Write-TestLog "Executing MWCS_CreateWidgets commandlet..." -Level Info
    Write-TestLog "  Mode: $Mode" -Level Info
    
    try {
        $process = Start-Process -FilePath $editorCmdExe -ArgumentList $arguments -Wait -PassThru -NoNewWindow
        $exitCode = $process.ExitCode
        
        if ($exitCode -eq 0) {
            Write-TestLog "MWCS_CreateWidgets completed successfully" -Level Success
            return 0
        }
        else {
            Write-TestLog "MWCS_CreateWidgets failed (exit code $exitCode)" -Level Error
            return $exitCode
        }
    }
    catch {
        Write-TestLog "Failed to execute MWCS_CreateWidgets: $_" -Level Error
        return 1
    }
}

function Invoke-MWCSValidateWidgets {
    <#
    .SYNOPSIS
    Execute MWCS_ValidateWidgets commandlet to validate generated widgets.
    
    .PARAMETER ProjectFile
    Path to the .uproject file.
    
    .PARAMETER UEPath
    Path to UE installation root.
    
    .OUTPUTS
    Exit code (0 = success, non-zero = failure)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectFile,
        
        [Parameter(Mandatory = $true)]
        [string]$UEPath
    )
    
    $editorCmdExe = Join-Path $UEPath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    
    if (!(Test-Path $editorCmdExe)) {
        Write-TestLog "UnrealEditor-Cmd.exe not found at: $editorCmdExe" -Level Error
        return 1
    }
    
    $arguments = @(
        "`"$ProjectFile`"",
        "-run=MWCS_ValidateWidgets",
        "-unattended",
        "-nopause"
    )
    
    Write-TestLog "Executing MWCS_ValidateWidgets commandlet..." -Level Info
    
    try {
        $process = Start-Process -FilePath $editorCmdExe -ArgumentList $arguments -Wait -PassThru -NoNewWindow
        $exitCode = $process.ExitCode
        
        if ($exitCode -eq 0) {
            Write-TestLog "MWCS_ValidateWidgets passed" -Level Success
            return 0
        }
        else {
            Write-TestLog "MWCS_ValidateWidgets failed (exit code $exitCode)" -Level Error
            return $exitCode
        }
    }
    catch {
        Write-TestLog "Failed to execute MWCS_ValidateWidgets: $_" -Level Error
        return 1
    }
}

function Invoke-MWCSExtractWidgets {
    <#
    .SYNOPSIS
    Execute MWCS_ExtractWidgets commandlet to extract specs from widgets.
    
    .PARAMETER ProjectFile
    Path to the .uproject file.
    
    .PARAMETER UEPath
    Path to UE installation root.
    
    .PARAMETER OutputPath
    Path for extracted specs output.
    
    .OUTPUTS
    Exit code (0 = success, non-zero = failure)
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectFile,
        
        [Parameter(Mandatory = $true)]
        [string]$UEPath,
        
        [string]$OutputPath = ""
    )
    
    $editorCmdExe = Join-Path $UEPath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    
    if (!(Test-Path $editorCmdExe)) {
        Write-TestLog "UnrealEditor-Cmd.exe not found at: $editorCmdExe" -Level Error
        return 1
    }
    
    $arguments = @(
        "`"$ProjectFile`"",
        "-run=MWCS_ExtractWidgets",
        "-unattended",
        "-nopause"
    )
    
    if ($OutputPath) {
        $arguments += "-OutputPath=`"$OutputPath`""
    }
    
    Write-TestLog "Executing MWCS_ExtractWidgets commandlet..." -Level Info
    
    try {
        $process = Start-Process -FilePath $editorCmdExe -ArgumentList $arguments -Wait -PassThru -NoNewWindow
        $exitCode = $process.ExitCode
        
        if ($exitCode -eq 0) {
            Write-TestLog "MWCS_ExtractWidgets completed successfully" -Level Success
            return 0
        }
        else {
            Write-TestLog "MWCS_ExtractWidgets failed (exit code $exitCode)" -Level Warning
            return $exitCode
        }
    }
    catch {
        Write-TestLog "Failed to execute MWCS_ExtractWidgets: $_" -Level Warning
        return 1
    }
}

# ============================================
# EXPORTS
# ============================================

Export-ModuleMember -Function @(
    'Invoke-MWCSCreateWidgets',
    'Invoke-MWCSValidateWidgets',
    'Invoke-MWCSExtractWidgets'
)
