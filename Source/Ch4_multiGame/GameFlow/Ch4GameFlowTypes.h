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

/** Authoritative reason recorded when a game session reaches a terminal phase. */
UENUM(BlueprintType)
enum class ECh4GameEndReason : uint8
{
	None UMETA(DisplayName="None"),
	GoalReached UMETA(DisplayName="Goal Reached"),
	CargoRuleFailed UMETA(DisplayName="Cargo Rule Failed")
};

/**
 * Designer-facing policy values for game-flow evaluation.
 * Runtime cargo counts are intentionally stored only in ACh4_multiGameGameState.
 */
USTRUCT(BlueprintType)
struct CH4_MULTIGAME_API FCh4GameRuleConfig
{
	GENERATED_BODY()

	/** Ends the game when no cargo remains. Preserves the existing default rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Failure")
	bool bFailWhenCargoEmpty = true;

	/** Ends the game after the first cargo loss, regardless of the remaining count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Failure")
	bool bFailOnAnyCargoLost = false;

	/** Delivered count for score providers; remaining count for providerless legacy/debug targets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Success", meta=(ClampMin="0", UIMin="0"))
	int32 MinimumCargoCountToClear = 1;

	/** Normalized cargo survival rate required when the goal is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Success", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float MinimumCargoSurvivalRateToClear = 0.0f;
};

/** Optional final-delivery snapshot supplied by a GameFlow target such as the future Cart. */
USTRUCT(BlueprintType)
struct CH4_MULTIGAME_API FCh4DeliveryScoreSummary
{
	GENERATED_BODY()

	/** False keeps legacy/debug targets compatible and resolves to a zero score. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Game Flow|Delivery Score")
	bool bHasScoreData = false;

	/** Cargo references that actually reached the goal with this target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Game Flow|Delivery Score")
	int32 DeliveredCargoCount = 0;

	/** Sum of the delivered Cargo actors' safe delivery scores. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Game Flow|Delivery Score")
	int32 DeliveredCargoScore = 0;
};

/** Replicated result snapshot captured at the first valid Goal arrival. */
USTRUCT(BlueprintType)
struct CH4_MULTIGAME_API FCh4GameResult
{
	GENERATED_BODY()

	/** True after the server has captured the first valid Goal result. */
	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	bool bResultAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	ECh4GamePhase GamePhase = ECh4GamePhase::Waiting;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	ECh4GameEndReason GameEndReason = ECh4GameEndReason::None;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	int32 InitialCargoCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	int32 RemainingCargoCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	int32 LostCargoCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	float CargoSurvivalRate = 0.0f;

	/** Team Cargo score finalized only when the match successfully reaches the goal. */
	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	int32 FinalCargoScore = 0;

	/** Playing time from the successful RequestGameStart call to the first valid Goal arrival. */
	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result", meta=(ClampMin="0.0", Units="s"))
	float ClearTimeSeconds = 0.0f;

	/** Cargo physically inside the reaching Cart at the Goal, sourced from its delivery summary. */
	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result", meta=(ClampMin="0"))
	int32 DeliveredCargoCount = 0;

	/** Synchronized server-world timestamp at which the result display period ends. */
	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result", meta=(Units="s"))
	float ResultDisplayEndServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	bool bGameEnded = false;

	UPROPERTY(BlueprintReadOnly, Category="Game Flow|Result")
	bool bSucceeded = false;
};
