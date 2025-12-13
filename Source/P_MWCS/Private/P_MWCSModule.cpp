#include "Modules/ModuleManager.h"

#include "MWCS_ToolMenus.h"
#include "MWCS_ToolTab.h"

#include "ToolMenus.h"

class FP_MWCSModule : public IModuleInterface
{
public:
    FDelegateHandle ToolMenusHandle;

    virtual void StartupModule() override
    {
        FMWCS_ToolTab::Register();
        ToolMenusHandle = MWCS_RegisterToolMenus(this);
    }

    virtual void ShutdownModule() override
    {
        MWCS_UnregisterToolMenus(ToolMenusHandle, this);
        FMWCS_ToolTab::Unregister();
    }
};

IMPLEMENT_MODULE(FP_MWCSModule, P_MWCS)
