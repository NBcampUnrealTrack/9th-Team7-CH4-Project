// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#pragma once

#include "ISimpleVoiceBackend.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/VoiceInterface.h"

class IOnlineSubsystem;
class APlayerState;
class USimpleVoiceChatTalkerComponent;

/** Legacy IOnlineVoice adapter supplied by the currently active OnlineSubsystem. */
class FOnlineSubsystemVoiceBackend final : public ISimpleVoiceBackend
{
public:
    virtual bool Initialize(UWorld* World) override;
    virtual void Shutdown() override;
    virtual void HandleWorldChanged(UWorld* NewWorld) override;
    virtual void RefreshTalkers(UWorld* World) override;
    virtual bool SetMicrophoneEnabled(bool bEnable) override;
    virtual bool IsAvailable() const override;
    virtual bool IsMicrophoneEnabled() const override;
    virtual bool IsLocalPlayerTalking() const override;

private:
    void RefreshLocalTalkers(UWorld* World);
    void RefreshRemoteTalkers(UWorld* World);
    void ApplyEncoderQualitySettings(UWorld* World);
    void EnsureManagedRemoteTalker(APlayerState* PlayerState, const FString& TalkerKey);
    void RemoveManagedRemoteTalker(const FString& TalkerKey);
    void RemoveAllManagedRemoteTalkers();
    void EnsureNullIdentityLogin(int32 LocalUserNum);
    void HandleIdentityLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
    void ClearIdentityLoginDelegates();
    void EnsureNullDirectIpVoiceSession(int32 LocalUserNum);
    void HandleNullVoiceSessionCreated(FName SessionName, bool bWasSuccessful);
    void ReleaseNullDirectIpVoiceSession();
    void UnregisterRemoteTalkers();
    void ReleaseAllTalkers();
    void ApplyMicrophoneStateToLocalTalker(uint32 LocalUserNum) const;
    static FString MakeRemoteTalkerKey(const FUniqueNetIdRepl& UniqueId);

    IOnlineSubsystem* OnlineSubsystem = nullptr;
    IOnlineIdentityPtr IdentityInterface;
    IOnlineSessionPtr SessionInterface;
    IOnlineVoicePtr VoiceInterface;
    TMap<int32, FDelegateHandle> IdentityLoginDelegateHandles;
    FDelegateHandle CreateSessionCompleteDelegateHandle;
    TSet<int32> ValidIdentityUsers;
    TSet<uint32> RegisteredLocalTalkers;
    TMap<FString, FUniqueNetIdRepl> RegisteredRemoteTalkers;
    TMap<FString, TWeakObjectPtr<USimpleVoiceChatTalkerComponent>> ManagedRemoteTalkers;
    bool bMicrophoneEnabled = false;
    bool bEncoderQualitySettingsApplied = false;
    bool bWarnedVoiceUnavailable = false;
    bool bWarnedInvalidRemoteIdentity = false;
    bool bNullVoiceSessionCreationPending = false;
    bool bOwnsNullDirectIpVoiceSession = false;
};
