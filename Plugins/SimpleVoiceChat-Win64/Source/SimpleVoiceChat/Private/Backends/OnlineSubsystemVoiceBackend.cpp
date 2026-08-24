// Copyright SimpleVoiceChat Contributors. All Rights Reserved.

#include "Backends/OnlineSubsystemVoiceBackend.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "SimpleVoiceChatLog.h"
#include "SimpleVoiceChatSettings.h"
#include "SimpleVoiceChatTalkerComponent.h"

namespace
{
const FName NullDirectIpVoiceSessionName(TEXT("SimpleVoiceChatDirectIp"));
}

bool FOnlineSubsystemVoiceBackend::Initialize(UWorld* World)
{
    IOnlineSubsystem* NewOnlineSubsystem = World ? Online::GetSubsystem(World) : nullptr;
    IOnlineIdentityPtr NewIdentityInterface = NewOnlineSubsystem ? NewOnlineSubsystem->GetIdentityInterface() : nullptr;
    IOnlineSessionPtr NewSessionInterface = NewOnlineSubsystem ? NewOnlineSubsystem->GetSessionInterface() : nullptr;
    IOnlineVoicePtr NewVoiceInterface = NewOnlineSubsystem ? NewOnlineSubsystem->GetVoiceInterface() : nullptr;

    if (OnlineSubsystem == NewOnlineSubsystem && VoiceInterface == NewVoiceInterface && VoiceInterface.IsValid())
    {
        if (IdentityInterface != NewIdentityInterface)
        {
            ClearIdentityLoginDelegates();
            IdentityInterface = MoveTemp(NewIdentityInterface);
            ValidIdentityUsers.Reset();
        }
        if (SessionInterface != NewSessionInterface)
        {
            ReleaseNullDirectIpVoiceSession();
            SessionInterface = MoveTemp(NewSessionInterface);
        }
        ApplyEncoderQualitySettings(World);
        return true;
    }

    ReleaseAllTalkers();
    ClearIdentityLoginDelegates();
    ReleaseNullDirectIpVoiceSession();
    OnlineSubsystem = NewOnlineSubsystem;
    IdentityInterface = MoveTemp(NewIdentityInterface);
    SessionInterface = MoveTemp(NewSessionInterface);
    VoiceInterface = MoveTemp(NewVoiceInterface);
    ValidIdentityUsers.Reset();
    bWarnedInvalidRemoteIdentity = false;
    bEncoderQualitySettingsApplied = false;

    if (!VoiceInterface.IsValid())
    {
        if (!bWarnedVoiceUnavailable)
        {
            const FString BackendName = OnlineSubsystem
                ? OnlineSubsystem->GetSubsystemName().ToString()
                : TEXT("None");
            UE_LOG(
                LogSimpleVoiceChat,
                Warning,
                TEXT("Voice is unavailable. Active OnlineSubsystem: %s. Check the backend and voice configuration."),
                *BackendName);
            bWarnedVoiceUnavailable = true;
        }
        return false;
    }

    bWarnedVoiceUnavailable = false;
    UE_LOG(
        LogSimpleVoiceChat,
        Log,
        TEXT("OnlineSubsystem voice backend ready: %s"),
        *OnlineSubsystem->GetSubsystemName().ToString());
    ApplyEncoderQualitySettings(World);
    return true;
}

void FOnlineSubsystemVoiceBackend::Shutdown()
{
    ReleaseAllTalkers();
    ClearIdentityLoginDelegates();
    ReleaseNullDirectIpVoiceSession();
    IdentityInterface.Reset();
    SessionInterface.Reset();
    VoiceInterface.Reset();
    OnlineSubsystem = nullptr;
    bMicrophoneEnabled = false;
    bEncoderQualitySettingsApplied = false;
    bWarnedVoiceUnavailable = false;
}

void FOnlineSubsystemVoiceBackend::HandleWorldChanged(UWorld* NewWorld)
{
    // Remote PlayerStates are world-owned. Drop their registrations before rebuilding from the new world.
    UnregisterRemoteTalkers();
}

