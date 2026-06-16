#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BWRewardItemWidget.generated.h"

UCLASS()
class BEYONDWING_API UBWRewardItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 이 위젯이 담당하는 보상의 인덱스
	int32 RewardIndex = -1;
	FRewardItem MyData;

	// 블루프린트에서 텍스트나 아이콘을 바꿀 때 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "BW Reward")
	void K2_SetupVisuals(const FRewardItem& Data);

	// 클릭 시 (블루프린트 버튼 OnClicked에서 호출하세요)
	UFUNCTION(BlueprintCallable, Category = "BW Reward")
	void HandleClaim();
};
