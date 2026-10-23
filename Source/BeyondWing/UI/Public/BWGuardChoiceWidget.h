#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWGuardChoiceWidget.generated.h"

class UButton;
class UTextBlock;
class UHorizontalBox;
class UImage;
class ABWCharacter;

// [추가] INT·FTH 매직실드: 적이 공격해 올 때 가드에 MP를 얼마나 걸지 고르는 화면.
// 적이 굴리기 전에 고른다 (걸어 둔 MP는 피해가 적어도 모두 사라진다). 위젯 블루프린트 없이 코드로 화면을 만든다.
// 조작: ◀ ▶ 버튼, ←/→ 또는 A/D, 숫자 키 0~9로 바로 지정, Space/Enter = 가드, Esc = 가드 안 함
UCLASS()
class BEYONDWING_API UBWGuardChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 화면 내용을 채운다. OnChosen은 고른 MP로 한 번만 불린다
	void Setup(ABWCharacter* InPlayer, const FString& EnemyName, const FString& ActionName, const FBWDiceRange& IncomingRange, TFunction<void(int32)> InOnChosen);

	// 걸 MP를 정한다 (0 ~ 걸 수 있는 최대)
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Guard") void SetSpend(int32 NewSpend);
	UFUNCTION(BlueprintPure, Category = "BW Combat|Guard") int32 GetSpend() const { return Spend; }
	// 지금 고른 MP로 확정 (화면을 닫고 적이 굴린다)
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Guard") void Confirm();
	// MP를 걸지 않고 맞는다
	UFUNCTION(BlueprintCallable, Category = "BW Combat|Guard") void ConfirmNone();

	// --- 문구 (Class Defaults에서 수정) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Guard|문구") FString TitleText = TEXT("마나 가드");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Guard|문구") FString GuardButtonText = TEXT("가드 (Space)");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Guard|문구") FString NoGuardButtonText = TEXT("가드 안 함 (Esc)");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Guard|문구", meta = (MultiLine = true))
	FString HintText = TEXT("←/→ 또는 숫자 키로 조절 · 걸어 둔 MP는 피해가 적어도 모두 사라집니다");

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildLayout();
	void Refresh();
	void Finish(int32 Chosen);
	void RebuildCells(int32 Count);

	UFUNCTION() void OnMinusClicked() { SetSpend(Spend - 1); }
	UFUNCTION() void OnPlusClicked() { SetSpend(Spend + 1); }
	UFUNCTION() void OnZeroClicked() { SetSpend(0); }
	UFUNCTION() void OnCoverClicked() { SetSpend(CoverSpend); }
	UFUNCTION() void OnMaxClicked() { SetSpend(MaxSpend); }
	UFUNCTION() void OnGuardClicked() { Confirm(); }
	UFUNCTION() void OnNoGuardClicked() { ConfirmNone(); }

	UPROPERTY() TObjectPtr<ABWCharacter> PlayerChar;
	TFunction<void(int32)> OnChosen;
	bool bFinished = false;
	int32 Spend = 0;
	int32 MaxSpend = 0;
	int32 CoverSpend = 0; // 적 최댓값을 다 막는 MP
	FBWDiceRange Incoming;
	float Age = 0.0f;

	UPROPERTY() TObjectPtr<UWidget> PanelWidget;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_Title;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_Threat;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_Spend;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_Block;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_Mana;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_Outcome;
	UPROPERTY() TObjectPtr<UTextBlock> TXT_CoverLabel;
	UPROPERTY() TObjectPtr<UHorizontalBox> CellRow;
	UPROPERTY() TArray<TObjectPtr<UImage>> Cells;
	UPROPERTY() TArray<TObjectPtr<UButton>> Buttons;
};