void FOnlineSubsystemVoiceBackend::RefreshTalkers(UWorld* World)
{
    if (!VoiceInterface.IsValid() || !World)
    {
        return;
    }

    RefreshLocalTalkers(World);
    RefreshRemoteTalkers(World);
}

bool FOnlineSubsystemVoiceBackend::SetMicrophoneEnabled(const bool bEnable)
{
    if (!VoiceInterface.IsValid())
    {
        return false;
    }

    if (bEnable && RegisteredLocalTalkers.IsEmpty())
    {
        return false;
    }

    bMicrophoneEnabled = bEnable;
    for (const uint32 LocalUserNum : RegisteredLocalTalkers)
    {
        ApplyMicrophoneStateToLocalTalker(LocalUserNum);
    }
    return true;
}

bool FOnlineSubsystemVoiceBackend::IsAvailable() const
{
    return VoiceInterface.IsValid();
}

bool FOnlineSubsystemVoiceBackend::IsMicrophoneEnabled() const
{
    return bMicrophoneEnabled;
}

bool FOnlineSubsystemVoiceBackend::IsLocalPlayerTalking() const
{
    if (!VoiceInterface.IsValid())
    {
        return false;
    }

    for (const uint32 LocalUserNum : RegisteredLocalTalkers)
    {
        if (VoiceInterface->IsLocalPlayerTalking(LocalUserNum))
        {
            return true;
        }
    }
    return false;
}

void FOnlineSubsystemVoiceBackend::RefreshLocalTalkers(UWorld* World)
{
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return;
    }

    TSet<uint32> PresentLocalUsers;
    for (const ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
    {
        if (!LocalPlayer)
        {
            continue;
        }

        const int32 ControllerId = LocalPlayer->GetControllerId();
        if (ControllerId < 0 || ControllerId > MAX_uint8)
        {
            UE_LOG(LogSimpleVoiceChat, Warning, TEXT("Ignoring unsupported local controller id: %d"), ControllerId);
            continue;
        }

        const uint32 LocalUserNum = static_cast<uint32>(ControllerId);
        PresentLocalUsers.Add(LocalUserNum);
        EnsureNullIdentityLogin(ControllerId);
        EnsureNullDirectIpVoiceSession(ControllerId);
        if (RegisteredLocalTalkers.Contains(LocalUserNum))
        {
            continue;
        }

        if (VoiceInterface->RegisterLocalTalker(LocalUserNum))
        {
            RegisteredLocalTalkers.Add(LocalUserNum);
            ApplyMicrophoneStateToLocalTalker(LocalUserNum);
            UE_LOG(LogSimpleVoiceChat, Log, TEXT("Registered local talker %u"), LocalUserNum);
        }
        else
        {
            UE_LOG(LogSimpleVoiceChat, Verbose, TEXT("Local talker %u is not ready; registration will be retried"), LocalUserNum);
        }
    }

    for (auto It = RegisteredLocalTalkers.CreateIterator(); It; ++It)
    {
        const uint32 LocalUserNum = *It;
        if (!PresentLocalUsers.Contains(LocalUserNum))
        {
            VoiceInterface->StopNetworkedVoice(static_cast<uint8>(LocalUserNum));
            VoiceInterface->UnregisterLocalTalker(LocalUserNum);
            UE_LOG(LogSimpleVoiceChat, Log, TEXT("Unregistered local talker %u"), LocalUserNum);
            It.RemoveCurrent();
        }
    }
}

