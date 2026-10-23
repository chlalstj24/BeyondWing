#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWTurnBannerWidget.generated.h"

class UBorder;
class UTextBlock;

// "내 턴" / "적 턴" 띠가 화면을 가로질러 들어왔다 나간다. 입력을 막지 않고, 끝나면 스스로 내려간다.
// 위젯 블루프린트로 꾸밀 때는 같은 이름의 위젯을 두면 된다
UCLASS()
class BEYONDWING_API UBWTurnBannerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UBorder* Band;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Title;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Sub;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner") float SlideTime = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Banner") float HoldTime = 0.7f;

	void ShowBanner(const FString& Title, const FString& Sub, const FLinearColor& Accent);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultLayout();
	float Age = -1.0f;
};
