#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLaserRifle, Log, All);

/**
 * LaserRifleMod runtime module.
 *
 * StartupModule logs a build marker (bumped every build) so the deployed DLL can
 * be verified as fresh + ours — see _team/playbooks/satisfactory-mod.md.
 */
class FLaserRifleModModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual bool IsGameModule() const override { return true; }
};
