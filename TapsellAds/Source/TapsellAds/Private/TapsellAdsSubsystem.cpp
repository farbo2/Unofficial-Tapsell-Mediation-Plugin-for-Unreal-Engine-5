#include "TapsellAdsSubsystem.h"
#include "TapsellAdsSettings.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Misc/Guid.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJavaEnv.h"
#include "Android/AndroidApplication.h"
#include <jni.h>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTapsellAds, Log, All);

namespace
{
	TWeakObjectPtr<UTapsellAdsSubsystem> GTapsellSubsystem;

	enum ETapsellEvent : int32
	{
		EvInitialized = 0,
		EvRequestOk = 1,
		EvRequestFail = 2,
		EvImpression = 3,
		EvClicked = 4,
		EvClosed = 5,
		EvAdFailed = 6,
		EvRewarded = 7,
		EvInitFailed = 8
	};

	ETapsellAdType ToAdType(ETapsellFullscreenAdType Type)
	{
		return static_cast<ETapsellAdType>(static_cast<uint8>(Type));
	}

	ETapsellCompletionState ParseCompletion(const FString& Name)
	{
		if (Name == TEXT("COMPLETED")) return ETapsellCompletionState::Completed;
		if (Name == TEXT("SKIPPED")) return ETapsellCompletionState::Skipped;
		return ETapsellCompletionState::Unknown;
	}
}

#if PLATFORM_ANDROID

namespace TapsellAndroid
{
	static const char* BridgeClass = "com/tapsellue/TapsellBridge";

	static bool ClearPendingException(JNIEnv* Env)
	{
		if (Env && Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			return true;
		}
		return false;
	}

	struct FCall
	{
		JNIEnv* Env = nullptr;
		jclass Class = nullptr;
		jmethodID Method = nullptr;

		FCall(const char* Name, const char* Sig)
		{
			Env = FAndroidApplication::GetJavaEnv();
			if (!Env) return;
			Class = FAndroidApplication::FindJavaClass(BridgeClass);
			if (ClearPendingException(Env)) Class = nullptr;
			if (!Class) { UE_LOG(LogTapsellAds, Error, TEXT("TapsellBridge class not found")); return; }
			Method = Env->GetStaticMethodID(Class, Name, Sig);
			if (ClearPendingException(Env)) Method = nullptr;
			if (!Method) { UE_LOG(LogTapsellAds, Error, TEXT("TapsellBridge method %hs not found"), Name); }
		}

		bool Ok() const { return Env && Class && Method; }

		template <typename... TArgs>
		bool Invoke(TArgs... Args)
		{
			if (!Ok()) return false;
			Env->CallStaticVoidMethod(Class, Method, Args...);
			return !ClearPendingException(Env);
		}

		template <typename... TArgs>
		bool InvokeBool(TArgs... Args)
		{
			if (!Ok()) return false;
			const jboolean Result = Env->CallStaticBooleanMethod(Class, Method, Args...);
			if (ClearPendingException(Env)) return false;
			return Result == JNI_TRUE;
		}
	};

	static void SetDebug(bool bDebug)
	{
		FCall C("setDebug", "(Z)V");
		C.Invoke((jboolean)bDebug);
	}

	static bool Initialize(int32 TimeoutMs)
	{
		FCall C("initialize", "(I)Z");
		return C.InvokeBool((jint)TimeoutMs);
	}

	static void SetUserConsent(bool bConsent)
	{
		FCall C("setUserConsent", "(Z)V");
		C.Invoke((jboolean)bConsent);
	}

	static void RequestAd(int32 Kind, const FString& Zone)
	{
		FCall C("requestAd", "(ILjava/lang/String;)V");
		if (!C.Ok()) return;
		auto JZone = FJavaHelper::ToJavaString(C.Env, Zone);
		C.Invoke((jint)Kind, *JZone);
	}

	static void ShowAd(int32 Kind, const FString& AdId)
	{
		FCall C("showAd", "(ILjava/lang/String;)V");
		if (!C.Ok()) return;
		auto JId = FJavaHelper::ToJavaString(C.Env, AdId);
		C.Invoke((jint)Kind, *JId);
	}

