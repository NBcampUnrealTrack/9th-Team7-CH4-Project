// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/GrabbableInterface.h"
#include "CargoActor.generated.h"

class UCargoDataAsset;
class UPrimitiveComponent;
class UStaticMeshComponent;
class IGameFlowRuleInterface;

UENUM(BlueprintType)
enum class ECargoState : uint8
{
	Active UMETA(DisplayName="Active"),
	Lost UMETA(DisplayName="Lost")
};

/**
 * Server-authoritative, data-driven physics Cargo with no Cart dependency.
 * External authoritative systems decide when to call MarkAsLost().
 */
UCLASS(Blueprintable)
class CH4_MULTIGAME_API ACargoActor : public AActor, public IGrabbableInterface
{
	GENERATED_BODY()

public:
	ACargoActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Cargo")
	UCargoDataAsset* GetCargoData() const { return CargoData; }

	UFUNCTION(BlueprintPure, Category="Cargo|State")
	ECargoState GetCargoState() const { return CargoState; }

	UFUNCTION(BlueprintPure, Category="Cargo|State")
	bool IsLost() const { return CargoState == ECargoState::Lost; }

	/** Returns zero for Lost Cargo, missing data, or invalid negative data. */
	UFUNCTION(BlueprintPure, Category="Cargo|Score")
	int32 GetDeliveryScore() const;

	virtual UPrimitiveComponent* GetGrabbableComponent() override;

	/** Server-only runtime count. It is intentionally not replicated or stored in CargoData. */
	UFUNCTION(BlueprintPure, Category="Cargo|Ground Impact")
	int32 GetGroundImpactCount() const { return GroundImpactCount; }

	/**
	 * Reports one Cargo loss through IGameFlowRuleInterface and changes state exactly once.
	 * Client calls and rejected GameFlow notifications leave the Actor unchanged.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Cargo|State")
	bool MarkAsLost();

#if WITH_DEV_AUTOMATION_TESTS
	void SetCargoDataForTesting(UCargoDataAsset* NewCargoData) { CargoData = NewCargoData; }
	bool ApplyCargoDataForTesting() { return ApplyCargoData(); }
	void SetGameFlowRuleOverrideForTesting(IGameFlowRuleInterface* NewGameFlowRule)
	{
		GameFlowRuleOverrideForTesting = NewGameFlowRule;
	}
	bool ProcessGroundImpactForTesting(
		ECollisionChannel OtherObjectType,
		bool bOtherComponentIsStatic,
		float ImpactNormalZ,
		float NormalImpulseMagnitude,
		double CurrentTimeSeconds)
	{
		return ProcessGroundImpact(
			OtherObjectType,
			bOtherComponentIsStatic,
			ImpactNormalZ,
			NormalImpulseMagnitude,
			CurrentTimeSeconds,
			FVector::ZeroVector,
			FVector(0.0f, 0.0f, ImpactNormalZ));
	}
#endif

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	/** Applies all static definition values through one editor-safe/runtime-safe path. */
	bool ApplyCargoData();
	bool ProcessGroundImpact(
		ECollisionChannel OtherObjectType,
		bool bOtherComponentIsStatic,
		float ImpactNormalZ,
		float NormalImpulseMagnitude,
		double CurrentTimeSeconds,
		const FVector& ImpactPoint,
		const FVector& ImpactNormal);
	bool TryBreakCargo(const FVector& ImpactPoint, const FVector& ImpactNormal);

	UFUNCTION()
	void OnCargoMeshHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayBreakEffect(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal);

	UFUNCTION()
	void OnRep_CargoState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Cargo", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CargoMesh;

	/** Configure on a Blueprint default or a level-placed Cargo instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cargo", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCargoDataAsset> CargoData;

	/** Runtime-only state; the static DataAsset never stores per-instance state. */
	UPROPERTY(ReplicatedUsing=OnRep_CargoState, VisibleInstanceOnly, BlueprintReadOnly, Transient,
		Category="Cargo|State", meta=(AllowPrivateAccess="true"))
	ECargoState CargoState = ECargoState::Active;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Cargo|Ground Impact",
		meta=(AllowPrivateAccess="true"))
	int32 GroundImpactCount = 0;

	double NextAllowedGroundImpactTimeSeconds = 0.0;
	bool bBreakInProgress = false;

#if WITH_DEV_AUTOMATION_TESTS
	IGameFlowRuleInterface* GameFlowRuleOverrideForTesting = nullptr;
#endif
};
