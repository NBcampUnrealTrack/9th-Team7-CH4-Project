// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CargoActor.generated.h"

class UCargoDataAsset;
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
class CH4_MULTIGAME_API ACargoActor : public AActor
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
#endif

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	/** Applies all static definition values through one editor-safe/runtime-safe path. */
	bool ApplyCargoData();

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

#if WITH_DEV_AUTOMATION_TESTS
	IGameFlowRuleInterface* GameFlowRuleOverrideForTesting = nullptr;
#endif
};