void FOnlineSubsystemVoiceBackend::RefreshRemoteTalkers(UWorld* World)
{
    AGameStateBase* GameState = World->GetGameState();
    if (!GameState)
    {
        return;
    }

    TSet<FString> PresentRemoteTalkers;
    bool bHasInvalidRemoteIdentity = false;
    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        if (!IsValid(PlayerState) || PlayerState->IsInactive())
        {
            continue;
        }

        const APlayerController* PlayerController = PlayerState->GetPlayerController();
        if (PlayerController && PlayerController->IsLocalController())
        {
            continue;
        }

        const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
        if (!UniqueId.IsValid() || !UniqueId.IsV1())
        {
            bHasInvalidRemoteIdentity = true;
            continue;
        }

        const FString TalkerKey = MakeRemoteTalkerKey(UniqueId);
        PresentRemoteTalkers.Add(TalkerKey);
        EnsureManagedRemoteTalker(PlayerState, TalkerKey);
        if (RegisteredRemoteTalkers.Contains(TalkerKey))
        {
            continue;
        }

        if (VoiceInterface->RegisterRemoteTalker(*UniqueId))
        {
            RegisteredRemoteTalkers.Add(TalkerKey, UniqueId);
            UE_LOG(LogSimpleVoiceChat, Log, TEXT("Registered remote talker: %s"), *TalkerKey);
        }
        else
        {
            UE_LOG(LogSimpleVoiceChat, Verbose, TEXT("Remote talker is not ready; registration will be retried: %s"), *TalkerKey);
        }
    }

    if (bHasInvalidRemoteIdentity && !bWarnedInvalidRemoteIdentity)
    {
        UE_LOG(
            LogSimpleVoiceChat,
            Warning,
            TEXT("A remote PlayerState has no valid legacy UniqueNetId. Voice cannot register that player. "
                 "Complete identity login before opening a listen server or joining by IP, then reconnect."));
    }
    bWarnedInvalidRemoteIdentity = bHasInvalidRemoteIdentity;

    for (auto It = RegisteredRemoteTalkers.CreateIterator(); It; ++It)
    {
        if (!PresentRemoteTalkers.Contains(It.Key()))
        {
            const FUniqueNetIdRepl& UniqueId = It.Value();
            if (UniqueId.IsValid() && UniqueId.IsV1())
            {
                VoiceInterface->UnregisterRemoteTalker(*UniqueId);
            }
            RemoveManagedRemoteTalker(It.Key());
            UE_LOG(LogSimpleVoiceChat, Log, TEXT("Unregistered remote talker: %s"), *It.Key());
            It.RemoveCurrent();
        }
    }

    TArray<FString> StaleManagedTalkers;
    for (const TPair<FString, TWeakObjectPtr<USimpleVoiceChatTalkerComponent>>& Pair : ManagedRemoteTalkers)
    {
        if (!PresentRemoteTalkers.Contains(Pair.Key))
        {
            StaleManagedTalkers.Add(Pair.Key);
        }
    }
    for (const FString& TalkerKey : StaleManagedTalkers)
    {
        RemoveManagedRemoteTalker(TalkerKey);
    }
}

void FOnlineSubsystemVoiceBackend::ApplyEncoderQualitySettings(UWorld* World)
{
    if (bEncoderQualitySettingsApplied || !World || !GEngine || !VoiceInterface.IsValid())
    {
        return;
    }

    const USimpleVoiceChatSettings* Settings = GetDefault<USimpleVoiceChatSettings>();
    const int32 Bitrate = FMath::Clamp(Settings->EncoderBitrate, 6000, 64000);
    const int32 Complexity = FMath::Clamp(Settings->EncoderComplexity, 0, 10);

    const bool bVbrHandled = GEngine->Exec(
        World,
        Settings->bUseVariableBitrate ? TEXT("vcvbr 1") : TEXT("vcvbr 0"));
    const bool bBitrateHandled = GEngine->Exec(
        World,
        *FString::Printf(TEXT("vcbitrate %d"), Bitrate));
    const bool bComplexityHandled = GEngine->Exec(
        World,
        *FString::Printf(TEXT("vccomplexity %d"), Complexity));

    bEncoderQualitySettingsApplied = true;
    if (bVbrHandled && bBitrateHandled && bComplexityHandled)
    {
        UE_LOG(
            LogSimpleVoiceChat,
            Log,
            TEXT("Opus quality applied: bitrate=%d bps, complexity=%d, VBR=%s"),
            Bitrate,
            Complexity,
            Settings->bUseVariableBitrate ? TEXT("on") : TEXT("off"));
    }
    else
    {
        UE_LOG(
            LogSimpleVoiceChat,
            Warning,
            TEXT("The active voice backend did not accept every Opus quality command; backend defaults remain in use"));
    }
}

