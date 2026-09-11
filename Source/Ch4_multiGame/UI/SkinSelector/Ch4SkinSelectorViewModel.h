#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Player/Ch4CharacterTypes.h"
#include "Ch4SkinSelectorViewModel.generated.h"

class ACh4_CharacterPreviewStudio;
class ACh4_multiGamePlayerController;
class ACh4_multiGamePlayerState;
class ACh4_PlayerCharacter;

/**
 * 피팅룸(WBP_SkinSelector)의 상태 관리, 3D 미리보기 스튜디오 연동 및 서버 동기화를 제어하는 MVVM ViewModel
 */
UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4SkinSelectorViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	// ----------------------------------------------------------------
	// [State] View Binding용 상태 프로퍼티 (FieldNotify)
	// ----------------------------------------------------------------

	// 현재 선택 대기 중인 캐릭터 타입 (Cat, Dog, Gorilla, Otter)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	ECh4CharacterType PendingCharacterType = ECh4CharacterType::Cat;

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|State")
	void SetPendingCharacterType(ECh4CharacterType NewType);

	UFUNCTION(BlueprintPure, Category = "SkinSelector|State")
	ECh4CharacterType GetPendingCharacterType() const { return PendingCharacterType; }

	// 현재 선택 대기 중인 모자 ID (None, DrinkingHat, IronHelmet, Snapback, TrooperHat)
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	FName PendingHeadwearID = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|State")
	void SetPendingHeadwearID(FName NewHeadwearID);

	UFUNCTION(BlueprintPure, Category = "SkinSelector|State")
	FName GetPendingHeadwearID() const { return PendingHeadwearID; }

	// UI 동물 카드 활성화(선택 하이라이트)용 편의 바인딩 프로퍼티
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsCatSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsDogSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsGorillaSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsOtterSelected = false;

	// UI 모자 버튼 활성화(선택 하이라이트)용 바인딩 프로퍼티
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsHatNoneSelected = true;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsHatDrinkingSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsHatHelmetSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsHatTrooperSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsHatSnapbackSelected = false;

	// ----------------------------------------------------------------
	// [Preview Studio Lifecycle & Control]
	// ----------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkinSelector|Preview")
	TSubclassOf<ACh4_CharacterPreviewStudio> PreviewStudioClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SkinSelector|Preview")
	TObjectPtr<ACh4_CharacterPreviewStudio> ActivePreviewStudio;

	/** UI가 열릴 때 호출: 지하 고립 구역에 3D 스튜디오 액터를 스폰하고 현재 외형으로 초기화 */
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Preview")
	void StartPreviewStudio();

	/** UI가 닫힐 때 호출: 3D 스튜디오 액터를 파괴하여 리소스 정리 */
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Preview")
	void StopPreviewStudio();

	/** 마우스 드래그 Yaw 회전 전달 */
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Preview")
	void AddPreviewYaw(float DeltaYaw);

	/** 마네킹 회전 각도 초기화 */
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Preview")
	void ResetPreviewRotation();

	/** 스튜디오 배경 색상 동적 변경 */
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Preview")
	void SetPreviewBackdropColor(const FLinearColor& NewColor);

	// ----------------------------------------------------------------
	// [Lifecycle & Initialization]
	// ----------------------------------------------------------------

	/** UI가 열릴 때 PlayerState 및 Pawn으로부터 현재 착용 상태를 읽어와 초기화 */
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Lifecycle")
	void InitializeFromPlayerState();

	// ----------------------------------------------------------------
	// [Actions] 버튼 클릭 명령 (Actions)
	// ----------------------------------------------------------------

	// 동물 선택
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectCharacterType(ECh4CharacterType NewType);

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectCat() { SelectCharacterType(ECh4CharacterType::Cat); }

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectDog() { SelectCharacterType(ECh4CharacterType::Dog); }

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectGorilla() { SelectCharacterType(ECh4CharacterType::Gorilla); }

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectOtter() { SelectCharacterType(ECh4CharacterType::Otter); }

	// 모자 선택
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectHeadwear(FName HeadwearID);

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectHatNone() { SelectHeadwear(NAME_None); }

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectDrinkingHat() { SelectHeadwear(TEXT("DrinkingHat")); }

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectHelmet() { SelectHeadwear(TEXT("IronHelmet")); }

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectTrooperHat() { SelectHeadwear(TEXT("TrooperHat")); }

	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SelectSnapback() { SelectHeadwear(TEXT("Snapback")); }

	// [💾 Save] 버튼 클릭: 서버 RPC 호출 & 인게임 캐릭터 확정 반영
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SaveSelection();

	// [🔄 Reset] 버튼 클릭: 원래 스킨/모자로 되돌리기
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void ResetSelection();

	// [✖ Close] 버튼 클릭: 스튜디오 정리 후 닫기
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void CloseFittingRoom();

	virtual void BeginDestroy() override;

private:
	UUserWidget* GetOwningUserWidget() const;
	ACh4_multiGamePlayerController* GetOwningCh4PlayerController() const;
	ACh4_multiGamePlayerState* GetOwningCh4PlayerState() const;
	ACh4_PlayerCharacter* GetOwningCh4Character() const;

	void RestoreGameInputMode();
	void UpdateAnimalSelectionBooleans();
	void UpdateHatSelectionBooleans();
	FName GetEquippedHeadwearIDFromCharacter() const;
	void ApplyHeadwearToInGameCharacter(FName HeadwearID);
};
