# Simple Voice Chat

`SimpleVoiceChat` is a reusable Unreal Engine Runtime plugin that exposes a small toggle-microphone API over the voice interface supplied by the **currently active OnlineSubsystem**.

It does not capture microphone samples directly, encode audio, send custom UDP packets, or integrate a platform SDK. Transport, identity, codecs, permissions, and playback remain the responsibility of Unreal's OnlineSubsystem voice implementation.

## Installation

- Unreal Engine 5.8 (the packaged validation described below targets Win64).
- An OnlineSubsystem backend that supplies a valid legacy `IOnlineVoice` interface.
- A working microphone and operating-system microphone permission.

1. Copy the complete `SimpleVoiceChat` folder.
2. Place it at `<MyProject>/Plugins/SimpleVoiceChat` (create `Plugins` if needed).
3. Regenerate C++ project files and build the project.
4. Start Unreal Editor and enable **Simple Voice Chat** in Edit > Plugins.
5. Restart the project when prompted.

For engine-wide reuse, the alternative location is:

```text
<Project>/Plugins/SimpleVoiceChat
```

or, for engine-wide reuse:

```text
<Engine>/Plugins/Marketplace/SimpleVoiceChat
```

The plugin declares its dependencies on `OnlineSubsystem` and `OnlineSubsystemUtils`; it does not select or configure a platform backend for the host project.

## Required voice configuration

The host project normally needs these voice flags in `Config/DefaultEngine.ini`:

```ini
[OnlineSubsystem]
bHasVoiceEnabled=true

[Voice]
bEnabled=true

[/Script/Engine.AudioSettings]
VoiPSampleRate=Normal24000Hz
```

`Normal24000Hz` is recommended for clearer speech than Unreal's 16 kHz default. Changing the
VOIP sample rate requires a full Editor/game restart so capture, encoder, decoder, and playback
are all created at the same rate.

The active OnlineSubsystem and its authentication/session requirements must also be configured by the host project. If no voice interface is exposed, the plugin remains safe, logs one warning, and `IsVoiceAvailable()` returns `false`.

## Lifecycle

`USimpleVoiceChatSubsystem` is a `UGameInstanceSubsystem`. It initializes once per game instance, survives ordinary map travel and ServerTravel, and shuts down with the game instance. It automatically refreshes local and remote talker registration without using Actor or UObject Tick. Remote registrations are rebuilt after map travel and removed when their replicated PlayerState disappears.

The microphone is **off by default**. Local talker registration is separate from transmitting voice; newly registered local talkers are immediately kept stopped until the user enables the microphone.

## Basic Usage

- Toggle microphone: `T`
- Microphone on startup: `false`
- Automatic toggle input: enabled

The input processor only reacts while a game/PIE scene viewport has keyboard focus. It ignores repeated key events and an open Unreal console, so console typing and normal text-entry focus do not toggle the microphone. It always returns `false`, which means it never globally consumes the key. Projects may disable automatic input and call the API from Enhanced Input, UI, or gameplay code instead.

## Blueprint usage

1. Use **Get Game Instance Subsystem** and select `SimpleVoiceChatSubsystem`.
2. Call `Is Voice Available` before presenting an enabled voice control.
3. Call `Enable Microphone`, `Disable Microphone`, or `Toggle Microphone`.
4. Bind `On Microphone State Changed` to update an icon or settings UI.
5. Use `Is Local Player Talking` only for status display; do not poll it every frame unless the UI truly needs that cadence.

Initialization happens automatically when enabled in settings. `Initialize Voice` and `Shutdown Voice` remain public for explicit lifecycle control and tests.

## C++ usage

Add `SimpleVoiceChat` to the consuming module's dependency list, then:

```cpp
#include "SimpleVoiceChatSubsystem.h"

if (UGameInstance* GameInstance = GetGameInstance())
{
    if (USimpleVoiceChatSubsystem* Voice =
        GameInstance->GetSubsystem<USimpleVoiceChatSubsystem>())
    {
        if (Voice->IsVoiceAvailable())
        {
            Voice->ToggleMicrophone();
        }
    }
}
```

Public Blueprint/C++ API:

```text
InitializeVoice
ShutdownVoice
EnableMicrophone
DisableMicrophone
ToggleMicrophone
IsMicrophoneEnabled
IsVoiceAvailable
IsLocalPlayerTalking
OnMicrophoneStateChanged
```

## Project Settings

Open **Project Settings > Plugins > Simple Voice Chat**:

