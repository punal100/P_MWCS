#include "MWCS_Commandlets.h"

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

int32 UMWCS_ValidateWidgetsCommandlet::Main(const FString &Params)
{
    FMWCS_Report Report = FMWCS_Service::Get().ValidateAll();
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
