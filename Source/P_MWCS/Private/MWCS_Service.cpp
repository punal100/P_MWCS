#include "MWCS_Service.h"

#include "MWCS_Report.h"
#include "MWCS_Settings.h"
#include "MWCS_WidgetBuilder.h"
#include "MWCS_WidgetRegistry.h"
#include "MWCS_SpecParser.h"
#include "MWCS_WidgetValidator.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FMWCS_Service &FMWCS_Service::Get()
{
    static FMWCS_Service Instance;
    return Instance;
}

static FString TimestampForFilename()
{
    return FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
}

bool FMWCS_Service::SaveReportJson(const FMWCS_Report &Report, const FString &FileLabel) const
{
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MWCS"), TEXT("Reports"));
    IPlatformFile &PF = FPlatformFileManager::Get().GetPlatformFile();
    PF.CreateDirectoryTree(*Dir);

    const FString Filename = FString::Printf(TEXT("MWCS_%s_%s.json"), *FileLabel, *TimestampForFilename());
    const FString FullPath = FPaths::Combine(Dir, Filename);
    const FString Json = MWCS_ReportJson::ToJsonString(Report);
    return FFileHelper::SaveStringToFile(Json, *FullPath);
}

FMWCS_Report FMWCS_Service::ValidateAll()
{
    FMWCS_Report Report;
    TArray<FMWCS_WidgetSpec> Specs;
    FMWCS_WidgetRegistry::CollectSpecs(Specs, Report);
    for (const FMWCS_WidgetSpec &Spec : Specs)
    {
        FMWCS_WidgetValidator::ValidateSpecAsset(Spec, Report);
    }
    SaveReportJson(Report, TEXT("Validate"));
    return Report;
}

FMWCS_Report FMWCS_Service::BuildAll(EMWCS_BuildMode Mode)
{
    FMWCS_Report Report;
    TArray<FMWCS_WidgetSpec> Specs;
    FMWCS_WidgetRegistry::CollectSpecs(Specs, Report);
    for (const FMWCS_WidgetSpec &Spec : Specs)
    {
        FMWCS_WidgetBuilder::CreateOrUpdateFromSpec(Spec, Mode, Report);
    }
    SaveReportJson(Report, TEXT("Build"));
    return Report;
}

FMWCS_Report FMWCS_Service::GenerateOrRepairToolEuw()
{
    FMWCS_Report Report;
    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    if (!Settings)
    {
        return Report;
    }

    UClass *ProviderClass = Settings->ToolEuwSpecProviderClass.TryLoadClass<UObject>();
    if (!ProviderClass)
    {
        FMWCS_Issue Issue;
        Issue.Severity = EMWCS_IssueSeverity::Error;
        Issue.Code = TEXT("ToolEUW.ProviderLoadFailed");
        Issue.Message = TEXT("Failed to load Tool EUW spec provider class.");
        Issue.Context = Settings->ToolEuwSpecProviderClass.ToString();
        Report.Issues.Add(MoveTemp(Issue));
        return Report;
    }

    UFunction *Func = ProviderClass->FindFunctionByName(TEXT("GetWidgetSpec"));
    if (!Func)
    {
        FMWCS_Issue Issue;
        Issue.Severity = EMWCS_IssueSeverity::Error;
        Issue.Code = TEXT("ToolEUW.NoGetWidgetSpec");
        Issue.Message = TEXT("Tool EUW provider missing GetWidgetSpec.");
        Issue.Context = ProviderClass->GetPathName();
        Report.Issues.Add(MoveTemp(Issue));
        return Report;
    }

    struct FReturnValue
    {
        FString ReturnValue;
    };
    FReturnValue Params;
    ProviderClass->GetDefaultObject()->ProcessEvent(Func, &Params);

    FMWCS_WidgetSpec ToolSpec;
    if (!FMWCS_SpecParser::ParseSpecJson(Params.ReturnValue, ToolSpec, Report, ProviderClass->GetPathName()))
    {
        SaveReportJson(Report, TEXT("ToolEUW"));
        return Report;
    }

    FMWCS_WidgetBuilder::CreateOrUpdateToolEuwFromSpec(ToolSpec, EMWCS_BuildMode::Repair, Report);
    SaveReportJson(Report, TEXT("ToolEUW"));
    return Report;
}

