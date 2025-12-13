#include "MWCS_ToolMenus.h"

#include "MWCS_ToolTab.h"

#include "ToolMenus.h"

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
    FToolMenuSection &Section = Menu->FindOrAddSection(TEXT("MWCS"));
    Section.AddMenuEntry(
        TEXT("MWCS.Open"),
        FText::FromString(TEXT("MWCS")),
        FText::FromString(TEXT("Open Modular Widget Creation System")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FMWCS_ToolTab::Open)));
}

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
