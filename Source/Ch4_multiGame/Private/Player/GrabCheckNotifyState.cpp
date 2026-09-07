#include "Player/GrabCheckNotifyState.h"

#include "Components/SkeletalMeshComponent.h"
#include "Player/Ch4_PlayerCharacter.h"

void UGrabCheckNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (ACh4_PlayerCharacter* Character = Cast<ACh4_PlayerCharacter>(Owner))
		{
			Character->BeginGrabDetection();
		}
	}
}

void UGrabCheckNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (ACh4_PlayerCharacter* Character = Cast<ACh4_PlayerCharacter>(Owner))
		{
			Character->EndGrabDetection();
		}
	}
}
