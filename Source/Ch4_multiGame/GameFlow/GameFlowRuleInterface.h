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
	/** Registers the initial cargo count before play starts. Call on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Rule")
	virtual bool RequestCargoInitialization(int32 InitialCargoCount) = 0;

	/** Requests the Waiting to Playing transition. Call on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Rule")
	virtual bool RequestGameStart() = 0;

	/** Reports cargo loss as a positive delta. Call on the server when cargo is lost. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Rule", meta=(ClampMin="1"))
	virtual bool NotifyCargoLost(int32 LostCargoCount = 1) = 0;

	/** Reports that a valid delivery target reached the final goal. Call on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Game Flow|Rule")
	virtual bool NotifyGoalReached(AActor* ReachingActor) = 0;
};
