// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FinalDeliveryZone.generated.h"

class UBoxComponent;
class USceneComponent;

/** Placement wrapper. UFinalDeliveryZoneComponent owns all delivery evaluation. */
UCLASS()
class AFinalDeliveryZone : public AActor
{
	GENERATED_BODY()

public:
	AFinalDeliveryZone();

protected:
	// Preserve the serialized Root/TriggerCollision names and property types for existing BPs.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> TriggerCollision;

};
