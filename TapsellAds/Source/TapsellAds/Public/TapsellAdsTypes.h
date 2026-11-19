#pragma once

#include "CoreMinimal.h"
#include "TapsellAdsTypes.generated.h"

UENUM(BlueprintType)
enum class ETapsellAdType : uint8
{
	Interstitial = 0,
	Rewarded = 1,
	AppOpen = 2,
	Banner = 3
};

UENUM(BlueprintType)
enum class ETapsellFullscreenAdType : uint8
{
	Interstitial = 0,
	Rewarded = 1,
	AppOpen = 2
};

UENUM(BlueprintType)
enum class ETapsellCompletionState : uint8
{
	Completed,
	Skipped,
	Unknown
};

UENUM(BlueprintType)
enum class ETapsellBannerPosition : uint8
{
	Top = 0,
	Bottom = 1,
	TopLeft = 2,
	TopRight = 3,
	BottomLeft = 4,
	BottomRight = 5,
	Center = 6
};

UENUM(BlueprintType)
enum class ETapsellBannerSize : uint8
{
	Banner_300x50,
	Banner_320x50,
	Banner_320x90,
	Banner_320x100,
	Banner_250x250,
	Banner_300x250,
	Banner_468x60,
	Banner_728x90,
	Banner_160x600,
	Adaptive
};
