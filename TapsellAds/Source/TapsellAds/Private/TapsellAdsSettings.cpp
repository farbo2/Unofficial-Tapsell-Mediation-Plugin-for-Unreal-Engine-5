#include "TapsellAdsSettings.h"

UTapsellAdsSettings::UTapsellAdsSettings()
	: SdkVersion(TEXT("1.4.0-alpha04"))
	, bAutoInitialize(true)
	, InitTimeoutSeconds(10.f)
	, bVerboseLogging(false)
	, bSimulateWhenNotOnDevice(true)
	, SimulatedLatency(0.4f)
{
}
