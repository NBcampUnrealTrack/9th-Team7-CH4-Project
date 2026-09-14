#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ch4TutorialWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * WBP_Tutorial 전용 C++ 베이스 위젯 클래스.
 * 4종 튜토리얼 이미지 슬라이드 전환, 버튼 이벤트,
 * 키보드 단축키(A, D, E, ESC, 좌우 방향키), 입력 모드 자동 전환을 처리합니다.
 */
UCLASS()
class CH4_MULTIGAME_API UCh4TutorialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCh4TutorialWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 지정한 인덱스의 페이지로 갱신 (이미지, 1/4 텍스트, 버튼 활성화 여부) */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void UpdatePage(int32 NewIndex);

	/** 다음 페이지로 이동 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void NextPage();

	/** 이전 페이지로 이동 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void PrevPage();

	/** 튜토리얼 팝업 닫기 및 게임 전용 입력 모드 복구 */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void CloseTutorial();

	/** 현재 페이지 인덱스 반환 */
	UFUNCTION(BlueprintPure, Category = "Tutorial")
	int32 GetCurrentPageIndex() const { return CurrentPageIndex; }

	/** 총 페이지 수 반환 */
	UFUNCTION(BlueprintPure, Category = "Tutorial")
	int32 GetTotalPages() const { return TutorialTextures.Num(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** 튜토리얼에 표시할 슬라이드 텍스처 배열 (기본 4종 자동 등록) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Tutorial|Assets")
	TArray<TObjectPtr<UTexture2D>> TutorialTextures;

	/** 현재 표시 중인 페이지 번호 (0부터 시작) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tutorial|State")
	int32 CurrentPageIndex = 0;

	/** UMG 위젯 바인딩: 닫기 버튼 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Close;

	/** UMG 위젯 바인딩: 이전 페이지 버튼 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Prev;

	/** UMG 위젯 바인딩: 다음 페이지 버튼 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Next;

	/** UMG 위젯 바인딩: 중앙 메인 슬라이드 이미지 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Slide;

	/** UMG 위젯 바인딩: 하단 페이지 번호 텍스트 ("1 / 4") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PageNum;
};
