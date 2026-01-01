#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "MWCS_Settings.generated.h"

/**
 * Configuration for an external Tool EUW (Editor Utility Widget).
 * Any plugin can register its own Tool EUW by adding an entry to ExternalToolEuws.
 */
USTRUCT()
struct FMWCS_ExternalToolEuwConfig
{
    GENERATED_BODY()

    /** Unique identifier for this tool (e.g., "EAIS", "MyPlugin") */
    UPROPERTY(EditAnywhere, Config, meta = (DisplayName = "Tool Name"))
    FString ToolName;

    /** Output path for the generated widget (e.g., /Game/Editor/EAIS) */
    UPROPERTY(EditAnywhere, Config, meta = (DisplayName = "Output Path"))
    FString OutputPath;

    /** Asset name for the generated widget (e.g., EUW_EAIS_AIEditor) */
    UPROPERTY(EditAnywhere, Config, meta = (DisplayName = "Asset Name"))
    FString AssetName;

    /** Class path of the spec provider (e.g., /Script/P_EAISTools.EAIS_EditorWidgetSpec) */
    UPROPERTY(EditAnywhere, Config, meta = (DisplayName = "Spec Provider Class"))
    FSoftClassPath SpecProviderClass;
};

UCLASS(config = Editor, defaultconfig)
class P_MWCS_API UMWCS_Settings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMWCS_Settings();

    static const UMWCS_Settings *Get();

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Discovery", meta = (DisplayName = "Spec Provider Classes (Allowlist)"))
    TArray<FSoftClassPath> SpecProviderClasses;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Output", meta = (DisplayName = "Widget Blueprint Output Root", ToolTip = "Long package path, e.g. /Game/UI/Widgets"))
    FString OutputRootPath;

    // MWCS Tool EUW (built-in)
    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Tool EUW", meta = (DisplayName = "Tool EUW Output Path", ToolTip = "Long package path, e.g. /Game/Editor/MWCS"))
    FString ToolEuwOutputPath;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Tool EUW", meta = (DisplayName = "Tool EUW Asset Name"))
    FString ToolEuwAssetName;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Tool EUW", meta = (DisplayName = "Tool EUW Spec Provider Class"))
    FSoftClassPath ToolEuwSpecProviderClass;

    // External Tool EUWs (modular - allows any plugin to register its Tool EUW)
    UPROPERTY(EditAnywhere, Config, Category = "MWCS|External Tool EUWs", meta = (DisplayName = "External Tool EUWs", ToolTip = "Configure external plugin Tool EUWs. Each entry allows a plugin to generate its own Editor Utility Widget."))
    TArray<FMWCS_ExternalToolEuwConfig> ExternalToolEuws;

    /** Helper to find an external Tool EUW config by name. Returns nullptr if not found. */
    const FMWCS_ExternalToolEuwConfig* FindExternalToolEuw(const FString& ToolName) const;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Designer Preview", meta = (DisplayName = "Designer Preview Zoom Level Min"))
    int32 DesignerZoomLevelMin = 0;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Designer Preview", meta = (DisplayName = "Designer Preview Zoom Level Max"))
    int32 DesignerZoomLevelMax = 20;
};

