// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFlowRuleInterface.generated.h"

class AActor;

/**
 * Server-authoritative entry points used by gameplay actors to notify the game rules.
 * Implementations are native-only so Blueprint callers cannot replace authority checks.
 */
UINTERFACE(MinimalAPI, BlueprintType, meta=(CannotImplementInterfaceInBlueprint))
class UGameFlowRuleInterface : public UInterface
{
	GENERATED_BODY()
};

class CH4_MULTIGAME_API IGameFlowRuleInterface
{
	GENERATED_BODY()

public:
	/** Registers a positive initial Cargo count once during Waiting. Returns false without mutation otherwise. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Rule")
	virtual bool RequestCargoInitialization(int32 InitialCargoCount) = 0;

	/** Requests Waiting to Playing after Cargo initialization. Duplicate or out-of-order calls return false. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Rule")
	virtual bool RequestGameStart() = 0;

	/** Reports a positive Delta no greater than remaining Cargo while Playing. Invalid calls do not mutate state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Rule", meta=(ClampMin="1"))
	virtual bool NotifyCargoLost(int32 LostCargoCount = 1) = 0;

	/** Reports a valid same-world Actor reaching the goal while Playing. Rule failure returns false unchanged. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Rule")
	virtual bool NotifyGoalReached(AActor* ReachingActor) = 0;
};
