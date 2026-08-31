// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFlow/GameFlowDebugDriver.h"

#include "Ch4_multiGame.h"
#include "Engine/World.h"
#include "GameFlow/GameFlowRuleInterface.h"
#include "GameFramework/GameModeBase.h"

AGameFlowDebugDriver::AGameFlowDebugDriver()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AGameFlowDebugDriver::BeginPlay()
{
	Super::BeginPlay();

	TryStartDebugGame();
}

bool AGameFlowDebugDriver::TryStartDebugGame()
{
	const bool bHasServerAuthority = HasAuthority() && GetNetMode() != NM_Client;
	if (!CanAttemptDebugStartup(bAutoStartDebugGame, bHasServerAuthority, DebugInitialCargoCount))
	{
		if (bAutoStartDebugGame && !bHasServerAuthority)
		{
			UE_LOG(LogCh4_multiGame, Verbose, TEXT("[GameFlow Debug] Startup ignored on a client instance"));
		}
		else if (bAutoStartDebugGame && DebugInitialCargoCount <= 0)
		{
			UE_LOG(LogCh4_multiGame, Warning,
				TEXT("[GameFlow Debug] Startup requires a positive Cargo count: %d"),
				DebugInitialCargoCount);
		}
		return false;
	}

	IGameFlowRuleInterface* GameFlowRule = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	GameFlowRule = GameFlowRuleOverrideForTesting;
#endif
	if (!GameFlowRule)
	{
		AGameModeBase* AuthGameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
		GameFlowRule = Cast<IGameFlowRuleInterface>(AuthGameMode);
	}

	if (!GameFlowRule)
	{
		UE_LOG(LogCh4_multiGame, Error,
			TEXT("[GameFlow Debug] Active GameMode does not implement IGameFlowRuleInterface"));
		return false;
	}

	if (!GameFlowRule->RequestCargoInitialization(DebugInitialCargoCount))
	{
		UE_LOG(LogCh4_multiGame, Warning,
			TEXT("[GameFlow Debug] Cargo initialization request was rejected: %d"),
			DebugInitialCargoCount);
		return false;
	}

	if (!GameFlowRule->RequestGameStart())
	{
		UE_LOG(LogCh4_multiGame, Warning, TEXT("[GameFlow Debug] Game start request was rejected"));
		return false;
	}

	UE_LOG(LogCh4_multiGame, Log,
		TEXT("[GameFlow Debug] Started with %d Cargo through IGameFlowRuleInterface"),
		DebugInitialCargoCount);
	return true;
}

bool AGameFlowDebugDriver::CanAttemptDebugStartup(
	const bool bEnabled,
	const bool bHasServerAuthority,
	const int32 InitialCargoCount)
{
	return bEnabled && bHasServerAuthority && InitialCargoCount > 0;
}

#if WITH_DEV_AUTOMATION_TESTS
void AGameFlowDebugDriver::ConfigureForTesting(
	const bool bNewAutoStartDebugGame,
	const int32 NewDebugInitialCargoCount,
	IGameFlowRuleInterface* NewGameFlowRuleOverride)
{
	bAutoStartDebugGame = bNewAutoStartDebugGame;
	DebugInitialCargoCount = NewDebugInitialCargoCount;
	GameFlowRuleOverrideForTesting = NewGameFlowRuleOverride;
}

bool AGameFlowDebugDriver::RunDebugStartupForTesting()
{
	return TryStartDebugGame();
}

bool AGameFlowDebugDriver::CanAttemptDebugStartupForTesting(
	const bool bEnabled,
	const bool bHasServerAuthority,
	const int32 InitialCargoCount)
{
	return CanAttemptDebugStartup(bEnabled, bHasServerAuthority, InitialCargoCount);
}
#endif
