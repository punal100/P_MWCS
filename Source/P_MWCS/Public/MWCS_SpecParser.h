#pragma once

#include "CoreMinimal.h"

#include "MWCS_Report.h"
#include "MWCS_WidgetSpec.h"

class FMWCS_SpecParser
{
public:
    static bool ParseSpecJson(const FString &JsonString, FMWCS_WidgetSpec &OutSpec, FMWCS_Report &InOutReport, const FString &Context);
};
