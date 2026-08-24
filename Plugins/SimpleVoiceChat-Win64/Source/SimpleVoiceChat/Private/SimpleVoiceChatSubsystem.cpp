// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#include "SimpleVoiceChatSubsystem.h"

#include "Backends/OnlineSubsystemVoiceBackend.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Net/VoiceConfig.h"
#include "SimpleVoiceChatLog.h"
#include "SimpleVoiceChatSettings.h"
#include "UObject/UObjectGlobals.h"

USimpleVoiceChatSubsystem::~USimpleVoiceChatSubsystem() = default;

void USimpleVoiceChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        LocalPlayerAddedHandle = GameInstance->OnLocalPlayerAddedEvent.AddUObject(
            this,
            &USimpleVoiceChatSubsystem::HandleLocalPlayerAdded);
    }

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this,
        &USimpleVoiceChatSubsystem::HandlePostLoadMap);

    const USimpleVoiceChatSettings* Settings = GetDefault<USimpleVoiceChatSettings>();
    if (!Settings->bEnableVoicePlugin)
    {
        UE_LOG(LogSimpleVoiceChat, Log, TEXT("Subsystem created with voice disabled in project settings"));
        return;
    }

    const bool bEnableOnStart = Settings->bMicrophoneEnabledOnStart;
    InitializeVoice();
    if (bEnableOnStart)
    {
        EnableMicrophone();
    }
}

void USimpleVoiceChatSubsystem::Deinitialize()
{
    if (LocalPlayerAddedHandle.IsValid())
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            GameInstance->OnLocalPlayerAddedEvent.Remove(LocalPlayerAddedHandle);
        }
        LocalPlayerAddedHandle.Reset();
    }

    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }

    ShutdownVoice();
    Super::Deinitialize();
}

bool USimpleVoiceChatSubsystem::InitializeVoice()
{
    const USimpleVoiceChatSettings* Settings = GetDefault<USimpleVoiceChatSettings>();
    if (!Settings->bEnableVoicePlugin)
    {
        return false;
    }

    if (!VoiceBackend)
    {
        VoiceBackend = MakeUnique<FOnlineSubsystemVoiceBackend>();
    }

    ApplyRuntimeAudioSettings();
    bVoiceInitialized = true;
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    StartRefreshTimer(World);

    if (!VoiceBackend->Initialize(World))
    {
        return false;
    }

    VoiceBackend->RefreshTalkers(World);
    VoiceBackend->SetMicrophoneEnabled(bMicrophoneEnabled);
    return VoiceBackend->IsAvailable();
}

void USimpleVoiceChatSubsystem::ShutdownVoice()
{
    StopRefreshTimer();

    if (VoiceBackend)
    {
        VoiceBackend->Shutdown();
        VoiceBackend.Reset();
    }

    bVoiceInitialized = false;
    bRuntimeAudioSettingsApplied = false;
    if (bMicrophoneEnabled)
    {
        bMicrophoneEnabled = false;
        OnMicrophoneStateChanged.Broadcast(false);
    }
}

bool USimpleVoiceChatSubsystem::EnableMicrophone()
{
    return ApplyMicrophoneState(true);
}

bool USimpleVoiceChatSubsystem::DisableMicrophone()
{
    return ApplyMicrophoneState(false);
}

bool USimpleVoiceChatSubsystem::ToggleMicrophone()
{
    return ApplyMicrophoneState(!bMicrophoneEnabled);
}

bool USimpleVoiceChatSubsystem::IsMicrophoneEnabled() const
{
    return bMicrophoneEnabled;
}

bool USimpleVoiceChatSubsystem::IsVoiceAvailable() const
{
    return VoiceBackend && VoiceBackend->IsAvailable();
}

bool USimpleVoiceChatSubsystem::IsLocalPlayerTalking() const
{
    return VoiceBackend && VoiceBackend->IsLocalPlayerTalking();
}

void USimpleVoiceChatSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (!bVoiceInitialized || !LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
    {
        return;
    }

    if (VoiceBackend)
    {
        VoiceBackend->HandleWorldChanged(LoadedWorld);
    }
    StartRefreshTimer(LoadedWorld);
    RefreshVoiceState();
}

void USimpleVoiceChatSubsystem::HandleLocalPlayerAdded(ULocalPlayer* NewLocalPlayer)
{
    // This runs early enough for NULL identity creation to precede a later direct-IP connection.
    RefreshVoiceState();
}

