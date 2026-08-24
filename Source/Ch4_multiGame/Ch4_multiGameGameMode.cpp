// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ch4_multiGameGameMode.h"

#include "Ch4_multiGame.h"
#include "GameFlow/Ch4_multiGameGameState.h"

ACh4_multiGameGameMode::ACh4_multiGameGameMode()
{
	GameStateClass = ACh4_multiGameGameState::StaticClass();
}

void ACh4_multiGameGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStartGame)
	{
		if (InitializeCargoCount(DebugInitialCargoCount))
		{
			StartGame();
		}
	}
}

bool ACh4_multiGameGameMode::InitializeCargoCount(const int32 CargoCount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] InitializeCargoCount rejected without server authority"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Waiting)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Cargo can only be initialized while Waiting"));
		return false;
	}

	if (CargoCount <= 0)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Cargo initialization requires at least one item"));
		return false;
	}

	GameFlowState->SetCargoCounts(CargoCount, CargoCount);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Cargo Initialized: %d"), CargoCount);
	return true;
}

bool ACh4_multiGameGameMode::StartGame()
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] StartGame rejected without server authority"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Waiting)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] StartGame requires the Waiting phase"));
		return false;
	}

	if (GameFlowState->GetInitialCargoCount() <= 0 || GameFlowState->GetRemainingCargoCount() <= 0)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] StartGame requires initialized cargo"));
		return false;
	}

	GameFlowState->SetCurrentGamePhase(ECh4GamePhase::Playing);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Game Started"));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Remaining Cargo: %d / %d"),
		GameFlowState->GetRemainingCargoCount(), GameFlowState->GetInitialCargoCount());
	return true;
}

bool ACh4_multiGameGameMode::UpdateRemainingCargo(const int32 NewRemainingCargo)
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] UpdateRemainingCargo rejected without server authority"));
		return false;
	}

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Cargo change ignored outside the Playing phase"));
		return false;
	}

	const int32 PreviousCargoCount = GameFlowState->GetRemainingCargoCount();
	const int32 ValidatedCargoCount = FMath::Clamp(NewRemainingCargo, 0, GameFlowState->GetInitialCargoCount());

	if (ValidatedCargoCount != NewRemainingCargo)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Cargo count clamped from %d to %d"), NewRemainingCargo, ValidatedCargoCount);
	}

	if (!GameFlowState->SetRemainingCargoCount(ValidatedCargoCount))
	{
		return false;
	}

	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Cargo Changed: %d -> %d"), PreviousCargoCount, ValidatedCargoCount);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Remaining Cargo: %d / %d"),
		ValidatedCargoCount, GameFlowState->GetInitialCargoCount());

	if (ValidatedCargoCount <= 0)
	{
		UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] No Cargo Remaining"));
		EndGameAsGameOver();
	}

	return true;
}

bool ACh4_multiGameGameMode::TryCompleteGame()
{
	if (!HasAuthority())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] TryCompleteGame rejected without server authority"));
		return false;
	}

	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Final Delivery Zone Reached"));

	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow] Clear request ignored outside the Playing phase"));
		return false;
	}

	if (GameFlowState->GetRemainingCargoCount() <= 0)
	{
		EndGameAsGameOver();
		return false;
	}

	EndGameAsClear();
	return true;
}

ACh4_multiGameGameState* ACh4_multiGameGameMode::GetGameFlowGameState() const
{
	ACh4_multiGameGameState* GameFlowState = GetWorld()
		? GetWorld()->GetGameState<ACh4_multiGameGameState>()
		: nullptr;
	if (!GameFlowState)
	{
		UE_LOG(LogCh4_multiGame, Error, TEXT("[GameFlow] ACh4_multiGameGameState is not active. Check the GameMode GameStateClass."));
	}

	return GameFlowState;
}

void ACh4_multiGameGameMode::EndGameAsClear()
{
	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return;
	}

	GameFlowState->SetCurrentGamePhase(ECh4GamePhase::Cleared);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] GAME CLEARED"));
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] Remaining Cargo: %d / %d"),
		GameFlowState->GetRemainingCargoCount(), GameFlowState->GetInitialCargoCount());
}

void ACh4_multiGameGameMode::EndGameAsGameOver()
{
	ACh4_multiGameGameState* GameFlowState = GetGameFlowGameState();
	if (!GameFlowState || GameFlowState->GetCurrentGamePhase() != ECh4GamePhase::Playing)
	{
		return;
	}

	GameFlowState->SetCurrentGamePhase(ECh4GamePhase::GameOver);
	UE_LOG(LogCh4_multiGame, Log, TEXT("[GameFlow] GAME OVER"));
}
