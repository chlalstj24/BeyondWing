#include "BeyondWing/UI/Public/BWRewardWidget.h"
#include "BeyondWing/UI/Public/BWRewardItemWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Components/PanelWidget.h"

void UBWRewardWidget::RenderRewards()
{
	if (!RewardContainer || !RewardItemClass) return;

	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	// 기존 목록 지우기
	RewardContainer->ClearChildren();

	// GameInstance의 PendingRewards 배열을 순회하며 위젯 생성
	for (int32 i = 0; i < GI->PendingRewards.Num(); ++i)
	{
		UBWRewardItemWidget* NewItem = CreateWidget<UBWRewardItemWidget>(this, RewardItemClass);
		if (NewItem)
		{
			NewItem->RewardIndex = i;
			NewItem->MyData = GI->PendingRewards[i];
			
			// 블루프린트에게 비주얼 설정 명령
			NewItem->K2_SetupVisuals(GI->PendingRewards[i]);
			
			// 컨테이너에 추가
			RewardContainer->AddChild(NewItem);
		}
	}
}
