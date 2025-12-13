#pragma once

#include "CoreMinimal.h"

FDelegateHandle MWCS_RegisterToolMenus(void *Owner);
void MWCS_UnregisterToolMenus(FDelegateHandle Handle, void *Owner);
