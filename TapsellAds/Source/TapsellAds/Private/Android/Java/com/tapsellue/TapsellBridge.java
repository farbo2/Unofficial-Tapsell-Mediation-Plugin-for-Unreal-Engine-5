package com.tapsellue;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

import ir.tapsell.mediation.MediationInitializationListener;
import ir.tapsell.mediation.Tapsell;
import ir.tapsell.mediation.ad.AdStateListener;
import ir.tapsell.mediation.ad.request.BannerSize;
import ir.tapsell.mediation.ad.request.RequestResultListener;
import ir.tapsell.mediation.ad.show.AdShowCompletionState;
import ir.tapsell.mediation.ad.views.banner.BannerContainer;

public class TapsellBridge {

	private static final String TAG = "TapsellBridge";

	public static final int KIND_INTERSTITIAL = 0;
	public static final int KIND_REWARDED = 1;
	public static final int KIND_APP_OPEN = 2;
	public static final int KIND_BANNER = 3;

	private static final int POS_TOP = 0;
	private static final int POS_BOTTOM = 1;
	private static final int POS_TOP_LEFT = 2;
	private static final int POS_TOP_RIGHT = 3;
	private static final int POS_BOTTOM_LEFT = 4;
	private static final int POS_BOTTOM_RIGHT = 5;
	private static final int POS_CENTER = 6;

	private static final int EV_INITIALIZED = 0;
	private static final int EV_REQUEST_OK = 1;
	private static final int EV_REQUEST_FAIL = 2;
	private static final int EV_IMPRESSION = 3;
	private static final int EV_CLICKED = 4;
	private static final int EV_CLOSED = 5;
	private static final int EV_AD_FAILED = 6;
	private static final int EV_REWARDED = 7;
	private static final int EV_INIT_FAILED = 8;

	private static volatile boolean sDebug = false;
	private static volatile Activity sActivity;
	private static final Map<String, FrameLayout> sBanners = new HashMap<>();
	private static final AtomicInteger sInitGeneration = new AtomicInteger(0);
	private static final AtomicBoolean sInitFinished = new AtomicBoolean(false);
	private static final Handler sInitHandler = new Handler(Looper.getMainLooper());
	private static boolean sSdkInitCalled = false;

	public static native void nativeOnEvent(int event, int kind, String id, String extra);

	public static void setDebug(boolean debug) {
		sDebug = debug;
	}

	private static void emit(int event, int kind, String id, String extra) {
		if (sDebug) {
			Log.d(TAG, "event=" + event + " kind=" + kind + " id=" + id + " extra=" + extra);
		}
		try {
			nativeOnEvent(event, kind, id, extra);
		} catch (Throwable e) {
			Log.e(TAG, "nativeOnEvent failed", e);
		}
	}

	public static void attach(Activity activity) {
		sActivity = activity;
	}

	public static void detach(Activity activity) {
		if (sActivity != activity) {
			return;
		}
		sActivity = null;
		Runnable cleanup = guarded(() -> {
			for (String adId : new ArrayList<>(sBanners.keySet())) {
				removeBanner(adId);
				try {
					Tapsell.destroyBannerAd(adId);
				} catch (RuntimeException e) {
					Log.w(TAG, "destroyBannerAd failed", e);
				}
			}
		}, -1, 0, "");
		if (Looper.myLooper() == Looper.getMainLooper()) {
			cleanup.run();
		} else {
			new Handler(Looper.getMainLooper()).post(cleanup);
		}
	}

	private static Runnable guarded(final Runnable r, final int failEvent, final int kind, final String id) {
		return () -> {
			try {
				r.run();
			} catch (Throwable ex) {
				Log.e(TAG, "SDK call failed", ex);
				if (failEvent >= 0) {
					emit(failEvent, kind, id, String.valueOf(ex));
				}
			}
		};
	}

	private static boolean runOnUi(Runnable r, int failEvent, int kind, String id) {
		Activity a = sActivity;
		if (a == null) {
			Log.e(TAG, "No activity attached: call dropped.");
			return false;
		}
		a.runOnUiThread(guarded(r, failEvent, kind, id));
		return true;
	}

	private static boolean runOnUi(Runnable r) {
		return runOnUi(r, -1, 0, "");
	}

