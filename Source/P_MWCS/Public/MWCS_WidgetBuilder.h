#pragma once

#include "CoreMinimal.h"

#include "MWCS_Report.h"
#include "MWCS_Types.h"
#include "MWCS_WidgetSpec.h"

class UWidgetBlueprint;

class FMWCS_WidgetBuilder
{
public:
    static bool CreateOrUpdateFromSpec(const FMWCS_WidgetSpec &Spec, EMWCS_BuildMode Mode, FMWCS_Report &InOutReport);
    static bool CreateOrUpdateToolEuwFromSpec(const FMWCS_WidgetSpec &Spec, EMWCS_BuildMode Mode, FMWCS_Report &InOutReport);
};
