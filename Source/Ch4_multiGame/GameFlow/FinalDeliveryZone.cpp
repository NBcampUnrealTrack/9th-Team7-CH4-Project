// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFlow/FinalDeliveryZone.h"

#include "Components/SceneComponent.h"
#include "Map/FinalDeliveryZoneComponent.h"

AFinalDeliveryZone::AFinalDeliveryZone()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	TriggerCollision = CreateDefaultSubobject<UFinalDeliveryZoneComponent>(TEXT("TriggerCollision"));
	TriggerCollision->SetupAttachment(SceneRoot);
}
