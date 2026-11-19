#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TapsellAdsSettings.generated.h"

UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "Tapsell Ads"))
class TAPSELLADS_API UTapsellAdsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTapsellAdsSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Application key from the Tapsell panel. Written into the Android manifest at build time. */
	UPROPERTY(Config, EditAnywhere, Category = "Tapsell")
	FString AppKey;

	/** Version of ir.tapsell:tapsell and the legacy adapter. Read at build time. */
	UPROPERTY(Config, EditAnywhere, Category = "Tapsell")
	FString SdkVersion;

	/** Initialize the SDK when the game instance starts. If off, call Initialize Tapsell yourself (e.g. after consent). */
	UPROPERTY(Config, EditAnywhere, Category = "Tapsell")
	bool bAutoInitialize;

	/** Seconds to wait for the SDK to confirm initialization before On Initialization Failed fires. */
	UPROPERTY(Config, EditAnywhere, Category = "Tapsell", meta = (ClampMin = "2.0"))
	float InitTimeoutSeconds;

	/** Log SDK bridge events (Android logcat tag "TapsellBridge" and LogTapsellAds). Warnings and errors are always logged. */
	UPROPERTY(Config, EditAnywhere, Category = "Tapsell")
	bool bVerboseLogging;

	/** On platforms other than Android (editor, Windows), fire fake callbacks so ad flows can be tested. */
	UPROPERTY(Config, EditAnywhere, Category = "Simulation")
	bool bSimulateWhenNotOnDevice;

	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "0.0"))
	float SimulatedLatency;
};
