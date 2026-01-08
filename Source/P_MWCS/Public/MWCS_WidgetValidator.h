#pragma once

#include "CoreMinimal.h"

#include "MWCS_Report.h"
#include "MWCS_WidgetSpec.h"

class UWidgetBlueprint;

class FMWCS_WidgetValidator
{
public:
    /**
     * Validates a generated widget blueprint against its spec.
     * This is the main validation entry point that checks:
     * - Hierarchy structure and widget types
     * - Design section properties
     * - Designer preview settings
     * - Required bindings
     */
    static bool ValidateSpecAsset(const FMWCS_WidgetSpec &Spec, FMWCS_Report &InOutReport);
    
    /**
     * Validates hierarchy-level properties for a generated widget.
     * This checks properties specified directly in hierarchy nodes:
     * - Border: BrushColor, Padding
     * - TextBlock: Text, FontSize, Justification
     * Called automatically as part of ValidateSpecAsset.
     */
    static void ValidateHierarchyProperties(
        const FMWCS_WidgetSpec &Spec,
        UWidgetBlueprint *BP,
        FMWCS_Report &InOutReport,
        const FString &Context);
    
    /**
     * Post-generation validation for a batch of normal widgets.
     * Called automatically by MWCS_Service after BuildAll completes.
     * @param Specs - All specs that were built  
     * @param Report - Report to accumulate validation issues
     * @param bLogSummary - If true, logs a summary of validation results
     * @return true if all validations passed
     */
    static bool ValidatePostGeneration(
        const TArray<FMWCS_WidgetSpec> &Specs,
        FMWCS_Report &InOutReport,
        bool bLogSummary = true);
    
    /**
     * Post-generation validation for a single Tool EUW.
     * Called automatically by MWCS_Service after EUW generation.
     * @param Spec - The tool spec that was built
     * @param Report - Report to accumulate validation issues
     * @param OutputPath - Package path where the EUW was created
     * @param AssetName - Name of the generated asset
     * @return true if validation passed
     */
    static bool ValidateToolEuwPostGeneration(
        const FMWCS_WidgetSpec &Spec,
        FMWCS_Report &InOutReport,
        const FString &OutputPath,
        const FString &AssetName);
};
