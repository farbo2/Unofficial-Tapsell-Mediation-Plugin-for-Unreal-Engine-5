#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TapsellAdsTypes.h"
#include "TapsellAdsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTapsellInitialized);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTapsellInitializationFailed, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTapsellRequestSucceeded, ETapsellAdType, AdType, const FString&, ZoneId, const FString&, AdId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTapsellRequestFailed, ETapsellAdType, AdType, const FString&, ZoneId, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTapsellAdEvent, ETapsellAdType, AdType, const FString&, AdId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTapsellAdClosed, ETapsellAdType, AdType, const FString&, AdId, ETapsellCompletionState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTapsellAdFailed, ETapsellAdType, AdType, const FString&, AdId, const FString&, Message);

UCLASS()
class TAPSELLADS_API UTapsellAdsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Starts the SDK. Fires OnInitialized when the SDK confirms, or OnInitializationFailed. Call again to retry after a failure. */
	UFUNCTION(BlueprintCallable, Category = "Tapsell|Initialization")
	void InitializeTapsell();

	UFUNCTION(BlueprintCallable, Category = "Tapsell|Initialization")
	void SetUserConsent(bool bConsent);

	UFUNCTION(BlueprintPure, Category = "Tapsell|Initialization")
	bool IsTapsellInitialized() const { return InitState == EInitState::Ready; }

	/** Fails with OnRequestFailed until the SDK is initialized. */
	UFUNCTION(BlueprintCallable, Category = "Tapsell|Ads")
	void RequestAd(ETapsellFullscreenAdType AdType, const FString& ZoneId);

	/** Each AdId can be shown once. */
	UFUNCTION(BlueprintCallable, Category = "Tapsell|Ads")
	void ShowAd(ETapsellFullscreenAdType AdType, const FString& AdId);

	UFUNCTION(BlueprintCallable, Category = "Tapsell|Banner")
	void RequestBanner(const FString& ZoneId, ETapsellBannerSize Size);

	UFUNCTION(BlueprintCallable, Category = "Tapsell|Banner")
	void ShowBanner(const FString& AdId, ETapsellBannerPosition Position);

	/** Hides or shows a banner without destroying it. */
	UFUNCTION(BlueprintCallable, Category = "Tapsell|Banner")
	void SetBannerVisible(const FString& AdId, bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Tapsell|Banner")
	void DestroyBanner(const FString& AdId);

	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Initialization")
	FTapsellInitialized OnInitialized;

	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Initialization")
	FTapsellInitializationFailed OnInitializationFailed;

	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Ads")
	FTapsellRequestSucceeded OnRequestSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Ads")
	FTapsellRequestFailed OnRequestFailed;

	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Ads")
	FTapsellAdEvent OnAdImpression;

	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Ads")
	FTapsellAdEvent OnAdClicked;

	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Ads")
	FTapsellAdClosed OnAdClosed;

	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Ads")
	FTapsellAdFailed OnAdFailed;

	/** Rewarded ads only. */
	UPROPERTY(BlueprintAssignable, Category = "Tapsell|Events|Ads")
	FTapsellAdEvent OnRewarded;

	static void DispatchNativeEvent(int32 Event, int32 Kind, const FString& Id, const FString& Extra);

private:
	void HandleEvent(int32 Event, int32 Kind, const FString& Id, const FString& Extra);
	bool IsVerbose() const;

#if !PLATFORM_ANDROID
	void RunAfterLatency(TFunction<void()> Fn);
#endif

	enum class EInitState : uint8 { NotStarted, Initializing, Ready, Failed };

	void FailInit(const FString& Message);
	bool Reject(ETapsellAdType AdType, const FString& Id, bool bIsRequest, const FString& Message);
	bool EnsureReady(ETapsellAdType AdType, const FString& Id, bool bIsRequest);

	EInitState InitState = EInitState::NotStarted;
};
