#include "MWCS_Commandlets.h"

#include "MWCS_Report.h"
#include "MWCS_Service.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

static bool ShouldFailOnWarnings(const FString &Params)
{
    return Params.Contains(TEXT("-FailOnWarnings"));
}

static bool ShouldFailOnErrors(const FString &Params)
{
    return Params.Contains(TEXT("-FailOnErrors"));
}

static EMWCS_BuildMode ParseMode(const FString &Params)
{
    FString Mode;
    if (FParse::Value(*Params, TEXT("-Mode="), Mode))
    {
        if (Mode.Equals(TEXT("Repair"), ESearchCase::IgnoreCase))
            return EMWCS_BuildMode::Repair;
        if (Mode.Equals(TEXT("ForceRecreate"), ESearchCase::IgnoreCase))
            return EMWCS_BuildMode::ForceRecreate;
    }
    return EMWCS_BuildMode::CreateMissing;
}

static void LogReportToOutput(const FMWCS_Report &Report)
{
    UE_LOG(LogTemp, Display, TEXT("MWCS Report: %d error(s), %d warning(s); SpecsProcessed=%d, AssetsCreated=%d, AssetsRepaired=%d, AssetsRecreated=%d"),
           Report.NumErrors(),
           Report.NumWarnings(),
           Report.SpecsProcessed,
           Report.AssetsCreated,
           Report.AssetsRepaired,
           Report.AssetsRecreated);

    for (const FMWCS_Issue &Issue : Report.Issues)
    {
        const FString Ctx = Issue.Context.IsEmpty() ? TEXT("<no context>") : Issue.Context;
        if (Issue.Severity == EMWCS_IssueSeverity::Error)
        {
            UE_LOG(LogTemp, Error, TEXT("MWCS[%s] %s | %s"), *Issue.Code, *Ctx, *Issue.Message);
        }
        else if (Issue.Severity == EMWCS_IssueSeverity::Warning)
        {
            UE_LOG(LogTemp, Warning, TEXT("MWCS[%s] %s | %s"), *Issue.Code, *Ctx, *Issue.Message);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("MWCS[%s] %s | %s"), *Issue.Code, *Ctx, *Issue.Message);
        }
    }
}

int32 UMWCS_ValidateWidgetsCommandlet::Main(const FString &Params)
{
    FMWCS_Report Report = FMWCS_Service::Get().ValidateAll();
    LogReportToOutput(Report);
    const bool bFailWarnings = ShouldFailOnWarnings(Params);
    const bool bFailErrors = ShouldFailOnErrors(Params);
    if (bFailErrors && Report.HasErrors())
    {
        return 1;
    }
    if (bFailWarnings && Report.HasWarnings())
    {
        return 2;
    }
    return 0;
}

int32 UMWCS_CreateWidgetsCommandlet::Main(const FString &Params)
{
    const EMWCS_BuildMode Mode = ParseMode(Params);
    FMWCS_Report Report = FMWCS_Service::Get().BuildAll(Mode);
    LogReportToOutput(Report);
    const bool bFailWarnings = ShouldFailOnWarnings(Params);
    const bool bFailErrors = ShouldFailOnErrors(Params);
    if (bFailErrors && Report.HasErrors())
    {
        return 1;
    }
    if (bFailWarnings && Report.HasWarnings())
    {
        return 2;
    }
    return 0;
}