void FOnlineSubsystemVoiceBackend::EnsureManagedRemoteTalker(
    APlayerState* PlayerState,
    const FString& TalkerKey)
{
    if (!PlayerState)
    {
        return;
    }

    const USimpleVoiceChatSettings* Settings = GetDefault<USimpleVoiceChatSettings>();
    if (TWeakObjectPtr<USimpleVoiceChatTalkerComponent>* ExistingManaged = ManagedRemoteTalkers.Find(TalkerKey))
    {
        if (USimpleVoiceChatTalkerComponent* Talker = ExistingManaged->Get())
        {
            Talker->SetPlaybackVolumeMultiplier(Settings->PlaybackVolumeMultiplier);
            return;
        }
        ManagedRemoteTalkers.Remove(TalkerKey);
    }

    if (UVOIPTalker* ExistingTalker = PlayerState->FindComponentByClass<UVOIPTalker>())
    {
        if (USimpleVoiceChatTalkerComponent* ManagedTalker = Cast<USimpleVoiceChatTalkerComponent>(ExistingTalker))
        {
            ManagedTalker->SetPlaybackVolumeMultiplier(Settings->PlaybackVolumeMultiplier);
            ManagedRemoteTalkers.Add(TalkerKey, ManagedTalker);
        }
        return;
    }

    USimpleVoiceChatTalkerComponent* Talker = NewObject<USimpleVoiceChatTalkerComponent>(
        PlayerState,
        USimpleVoiceChatTalkerComponent::StaticClass(),
        TEXT("SimpleVoiceChatTalker"));
    if (!Talker)
    {
        UE_LOG(LogSimpleVoiceChat, Warning, TEXT("Could not create playback controller for %s"), *TalkerKey);
        return;
    }

    Talker->SetPlaybackVolumeMultiplier(Settings->PlaybackVolumeMultiplier);
    PlayerState->AddInstanceComponent(Talker);
    Talker->RegisterComponent();
    Talker->RegisterWithPlayerState(PlayerState);
    ManagedRemoteTalkers.Add(TalkerKey, Talker);
}

void FOnlineSubsystemVoiceBackend::RemoveManagedRemoteTalker(const FString& TalkerKey)
{
    if (TWeakObjectPtr<USimpleVoiceChatTalkerComponent>* ManagedTalker = ManagedRemoteTalkers.Find(TalkerKey))
    {
        if (USimpleVoiceChatTalkerComponent* Talker = ManagedTalker->Get())
        {
            Talker->DestroyComponent();
        }
        ManagedRemoteTalkers.Remove(TalkerKey);
    }
}

void FOnlineSubsystemVoiceBackend::RemoveAllManagedRemoteTalkers()
{
    for (const TPair<FString, TWeakObjectPtr<USimpleVoiceChatTalkerComponent>>& Pair : ManagedRemoteTalkers)
    {
        if (USimpleVoiceChatTalkerComponent* Talker = Pair.Value.Get())
        {
            Talker->DestroyComponent();
        }
    }
    ManagedRemoteTalkers.Reset();
}

void FOnlineSubsystemVoiceBackend::EnsureNullIdentityLogin(const int32 LocalUserNum)
{
    const USimpleVoiceChatSettings* Settings = GetDefault<USimpleVoiceChatSettings>();
    if (!Settings->bAutoLoginNullSubsystemForDirectIp
        || !OnlineSubsystem
        || OnlineSubsystem->GetSubsystemName() != NULL_SUBSYSTEM
        || !IdentityInterface.IsValid())
    {
        return;
    }

    const FUniqueNetIdPtr ExistingId = IdentityInterface->GetUniquePlayerId(LocalUserNum);
    if (IdentityInterface->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn
        && ExistingId.IsValid()
        && ExistingId->IsValid())
    {
        if (!ValidIdentityUsers.Contains(LocalUserNum))
        {
            ValidIdentityUsers.Add(LocalUserNum);
            UE_LOG(
                LogSimpleVoiceChat,
                Log,
                TEXT("NULL identity ready for local user %d: %s"),
                LocalUserNum,
                *ExistingId->ToDebugString());
        }
        return;
    }

    if (IdentityLoginDelegateHandles.Contains(LocalUserNum))
    {
        return;
    }

    const FDelegateHandle LoginHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(
        LocalUserNum,
        FOnLoginCompleteDelegate::CreateRaw(this, &FOnlineSubsystemVoiceBackend::HandleIdentityLoginComplete));
    IdentityLoginDelegateHandles.Add(LocalUserNum, LoginHandle);

    UE_LOG(LogSimpleVoiceChat, Log, TEXT("Starting NULL identity auto-login for local user %d"), LocalUserNum);
    if (!IdentityInterface->AutoLogin(LocalUserNum))
    {
        if (const FDelegateHandle* StoredHandle = IdentityLoginDelegateHandles.Find(LocalUserNum))
        {
            FDelegateHandle MutableHandle = *StoredHandle;
            IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, MutableHandle);
        }
        IdentityLoginDelegateHandles.Remove(LocalUserNum);
        UE_LOG(LogSimpleVoiceChat, Warning, TEXT("NULL identity auto-login could not start for local user %d"), LocalUserNum);
    }
}

