#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GridTile.h"
#include "BWCardWidget.generated.h"

UCLASS()
class BEYONDWING_API UBWCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card", meta = (ExposeOnSpawn = "true"))
	FCardData CardData;

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

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Card")
	bool bIsHovered;
};
