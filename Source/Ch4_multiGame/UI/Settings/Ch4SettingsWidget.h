#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ch4SettingsWidget.generated.h"

class UBorder;
class UButton;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsClosed);

/**
 * WBP_Settings 전용 C++ 베이스 위젯 클래스.
 * 슬라이드 다운 애니메이션 수명주기, 종료 시 중앙 고정, 닫기 역재생 및 가시성을 완벽하게 제어합니다.
 */
UCLASS()
class CH4_MULTIGAME_API UCh4SettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 설정창이 완전히 닫혔을 때(Collapsed) 발화하는 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnSettingsClosed OnSettingsClosed;

	// 설정창 열기 (위젯 표시 및 슬라이드 다운 애니메이션 재생)
	UFUNCTION(BlueprintCallable, Category = "Settings|Animation")
	void OpenSettings();

	// 설정창 닫기 (슬라이드 업 역재생 후 Collapsed)
	UFUNCTION(BlueprintCallable, Category = "Settings|Animation")
	void CloseSettings();

	// 애니메이션 없이 즉시 닫기 (Collapsed 처리 및 위치 초기화)
	UFUNCTION(BlueprintCallable, Category = "Settings|Animation")
	void CloseSettingsImmediate();

	// 현재 닫히는 중인지 여부
	UFUNCTION(BlueprintPure, Category = "Settings|Animation")
	bool IsClosing() const { return bIsClosing; }

protected:
	virtual void NativeConstruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 애니메이션 대상인 카드 외곽선 테두리
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_0;

	// 닫기 (X / 뒤로가기) 버튼
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Back;

	// 슬라이드 다운 애니메이션
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_SlideDown;

private:
	bool bIsClosing = false;
	FTimerHandle CloseTimerHandle;
};
