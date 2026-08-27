// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "GameFlow/GameFlowRuleInterface.h"
#include "GameFramework/GameModeBase.h"
#include "Ch4_multiGameGameMode.generated.h"

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

	/** Deprecated Blueprint compatibility API that converts a lower absolute count into NotifyCargoLost. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Legacy",
		meta=(DeprecatedFunction, DeprecationMessage="Use NotifyCargoLost for delta-based Cargo loss."))
	bool UpdateRemainingCargo(int32 NewRemainingCargo);

	// IGameFlowRuleInterface
	virtual bool RequestCargoInitialization(int32 InitialCargoCount) override;
	virtual bool RequestGameStart() override;
	virtual bool NotifyCargoLost(int32 LostCargoCount = 1) override;
	virtual bool NotifyGoalReached(AActor* ReachingActor) override;

	/** Returns the active rule policy. Runtime cargo values live in GameState instead. */
	UFUNCTION(BlueprintPure, Category="Game Flow|Rules")
	FCh4GameRuleConfig GetGameRuleConfig() const { return GameRuleConfig; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Test-only configuration injection used before a test session starts. */
	void SetGameRuleConfigForTesting(const FCh4GameRuleConfig& NewGameRuleConfig);
#endif

protected:
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
	bool CanCompleteGame(const ACh4_multiGameGameState& GameFlowState) const;
	bool EvaluateGameOutcome(EGameRuleEvaluationEvent EvaluationEvent);
	bool IsGamePhaseTransitionAllowed(ECh4GamePhase CurrentPhase, ECh4GamePhase NewPhase) const;
	bool IsGameEndReasonValidForPhase(ECh4GamePhase GamePhase, ECh4GameEndReason EndReason) const;
	bool TryTransitionGamePhase(ECh4GamePhase NewPhase, ECh4GameEndReason EndReason);
	bool ApplyRemainingCargoCount(int32 NewRemainingCargo);
	void EndGameAsClear(ECh4GameEndReason EndReason);
	void EndGameAsGameOver(ECh4GameEndReason EndReason);
};



