# Tapsell Ads for Unreal Engine

An **unofficial community plugin** that integrates the Tapsell Mediation SDK into Unreal Engine projects for Android. It exposes ad requests, display controls, and callbacks to Blueprints and C++.

This project is not affiliated with, endorsed by, or officially supported by Tapsell.

## Features

- Interstitial ads
- Rewarded ads
- App Open ads
- Banner ads with configurable size and position
- Blueprint-accessible API and event dispatch
- Optional initialization at game startup
- User-consent control
- Editor and desktop simulation for testing ad-flow logic

## Compatibility

| Component | Version / support |
| --- | --- |
| Unreal Engine | 5.8 |
| Target platform for real ads | Android |
| Tapsell Mediation SDK | `1.4.0-alpha04` by default |
| Other platforms | Simulated callbacks only, when simulation is enabled |

The SDK version can be changed in **Project Settings → Plugins → Tapsell Ads**. The default SDK version is an alpha release; its APIs and behavior may change between releases.

The Android SDK is resolved by Gradle from `https://maven.tapsell.ir` during the Android build. It is not bundled in this repository. Your build environment must be able to access that Maven repository.

## Installation

1. Copy the `TapsellAds` directory into your project's `Plugins` directory:

   ```text
   YourProject/
   └── Plugins/
       └── TapsellAds/
   ```

2. Open the project in Unreal Engine. If prompted, allow the plugin modules to compile.
3. Open **Edit → Project Settings → Plugins → Tapsell Ads**.
4. Set your Tapsell **App Key**.
5. Package the project for Android.

If the plugin does not appear, close the editor, regenerate project files if applicable, and reopen the project.

## Configuration

The plugin settings are available under **Project Settings → Plugins → Tapsell Ads**.

| Setting | Description |
| --- | --- |
| App Key | Application key obtained from the Tapsell panel. |
| SDK Version | Maven version used for the Tapsell SDK and legacy mediation adapter. |
| Auto Initialize | Initializes the SDK when the Game Instance subsystem starts. Enabled by default. |
| Init Timeout Seconds | Maximum time to wait for initialization confirmation. Minimum value: 2 seconds. |
| Verbose Logging | Enables additional plugin and Java bridge logs. |
| Simulate When Not On Device | Enables simulated ad callbacks outside Android, for example in the editor or on Windows. |
| Simulated Latency | Delay used by simulated callbacks. |

When `Auto Initialize` is disabled, call `Initialize Tapsell` from the **Tapsell Ads Subsystem** yourself. This can be useful when initialization should happen after your consent flow.

## Blueprint API

Get the **Tapsell Ads Subsystem** from the Game Instance. With automatic initialization enabled, wait for `On Initialized` before requesting ads.

### Initialization and consent

- `Initialize Tapsell`: Starts SDK initialization.
- `Is Tapsell Initialized`: Returns whether the subsystem has received initialization confirmation.
- `Set User Consent`: Passes the consent value to the Tapsell SDK.

`On Initialization Failed` is broadcast if initialization fails or the configured timeout expires. Calling `Initialize Tapsell` again starts a retry attempt. A callback arriving after an attempt has timed out is ignored by that attempt; verify retry behavior with the SDK version used by your project.

### Full-screen ads

Supported types:

- `Interstitial`
- `Rewarded`
- `AppOpen`

Typical flow:

1. Call `Request Ad` with the ad type and its Zone ID.
2. Handle `On Request Succeeded` and store the returned Ad ID.
3. Call `Show Ad` with the same ad type and Ad ID.
4. Handle impression, click, close, failure, and reward events as needed.

Each returned Ad ID is intended for one display. Request another ad after the current ad has closed.

### Banner ads

Typical flow:

1. Call `Request Banner` with the Zone ID and desired banner size.
2. Handle `On Request Succeeded` and store the returned Ad ID.
3. Call `Show Banner` with the Ad ID and desired screen position.
4. Use `Set Banner Visible` to hide or show the displayed banner.
5. Call `Destroy Banner` when the banner is no longer needed.

Available banner sizes:

- `Banner_300x50`
- `Banner_320x50`
- `Banner_320x90`
- `Banner_320x100`
- `Banner_250x250`
- `Banner_300x250`
- `Banner_468x60`
- `Banner_728x90`
- `Banner_160x600`
- `Adaptive`

Available positions:

- `Top`
- `Bottom`
- `TopLeft`
- `TopRight`
- `BottomLeft`
- `BottomRight`
- `Center`

The plugin removes its banner views and asks the SDK to destroy active banners when the Android Activity is detached. Test banner behavior across app pause/resume and Activity recreation in your target project.

## Events

| Event | Description |
| --- | --- |
| `On Initialized` | The SDK reports successful initialization. |
| `On Initialization Failed` | Initialization fails or times out. Includes an error message. |
| `On Request Succeeded` | An ad request succeeds. Includes ad type, Zone ID, and Ad ID. |
| `On Request Failed` | An ad request fails. Includes ad type, Zone ID, and error message. |
| `On Ad Impression` | An ad impression is reported. |
| `On Ad Clicked` | An ad click is reported. |
| `On Ad Closed` | A full-screen ad closes. Includes its completion state. |
| `On Ad Failed` | Displaying an ad fails. Includes an error message. |
| `On Rewarded` | A rewarded ad reports that the reward was earned. |

`On Ad Closed` uses these completion states:

- `Completed`
- `Skipped`
- `Unknown`

The plugin dispatches native SDK events to the Unreal Game Thread before broadcasting Blueprint delegates.

## Platform simulation

Real Tapsell ads are supported on Android only. On other platforms, enable `Simulate When Not On Device` to test application flow without contacting the Tapsell SDK.

Simulation generates placeholder request IDs and simulated callbacks. It does **not** display ads, validate Zone IDs, or verify your Tapsell account configuration.

## Android notes

- The plugin adds the `com.google.android.gms.permission.AD_ID` permission to the Android manifest.
- Review your app's advertising ID usage and complete the relevant Google Play Console declarations for your app.
- Ensure the App Key and Zone IDs belong to the application configured in your Tapsell account.
- Gradle must be able to download the configured SDK artifacts from the Tapsell Maven repository.
- `Set User Consent`, `Set Banner Visible`, and `Destroy Banner` do not expose Blueprint completion events. Java-side exceptions are written to Android logcat.
- Enable `Verbose Logging` when diagnosing initialization and ad callback issues. The Java bridge uses the `TapsellBridge` log tag.

## Troubleshooting

**Initialization fails**

- Confirm that the App Key is set in Project Settings.
- Check that the Android build can access `maven.tapsell.ir`.
- Enable `Verbose Logging` and inspect Android logcat.
- Confirm that the SDK version configured in the plugin is available in the Maven repository.

**Ad requests fail**

- Confirm that initialization completed before requesting an ad.
- Check that the Zone ID matches the ad format and application in the Tapsell panel.
- Inspect `On Request Failed` and Android logcat for the reported error.

**No ads appear outside Android**

- This is expected unless simulation is enabled. Simulation only exercises callback flow and does not render ads.

## License

This plugin's source code is distributed under the MIT License. See [LICENSE](LICENSE).

The Tapsell SDK and its dependencies are third-party software and remain subject to their respective licenses and terms.
