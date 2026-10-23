#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWUnitStatusWidget.generated.h"

class ABWCharacter;
class UProgressBar;
class UTextBlock;

// 말 머리 위 상태 표시: 이번 턴 행동(적), HP 바(깎이면 흰 잔상이 따라 줄어듦), 방어도, 상태이상.
// 말이 BeginPlay에서 화면 공간 위젯 컴포넌트로 붙인다. 위젯 블루프린트로 꾸밀 때는 같은 이름의 위젯을 둔다
UCLASS()
class BEYONDWING_API UBWUnitStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Intent;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UProgressBar* PB_HPLag;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UProgressBar* PB_HP;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_HP;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Status;

	// 잔상이 따라 줄어드는 속도 (초당 비율)와 시작 전 대기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status") float LagSpeed = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Status") float LagDelay = 0.35f;

	void SetOwnerCharacter(ABWCharacter* InOwner) { OwnerChar = InOwner; }

	// 적의 이번 턴 행동을 짧게 ("공격 2~5", "회복 3", "강화 +2 → 공격 4~7")
	static FString DescribeIntent(const ABWCharacter* Enemy, FLinearColor& OutColor);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultLayout();
	TWeakObjectPtr<ABWCharacter> OwnerChar;
	float ShownLag = -1.0f;
	float LagWait = 0.0f;
	int32 LastHP = -1;
};
