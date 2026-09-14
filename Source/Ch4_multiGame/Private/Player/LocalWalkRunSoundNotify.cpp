#include "Player/LocalWalkRunSoundNotify.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

void ULocalWalkRunSoundNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if (IsValid(MeshComp) == false || IsValid(WalkRunSound) == false)
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(MeshComp->GetOwner());
	if (IsValid(OwnerCharacter) == false || OwnerCharacter->IsLocallyControlled() == false)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(OwnerCharacter, WalkRunSound);
}
