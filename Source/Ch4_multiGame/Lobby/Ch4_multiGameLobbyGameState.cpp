// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lobby/Ch4_multiGameLobbyGameState.h"

#include "Ch4_multiGame.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void ACh4_multiGameLobbyGameState::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_Client)
	{
		UE_LOG(LogCh4_multiGame, Log,
			TEXT("[Lobby] CLIENT CONNECTED | Replicated Players: %d / %d"),
			CurrentPlayerCount,
			MaxPlayerCount);
	}
}

void ACh4_multiGameLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACh4_multiGameLobbyGameState, CurrentPlayerCount);
	DOREPLIFETIME(ACh4_multiGameLobbyGameState, ReadyPlayerCount);
	DOREPLIFETIME(ACh4_multiGameLobbyGameState, MaxPlayerCount);
}

bool ACh4_multiGameLobbyGameState::SetLobbyCounts(
	const int32 NewCurrentPlayerCount,
	const int32 NewReadyPlayerCount,
	const int32 NewMaxPlayerCount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[Lobby] Rejected a client-side lobby-count change"));
		return false;
	}

	const int32 ValidatedMaxPlayerCount = FMath::Max(NewMaxPlayerCount, 1);
	const int32 ValidatedCurrentPlayerCount = FMath::Clamp(
		NewCurrentPlayerCount,
		0,
		ValidatedMaxPlayerCount);
	const int32 ValidatedReadyPlayerCount = FMath::Clamp(
		NewReadyPlayerCount,
		0,
		ValidatedCurrentPlayerCount);
	const bool bPlayerCountChanged = CurrentPlayerCount != ValidatedCurrentPlayerCount
		|| MaxPlayerCount != ValidatedMaxPlayerCount;
	const bool bReadySummaryChanged = ReadyPlayerCount != ValidatedReadyPlayerCount
		|| CurrentPlayerCount != ValidatedCurrentPlayerCount;

	if (!bPlayerCountChanged && !bReadySummaryChanged)
	{
		return false;
	}

	CurrentPlayerCount = ValidatedCurrentPlayerCount;
	ReadyPlayerCount = ValidatedReadyPlayerCount;
	MaxPlayerCount = ValidatedMaxPlayerCount;
	if (bPlayerCountChanged)
	{
		BroadcastPlayerCountChanged();
	}
	if (bReadySummaryChanged)
	{
		BroadcastReadySummaryChanged();
	}
	ForceNetUpdate();
	return true;
}

void ACh4_multiGameLobbyGameState::OnRep_CurrentPlayerCount()
{
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Replicated Players: %d / %d"),
		CurrentPlayerCount,
		MaxPlayerCount);
	BroadcastPlayerCountChanged();
	BroadcastReadySummaryChanged();
}

void ACh4_multiGameLobbyGameState::OnRep_ReadyPlayerCount()
{
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Replicated Ready Players: %d / %d"),
		ReadyPlayerCount,
		CurrentPlayerCount);
	BroadcastReadySummaryChanged();
}

void ACh4_multiGameLobbyGameState::OnRep_MaxPlayerCount()
{
	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Replicated Players: %d / %d"),
		CurrentPlayerCount,
		MaxPlayerCount);
	BroadcastPlayerCountChanged();
}

void ACh4_multiGameLobbyGameState::BroadcastPlayerCountChanged()
{
	OnPlayerCountChanged.Broadcast(CurrentPlayerCount, MaxPlayerCount);
}

void ACh4_multiGameLobbyGameState::BroadcastReadySummaryChanged()
{
	OnReadySummaryChanged.Broadcast(ReadyPlayerCount, CurrentPlayerCount);
}
