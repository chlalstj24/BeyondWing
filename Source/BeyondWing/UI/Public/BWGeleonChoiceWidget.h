#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWGeleonChoiceWidget.generated.h"

class UButton;
class UTextBlock;

// 11-4 겔레온: 전투 시작 때 효과카드를 쓸지 고르는 화면 (BTN_Attack = 효과카드 안 씀, BTN_Effect = 효과카드 사용). 위젯 블루프린트 없이도 쓸 수 있도록 화면을 코드로 만든다.
// 블루프린트로 꾸밀 때는 같은 이름의 위젯(TXT_Title, TXT_Desc, BTN_Attack, TXT_Attack, BTN_Effect, TXT_Effect)을 두면 그대로 연결된다.
UCLASS()
class BEYONDWING_API UBWGeleonChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Title;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Desc;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Attack;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Attack;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Effect;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Effect;

	// 덱의 종류별 장수로 버튼 글자를 갱신한다
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Dimension")
	void SetDeckCounts(int32 AttackCount, int32 EffectCount);

	// true = 효과카드 사용 (강화에 쓰는 공격카드는 처음 쓴 1종만), false = 효과카드 안 씀
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Dimension")
	void Choose(bool bUseEffects);

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildDefaultLayout();

	UFUNCTION() void OnAttackClicked() { Choose(false); }
	UFUNCTION() void OnEffectClicked() { Choose(true); }
};
