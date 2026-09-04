// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Ch4GameFlowTypes.h"
#include "UObject/Interface.h"
#include "GameFlowTargetInterface.generated.h"

/** Interface for an actor that can complete delivery and optionally report its delivered Cargo score. */
UINTERFACE(MinimalAPI, Blueprintable)
class UGameFlowTargetInterface : public UInterface
{
	GENERATED_BODY()
};

/** Implementing this interface remains enough for FinalDeliveryZone to recognize an actor. */
class CH4_MULTIGAME_API IGameFlowTargetInterface
{
	GENERATED_BODY()

public:
	/**
	 * Returns the Cargo score snapshot owned by this target at the moment it reaches the goal.
	 * Legacy/debug targets may keep the default implementation and still clear with zero score.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Game Flow|Delivery Score")
	FCh4DeliveryScoreSummary GetDeliveryScoreSummary() const;
	virtual FCh4DeliveryScoreSummary GetDeliveryScoreSummary_Implementation() const
	{
		return FCh4DeliveryScoreSummary();
	}
};
