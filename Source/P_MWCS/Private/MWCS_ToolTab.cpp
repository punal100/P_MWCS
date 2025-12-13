#include "MWCS_ToolTab.h"

#include "MWCS_Service.h"

#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

const FName FMWCS_ToolTab::TabName(TEXT("MWCS.ToolTab"));

class SMWCS_ToolPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMWCS_ToolPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments &InArgs)
    {
        ChildSlot
            [SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).Text(FText::FromString(TEXT("MWCS — Modular Widget Creation System")))] + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("Validate"))).OnClicked(this, &SMWCS_ToolPanel::OnValidate)] + SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("Create Missing"))).OnClicked(this, &SMWCS_ToolPanel::OnCreateMissing)] + SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("Repair"))).OnClicked(this, &SMWCS_ToolPanel::OnRepair)] + SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("Force Recreate"))).OnClicked(this, &SMWCS_ToolPanel::OnForceRecreate)] + SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("Generate/Repair Tool EUW"))).OnClicked(this, &SMWCS_ToolPanel::OnGenerateToolEuw)]] + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SSeparator)] + SVerticalBox::Slot().FillHeight(1.0f).Padding(8)[SAssignNew(LogBox, SMultiLineEditableTextBox).IsReadOnly(true).Text(FText::FromString(TEXT("")))]];
    }

private:
    TSharedPtr<SMultiLineEditableTextBox> LogBox;

    void AppendLine(const FString &Line)
    {
        if (!LogBox.IsValid())
        {
            return;
        }
        FString Existing = LogBox->GetText().ToString();
        if (!Existing.IsEmpty())
        {
            Existing += TEXT("\n");
        }
        Existing += Line;
        LogBox->SetText(FText::FromString(Existing));
    }

    void AppendReport(const FString &Title, const FMWCS_Report &Report)
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

    FReply OnValidate()
    {
        AppendReport(TEXT("Validate"), FMWCS_Service::Get().ValidateAll());
        return FReply::Handled();
    }

    FReply OnCreateMissing()
    {
        AppendReport(TEXT("CreateMissing"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::CreateMissing));
        return FReply::Handled();
    }

    FReply OnRepair()
    {
        AppendReport(TEXT("Repair"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::Repair));
        return FReply::Handled();
    }

    FReply OnForceRecreate()
    {
        AppendReport(TEXT("ForceRecreate"), FMWCS_Service::Get().BuildAll(EMWCS_BuildMode::ForceRecreate));
        return FReply::Handled();
    }

    FReply OnGenerateToolEuw()
    {
        AppendReport(TEXT("ToolEUW"), FMWCS_Service::Get().GenerateOrRepairToolEuw());
        return FReply::Handled();
    }
};

static TSharedRef<SDockTab> SpawnMWCSTab(const FSpawnTabArgs &Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
            [SNew(SMWCS_ToolPanel)];
}

void FMWCS_ToolTab::Register()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateStatic(&SpawnMWCSTab)).SetDisplayName(FText::FromString(TEXT("MWCS"))).SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FMWCS_ToolTab::Unregister()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

void FMWCS_ToolTab::Open()
{
    FGlobalTabmanager::Get()->TryInvokeTab(TabName);
}
