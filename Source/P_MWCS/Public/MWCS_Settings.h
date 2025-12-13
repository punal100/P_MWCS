#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "MWCS_Settings.generated.h"

UCLASS(config = Editor, defaultconfig)
class UMWCS_Settings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UMWCS_Settings();

    static const UMWCS_Settings *Get();

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Discovery", meta = (DisplayName = "Spec Provider Classes (Allowlist)"))
    TArray<FSoftClassPath> SpecProviderClasses;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Output", meta = (DisplayName = "Widget Blueprint Output Root", ToolTip = "Long package path, e.g. /Game/UI/Widgets"))
    FString OutputRootPath;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Tool EUW", meta = (DisplayName = "Tool EUW Output Path", ToolTip = "Long package path, e.g. /Game/Editor/MWCS"))
    FString ToolEuwOutputPath;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Tool EUW", meta = (DisplayName = "Tool EUW Asset Name"))
    FString ToolEuwAssetName;

    UPROPERTY(EditAnywhere, Config, Category = "MWCS|Tool EUW", meta = (DisplayName = "Tool EUW Spec Provider Class"))
    FSoftClassPath ToolEuwSpecProviderClass;
};
