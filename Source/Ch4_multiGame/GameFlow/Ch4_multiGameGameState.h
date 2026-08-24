// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "Ch4_multiGameGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCh4GamePhaseChangedSignature, ECh4GamePhase, NewGamePhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCh4CargoCountChangedSignature, int32, RemainingCargoCount, int32, InitialCargoCount);

/**
 * Replicated data store for the authoritative game flow.
 * Only ACh4_multiGameGameMode is allowed to mutate these values.
 */
UCLASS()
class ACh4_multiGameGameState : public AGameStateBase
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

	UFUNCTION(BlueprintPure, Category="Game Flow")
	ECh4GamePhase GetCurrentGamePhase() const { return CurrentGamePhase; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Cargo")
	int32 GetInitialCargoCount() const { return InitialCargoCount; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Cargo")
	int32 GetRemainingCargoCount() const { return RemainingCargoCount; }

	UFUNCTION(BlueprintPure, Category="Game Flow|Cargo")
	int32 GetLostCargoCount() const;

	/** Returns a normalized value in the 0.0 to 1.0 range. */
	UFUNCTION(BlueprintPure, Category="Game Flow|Cargo")
	float GetCargoSurvivalRate() const;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	bool SetCurrentGamePhase(ECh4GamePhase NewGamePhase);
	bool SetCargoCounts(int32 NewInitialCargoCount, int32 NewRemainingCargoCount);
	bool SetRemainingCargoCount(int32 NewRemainingCargoCount);

	UFUNCTION()
	void OnRep_CurrentGamePhase();

	UFUNCTION()
	void OnRep_CargoCounts();

	void BroadcastCargoCountChanged();

private:
	UPROPERTY(ReplicatedUsing=OnRep_CargoCounts, BlueprintReadOnly, Category="Game Flow|Cargo", meta=(AllowPrivateAccess="true"))
	int32 InitialCargoCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CargoCounts, BlueprintReadOnly, Category="Game Flow|Cargo", meta=(AllowPrivateAccess="true"))
	int32 RemainingCargoCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentGamePhase, BlueprintReadOnly, Category="Game Flow", meta=(AllowPrivateAccess="true"))
	ECh4GamePhase CurrentGamePhase = ECh4GamePhase::Waiting;
};
