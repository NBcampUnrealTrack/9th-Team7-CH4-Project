// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISimpleVoiceBackend.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "SimpleVoiceChatSubsystem.generated.h"

class ULocalPlayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSimpleMicrophoneStateChanged, bool, bIsEnabled);

/**
 * Game-instance lifetime facade for the active OnlineSubsystem voice interface.
 * It survives ordinary map travel and keeps voice policy out of game-specific classes.
 */
UCLASS()
class SIMPLEVOICECHAT_API USimpleVoiceChatSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual ~USimpleVoiceChatSubsystem() override;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Creates the backend, registers talkers when possible, and starts lightweight synchronization. */
    UFUNCTION(BlueprintCallable, Category="Simple Voice Chat")
    bool InitializeVoice();

    /** Unregisters talkers and releases this plugin's backend references. */
    UFUNCTION(BlueprintCallable, Category="Simple Voice Chat")
    void ShutdownVoice();

    UFUNCTION(BlueprintCallable, Category="Simple Voice Chat")
    bool EnableMicrophone();

    UFUNCTION(BlueprintCallable, Category="Simple Voice Chat")
    bool DisableMicrophone();

    UFUNCTION(BlueprintCallable, Category="Simple Voice Chat")
    bool ToggleMicrophone();

    UFUNCTION(BlueprintPure, Category="Simple Voice Chat")
    bool IsMicrophoneEnabled() const;

    UFUNCTION(BlueprintPure, Category="Simple Voice Chat")
    bool IsVoiceAvailable() const;

    UFUNCTION(BlueprintPure, Category="Simple Voice Chat")
    bool IsLocalPlayerTalking() const;

    /** UI and Blueprint consumers can react without polling. */
    UPROPERTY(BlueprintAssignable, Category="Simple Voice Chat")
    FSimpleMicrophoneStateChanged OnMicrophoneStateChanged;

private:
    void HandlePostLoadMap(UWorld* LoadedWorld);
    void HandleLocalPlayerAdded(ULocalPlayer* NewLocalPlayer);
    void HandlePeriodicVoiceRefresh();
    void RefreshVoiceState();
    void StartRefreshTimer(UWorld* World);
    void StopRefreshTimer();
    void ApplyRuntimeAudioSettings();
    bool ApplyMicrophoneState(bool bEnable);

    TUniquePtr<ISimpleVoiceBackend> VoiceBackend;
    FDelegateHandle PostLoadMapHandle;
    FDelegateHandle LocalPlayerAddedHandle;
    FTimerHandle RefreshTimerHandle;
    TWeakObjectPtr<UWorld> RefreshTimerWorld;
    double LastPeriodicRefreshTimeSeconds = 0.0;
    bool bVoiceInitialized = false;
    bool bMicrophoneEnabled = false;
    bool bRuntimeAudioSettingsApplied = false;
};
