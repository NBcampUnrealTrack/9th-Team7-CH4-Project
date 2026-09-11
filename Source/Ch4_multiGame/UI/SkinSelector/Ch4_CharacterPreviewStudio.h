#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Ch4CharacterTypes.h"
#include "Ch4_CharacterPreviewStudio.generated.h"

class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UTextureRenderTarget2D;
class UDataTable;

/**
 * 피팅룸(WBP_SkinSelector) 전용 3D 프리뷰 스튜디오 액터.
 * 인게임 월드와 격리된 지하에 로컬 스폰되어,
 * 그림자/루멘/블룸 없는 깔끔한 단색 룩으로 캐릭터와 모자를 RT_CharacterPreview에 렌더링합니다.
 */
UCLASS()
class CH4_MULTIGAME_API ACh4_CharacterPreviewStudio : public AActor
{
	GENERATED_BODY()

public:
	ACh4_CharacterPreviewStudio();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

public:
	/** 미리보기 마네킹의 동물 외형 및 애니메이션을 변경합니다. */
	UFUNCTION(BlueprintCallable, Category = "Preview|Appearance")
	void SetPreviewCharacterType(ECh4CharacterType NewType);

	/** 미리보기 마네킹의 모자를 변경하거나 해제합니다 (NAME_None 시 해제). */
	UFUNCTION(BlueprintCallable, Category = "Preview|Appearance")
	void SetPreviewHeadwear(FName HeadwearID);

	/** 마우스 드래그 입력을 받아 마네킹을 좌우로 회전시킵니다. */
	UFUNCTION(BlueprintCallable, Category = "Preview|Interaction")
	void AddPreviewYaw(float DeltaYaw);

	/** 마네킹의 회전 각도를 정면으로 초기화합니다. */
	UFUNCTION(BlueprintCallable, Category = "Preview|Interaction")
	void ResetPreviewRotation();

	UFUNCTION(BlueprintCallable, Category = "Preview|Backdrop")
	void SetBackdropColor(const FLinearColor& NewColor);

	UFUNCTION(BlueprintPure, Category = "Preview|Appearance")
	ECh4CharacterType GetCurrentCharacterType() const { return CurrentCharacterType; }

	UFUNCTION(BlueprintPure, Category = "Preview|Appearance")
	FName GetCurrentHeadwearID() const { return CurrentHeadwearID; }

	UFUNCTION(BlueprintPure, Category = "Preview|Components")
	USceneCaptureComponent2D* GetCaptureComponent() const { return CaptureComponent; }

	UFUNCTION(BlueprintPure, Category = "Preview|Components")
	USkeletalMeshComponent* GetPreviewMeshComponent() const { return PreviewMeshComponent; }

	UFUNCTION(BlueprintPure, Category = "Preview|Components")
	UStaticMeshComponent* GetHatMeshComponent() const { return HatMeshComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview|Components")
	TObjectPtr<USceneComponent> StudioRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview|Components")
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview|Components")
	TObjectPtr<USkeletalMeshComponent> PreviewMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview|Components")
	TObjectPtr<UStaticMeshComponent> HatMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview|Components")
	TObjectPtr<UStaticMeshComponent> BackdropMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview|Components")
	TObjectPtr<UDirectionalLightComponent> KeyLightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview|Components")
	TObjectPtr<UDirectionalLightComponent> FillLightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview|Components")
	TObjectPtr<USkyLightComponent> SkyLightComponent;

	/** 스튜디오 배경 사용 여부 (false 시 투명 배경) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Backdrop")
	bool bUseBackdrop = true;

	/** 스튜디오 배경 색상 (세련된 소프트 슬레이트 네이비 스튜디오 기본값) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Backdrop")
	FLinearColor BackdropColor = FLinearColor(0.08f, 0.095f, 0.15f, 1.0f);

	/** 비네팅 강도 (모서리를 은은하게 정돈, 0.0 ~ 1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|PostProcess")
	float VignetteIntensity = 0.2f;

	/** 블룸 강도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|PostProcess")
	float BloomIntensity = 0.0f;

	/** 카메라 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Camera")
	float CameraDistance = 205.0f;

	/** 카메라 높이 (낮출수록 캐릭터가 화면 위로 올라옵니다) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Camera")
	float CameraHeight = 32.0f;

	/** 카메라 각도 (올릴수록 캐릭터가 화면 위로 올라옵니다) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Camera")
	float CameraPitch = 5.0f;

	/** 카메라 화각 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Camera")
	float CameraFOV = 36.0f;

	UFUNCTION(BlueprintCallable, Category = "Preview|Camera")
	void UpdateCameraTransform();

	/** 프리뷰 화면을 캡처할 렌더 타겟 (기본값: RT_CharacterPreview) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Settings")
	TObjectPtr<UTextureRenderTarget2D> PreviewRenderTarget;

	/** 캡처 소스 (SCS_FinalColorLDR 또는 SCS_SceneColorHDR) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Settings")
	TEnumAsByte<ESceneCaptureSource> CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	/** 동물 스켈레톤의 모자 부착 소켓 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview|Settings")
	FName HatSocketName = TEXT("S_Headwear");

	/** 모자 데이터 테이블 (DT_Headwear) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview|Settings")
	TObjectPtr<UDataTable> HeadwearDataTable;

	/** BPC_AccessoryEquipment 컴포넌트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview|Settings")
	TSubclassOf<UActorComponent> AccessoryEquipmentClass;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> AccessoryEquipment;

private:
	UPROPERTY(Transient)
	ECh4CharacterType CurrentCharacterType = ECh4CharacterType::Cat;

	UPROPERTY(Transient)
	FName CurrentHeadwearID = NAME_None;

	void ApplyHeadwearInternal(FName HeadwearID);
	void UpdateAccessoryAnimalType();
};
