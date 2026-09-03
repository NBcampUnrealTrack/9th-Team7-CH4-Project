#include "Player/GrabReleaseNotify.h"
#include "Player/Ch4_PlayerCharacter.h"

void UGrabReleaseNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (AActor* Owner = MeshComp->GetOwner())
	{
		if (ACh4_PlayerCharacter* Character = Cast<ACh4_PlayerCharacter>(Owner))
		{
			Character->OnGrabReleaseNotify();
		}
	}
}
