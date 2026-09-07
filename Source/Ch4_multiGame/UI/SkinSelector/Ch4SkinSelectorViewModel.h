#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Player/Ch4CharacterTypes.h"
#include "Ch4SkinSelectorViewModel.generated.h"

/**
 * 피팅룸(WBP_SkinSelector)의 상태 관리, 실시간 미리보기 및 서버 동기화를 제어하는 MVVM ViewModel
 */
UCLASS(BlueprintType)
class CH4_MULTIGAME_API UCh4SkinSelectorViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
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

	// UI 카드 활성화(선택 하이라이트)용 편의 바인딩 프로퍼티
	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsCatSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsDogSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsGorillaSelected = false;

	UPROPERTY(FieldNotify, BlueprintReadOnly, Category = "SkinSelector|State")
	bool bIsOtterSelected = false;

	// ----------------------------------------------------------------
	// [Lifecycle & Initialization]
	// ----------------------------------------------------------------

	// UI가 열릴 때 PlayerState로부터 현재 스킨을 읽어와 초기화
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Lifecycle")
	void InitializeFromPlayerState();

	// ----------------------------------------------------------------
	// [Actions] 버튼 클릭 명령 (Actions)
	// ----------------------------------------------------------------

	// 동물 선택 버튼 클릭
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

	// [💾 Save] 버튼 클릭: 서버 RPC 호출로 영구 확정 & 닫기
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void SaveSelection();

	// [🔄 Reset] 버튼 클릭: 원래 스킨으로 되돌리기
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void ResetSelection();

	// [✖ Close] 버튼 클릭: 원래대로 복원 후 닫기
	UFUNCTION(BlueprintCallable, Category = "SkinSelector|Actions")
	void CloseFittingRoom();

private:
	class ACh4_multiGamePlayerController* GetOwningCh4PlayerController() const;
	class ACh4_multiGamePlayerState* GetOwningCh4PlayerState() const;
	class ACh4_PlayerCharacter* GetOwningCh4Character() const;

	void UpdateSelectionBooleans();
	void ApplyPreview(ECh4CharacterType TypeToPreview);
};