void FOnlineSubsystemVoiceBackend::HandleIdentityLoginComplete(
    const int32 LocalUserNum,
    const bool bWasSuccessful,
    const FUniqueNetId& UserId,
    const FString& Error)
{
    if (IdentityInterface.IsValid())
    {
        if (const FDelegateHandle* LoginHandle = IdentityLoginDelegateHandles.Find(LocalUserNum))
        {
            FDelegateHandle MutableHandle = *LoginHandle;
            IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, MutableHandle);
        }
    }
    IdentityLoginDelegateHandles.Remove(LocalUserNum);

    if (bWasSuccessful && UserId.IsValid())
    {
        ValidIdentityUsers.Add(LocalUserNum);
        UE_LOG(
            LogSimpleVoiceChat,
            Log,
            TEXT("NULL identity auto-login succeeded for local user %d: %s"),
            LocalUserNum,
            *UserId.ToDebugString());
        return;
    }

    UE_LOG(
        LogSimpleVoiceChat,
        Warning,
        TEXT("NULL identity auto-login failed for local user %d: %s"),
        LocalUserNum,
        Error.IsEmpty() ? TEXT("Unknown error") : *Error);
}

void FOnlineSubsystemVoiceBackend::ClearIdentityLoginDelegates()
{
    if (IdentityInterface.IsValid())
    {
        for (const TPair<int32, FDelegateHandle>& DelegatePair : IdentityLoginDelegateHandles)
        {
            FDelegateHandle MutableHandle = DelegatePair.Value;
            IdentityInterface->ClearOnLoginCompleteDelegate_Handle(DelegatePair.Key, MutableHandle);
        }
    }
    IdentityLoginDelegateHandles.Reset();
}

void FOnlineSubsystemVoiceBackend::EnsureNullDirectIpVoiceSession(const int32 LocalUserNum)
{
    const USimpleVoiceChatSettings* Settings = GetDefault<USimpleVoiceChatSettings>();
    if (!Settings->bAutoCreateNullVoiceSessionForDirectIp
        || !OnlineSubsystem
        || OnlineSubsystem->GetSubsystemName() != NULL_SUBSYSTEM
        || !SessionInterface.IsValid()
        || SessionInterface->GetNumSessions() > 0
        || bNullVoiceSessionCreationPending)
    {
        return;
    }

    CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateRaw(
            this,
            &FOnlineSubsystemVoiceBackend::HandleNullVoiceSessionCreated));
    bNullVoiceSessionCreationPending = true;

    FOnlineSessionSettings VoiceSessionSettings;
    VoiceSessionSettings.NumPublicConnections = 0;
    VoiceSessionSettings.NumPrivateConnections = 0;
    VoiceSessionSettings.bShouldAdvertise = false;
    VoiceSessionSettings.bAllowJoinInProgress = false;
    VoiceSessionSettings.bIsLANMatch = false;
    VoiceSessionSettings.bUsesPresence = false;
    VoiceSessionSettings.bUseLobbiesIfAvailable = false;
    VoiceSessionSettings.bUseLobbiesVoiceChatIfAvailable = false;

    UE_LOG(LogSimpleVoiceChat, Log, TEXT("Creating private NULL voice session for direct-IP testing"));
    if (!SessionInterface->CreateSession(LocalUserNum, NullDirectIpVoiceSessionName, VoiceSessionSettings))
    {
        FDelegateHandle MutableHandle = CreateSessionCompleteDelegateHandle;
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(MutableHandle);
        CreateSessionCompleteDelegateHandle.Reset();
        bNullVoiceSessionCreationPending = false;
        UE_LOG(LogSimpleVoiceChat, Warning, TEXT("Could not create the private NULL voice session"));
    }
}

