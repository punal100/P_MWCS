#include "MWCS_Settings.h"

#include "MWCS_ToolWidgetSpecProvider.h"

UMWCS_Settings::UMWCS_Settings()
{
    OutputRootPath = TEXT("/Game/UI/Widgets");
    ToolEuwOutputPath = TEXT("/Game/Editor/MWCS");
    ToolEuwAssetName = TEXT("EUW_MWCS_Tool");
    ToolEuwSpecProviderClass = FSoftClassPath(TEXT("/Script/P_MWCS.MWCS_ToolWidgetSpecProvider"));

    DesignerZoomLevelMin = 0;
    DesignerZoomLevelMax = 20;
}

const UMWCS_Settings *UMWCS_Settings::Get()
{
    return GetDefault<UMWCS_Settings>();
}

const FMWCS_ExternalToolEuwConfig* UMWCS_Settings::FindExternalToolEuw(const FString& ToolName) const
{
    for (const FMWCS_ExternalToolEuwConfig& Config : ExternalToolEuws)
    {
        if (Config.ToolName.Equals(ToolName, ESearchCase::IgnoreCase))
        {
            return &Config;
        }
    }
    return nullptr;
}