	private static void runOnUiOrFail(int failEvent, int kind, String id, Runnable r) {
		if (!runOnUi(r, failEvent, kind, id)) {
			emit(failEvent, kind, id, "No activity");
		}
	}
	private static void finishInit(int generation, boolean success, String message) {
		if (generation != sInitGeneration.get() || !sInitFinished.compareAndSet(false, true)) {
			return;
		}
		sInitHandler.removeCallbacksAndMessages(null);
		if (success) {
			emit(EV_INITIALIZED, 0, "", "");
		} else {
			emit(EV_INIT_FAILED, 0, "", message);
		}
	}

	private static Runnable guardedInit(final int generation, final Runnable r) {
		return () -> {
			try {
				r.run();
			} catch (Throwable ex) {
				Log.e(TAG, "Tapsell initialization failed", ex);
				finishInit(generation, false, String.valueOf(ex));
			}
		};
	}

	private static void registerInitListener(final int generation) {
		if (generation != sInitGeneration.get() || sInitFinished.get()) {
			return;
		}
		Tapsell.setInitializationListener(new MediationInitializationListener() {
			@Override
			public void onInitializationComplete() {
				finishInit(generation, true, "");
			}
		});
	}

	public static boolean initialize(final int timeoutMs) {
		return runOnUi(() -> {
			final int generation = sInitGeneration.incrementAndGet();
			sInitFinished.set(false);
			sInitHandler.removeCallbacksAndMessages(null);

			guardedInit(generation, () -> {
				Activity a = sActivity;
				if (a == null) {
					finishInit(generation, false, "No activity");
					return;
				}
				if (!sSdkInitCalled) {
					Tapsell.initialize(a.getApplicationContext());
					sSdkInitCalled = true;
				}
				sInitHandler.postDelayed(guardedInit(generation, () -> registerInitListener(generation)), 300);
				sInitHandler.postDelayed(guardedInit(generation, () -> registerInitListener(generation)), 1500);
				sInitHandler.postDelayed(guardedInit(generation, () ->
						finishInit(generation, false, "Timed out waiting for the SDK to confirm initialization")), timeoutMs);
			}).run();
		});
	}

	public static void setUserConsent(final boolean consent) {
		runOnUi(() -> {
			Activity a = sActivity;
			if (a != null) {
				Tapsell.setUserConsent(a, consent);
			}
		});
	}

	private static RequestResultListener requestListener(final int kind, final String zoneId) {
		return new RequestResultListener() {
			@Override
			public void onSuccess(String adId) {
				emit(EV_REQUEST_OK, kind, zoneId, adId);
			}

			@Override
			public void onFailure(String message) {
				emit(EV_REQUEST_FAIL, kind, zoneId, message);
			}
		};
	}

	public static void requestAd(final int kind, final String zoneId) {
		runOnUiOrFail(EV_REQUEST_FAIL, kind, zoneId, () -> {
			RequestResultListener l = requestListener(kind, zoneId);
			switch (kind) {
				case KIND_INTERSTITIAL: Tapsell.requestInterstitialAd(zoneId, l); break;
				case KIND_REWARDED: Tapsell.requestRewardedAd(zoneId, l); break;
				case KIND_APP_OPEN: Tapsell.requestAppOpenAd(zoneId, l); break;
				default: emit(EV_REQUEST_FAIL, kind, zoneId, "Invalid ad type"); break;
			}
		});
	}

	public static void showAd(final int kind, final String adId) {
		runOnUiOrFail(EV_AD_FAILED, kind, adId, () -> {
			final Activity a = sActivity;
			if (a == null) {
				emit(EV_AD_FAILED, kind, adId, "No activity");
				return;
			}
			switch (kind) {
				case KIND_INTERSTITIAL:
					Tapsell.showInterstitialAd(adId, a, new AdStateListener.Interstitial() {
						@Override public void onAdImpression() { emit(EV_IMPRESSION, kind, adId, ""); }
						@Override public void onAdClicked() { emit(EV_CLICKED, kind, adId, ""); }
						@Override public void onAdClosed(AdShowCompletionState s) { emit(EV_CLOSED, kind, adId, s.name()); }
						@Override public void onAdFailed(String m) { emit(EV_AD_FAILED, kind, adId, m); }
					});
					break;
				case KIND_REWARDED:
					Tapsell.showRewardedAd(adId, a, new AdStateListener.Rewarded() {
						@Override public void onAdImpression() { emit(EV_IMPRESSION, kind, adId, ""); }
						@Override public void onAdClicked() { emit(EV_CLICKED, kind, adId, ""); }
						@Override public void onRewarded() { emit(EV_REWARDED, kind, adId, ""); }
						@Override public void onAdClosed(AdShowCompletionState s) { emit(EV_CLOSED, kind, adId, s.name()); }
						@Override public void onAdFailed(String m) { emit(EV_AD_FAILED, kind, adId, m); }
					});
					break;
				case KIND_APP_OPEN:
					Tapsell.showAppOpenAd(adId, a, new AdStateListener.AppOpen() {
						@Override public void onAdImpression() { emit(EV_IMPRESSION, kind, adId, ""); }
						@Override public void onAdClicked() { emit(EV_CLICKED, kind, adId, ""); }
						@Override public void onAdClosed(AdShowCompletionState s) { emit(EV_CLOSED, kind, adId, s.name()); }
						@Override public void onAdFailed(String m) { emit(EV_AD_FAILED, kind, adId, m); }
					});
					break;
				default:
					emit(EV_AD_FAILED, kind, adId, "Invalid ad type");
					break;
			}
		});
	}

