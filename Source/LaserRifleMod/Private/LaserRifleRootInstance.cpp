#include "LaserRifleRootInstance.h"
#include "LaserRifleSessionSettings.h"
#include "SessionSettings/SessionSetting.h"

ULaserRifleRootInstance::ULaserRifleRootInstance()
{
	bRootModule = true;

	// Build the SML session settings as Instanced subobjects of this module CDO;
	// RegisterDefaultContent (called by SML) registers each one.
	LaserRifleSettings::BuildSessionSettings(this, SessionSettings);
}
