#include "MWCS_WidgetRegistry.h"

#include "MWCS_Settings.h"
#include "MWCS_SpecParser.h"

#include "UObject/Class.h"

static void AddIssue(FMWCS_Report &Report, EMWCS_IssueSeverity Severity, const FString &Code, const FString &Message, const FString &Context)
{
    FMWCS_Issue Issue;
    Issue.Severity = Severity;
    Issue.Code = Code;
    Issue.Message = Message;
    Issue.Context = Context;
    Report.Issues.Add(MoveTemp(Issue));
}

static bool CallGetWidgetSpec(UClass *ProviderClass, FString &OutJson)
{
    if (!ProviderClass)
    {
        return false;
    }
    UFunction *Func = ProviderClass->FindFunctionByName(TEXT("GetWidgetSpec"));
    if (!Func)
    {
        return false;
    }
    UObject *CDO = ProviderClass->GetDefaultObject();
    if (!CDO)
    {
        return false;
    }

    struct FReturnValue
    {
        FString ReturnValue;
    };

    FReturnValue Params;
    CDO->ProcessEvent(Func, &Params);
    OutJson = MoveTemp(Params.ReturnValue);
    return true;
}

bool FMWCS_WidgetRegistry::CollectSpecs(TArray<FMWCS_WidgetSpec> &OutSpecs, FMWCS_Report &InOutReport)
{
    OutSpecs.Reset();

    const UMWCS_Settings *Settings = UMWCS_Settings::Get();
    if (!Settings)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Settings.Missing"), TEXT("MWCS settings not available."), TEXT("Registry"));
        return false;
    }

    TArray<FSoftClassPath> Providers = Settings->SpecProviderClasses;
    if (Providers.Num() == 0)
    {
        AddIssue(InOutReport, EMWCS_IssueSeverity::Warning, TEXT("Registry.NoProviders"), TEXT("No SpecProviderClasses configured."), TEXT("Registry"));
        return true;
    }

    for (const FSoftClassPath &ProviderPath : Providers)
    {
        UClass *ProviderClass = ProviderPath.TryLoadClass<UObject>();
        const FString Context = ProviderPath.ToString();
        if (!ProviderClass)
        {
            AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Registry.ProviderLoadFailed"), TEXT("Failed to load provider class."), Context);
            continue;
        }

        FString Json;
        if (!CallGetWidgetSpec(ProviderClass, Json))
        {
            AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Registry.MissingGetWidgetSpec"), TEXT("Provider missing callable static GetWidgetSpec()."), Context);
            continue;
        }
        if (Json.IsEmpty())
        {
            AddIssue(InOutReport, EMWCS_IssueSeverity::Error, TEXT("Registry.EmptySpec"), TEXT("GetWidgetSpec returned empty JSON."), Context);
            continue;
        }

        FMWCS_WidgetSpec Spec;
        if (FMWCS_SpecParser::ParseSpecJson(Json, Spec, InOutReport, Context))
        {
            OutSpecs.Add(MoveTemp(Spec));
        }
    }

    InOutReport.SpecsProcessed = OutSpecs.Num();
    return true;
}