void FOnlineSubsystemVoiceBackend::HandleNullVoiceSessionCreated(
    const FName SessionName,
    const bool bWasSuccessful)
{
    if (SessionInterface.IsValid() && CreateSessionCompleteDelegateHandle.IsValid())
    {
        FDelegateHandle MutableHandle = CreateSessionCompleteDelegateHandle;
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(MutableHandle);
    }
    CreateSessionCompleteDelegateHandle.Reset();
    bNullVoiceSessionCreationPending = false;

    if (SessionName != NullDirectIpVoiceSessionName)
    {
        return;
    }

    bOwnsNullDirectIpVoiceSession = bWasSuccessful;
    if (bWasSuccessful)
    {
        UE_LOG(LogSimpleVoiceChat, Log, TEXT("Private NULL voice session ready"));
    }
    else
    {
        UE_LOG(LogSimpleVoiceChat, Warning, TEXT("Private NULL voice session creation failed"));
    }
}

void FOnlineSubsystemVoiceBackend::ReleaseNullDirectIpVoiceSession()
{
    if (SessionInterface.IsValid() && CreateSessionCompleteDelegateHandle.IsValid())
    {
        FDelegateHandle MutableHandle = CreateSessionCompleteDelegateHandle;
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(MutableHandle);
    }
    CreateSessionCompleteDelegateHandle.Reset();
    bNullVoiceSessionCreationPending = false;

    if (SessionInterface.IsValid()
        && bOwnsNullDirectIpVoiceSession
        && SessionInterface->GetNamedSession(NullDirectIpVoiceSessionName))
    {
        SessionInterface->DestroySession(NullDirectIpVoiceSessionName);
    }
    bOwnsNullDirectIpVoiceSession = false;
}

void FOnlineSubsystemVoiceBackend::UnregisterRemoteTalkers()
{
    if (VoiceInterface.IsValid())
    {
        for (const TPair<FString, FUniqueNetIdRepl>& Talker : RegisteredRemoteTalkers)
        {
            if (Talker.Value.IsValid() && Talker.Value.IsV1())
            {
                VoiceInterface->UnregisterRemoteTalker(*Talker.Value);
            }
        }
    }
    RegisteredRemoteTalkers.Reset();
    RemoveAllManagedRemoteTalkers();
}

void FOnlineSubsystemVoiceBackend::ReleaseAllTalkers()
{
    if (VoiceInterface.IsValid())
    {
        UnregisterRemoteTalkers();
        for (const uint32 LocalUserNum : RegisteredLocalTalkers)
        {
            VoiceInterface->StopNetworkedVoice(static_cast<uint8>(LocalUserNum));
            VoiceInterface->UnregisterLocalTalker(LocalUserNum);
        }
    }
    else
    {
        RegisteredRemoteTalkers.Reset();
        RemoveAllManagedRemoteTalkers();
    }
    RegisteredLocalTalkers.Reset();
}

void FOnlineSubsystemVoiceBackend::ApplyMicrophoneStateToLocalTalker(const uint32 LocalUserNum) const
{
    if (!VoiceInterface.IsValid() || LocalUserNum > MAX_uint8)
    {
        return;
    }

    if (bMicrophoneEnabled)
    {
        VoiceInterface->StartNetworkedVoice(static_cast<uint8>(LocalUserNum));
    }
    else
    {
        VoiceInterface->StopNetworkedVoice(static_cast<uint8>(LocalUserNum));
    }
}

FString FOnlineSubsystemVoiceBackend::MakeRemoteTalkerKey(const FUniqueNetIdRepl& UniqueId)
{
    return FString::Printf(TEXT("%s:%s"), *UniqueId.GetType().ToString(), *UniqueId.ToString());
}
