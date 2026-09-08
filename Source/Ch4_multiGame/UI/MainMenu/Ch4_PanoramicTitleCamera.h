#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ch4_PanoramicTitleCamera.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class CH4_MULTIGAME_API ACh4_PanoramicTitleCamera : public AActor
{
	GENERATED_BODY()

public:
	ACh4_PanoramicTitleCamera();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArmComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
	float RotationSpeed = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
	float CameraPitch = -10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
	float ArmLength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
	float CameraFOV = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Settings")
	bool bAutoActivate = true;
};
