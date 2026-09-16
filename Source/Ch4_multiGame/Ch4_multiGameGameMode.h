// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "GameFlow/GameFlowRuleInterface.h"
#include "GameFramework/GameModeBase.h"
#include "Ch4_multiGameGameMode.generated.h"

class ACartBase;
class ACh4_PlayerCharacter;

enum class ECh4PlayerRecoveryReason : uint8
{
	None,
	DistanceExceeded,
	BelowCart,
	InvalidLocation
};

/**
 * Authoritative rules owner for the shared game flow.
 */
UCLASS()
class ACh4_multiGameGameMode : public AGameModeBase, public IGameFlowRuleInterface
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	ACh4_multiGameGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Deprecated Blueprint compatibility API that converts a lower absolute count into NotifyCargoLost. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Legacy",
		meta=(DeprecatedFunction, DeprecationMessage="Use NotifyCargoLost for delta-based Cargo loss."))
	bool UpdateRemainingCargo(int32 NewRemainingCargo);

	// IGameFlowRuleInterface
	virtual bool RequestCargoInitialization(int32 InitialCargoCount) override;
	virtual bool RequestGameStart() override;
	virtual bool NotifyCargoLost(int32 LostCargoCount = 1) override;
	virtual bool NotifyGoalReached(AActor* ReachingActor) override;
	virtual bool IsCargoPartOfMatch(const AActor* CargoActor) const override;

	/** Production initialization uses exactly the Cart's one-shot preparation snapshot. */
	bool InitializePreparationCargo(const TArray<class ACargoActor*>& CargoSnapshot);
	bool FinishPreparationTransition();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Lobby Return")
	bool ScheduleReturnToLobby();

	/** Reusable authoritative route for terminal result flows. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Lobby Return")
	bool ReturnToLobby();

	/** Immediate Waiting-phase return used only when preparation expires with an empty tracked Cart. */
	bool ReturnToLobbyFromPreparation();

	/** Registers the one authoritative ForestLevel Cart supplied by the preparation transition actor. */
	bool RegisterGameplayCart(ACartBase* Cart);

	/** Pure recovery policy shared by runtime checks and automation. */
	static ECh4PlayerRecoveryReason EvaluatePlayerRecovery(
		ECh4GamePhase GamePhase,
		const FVector& PlayerLocation,
		const FVector& CartLocation,
		float MaximumDistance,
		float MaximumDistanceBelowCart);

	/** Builds ordered recovery candidates from Cart yaw only: back, back-right, back-left, farther back. */
	static void BuildPlayerRecoveryCandidates(
		const FVector& CartLocation,
		const FRotator& CartRotation,
		float BehindDistance,
		float HeightOffset,
		float LateralOffset,
		TArray<FVector>& OutCandidates);

	UFUNCTION(BlueprintPure, Category="Game Flow|Lobby Return")
	bool IsReturnToLobbyScheduled() const { return bReturnToLobbyScheduled; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Lobby Return")
	TSoftObjectPtr<UWorld> LobbyMap;

	/** Shared time that success and empty-Goal results remain visible before Lobby travel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Result", meta=(ClampMin="0.0", Units="s"))
	float ResultDisplayDurationSeconds = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Player Recovery")
	bool bEnablePlayerCartRecovery = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Player Recovery",
		meta=(ClampMin="0.1", Units="s"))
	float PlayerRecoveryCheckIntervalSeconds = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Player Recovery",
		meta=(ClampMin="1.0", Units="cm"))
	float MaximumPlayerCartDistance = 12000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Player Recovery",
		meta=(ClampMin="1.0", Units="cm"))
	float MaximumVerticalDistanceBelowCart = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Player Recovery",
		meta=(ClampMin="0.0", Units="cm"))
	float RecoveryBehindCartDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Player Recovery",
		meta=(ClampMin="0.0", Units="cm"))
	float RecoveryHeightOffset = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Player Recovery",
		meta=(ClampMin="0.0", Units="cm"))
	float RecoveryGroundTraceHeight = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Player Recovery",
		meta=(ClampMin="1.0", Units="cm"))
	float RecoveryGroundTraceDepth = 1500.0f;

	/** Returns the active rule policy. Runtime cargo values live in GameState instead. */
	UFUNCTION(BlueprintPure, Category="Game Flow|Rules")
	FCh4GameRuleConfig GetGameRuleConfig() const { return GameRuleConfig; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Test-only configuration injection used before a test session starts. */
	void SetGameRuleConfigForTesting(const FCh4GameRuleConfig& NewGameRuleConfig);
	void SetDeliveryScoreSummaryForTesting(
		AActor* TargetActor,
		const FCh4DeliveryScoreSummary& NewDeliveryScoreSummary);
	void SetResultDisplayDurationForTesting(float NewDurationSeconds);
	void SetGameplayTimesForTesting(float StartServerTimeSeconds, float GoalServerTimeSeconds);
	TFunction<bool(const FString&)> LobbyTravelForTesting;
#endif

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Authoritative success/failure policy, separate from runtime state and debug settings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Rules")
	FCh4GameRuleConfig GameRuleConfig;

	/** Legacy serialized setting. Debug startup is now owned by AGameFlowDebugDriver. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Legacy Debug",
		meta=(DeprecatedProperty, DeprecationMessage="Use AGameFlowDebugDriver instead."))
	bool bAutoStartGame = false;

	/** Legacy serialized setting. It is intentionally inactive in the production GameMode. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game Flow|Legacy Debug",
		meta=(DeprecatedProperty, DeprecationMessage="Use AGameFlowDebugDriver.DebugInitialCargoCount instead.",
			EditCondition="bAutoStartGame", ClampMin="1"))
	int32 DebugInitialCargoCount = 20;

private:
	enum class EGameRuleEvaluationEvent : uint8
	{
		CargoChanged,
		GoalReached
	};

	class ACh4_multiGameGameState* GetGameFlowGameState() const;
	bool CanInitializeCargo(int32 InitialCargoCount, const ACh4_multiGameGameState& GameFlowState) const;
	bool CanStartGame(const ACh4_multiGameGameState& GameFlowState) const;
	bool CanProcessCargoChange(const ACh4_multiGameGameState& GameFlowState) const;
	bool ShouldFailGame(const ACh4_multiGameGameState& GameFlowState) const;
	bool CanCompleteGame(const ACh4_multiGameGameState& GameFlowState, int32 DeliveredCargoCount) const;
	FCh4DeliveryScoreSummary GetValidatedDeliveryScoreSummary(AActor* ReachingActor) const;
	bool EvaluateGameOutcome(EGameRuleEvaluationEvent EvaluationEvent, int32 FinalCargoScore = 0,
		int32 DeliveredCargoCount = INDEX_NONE);
	bool IsGamePhaseTransitionAllowed(ECh4GamePhase CurrentPhase, ECh4GamePhase NewPhase) const;
	bool IsGameEndReasonValidForPhase(ECh4GamePhase GamePhase, ECh4GameEndReason EndReason) const;
	bool TryTransitionGamePhase(
		ECh4GamePhase NewPhase,
		ECh4GameEndReason EndReason,
		int32 FinalCargoScore = 0);
	bool ApplyRemainingCargoCount(int32 NewRemainingCargo);
	bool EndGameAsClear(ECh4GameEndReason EndReason, int32 FinalCargoScore);
	bool EndGameAsGameOver(ECh4GameEndReason EndReason);
	bool CaptureGoalResultSnapshot(bool bSucceeded, int32 DeliveredCargoCount, int32 DeliveredCargoScore);
	float GetValidatedResultDisplayDuration() const;
	bool ScheduleEmptyCartFailure();
	void OnEmptyCartFailureTimer();
	void ClearEmptyCartFailure();
	bool GetLobbyTravelURL(FString& OutURL) const;
	bool StartLobbyTravel();
	void OnReturnToLobbyTimer();
	void StartPlayerRecoveryChecks();
	void StopPlayerRecoveryChecks();
	void CheckPlayersForRecovery();
	bool FindSafePlayerRecoveryLocation(const ACh4_PlayerCharacter& Player, FVector& OutLocation) const;
	bool RecoverPlayer(ACh4_PlayerCharacter& Player, ECh4PlayerRecoveryReason Reason);
	static const TCHAR* GetPlayerRecoveryReasonName(ECh4PlayerRecoveryReason Reason);
	FTimerHandle ReturnToLobbyTimer;
	FTimerHandle EmptyCartFailureTimer;
	FTimerHandle PlayerRecoveryTimer;
	TWeakObjectPtr<ACartBase> GameplayCart;
	TSet<TWeakObjectPtr<ACh4_PlayerCharacter>> RecoveryLocationFailureWarnings;
	bool bReturnToLobbyScheduled = false;
	bool bLobbyTravelStarted = false;
	bool bEmptyCartFailurePending = false;
	bool bUsesPreparationCargoRoster = false;
	bool bPreparationTransitionPending = false;
	bool bGameplayStartTimeRecorded = false;
	float GameplayStartServerTimeSeconds = 0.0f;
	TSet<TWeakObjectPtr<AActor>> PreparationCargoRoster;

#if WITH_DEV_AUTOMATION_TESTS
	TWeakObjectPtr<AActor> DeliveryScoreTargetOverrideForTesting;
	FCh4DeliveryScoreSummary DeliveryScoreSummaryOverrideForTesting;
	TOptional<float> GoalServerTimeOverrideForTesting;
#endif
};