void USimpleVoiceChatSubsystem::RefreshVoiceState()
{
    if (!bVoiceInitialized || !VoiceBackend)
    {
        return;
    }

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!VoiceBackend->Initialize(World))
    {
        return;
    }

    VoiceBackend->RefreshTalkers(World);
    if (VoiceBackend->IsMicrophoneEnabled() != bMicrophoneEnabled)
    {
        VoiceBackend->SetMicrophoneEnabled(bMicrophoneEnabled);
    }
}

void USimpleVoiceChatSubsystem::HandlePeriodicVoiceRefresh()
{
    const double CurrentTimeSeconds = FPlatformTime::Seconds();
    if (LastPeriodicRefreshTimeSeconds > 0.0)
    {
        const double ElapsedSeconds = CurrentTimeSeconds - LastPeriodicRefreshTimeSeconds;
        if (ElapsedSeconds > 1.5)
        {
            UE_LOG(
                LogSimpleVoiceChat,
                Warning,
                TEXT("Voice servicing was delayed for %.2f seconds by a game-thread stall; transmitted audio may sound choppy"),
                ElapsedSeconds);
        }
    }
    LastPeriodicRefreshTimeSeconds = CurrentTimeSeconds;
    RefreshVoiceState();
}

void USimpleVoiceChatSubsystem::StartRefreshTimer(UWorld* World)
{
    StopRefreshTimer();
    if (!World)
    {
        return;
    }

    RefreshTimerWorld = World;
    LastPeriodicRefreshTimeSeconds = FPlatformTime::Seconds();
    World->GetTimerManager().SetTimer(
        RefreshTimerHandle,
        this,
        &USimpleVoiceChatSubsystem::HandlePeriodicVoiceRefresh,
        1.0f,
        true,
        0.25f);
}

void USimpleVoiceChatSubsystem::StopRefreshTimer()
{
    if (UWorld* TimerWorld = RefreshTimerWorld.Get())
    {
        TimerWorld->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }
    RefreshTimerWorld.Reset();
    RefreshTimerHandle.Invalidate();
    LastPeriodicRefreshTimeSeconds = 0.0;
}

void USimpleVoiceChatSubsystem::ApplyRuntimeAudioSettings()
{
    if (bRuntimeAudioSettingsApplied)
    {
        return;
    }

    const USimpleVoiceChatSettings* Settings = GetDefault<USimpleVoiceChatSettings>();
    IConsoleVariable* MicrophoneGain = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicInputGain"));
    IConsoleVariable* JitterDelay = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.JitterBufferDelay"));
    if (!MicrophoneGain || !JitterDelay)
    {
        UE_LOG(LogSimpleVoiceChat, Warning, TEXT("Unreal voice quality console variables are unavailable"));
        return;
    }

    const float InputGain = FMath::Clamp(Settings->MicrophoneInputGain, 0.0f, 4.0f);
    const float JitterDelaySeconds = FMath::Clamp(Settings->JitterBufferDelaySeconds, 0.05f, 1.0f);
    MicrophoneGain->Set(InputGain, ECVF_SetByGameSetting);
    JitterDelay->Set(JitterDelaySeconds, ECVF_SetByGameSetting);
    bRuntimeAudioSettingsApplied = true;

    UE_LOG(
        LogSimpleVoiceChat,
        Log,
        TEXT("Voice audio settings applied: sample-rate=%d Hz, playback-volume=%.2f, microphone-gain=%.2f, jitter-buffer=%.2f s"),
        UVOIPStatics::GetVoiceSampleRate(),
        FMath::Clamp(Settings->PlaybackVolumeMultiplier, 0.0f, 4.0f),
        InputGain,
        JitterDelaySeconds);
}

bool USimpleVoiceChatSubsystem::ApplyMicrophoneState(const bool bEnable)
{
    if (!bVoiceInitialized && !InitializeVoice())
    {
        return false;
    }

    RefreshVoiceState();
    if (!VoiceBackend || !VoiceBackend->SetMicrophoneEnabled(bEnable))
    {
        UE_LOG(
            LogSimpleVoiceChat,
            Warning,
            TEXT("Cannot %s the microphone because no local voice talker is available"),
            bEnable ? TEXT("enable") : TEXT("disable"));
        return false;
    }

    if (bMicrophoneEnabled != bEnable)
    {
        bMicrophoneEnabled = bEnable;
        UE_LOG(LogSimpleVoiceChat, Log, TEXT("Microphone %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
        OnMicrophoneStateChanged.Broadcast(bMicrophoneEnabled);
    }
    return true;
}
