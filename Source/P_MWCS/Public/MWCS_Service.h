#pragma once

#include "CoreMinimal.h"

#include "MWCS_Report.h"
#include "MWCS_Types.h"

class FMWCS_Service
{
public:
    static FMWCS_Service &Get();

    FMWCS_Report ValidateAll();
    FMWCS_Report BuildAll(EMWCS_BuildMode Mode);
    FMWCS_Report GenerateOrRepairToolEuw();

    bool SaveReportJson(const FMWCS_Report &Report, const FString &FileLabel) const;
};
