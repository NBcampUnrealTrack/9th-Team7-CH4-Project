#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/GrabbableInterface.h"
#include "TestPickUpActor.generated.h"

UCLASS()
class CH4_MULTIGAME_API ATestPickUpActor : public AActor, public IGrabbableInterface
{
	GENERATED_BODY()
	
public:	
	ATestPickUpActor();
	
	UPROPERTY(VisibleAnywhere, Category="Grab")
	class UStaticMeshComponent* MeshComponent;
	
	virtual UPrimitiveComponent* GetGrabbableComponent() override { return MeshComponent; }
};
