#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

UCLASS()
class CH4_MULTIGAME_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AEnemyProjectile();

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

public:	
	virtual void Tick(float DeltaTime) override;

};
