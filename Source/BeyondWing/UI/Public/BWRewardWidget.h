#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWRewardWidget.generated.h"

UCLASS()
class BEYONDWING_API UBWRewardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 보상 한 줄을 담당할 위젯 클래스
	UPROPERTY(EditAnywhere, Category = "BW Reward")
	TSubclassOf<class UBWRewardItemWidget> RewardItemClass;

	// 보상들이 담길 수직(Vertical) 컨테이너
	UPROPERTY(meta = (BindWidget))
	class UPanelWidget* RewardContainer;

	// 보상 목록 그리기
	UFUNCTION(BlueprintCallable, Category = "BW Reward")
	void RenderRewards();
};