	public static void requestBanner(final String zoneId, final String sizeName) {
		runOnUiOrFail(EV_REQUEST_FAIL, KIND_BANNER, zoneId, () -> {
			BannerSize size;
			try {
				size = BannerSize.valueOf(sizeName);
			} catch (IllegalArgumentException e) {
				emit(EV_REQUEST_FAIL, KIND_BANNER, zoneId, "Invalid banner size: " + sizeName);
				return;
			}
			Tapsell.requestBannerAd(zoneId, size, requestListener(KIND_BANNER, zoneId));
		});
	}

	private static int toGravity(int position) {
		switch (position) {
			case POS_TOP: return Gravity.TOP | Gravity.CENTER_HORIZONTAL;
			case POS_BOTTOM: return Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL;
			case POS_TOP_LEFT: return Gravity.TOP | Gravity.START;
			case POS_TOP_RIGHT: return Gravity.TOP | Gravity.END;
			case POS_BOTTOM_LEFT: return Gravity.BOTTOM | Gravity.START;
			case POS_BOTTOM_RIGHT: return Gravity.BOTTOM | Gravity.END;
			case POS_CENTER: return Gravity.CENTER;
			default: return Gravity.NO_GRAVITY;
		}
	}

	private static void removeBanner(String adId) {
		FrameLayout wrapper = sBanners.remove(adId);
		if (wrapper != null && wrapper.getParent() instanceof ViewGroup) {
			((ViewGroup) wrapper.getParent()).removeView(wrapper);
		}
	}

	public static void showBanner(final String adId, final int position) {
		runOnUiOrFail(EV_AD_FAILED, KIND_BANNER, adId, () -> {
			final Activity a = sActivity;
			if (a == null) {
				emit(EV_AD_FAILED, KIND_BANNER, adId, "No activity");
				return;
			}
			int gravity = toGravity(position);
			if (gravity == Gravity.NO_GRAVITY) {
				emit(EV_AD_FAILED, KIND_BANNER, adId, "Invalid banner position");
				return;
			}
			removeBanner(adId);

			BannerContainer container = new BannerContainer(a);
			FrameLayout wrapper = new FrameLayout(a);
			wrapper.addView(container);

			boolean fullWidth = position == POS_TOP || position == POS_BOTTOM;
			FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
					fullWidth ? ViewGroup.LayoutParams.MATCH_PARENT : ViewGroup.LayoutParams.WRAP_CONTENT,
					ViewGroup.LayoutParams.WRAP_CONTENT,
					gravity);
			a.addContentView(wrapper, lp);
			sBanners.put(adId, wrapper);

			try {
				Tapsell.showBannerAd(adId, container, a, new AdStateListener.Banner() {
					@Override public void onAdImpression() { emit(EV_IMPRESSION, KIND_BANNER, adId, ""); }
					@Override public void onAdClicked() { emit(EV_CLICKED, KIND_BANNER, adId, ""); }
					@Override public void onAdFailed(String m) {
						emit(EV_AD_FAILED, KIND_BANNER, adId, m);
						runOnUi(() -> removeBanner(adId));
					}
				});
			} catch (RuntimeException e) {
				removeBanner(adId);
				emit(EV_AD_FAILED, KIND_BANNER, adId, String.valueOf(e.getMessage()));
			}
		});
	}

	public static void setBannerVisible(final String adId, final boolean visible) {
		runOnUi(() -> {
			FrameLayout wrapper = sBanners.get(adId);
			if (wrapper != null) {
				wrapper.setVisibility(visible ? View.VISIBLE : View.GONE);
			}
		});
	}

	public static void destroyBanner(final String adId) {
		runOnUi(() -> {
			removeBanner(adId);
			Tapsell.destroyBannerAd(adId);
		});
	}
}
