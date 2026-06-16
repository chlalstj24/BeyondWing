#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BWHandWidget.generated.h"

class UBWCardWidget;
class UCanvasPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStartReceived);

UCLASS()
class BEYONDWING_API UBWHandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand")
	TSubclassOf<UBWCardWidget> CardWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "Hand")
	void RefreshHand(const TArray<FBWCardData>& InHandCards);

	UFUNCTION(BlueprintCallable, Category = "Hand")
	void UpdateCardLayout();

	void OnCardHovered(UBWCardWidget* InCard);
	void OnCardUnhovered(UBWCardWidget* InCard);

	// 시전 애니메이션 시작 (PlayerController에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Hand")
	void PlayDiscardAnimation(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Hand")
	void LinkToPlayerDeck(class ABWCharacter* TargetCharacter);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Hand")
	void OnCombatStart_Received();

	UFUNCTION()
	void HandleHandChanged(const TArray<FBWCardData>& CurrentHand);

	UPROPERTY(BlueprintReadWrite, Category = "Hand")
	int32 SelectedCardIndex = -1;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCanvasPanel* HandCanvas;

	UPROPERTY() TArray<UBWCardWidget*> CardWidgets;
	UPROPERTY() UBWCardWidget* HoveredCard;
	
	// 소멸 애니메이션용 유령 카드
	UPROPERTY() UBWCardWidget* DiscardGhost;
	FVector2D GhostTranslation;
	float GhostScale = 1.0f;
	float DiscardAnimTimer = 0.0f;

	// 설정 값
	UPROPERTY(EditAnywhere, Category = "Hand|Layout") FVector2D CardSize = FVector2D(360.0f, 530.0f);
	UPROPERTY(EditAnywhere, Category = "Hand|Layout") float ArcRadius = 1500.0f;
	UPROPERTY(EditAnywhere, Category = "Hand|Layout") float AngleStep = 5.0f;
	UPROPERTY(EditAnywhere, Category = "Hand|Layout") FVector2D HandCenterOffset = FVector2D(0.0f, -100.0f);
	UPROPERTY(EditAnywhere, Category = "Hand|Selection") FVector2D SelectionScreenPos = FVector2D(600.0f, -550.0f);
	UPROPERTY(EditAnywhere, Category = "Hand|Selection") float BackgroundCardsYOffset = 500.0f;
	UPROPERTY(EditAnywhere, Category = "Hand|Selection") FVector2D DiscardTargetPos = FVector2D(800.0f, 200.0f);
	UPROPERTY(EditAnywhere, Category = "Hand|Hover") float HoverYOffset = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Hand|Hover") float HoverScale = 1.15f;
	UPROPERTY(EditAnywhere, Category = "Hand|Hover") float InterpSpeed = 15.0f;

private:
	TArray<FVector2D> CurrentTranslations;
	TArray<float> CurrentRotations;
	TArray<float> CurrentScales;
};