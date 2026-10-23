#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWCardWidget.generated.h"

UCLASS()
class BEYONDWING_API UBWCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card", meta = (ExposeOnSpawn = "true"))
	FBWCardData CardData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card", meta = (ExposeOnSpawn = "true"))
	int32 HandIndex;

	// 선택 상태를 수동으로 설정 (버튼 클릭 시 호출용)
	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetSelected();

	UFUNCTION(BlueprintCallable, Category = "Card")
	void SetHovered(bool bInHovered);

	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void OnHoverStateChanged(bool bInHovered);

	UPROPERTY(BlueprintReadWrite, Category = "Card")
	FVector2D TargetTranslation;

	UPROPERTY(BlueprintReadWrite, Category = "Card")
	float TargetRotation;

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	class UBWHandWidget* OwningHand;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Card")
	class UWidget* Visual_Group;

	UFUNCTION(BlueprintImplementableEvent, Category = "Card")
	void K2_InitCardUI();

	UFUNCTION(BlueprintCallable, Category = "Card")
	UTexture2D* GetCardTexture(TSoftObjectPtr<UTexture2D> SoftPtr);

	UFUNCTION(BlueprintPure, Category = "Card")
	FText GetCardNameText() const { return FText::FromString(CardData.CardName); }

	UFUNCTION(BlueprintPure, Category = "Card")
	FText GetCardDescriptionText() const { return FText::FromString(CardData.Description); }

	// [추가] 카드 왼쪽 위 칸: 공격카드는 숫자 범위 "4~9" (손패에서는 기본 공격에 더하는 몫 "+2~5"), 효과카드는 "효과". MP가 들면 "· MP 2"를 붙인다
	UFUNCTION(BlueprintPure, Category = "Card")
	FText GetCostText() const;

	// [추가] 손패가 SkillMin~SkillMax를 기본 공격 강화 몫으로 바꿔 넣었으면 true ("+a~b"로 표시)
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	bool bShowAsBoost = false;

	// [추가] 카드 아래 칸: 상태이상·밀기·회복 같은 핵심 효과를 짧게 ("밀기", "기절 1", "회복 6"). 없으면 "-"
	UFUNCTION(BlueprintPure, Category = "Card")
	FText GetKeywordText() const;

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	bool bIsHovered;
};