	static void RequestBanner(const FString& Zone, const FString& SizeName)
	{
		FCall C("requestBanner", "(Ljava/lang/String;Ljava/lang/String;)V");
		if (!C.Ok()) return;
		auto JZone = FJavaHelper::ToJavaString(C.Env, Zone);
		auto JSize = FJavaHelper::ToJavaString(C.Env, SizeName);
		C.Invoke(*JZone, *JSize);
	}

	static void ShowBanner(const FString& AdId, int32 Position)
	{
		FCall C("showBanner", "(Ljava/lang/String;I)V");
		if (!C.Ok()) return;
		auto JId = FJavaHelper::ToJavaString(C.Env, AdId);
		C.Invoke(*JId, (jint)Position);
	}

	static void SetBannerVisible(const FString& AdId, bool bVisible)
	{
		FCall C("setBannerVisible", "(Ljava/lang/String;Z)V");
		if (!C.Ok()) return;
		auto JId = FJavaHelper::ToJavaString(C.Env, AdId);
		C.Invoke(*JId, (jboolean)bVisible);
	}

	static void DestroyBanner(const FString& AdId)
	{
		FCall C("destroyBanner", "(Ljava/lang/String;)V");
		if (!C.Ok()) return;
		auto JId = FJavaHelper::ToJavaString(C.Env, AdId);
		C.Invoke(*JId);
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_tapsellue_TapsellBridge_nativeOnEvent(
	JNIEnv* Env, jclass, jint Event, jint Kind, jstring Id, jstring Extra)
{
	const FString IdStr = FJavaHelper::FStringFromParam(Env, Id);
	const FString ExtraStr = FJavaHelper::FStringFromParam(Env, Extra);
	UTapsellAdsSubsystem::DispatchNativeEvent((int32)Event, (int32)Kind, IdStr, ExtraStr);
}

#endif

void UTapsellAdsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GTapsellSubsystem = this;

	if (GetDefault<UTapsellAdsSettings>()->bAutoInitialize)
	{
		InitializeTapsell();
	}
}

void UTapsellAdsSubsystem::Deinitialize()
{
	if (GTapsellSubsystem.Get() == this)
	{
		GTapsellSubsystem.Reset();
	}
	Super::Deinitialize();
}

bool UTapsellAdsSubsystem::IsVerbose() const
{
	return GetDefault<UTapsellAdsSettings>()->bVerboseLogging;
}

void UTapsellAdsSubsystem::DispatchNativeEvent(int32 Event, int32 Kind, const FString& Id, const FString& Extra)
{
	AsyncTask(ENamedThreads::GameThread, [Event, Kind, Id, Extra]()
	{
		if (UTapsellAdsSubsystem* S = GTapsellSubsystem.Get())
		{
			S->HandleEvent(Event, Kind, Id, Extra);
		}
	});
}

void UTapsellAdsSubsystem::FailInit(const FString& Message)
{
	InitState = EInitState::Failed;
	UE_LOG(LogTapsellAds, Warning, TEXT("Initialization failed: %s"), *Message);
	OnInitializationFailed.Broadcast(Message);
}

bool UTapsellAdsSubsystem::Reject(ETapsellAdType AdType, const FString& Id, bool bIsRequest, const FString& Message)
{
	UE_LOG(LogTapsellAds, Warning, TEXT("%s"), *Message);
	if (bIsRequest)
	{
		OnRequestFailed.Broadcast(AdType, Id, Message);
	}
	else
	{
		OnAdFailed.Broadcast(AdType, Id, Message);
	}
	return false;
}

bool UTapsellAdsSubsystem::EnsureReady(ETapsellAdType AdType, const FString& Id, bool bIsRequest)
{
	if (Id.IsEmpty())
	{
		return Reject(AdType, Id, bIsRequest, bIsRequest ? TEXT("Zone id is empty") : TEXT("Ad id is empty"));
	}
	if (InitState != EInitState::Ready)
	{
		return Reject(AdType, Id, bIsRequest, TEXT("Tapsell is not initialized"));
	}
	return true;
}

void UTapsellAdsSubsystem::HandleEvent(int32 Event, int32 Kind, const FString& Id, const FString& Extra)
{
	const ETapsellAdType Type = static_cast<ETapsellAdType>(FMath::Clamp(Kind, 0, 3));
	if (IsVerbose())
	{
		UE_LOG(LogTapsellAds, Log, TEXT("Event %d kind %d id '%s' extra '%s'"), Event, Kind, *Id, *Extra);
	}

	switch (Event)
	{
	case EvInitialized:
		InitState = EInitState::Ready;
		OnInitialized.Broadcast();
		break;
	case EvInitFailed:
		FailInit(Extra);
		break;
	case EvRequestOk:
		OnRequestSucceeded.Broadcast(Type, Id, Extra);
		break;
	case EvRequestFail:
		UE_LOG(LogTapsellAds, Warning, TEXT("Ad request failed (zone %s): %s"), *Id, *Extra);
		OnRequestFailed.Broadcast(Type, Id, Extra);
		break;
	case EvImpression:
		OnAdImpression.Broadcast(Type, Id);
		break;
	case EvClicked:
		OnAdClicked.Broadcast(Type, Id);
		break;
	case EvClosed:
		OnAdClosed.Broadcast(Type, Id, ParseCompletion(Extra));
		break;
	case EvAdFailed:
		UE_LOG(LogTapsellAds, Warning, TEXT("Ad show failed (%s): %s"), *Id, *Extra);
		OnAdFailed.Broadcast(Type, Id, Extra);
		break;
	case EvRewarded:
		OnRewarded.Broadcast(Type, Id);
		break;
	default:
		break;
	}
}

#if !PLATFORM_ANDROID
void UTapsellAdsSubsystem::RunAfterLatency(TFunction<void()> Fn)
{
	TWeakObjectPtr<UTapsellAdsSubsystem> Weak(this);
	const float Latency = GetDefault<UTapsellAdsSettings>()->SimulatedLatency;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Fn](float)
	{
		if (Weak.IsValid()) { Fn(); }
		return false;
	}), Latency);
}
#endif

