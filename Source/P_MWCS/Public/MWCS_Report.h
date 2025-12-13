#pragma once

#include "CoreMinimal.h"
#include "MWCS_Types.h"

struct FMWCS_Issue
{
    EMWCS_IssueSeverity Severity = EMWCS_IssueSeverity::Info;
    FString Code;
    FString Message;
    FString Context;
};

struct FMWCS_Report
{
    TArray<FMWCS_Issue> Issues;
    int32 SpecsProcessed = 0;
    int32 AssetsCreated = 0;
    int32 AssetsRepaired = 0;
    int32 AssetsRecreated = 0;

    int32 NumErrors() const;
    int32 NumWarnings() const;
    bool HasErrors() const;
    bool HasWarnings() const;
};

namespace MWCS_ReportJson
{
    FString ToJsonString(const FMWCS_Report &Report);
}
