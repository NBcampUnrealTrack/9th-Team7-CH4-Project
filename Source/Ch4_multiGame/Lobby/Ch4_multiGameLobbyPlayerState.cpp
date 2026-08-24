// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lobby/Ch4_multiGameLobbyPlayerState.h"

#include "Ch4_multiGame.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

void ACh4_multiGameLobbyPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACh4_multiGameLobbyPlayerState, bIsReady);
}

bool ACh4_multiGameLobbyPlayerState::SetReady()
{
	if (!HasAuthority() || bIsReady)
	{
		return false;
	}

	bIsReady = true;
	OnReadyStateChanged.Broadcast(bIsReady);
	ForceNetUpdate();
	return true;
}

void ACh4_multiGameLobbyPlayerState::OnRep_IsReady()
{
	OnReadyStateChanged.Broadcast(bIsReady);

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[Lobby] Ready replicated: %s = %s"),
		*GetPlayerName(),
		bIsReady ? TEXT("READY") : TEXT("NOT READY"));

	const APawn* PlayerPawn = GetPawn();
	if (bIsReady && IsValid(PlayerPawn) && PlayerPawn->IsLocallyControlled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			8.0f,
			FColor::Green,
			TEXT("[LOBBY] READY confirmed by server"));
	}
}
