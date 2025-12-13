#include "MWCS_ToolEUW.h"

#include "MWCS_Report.h"
#include "MWCS_Service.h"
#include "MWCS_Settings.h"

#include "Modules/ModuleManager.h"

#include "Components/Button.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"

#include "ISettingsModule.h"

namespace
{
    FString BuildSettingsSummaryString()
    {
        const UMWCS_Settings *Settings = UMWCS_Settings::Get();
        if (!Settings)
        {
            return TEXT("MWCS Settings: <unavailable>");
        }

        return FString::Printf(TEXT("Output=%s | ToolEUW=%s/%s | Allowlist=%d | SpecProvider=%s"),
                               *Settings->OutputRootPath,
                               *Settings->ToolEuwOutputPath,
                               *Settings->ToolEuwAssetName,
                               Settings->SpecProviderClasses.Num(),
                               *Settings->ToolEuwSpecProviderClass.ToString());
    }
} // namespace

void UMWCS_ToolEUW::NativeConstruct()
{
    Super::NativeConstruct();

    if (OutputLog)
    {
        OutputLog->SetIsReadOnly(true);
    }

    RefreshSettingsSummary();

    if (Btn_OpenSettings)
    {
        Btn_OpenSettings->OnClicked.Clear();
        Btn_OpenSettings->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleOpenSettingsClicked);
    }

    if (Btn_Validate)
    {
        Btn_Validate->OnClicked.Clear();
        Btn_Validate->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleValidateClicked);
    }

    if (Btn_CreateMissing)
    {
        Btn_CreateMissing->OnClicked.Clear();
        Btn_CreateMissing->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleCreateMissingClicked);
    }

    if (Btn_Repair)
    {
        Btn_Repair->OnClicked.Clear();
        Btn_Repair->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleRepairClicked);
    }

    if (Btn_ForceRecreate)
    {
        Btn_ForceRecreate->OnClicked.Clear();
        Btn_ForceRecreate->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleForceRecreateClicked);
    }

    if (Btn_GenerateToolEUW)
    {
        Btn_GenerateToolEUW->OnClicked.Clear();
        Btn_GenerateToolEUW->OnClicked.AddDynamic(this, &UMWCS_ToolEUW::HandleGenerateToolEuwClicked);
    }

    AppendLine(TEXT("MWCS Tool EUW ready."));
}

void UMWCS_ToolEUW::RefreshSettingsSummary()
{
    if (!SettingsSummaryText)
    {
        return;
    }

    SettingsSummaryText->SetText(FText::FromString(BuildSettingsSummaryString()));
}

void UMWCS_ToolEUW::AppendLine(const FString &Line)
{
    if (!OutputLog)
    {
        return;
    }

    FString Existing = OutputLog->GetText().ToString();
    if (!Existing.IsEmpty())
    {
        Existing += TEXT("\n");
    }
    Existing += Line;
    OutputLog->SetText(FText::FromString(Existing));
}

void UMWCS_ToolEUW::AppendReport(const FString &Title, const FMWCS_Report &Report)
{
    AppendLine(FString::Printf(TEXT("[%s] Specs=%d Created=%d Repaired=%d Recreated=%d Errors=%d Warnings=%d"),
                               *Title,
                               Report.SpecsProcessed,
                               Report.AssetsCreated,
                               Report.AssetsRepaired,
                               Report.AssetsRecreated,
                               Report.NumErrors(),
                               Report.NumWarnings()));

    for (const FMWCS_Issue &Issue : Report.Issues)
    {
        AppendLine(FString::Printf(TEXT("- %s: %s (%s)"), *Issue.Code, *Issue.Message, *Issue.Context));
    }
}

void UMWCS_ToolEUW::HandleOpenSettingsClicked()
{
    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    if (!Settings)
    {
        AppendLine(TEXT("[Settings] Could not resolve UMWCS_Settings"));
        return;
    }

    ISettingsModule *SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings"));
    if (!SettingsModule)
    {
        AppendLine(TEXT("[Settings] Settings module not available"));
        return;
    }

    SettingsModule->ShowViewer(Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
}

void UMWCS_ToolEUW::HandleValidateClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("Validate"), FMWCS_Service::Get().ValidateAll());
}

void UMWCS_ToolEUW::HandleCreateMissingClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("CreateMissing"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::CreateMissing));
}

void UMWCS_ToolEUW::HandleRepairClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("Repair"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::Repair));
}

void UMWCS_ToolEUW::HandleForceRecreateClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("ForceRecreate"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::ForceRecreate));
}

void UMWCS_ToolEUW::HandleGenerateToolEuwClicked()
{
    RefreshSettingsSummary();
    AppendReport(TEXT("ToolEUW"), FMWCS_Service::Get().GenerateOrRepairToolEuw());
}
