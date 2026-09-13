#include "UI/Settings/Ch4SettingsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

void UCh4SettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddUniqueDynamic(this, &UCh4SettingsWidget::CloseSettings);
	}
}

void UCh4SettingsWidget::OpenSettings()
{
	bIsClosing = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CloseTimerHandle);
	}

	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();

	if (Border_0)
	{
		Border_0->SetRenderOpacity(1.0f);
		Border_0->SetRenderTranslation(FVector2D(0.f, -250.f));
	}

	if (Anim_SlideDown)
	{
		PlayAnimation(Anim_SlideDown, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
	else
	{
		if (Border_0)
		{
			Border_0->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}
}

void UCh4SettingsWidget::CloseSettings()
{
	bIsClosing = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CloseTimerHandle);
	}

	if (Anim_SlideDown)
	{
		// 역재생 실행 (StopAnimation을 부르면 의도치 않게 Finish 이벤트가 즉시 발화하므로 직접 역재생 실행)
		PlayAnimationReverse(Anim_SlideDown, 1.0f, false);

		// 애니메이션 종료 콜백 누락 방지용 안전 타이머
		const float Duration = Anim_SlideDown->GetEndTime();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CloseTimerHandle, this, &UCh4SettingsWidget::CloseSettingsImmediate, FMath::Max(Duration + 0.05f, 0.1f), false);
		}
	}
	else
	{
		CloseSettingsImmediate();
	}
}

void UCh4SettingsWidget::CloseSettingsImmediate()
{
	bIsClosing = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CloseTimerHandle);
	}

	if (Anim_SlideDown && IsAnimationPlaying(Anim_SlideDown))
	{
		StopAnimation(Anim_SlideDown);
	}

	SetVisibility(ESlateVisibility::Collapsed);

	if (Border_0)
	{
		Border_0->SetRenderOpacity(0.0f);
		Border_0->SetRenderTranslation(FVector2D(0.f, -250.f));
	}

	OnSettingsClosed.Broadcast();
}

void UCh4SettingsWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation == Anim_SlideDown)
	{
		if (bIsClosing)
		{
			// ✅ 역방향(닫기) 슬라이드 완료: 즉시 안전하게 숨김 처리하여 블러/클릭 차단 해제
			CloseSettingsImmediate();
		}
		else
		{
			// ✅ 정방향 슬라이드 다운 완료: 화면 중앙 위치(0,0)와 불투명도(1.0)를 C++에서 영구 고정!
			if (Border_0)
			{
				Border_0->SetRenderOpacity(1.0f);
				Border_0->SetRenderTranslation(FVector2D::ZeroVector);
			}
		}
	}
}

FReply UCh4SettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (bIsClosing)
		{
			CloseSettingsImmediate();
		}
		else
		{
			CloseSettings();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UCh4SettingsWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	SetFocus();
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

