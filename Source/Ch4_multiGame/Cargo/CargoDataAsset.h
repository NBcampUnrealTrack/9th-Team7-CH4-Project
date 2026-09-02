// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CargoDataAsset.generated.h"

class UPhysicalMaterial;
class UStaticMesh;
class UNiagaraSystem;

/** Immutable design-time definition shared by Cargo actors of the same type. */
UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCargoDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Designer-facing name used to identify this Cargo definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Definition")
	FText DisplayName;

	/** Mesh and mesh collision used by the Cargo actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Definition")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	/** Physics mass override in kilograms. Invalid values fall back safely at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Physics", meta=(ClampMin="0.1", UIMin="0.1", Units="kg"))
	float MassKg = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Physics", meta=(ClampMin="0.0", UIMin="0.0"))
	float LinearDamping = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Physics", meta=(ClampMin="0.0", UIMin="0.0"))
	float AngularDamping = 0.1f;

	/** Optional override. Null keeps the mesh/material's default physical material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Physics")
	TObjectPtr<UPhysicalMaterial> PhysicalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Physics")
	bool bEnableGravity = true;

	/** Lightweight identifier that does not introduce a Gameplay Tags dependency. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Definition")
	FName CargoCategory = NAME_None;

	/** Enables event-driven break damage from strong impacts against static ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Ground Impact")
	bool bBreakableFromGroundImpact = true;

	/** Number of accepted ground impacts required before this Cargo becomes Lost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Ground Impact",
		meta=(ClampMin="1", UIMin="1"))
	int32 GroundImpactsToBreak = 3;

	/** Minimum physics NormalImpulse magnitude required for a ground impact to count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Ground Impact",
		meta=(ClampMin="0.0", UIMin="0.0"))
	float MinimumGroundImpactImpulse = 8000.0f;

	/** Minimum game-time interval between accepted impacts from the same Cargo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Ground Impact",
		meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float GroundImpactCooldownSeconds = 0.4f;

	/** Minimum upward-facing impact normal. Higher values require flatter ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Ground Impact",
		meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float MinimumGroundNormalZ = 0.6f;

	/** Optional world-space Niagara effect spawned after MarkAsLost succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Ground Impact")
	TObjectPtr<UNiagaraSystem> BreakEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo|Ground Impact",
		meta=(ClampMin="0.0", UIMin="0.0"))
	FVector BreakEffectScale = FVector::OneVector;
};
