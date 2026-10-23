#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWAugmentSelectWidget.generated.h"

class UButton;
class UTextBlock;

// 막 시작 증강 선택 화면 (3택 1, 리롤 1회). 위젯 블루프린트 없이도 쓸 수 있도록 화면을 코드로 만든다.
// 블루프린트로 꾸밀 때는 같은 이름의 위젯(BTN_Choice0~2, TXT_Choice0~2, BTN_Reroll, TXT_Reroll, TXT_Title)을 두면 그대로 연결된다.
UCLASS()
class BEYONDWING_API UBWAugmentSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Title;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Choice0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Choice1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Choice2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Choice0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Choice1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Choice2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Reroll;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Reroll;

	// GameInstance의 현재 후보와 리롤 횟수로 글자를 갱신한다
	UFUNCTION(BlueprintCallable, Category = "BW Augment")
	void RefreshChoices();

	UFUNCTION(BlueprintCallable, Category = "BW Augment")
	void Choose(int32 ChoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "BW Augment")
	void Reroll();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

private:
	void BuildDefaultLayout();

	UFUNCTION() void OnChoice0Clicked() { Choose(0); }
	UFUNCTION() void OnChoice1Clicked() { Choose(1); }
	UFUNCTION() void OnChoice2Clicked() { Choose(2); }
	UFUNCTION() void OnRerollClicked() { Reroll(); }
};
