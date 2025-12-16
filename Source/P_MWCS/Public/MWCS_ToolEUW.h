#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"

#include "MWCS_ToolEUW.generated.h"

class UButton;
class UMultiLineEditableTextBox;
class UTextBlock;

UCLASS()
class UMWCS_ToolEUW : public UEditorUtilityWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock *TitleText = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock *SettingsSummaryText = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton *Btn_OpenSettings = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton *Btn_Validate = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton *Btn_CreateMissing = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton *Btn_Repair = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton *Btn_ForceRecreate = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton *Btn_GenerateToolEUW = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton *Btn_ExtractSelectedWBP = nullptr;

    UPROPERTY(meta = (BindWidget))
    UMultiLineEditableTextBox *OutputLog = nullptr;

private:
    void RefreshSettingsSummary();
    void AppendLine(const FString &Line);
    void AppendReport(const FString &Title, const struct FMWCS_Report &Report);

    bool ExportWidgetBlueprintHierarchyToJson(class UWidgetBlueprint *WidgetBlueprint,
                                              bool bWriteToFile = true,
                                              bool bCopyToClipboard = true,
                                              bool bShowOutputWindow = true,
                                              bool bIncludeSourceAssetField = false,
                                              bool bIncludeSlotLayout = true,
                                              bool bIncludeCanvasSlot = true,
                                              bool bIncludeTextProperties = true,
                                              bool bIncludeWidgetClassPaths = true,
                                              bool bIncludeIsVariable = false,
                                              bool bIncludePropertiesSection = true,
                                              bool bIncludeDesignSection = true);

    UFUNCTION()
    void HandleOpenSettingsClicked();

    UFUNCTION()
    void HandleValidateClicked();

    UFUNCTION()
    void HandleCreateMissingClicked();

    UFUNCTION()
    void HandleRepairClicked();

    UFUNCTION()
    void HandleForceRecreateClicked();

    UFUNCTION()
    void HandleGenerateToolEuwClicked();

    UFUNCTION()
    void HandleExtractSelectedWbpClicked();
};
