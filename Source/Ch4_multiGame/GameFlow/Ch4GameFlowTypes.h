// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Ch4GameFlowTypes.generated.h"

/** High-level state of a game flow session. */
UENUM(BlueprintType)
enum class ECh4GamePhase : uint8
{
	Waiting UMETA(DisplayName="Waiting"),
	Playing UMETA(DisplayName="Playing"),
	Cleared UMETA(DisplayName="Cleared"),
	GameOver UMETA(DisplayName="Game Over")
};

/** Read-only result snapshot derived from the replicated game-flow state. */
USTRUCT(BlueprintType)
struct CH4_MULTIGAME_API FCh4GameResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	ECh4GamePhase GamePhase = ECh4GamePhase::Waiting;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	int32 InitialCargoCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	int32 RemainingCargoCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	int32 LostCargoCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	float CargoSurvivalRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	bool bGameEnded = false;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	bool bSucceeded = false;
};
