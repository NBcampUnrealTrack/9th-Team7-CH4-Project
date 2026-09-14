#include "UI/Tutorial/Ch4TutorialWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"

UCh4TutorialWidget::UCh4TutorialWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex0(TEXT("/Game/UI/Tutorial/Tutorial_01_Load_EN.Tutorial_01_Load_EN"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex1(TEXT("/Game/UI/Tutorial/Tutorial_02_TeamUp_EN.Tutorial_02_TeamUp_EN"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex2(TEXT("/Game/UI/Tutorial/Tutorial_03_Deliver_EN.Tutorial_03_Deliver_EN"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex3(TEXT("/Game/UI/Tutorial/Tutorial_04_Controls_EN.Tutorial_04_Controls_EN"));

	if (Tex0.Succeeded()) { TutorialTextures.Add(Tex0.Object); }
	if (Tex1.Succeeded()) { TutorialTextures.Add(Tex1.Object); }
	if (Tex2.Succeeded()) { TutorialTextures.Add(Tex2.Object); }
	if (Tex3.Succeeded()) { TutorialTextures.Add(Tex3.Object); }
}

void UCh4TutorialWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &UCh4TutorialWidget::CloseTutorial);
	}
	if (Btn_Prev)
	{
		Btn_Prev->OnClicked.AddUniqueDynamic(this, &UCh4TutorialWidget::PrevPage);
	}
	if (Btn_Next)
	{
		Btn_Next->OnClicked.AddUniqueDynamic(this, &UCh4TutorialWidget::NextPage);
	}

	UpdatePage(0);

	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}
}

void UCh4TutorialWidget::NativeDestruct()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}

	Super::NativeDestruct();
}

FReply UCh4TutorialWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::A || Key == EKeys::Left)
	{
		PrevPage();
		return FReply::Handled();
	}
	if (Key == EKeys::D || Key == EKeys::Right)
	{
		NextPage();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::E)
	{
		CloseTutorial();
		return FReply::Handled();
	}

	// 팝업이 열려 있는 동안 모든 키보드 입력을 UI가 소비하여 캐릭터 이동(W, S 등) 차단
	return FReply::Handled();
}

void UCh4TutorialWidget::UpdatePage(int32 NewIndex)
{
	if (TutorialTextures.Num() == 0)
	{
		return;
	}

	CurrentPageIndex = FMath::Clamp(NewIndex, 0, TutorialTextures.Num() - 1);

	if (Img_Slide && TutorialTextures.IsValidIndex(CurrentPageIndex) && TutorialTextures[CurrentPageIndex])
	{
		Img_Slide->SetBrushFromTexture(TutorialTextures[CurrentPageIndex]);
	}

	if (Text_PageNum)
	{
		const FText PageText = FText::Format(
			FText::FromString(TEXT("{0} / {1}")),
			FText::AsNumber(CurrentPageIndex + 1),
			FText::AsNumber(TutorialTextures.Num())
		);
		Text_PageNum->SetText(PageText);
	}

	if (Btn_Prev)
	{
		Btn_Prev->SetIsEnabled(CurrentPageIndex > 0);
	}

	if (Btn_Next)
	{
		Btn_Next->SetIsEnabled(CurrentPageIndex < (TutorialTextures.Num() - 1));
	}
}

void UCh4TutorialWidget::NextPage()
{
	if (CurrentPageIndex < TutorialTextures.Num() - 1)
	{
		UpdatePage(CurrentPageIndex + 1);
	}
}

void UCh4TutorialWidget::PrevPage()
{
	if (CurrentPageIndex > 0)
	{
		UpdatePage(CurrentPageIndex - 1);
	}
}

void UCh4TutorialWidget::CloseTutorial()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}

	RemoveFromParent();
}