void UTapsellAdsSubsystem::InitializeTapsell()
{
	if (InitState == EInitState::Initializing || InitState == EInitState::Ready)
	{
		return;
	}

#if PLATFORM_ANDROID
	const UTapsellAdsSettings* Settings = GetDefault<UTapsellAdsSettings>();
	if (Settings->AppKey.IsEmpty())
	{
		FailInit(TEXT("App Key is empty. Set it in Project Settings > Plugins > Tapsell Ads."));
		return;
	}

	InitState = EInitState::Initializing;
	TapsellAndroid::SetDebug(IsVerbose());
	const int32 TimeoutMs = FMath::RoundToInt(FMath::Max(2.f, Settings->InitTimeoutSeconds) * 1000.f);
	if (!TapsellAndroid::Initialize(TimeoutMs))
	{
		FailInit(TEXT("Could not start the Tapsell SDK (Java bridge or Activity unavailable)."));
	}
#else
	InitState = EInitState::Initializing;
	if (GetDefault<UTapsellAdsSettings>()->bSimulateWhenNotOnDevice)
	{
		RunAfterLatency([this]() { HandleEvent(EvInitialized, 0, TEXT(""), TEXT("")); });
	}
	else
	{
		FailInit(TEXT("Tapsell is only available on Android."));
	}
#endif
}

void UTapsellAdsSubsystem::SetUserConsent(bool bConsent)
{
#if PLATFORM_ANDROID
	TapsellAndroid::SetUserConsent(bConsent);
#endif
}