- **Enable Voice Plugin**: creates and synchronizes the backend when true.
- **Enable Automatic Toggle Key**: enables the focus-safe Slate key listener.
- **Toggle Key**: defaults to `T`.
- **Microphone Enabled On Start**: defaults to false and should remain opt-in for privacy.
- **Encoder Bitrate**: defaults to 32000 bps for clearer speech in a small multiplayer group.
- **Encoder Complexity**: defaults to 5, improving Opus quality without using its maximum CPU setting.
- **Use Variable Bitrate**: defaults to true.
- **Playback Volume Multiplier**: defaults to 1.5 and boosts only received playback, avoiding microphone-side clipping.
- **Microphone Input Gain**: defaults to 1.0. Raise it only after checking the Windows input level because excess gain clips before encoding.
- **Jitter Buffer Delay Seconds**: defaults to 0.3. A larger value can hide network jitter but increases voice delay.
- **Auto Login Null Subsystem For Direct IP**: defaults to true and creates a temporary NULL identity before direct-IP testing. It never logs in Steam or another provider.
- **Auto Create Null Voice Session For Direct IP**: defaults to true and creates a non-advertised local marker session only when NULL has no existing session. Unreal's legacy NULL voice implementation requires one before it accepts remote talkers.

The defaults are stored in `Config/DefaultSimpleVoiceChat.ini`. A host project can override them in its own config without editing plugin source.

## Using with OnlineSubsystemSteam

Steam voice requires the host project to enable and correctly configure `OnlineSubsystemSteam`, including Steam authentication/session/network settings appropriate to that project. Select Steam as the active platform service through the project's normal OnlineSubsystem configuration, keep the two voice flags above enabled, and test with distinct logged-in Steam users in an appropriate build mode.

The plugin deliberately has no compile-time dependency on `OnlineSubsystemSteam` and contains no Steam App ID. When Steam is the active backend, it obtains voice through `IOnlineSubsystem::GetVoiceInterface()` like any other supported backend.

## Listen server, travel, and player changes

- Listen server and clients each own their own game-instance subsystem.
- Local users are registered by controller ID.
- replicated remote PlayerStates with valid legacy unique network IDs are registered as remote talkers.
- player logout removes stale remote talkers during synchronization.
- map travel clears world-owned remote registrations and rebuilds them from the new GameState.
- no player-count limit is hard-coded.

Voice still depends on the host project's networking/session/backend setup. This plugin does not create sessions, open maps, or perform travel.

## Hamachi and direct-IP warning

Hamachi only provides an IP path between machines. A successful `open <ip>` gameplay connection does **not** prove that the active OnlineSubsystem provides voice identity, transport, or a valid `IOnlineVoice` interface. The Null backend's voice support and user identifiers can vary by engine/platform/configuration.

For a direct-IP test, first confirm gameplay replication, then check for:

```text
LogSimpleVoiceChat: OnlineSubsystem voice backend ready: <BackendName>
LogSimpleVoiceChat: NULL identity auto-login succeeded for local user 0: ...
LogSimpleVoiceChat: Private NULL voice session ready
LogSimpleVoiceChat: Registered local talker 0
LogSimpleVoiceChat: Registered remote talker: ...
```

The identity-success line must appear **before** opening the listen server or joining the host. If a connection log contains `UniqueId: INVALID`, disconnect, wait for identity success, and reconnect.

If `IsVoiceAvailable()` is false or the log reports that voice is unavailable, changing Hamachi ports alone will not fix the missing voice backend.

## Troubleshooting

### `IsVoiceAvailable()` is false

- Confirm an OnlineSubsystem is active for the current world.
- Confirm that backend implements/exposes `IOnlineVoice`.
- Add `bHasVoiceEnabled=true` and `bEnabled=true` as shown above.
- Inspect the `LogSimpleVoiceChat` and backend-specific OnlineSubsystem logs.

### The talker is registered but no audio is heard

- Confirm operating-system microphone access and the correct input device.
- Confirm the microphone was toggled on; the default is off.
- Test with distinct online identities and separate machines where required by the backend.
- Confirm remote PlayerStates contain valid legacy unique network IDs.
- Check backend/session requirements, mute state, firewall, and platform permissions.

### Audio is quiet or distorted

- Confirm the startup log says `sample-rate=24000 Hz` and shows the configured Opus bitrate and complexity.
- Raise **Playback Volume Multiplier** first. This changes received volume without clipping the sender's encoded microphone signal.
- Keep **Microphone Input Gain** at `1.0` unless a Windows microphone recording is also quiet.
- If the log reports `Voice servicing was delayed`, retest in Standalone or a packaged Development build with shader compilation and heavy Editor work stopped. A long game-thread stall also delays Unreal's legacy voice capture/packet service and cannot be repaired by bitrate alone.
- Keep the game window focused and disable aggressive background CPU throttling during multi-window tests.

### Pressing T does nothing

- Click the game viewport so it has keyboard focus.
- Close the Unreal console and any focused text input.
- Check the plugin settings and configured key.
- Call `ToggleMicrophone()` directly to separate input setup from backend availability.

### Travel or logout leaves incorrect voice state

- Check that the destination world has a replicated GameState and PlayerStates.
- Wait briefly for PlayerState/unique-ID replication and inspect registration logs.
- Explicitly call `ShutdownVoice()` before a custom game-instance teardown path.

## Future work

- Proximity Voice and 3D spatial policies.
- Voice Channel support.
- EOS Backend through a separate adapter.
- Player Mute and volume APIs.
- Input Device Selection and Output Device Selection.
- provider-specific diagnostics.
- automated multi-client integration fixtures for supported backends.
- configurable synchronization strategy for projects with custom player identity lifecycles.
