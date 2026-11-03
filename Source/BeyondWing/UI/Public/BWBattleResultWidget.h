#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWBattleResultWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

// 전투가 끝나면 화면을 어둡게 덮고 "승리 / 패배"와 결과(턴·HP·보상)를 차례로 띄운다.
// 클릭·Space·Enter = 계속 (연출 중이면 먼저 끝까지 보여 주고, 다시 누르면 넘어감). 넘어갈 때 검게 어두워진 뒤 OnFinished
UCLASS()
class BEYONDWING_API UBWBattleResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UBorder* Dim;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UBorder* Fade;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Title;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UBorder* Rule; // 제목 아래 장식선
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UBorder* Plate; // 결과를 담는 석판
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Sub;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UVerticalBox* BOX_Lines;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Continue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result") float DimTime = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result") float TitleTime = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result") float LineInterval = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result") float FadeOutTime = 0.35f;

	// Lines: 한 줄씩 (이름, 값). 값이 비면 이름만 가운데에
	void ShowResult(bool bVictory, const FString& Sub, const TArray<TPair<FString, FString>>& Lines, const FString& ContinueText, TFunction<void()> InOnFinished);

	// 클릭·키 입력. 연출 중이면 끝까지 건너뛰고, 다 보였으면 넘어간다
	void Continue();
	bool IsShowing() const { return Age >= 0.0f; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildDefaultLayout();
	float LinesEndTime() const;

	UPROPERTY() TArray<TObjectPtr<UWidget>> LineWidgets;
	TFunction<void()> OnFinished;
	FLinearColor Accent = FLinearColor::White;
	float Age = -1.0f;
	float LeaveAge = -1.0f;
};
