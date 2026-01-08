#include "MWCS_ToolMenus.h"

#include "MWCS_ToolTab.h"

#include "ToolMenus.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "MWCS_ToolMenus"

static void *GMWCSMenuOwner = nullptr;
static FDelegateHandle GMWCS_MenuRegistrationHandle;

static void RegisterMWCSMenus()
{
    if (!GMWCSMenuOwner)
    {
        return;
    }
    FToolMenuOwnerScoped OwnerScoped(GMWCSMenuOwner);
    UToolMenu *Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
    FToolMenuSection &Section = Menu->FindOrAddSection(TEXT("MWCSTools"));
    Section.Label = LOCTEXT("MWCSSectionLabel", "MWCS Widget Creation System");
    Section.AddMenuEntry(
        TEXT("MWCS.Open"),
        LOCTEXT("MWCSOpenLabel", "MWCS Widget Creation System"),
        LOCTEXT("MWCSOpenTooltip", "Open MWCS Widget Creation System"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "WidgetDesigner.LayoutTransform"),
        FUIAction(FExecuteAction::CreateStatic(&FMWCS_ToolTab::Open)));
}

#undef LOCTEXT_NAMESPACE

FDelegateHandle MWCS_RegisterToolMenus(void *Owner)
{
    if (Owner == nullptr)
    {
        return FDelegateHandle();
    }

    GMWCSMenuOwner = Owner;
    if (!GMWCS_MenuRegistrationHandle.IsValid())
    {
        GMWCS_MenuRegistrationHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMWCSMenus));
    }

    return GMWCS_MenuRegistrationHandle;
}

void MWCS_UnregisterToolMenus(FDelegateHandle Handle, void *Owner)
{
    if (Handle.IsValid())
    {
        UToolMenus::UnRegisterStartupCallback(Handle);
    }

    if (Owner)
    {
        UToolMenus::UnregisterOwner(FToolMenuOwner(Owner));
    }

    GMWCSMenuOwner = nullptr;
    GMWCS_MenuRegistrationHandle.Reset();
}