FMWCS_Report FMWCS_Service::GenerateOrRepairExternalToolEuw(const FString& ToolName)
{
    FMWCS_Report Report;
    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    if (!Settings)
    {
        return Report;
    }

    const FMWCS_ExternalToolEuwConfig* Config = Settings->FindExternalToolEuw(ToolName);
    if (!Config)
    {
        FMWCS_Issue Issue;
        Issue.Severity = EMWCS_IssueSeverity::Error;
        Issue.Code = TEXT("ExternalToolEUW.NotFound");
        Issue.Message = FString::Printf(TEXT("External Tool EUW '%s' not found in ExternalToolEuws configuration."), *ToolName);
        Issue.Context = TEXT("DefaultEditor.ini [/Script/P_MWCS.MWCS_Settings] ExternalToolEuws");
        Report.Issues.Add(MoveTemp(Issue));
        return Report;
    }

    if (Config->OutputPath.IsEmpty() || Config->AssetName.IsEmpty())
    {
        FMWCS_Issue Issue;
        Issue.Severity = EMWCS_IssueSeverity::Error;
        Issue.Code = TEXT("ExternalToolEUW.SettingsMissing");
        Issue.Message = FString::Printf(TEXT("External Tool EUW '%s' has empty OutputPath or AssetName."), *ToolName);
        Issue.Context = ToolName;
        Report.Issues.Add(MoveTemp(Issue));
        return Report;
    }

    UClass *ProviderClass = Config->SpecProviderClass.TryLoadClass<UObject>();
    if (!ProviderClass)
    {
        FMWCS_Issue Issue;
        Issue.Severity = EMWCS_IssueSeverity::Error;
        Issue.Code = TEXT("ExternalToolEUW.ProviderLoadFailed");
        Issue.Message = FString::Printf(TEXT("Failed to load spec provider class for External Tool EUW '%s'."), *ToolName);
        Issue.Context = Config->SpecProviderClass.ToString();
        Report.Issues.Add(MoveTemp(Issue));
        return Report;
    }

    UFunction *Func = ProviderClass->FindFunctionByName(TEXT("GetWidgetSpec"));
    if (!Func)
    {
        FMWCS_Issue Issue;
        Issue.Severity = EMWCS_IssueSeverity::Error;
        Issue.Code = TEXT("ExternalToolEUW.NoGetWidgetSpec");
        Issue.Message = FString::Printf(TEXT("External Tool EUW '%s' provider missing GetWidgetSpec."), *ToolName);
        Issue.Context = ProviderClass->GetPathName();
        Report.Issues.Add(MoveTemp(Issue));
        return Report;
    }

    struct FReturnValue
    {
        FString ReturnValue;
    };
    FReturnValue Params;
    ProviderClass->GetDefaultObject()->ProcessEvent(Func, &Params);

    FMWCS_WidgetSpec ToolSpec;
    const FString ReportLabel = FString::Printf(TEXT("ExternalToolEUW_%s"), *ToolName);
    if (!FMWCS_SpecParser::ParseSpecJson(Params.ReturnValue, ToolSpec, Report, ProviderClass->GetPathName()))
    {
        SaveReportJson(Report, ReportLabel);
        return Report;
    }

    // Mark as Tool EUW so builder uses EditorUtilityWidgetBlueprint
    ToolSpec.bIsToolEUW = true;

    FMWCS_WidgetBuilder::CreateOrUpdateToolEuwFromSpecWithPath(
        ToolSpec, 
        Config->OutputPath, 
        Config->AssetName, 
        EMWCS_BuildMode::Repair, 
        Report);
    SaveReportJson(Report, ReportLabel);
    return Report;
}