void UTapsellAdsSubsystem::RequestAd(ETapsellFullscreenAdType AdType, const FString& ZoneId)
{
	if (!EnsureReady(ToAdType(AdType), ZoneId, true))
	{
		return;
	}
#if PLATFORM_ANDROID
	TapsellAndroid::RequestAd(static_cast<int32>(AdType), ZoneId);
#else
	if (GetDefault<UTapsellAdsSettings>()->bSimulateWhenNotOnDevice)
	{
		RunAfterLatency([this, AdType, ZoneId]()
		{
			HandleEvent(EvRequestOk, static_cast<int32>(AdType), ZoneId, FGuid::NewGuid().ToString(EGuidFormats::Digits));
		});
	}
#endif
}

void UTapsellAdsSubsystem::ShowAd(ETapsellFullscreenAdType AdType, const FString& AdId)
{
	if (!EnsureReady(ToAdType(AdType), AdId, false))
	{
		return;
	}
#if PLATFORM_ANDROID
	TapsellAndroid::ShowAd(static_cast<int32>(AdType), AdId);
#else
	if (GetDefault<UTapsellAdsSettings>()->bSimulateWhenNotOnDevice)
	{
		const int32 Kind = static_cast<int32>(AdType);
		RunAfterLatency([this, Kind, AdId]()
		{
			HandleEvent(EvImpression, Kind, AdId, TEXT(""));
			if (Kind == static_cast<int32>(ETapsellAdType::Rewarded))
			{
				HandleEvent(EvRewarded, Kind, AdId, TEXT(""));
			}
			HandleEvent(EvClosed, Kind, AdId, TEXT("COMPLETED"));
		});
	}
#endif
}

void UTapsellAdsSubsystem::RequestBanner(const FString& ZoneId, ETapsellBannerSize Size)
{
	if (!EnsureReady(ETapsellAdType::Banner, ZoneId, true))
	{
		return;
	}
#if PLATFORM_ANDROID
	static const TCHAR* Names[] = {
		TEXT("BANNER_300_50"), TEXT("BANNER_320_50"), TEXT("BANNER_320_90"), TEXT("BANNER_320_100"),
		TEXT("BANNER_250_250"), TEXT("BANNER_300_250"), TEXT("BANNER_468_60"), TEXT("BANNER_728_90"),
		TEXT("BANNER_160_600"), TEXT("BANNER_ADAPTIVE") };
	const int32 Index = static_cast<int32>(Size);
	if (Index < 0 || Index >= UE_ARRAY_COUNT(Names))
	{
		Reject(ETapsellAdType::Banner, ZoneId, true, TEXT("Invalid banner size"));
		return;
	}
	TapsellAndroid::RequestBanner(ZoneId, Names[Index]);
#else
	if (GetDefault<UTapsellAdsSettings>()->bSimulateWhenNotOnDevice)
	{
		RunAfterLatency([this, ZoneId]()
		{
			HandleEvent(EvRequestOk, static_cast<int32>(ETapsellAdType::Banner), ZoneId, FGuid::NewGuid().ToString(EGuidFormats::Digits));
		});
	}
#endif
}

void UTapsellAdsSubsystem::ShowBanner(const FString& AdId, ETapsellBannerPosition Position)
{
	if (!EnsureReady(ETapsellAdType::Banner, AdId, false))
	{
		return;
	}
#if PLATFORM_ANDROID
	TapsellAndroid::ShowBanner(AdId, static_cast<int32>(Position));
#else
	if (GetDefault<UTapsellAdsSettings>()->bSimulateWhenNotOnDevice)
	{
		RunAfterLatency([this, AdId]()
		{
			HandleEvent(EvImpression, static_cast<int32>(ETapsellAdType::Banner), AdId, TEXT(""));
		});
	}
#endif
}

void UTapsellAdsSubsystem::SetBannerVisible(const FString& AdId, bool bVisible)
{
#if PLATFORM_ANDROID
	TapsellAndroid::SetBannerVisible(AdId, bVisible);
#endif
}

void UTapsellAdsSubsystem::DestroyBanner(const FString& AdId)
{
#if PLATFORM_ANDROID
	TapsellAndroid::DestroyBanner(AdId);
#endif
}
