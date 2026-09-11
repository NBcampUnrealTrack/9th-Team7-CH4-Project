// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Ch4_multiGameGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCh4GamePhaseChangedSignature, ECh4GamePhase, NewGamePhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCh4CargoCountChangedSignature, int32, RemainingCargoCount, int32, InitialCargoCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCh4PreparationTimerUpdatedSignature, float, RemainingSeconds, float, TotalSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCh4GameResultChangedSignature, FCh4GameResult, NewResult);

/**
 * Replicated data store for the authoritative game flow.
 * Only ACh4_multiGameGameMode is allowed to mutate these values.
 */
UCLASS()
class CH4_MULTIGAME_API ACh4_multiGameGameState : public AGameStateBase
{
	GENERATED_BODY()

	friend class ACh4_multiGameGameMode;

public:
	/** Fired on the server and clients whenever the game phase changes. */
	UPROPERTY(BlueprintAssignable, Category="Game Flow|Events")
	FCh4GamePhaseChangedSignature OnGamePhaseChanged;

	/** Fired on the server and clients whenever either cargo count changes. */
	UPROPERTY(BlueprintAssignable, Category="Game Flow|Events")
	FCh4CargoCountChangedSignature OnCargoCountChanged;

	/** Fired on the server and clients whenever the preparation timer is set or updated. */
	UPROPERTY(BlueprintAssignable, Category="Game Flow|Events")
	FCh4PreparationTimerUpdatedSignature OnPreparationTimerUpdated;

	/** Fired once when a Goal snapshot arrives and again if its terminal phase metadata changes. */
	UPROPERTY(BlueprintAssignable, Category="Game Flow|Events")
	FCh4GameResultChangedSignature OnGameResultChanged;

	UFUNCTION(BlueprintPure, Category="Game Flow")
	ECh4GamePhase GetCurrentGamePhase() const { return CurrentGamePhase; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Result")
	ECh4GameEndReason GetGameEndReason() const { return GameEndReason; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Timer")
	float GetPreparationEndTime() const { return PreparationEndTime; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Timer")
	float GetPreparationTotalDuration() const { return PreparationTotalDuration; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Timer")
	float GetRemainingPreparationTime() const;

	UFUNCTION(BlueprintPure, Category="Game Flow|Timer")
	float GetPreparationTimeRatio() const;

	/** Called on server to initialize the preparation countdown. */
	void SetPreparationTimer(float DurationSeconds);

	UFUNCTION(BlueprintPure, Category="Game Flow|Cargo")
	int32 GetInitialCargoCount() const { return InitialCargoCount; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Cargo")
	int32 GetRemainingCargoCount() const { return RemainingCargoCount; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Cargo")
	int32 GetLostCargoCount() const;

	/** Returns a normalized value in the 0.0 to 1.0 range. */
	UFUNCTION(BlueprintPure, Category="Game Flow|Cargo")
	float GetCargoSurvivalRate() const;

	/** Team Cargo score finalized by the authoritative GameMode when the match is Cleared. */
	UFUNCTION(BlueprintPure, Category="Game Flow|Result")
	int32 GetFinalCargoScore() const { return FinalCargoScore; }

	/** Returns the frozen Goal snapshot when available, otherwise the current derived flow state. */
	UFUNCTION(BlueprintPure, Category="Game Flow|Result")
	FCh4GameResult GetGameResult() const;

	UFUNCTION(BlueprintPure, Category="Game Flow|Result")
	bool HasGameResultSnapshot() const { return GameResultSnapshot.bResultAvailable; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	bool SetGamePhaseState(
		ECh4GamePhase NewGamePhase,
		ECh4GameEndReason NewEndReason,
		int32 NewFinalCargoScore);
	bool SetCargoCounts(int32 NewInitialCargoCount, int32 NewRemainingCargoCount);
	bool SetRemainingCargoCount(int32 NewRemainingCargoCount);
	bool SetGoalResultSnapshot(
		float ClearTimeSeconds,
		int32 DeliveredCargoCount,
		int32 DeliveredCargoScore,
		bool bSucceeded,
		float ResultDisplayEndServerTime);

	UFUNCTION()
	void OnRep_CurrentGamePhase();

	UFUNCTION()
	void OnRep_CargoCounts();

	UFUNCTION()
	void OnRep_PreparationEndTime();

	UFUNCTION()
	void OnRep_GameResultSnapshot();

	void BroadcastCargoCountChanged();

private:
	UPROPERTY(ReplicatedUsing=OnRep_CargoCounts, BlueprintReadOnly, Category="Game Flow|Cargo", meta=(AllowPrivateAccess="true"))
	int32 InitialCargoCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CargoCounts, BlueprintReadOnly, Category="Game Flow|Cargo", meta=(AllowPrivateAccess="true"))
	int32 RemainingCargoCount = 0;

	/**
	 * Replicated before terminal reason/phase so Cleared phase listeners read the matching score snapshot.
	 * It remains zero outside Cleared and has no public setter.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Game Flow|Result", meta=(AllowPrivateAccess="true"))
	int32 FinalCargoScore = 0;

	/** Replicated before CurrentGamePhase so phase listeners can read the matching terminal reason. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Game Flow|Result", meta=(AllowPrivateAccess="true"))
	ECh4GameEndReason GameEndReason = ECh4GameEndReason::None;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentGamePhase, BlueprintReadOnly, Category="Game Flow", meta=(AllowPrivateAccess="true"))
	ECh4GamePhase CurrentGamePhase = ECh4GamePhase::Waiting;

	UPROPERTY(ReplicatedUsing=OnRep_PreparationEndTime, BlueprintReadOnly, Category="Game Flow|Timer", meta=(AllowPrivateAccess="true"))
	float PreparationEndTime = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Game Flow|Timer", meta=(AllowPrivateAccess="true"))
	float PreparationTotalDuration = 60.0f;

	UPROPERTY(ReplicatedUsing=OnRep_GameResultSnapshot, BlueprintReadOnly, Category="Game Flow|Result", meta=(AllowPrivateAccess="true"))
	FCh4GameResult GameResultSnapshot;
};
