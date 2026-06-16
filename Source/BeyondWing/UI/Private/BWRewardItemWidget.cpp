#include "BeyondWing/UI/Public/BWRewardItemWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"

void UBWRewardItemWidget::HandleClaim()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI)
	{
		// 1. C++ 보상 획득 로직 실행
		GI->ClaimReward(RewardIndex);
		
		// 2. 획득했으니 화면에서 사라짐
		RemoveFromParent();
		
		UE_LOG(LogTemp, Warning, TEXT("[RewardUI] Item Claimed at Index: %d"), RewardIndex);
	}
}
