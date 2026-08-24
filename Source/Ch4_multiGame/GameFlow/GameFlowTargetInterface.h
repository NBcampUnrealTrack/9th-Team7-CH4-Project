// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFlowTargetInterface.generated.h"

/** Marker interface for the Cart actor that can complete the delivery. */
UINTERFACE(MinimalAPI, Blueprintable)
class UGameFlowTargetInterface : public UInterface
{
	GENERATED_BODY()
};

/** Implementing this marker is enough for FinalDeliveryZone to recognize an actor. */
class IGameFlowTargetInterface
{
	GENERATED_BODY()
};
