#pragma once

#include "Modules/ModuleManager.h"

class FP_MWCS_RuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
