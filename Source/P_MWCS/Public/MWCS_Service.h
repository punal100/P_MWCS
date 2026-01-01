#pragma once

#include "CoreMinimal.h"

#include "MWCS_Report.h"
#include "MWCS_Types.h"

class P_MWCS_API FMWCS_Service
{
public:
    static FMWCS_Service &Get();

    FMWCS_Report ValidateAll();
    FMWCS_Report BuildAll(EMWCS_BuildMode Mode);
    FMWCS_Report GenerateOrRepairToolEuw();
    
    /** Generate or repair an external Tool EUW by name (looks up config in ExternalToolEuws array) */
    FMWCS_Report GenerateOrRepairExternalToolEuw(const FString& ToolName);

    bool SaveReportJson(const FMWCS_Report &Report, const FString &FileLabel) const;
};

