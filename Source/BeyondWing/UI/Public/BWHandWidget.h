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

	// [추가] 숫자 키로 카드 집기: 카드가 마우스를 따라오고, 좌클릭하면 사용 (원래 자리를 누르면 내려놓음).
	// 같은 번호를 다시 누르면 내려놓고, 다른 번호를 누르면 그 카드로 바꿔 집는다
	void ToggleHoldCard(int32 Index);
	void CancelHeldCard();
	// 좌클릭으로 놓기: 원래 자리 위면 내려놓고 false, 아니면 카드를 쓰고 true
	bool ReleaseHeldCard();
	bool IsHoldingCard() const { return HeldIndex != -1; }
	// 집은 카드를 놓은 그 클릭이 카드 버튼 클릭으로도 들어가지 않게 (놓은 직후 잠깐은 카드 클릭 무시)
	bool ShouldIgnoreCardClick() const;

	// [추가] 지금 눈여겨보는 카드 (집은 카드 → 끄는 카드 → 마우스를 올린 카드 순). 없으면 -1
	int32 GetFocusedCardIndex() const;

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
	// [추가] 부채꼴 전체 각도 상한 (손패가 많으면 카드 사이 각도를 줄인다)
	UPROPERTY(EditAnywhere, Category = "Hand|Layout") float MaxFanAngle = 40.0f;
	UPROPERTY(EditAnywhere, Category = "Hand|Layout") FVector2D HandCenterOffset = FVector2D(0.0f, -100.0f);
	UPROPERTY(EditAnywhere, Category = "Hand|Selection") FVector2D SelectionScreenPos = FVector2D(600.0f, -550.0f);
	UPROPERTY(EditAnywhere, Category = "Hand|Selection") float BackgroundCardsYOffset = 500.0f;
	UPROPERTY(EditAnywhere, Category = "Hand|Selection") FVector2D DiscardTargetPos = FVector2D(800.0f, 200.0f);
	UPROPERTY(EditAnywhere, Category = "Hand|Hover") float HoverYOffset = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Hand|Hover") float HoverScale = 1.15f;
	UPROPERTY(EditAnywhere, Category = "Hand|Hover") float InterpSpeed = 15.0f;

	// [추가] 카드를 이만큼(화면 좌표) 위로 끌었다 놓으면 사용한다
	UPROPERTY(EditAnywhere, Category = "Hand|Drag") float DragUseDistance = 140.0f;

private:
	// [추가] 끌기 상태: 누른 카드 번호(-1이면 없음)와 시작 위치(로컬)
	int32 DragIndex = -1;
	FVector2D DragStartLocal = FVector2D::ZeroVector;
	FVector2D DragOffset = FVector2D::ZeroVector;
	bool bWasMouseDown = false;
	void UpdateDrag(const FGeometry& MyGeometry);

	// 숫자 키로 집은 카드 번호(-1이면 없음)와 그 카드가 손패에서 쉬던 자리의 가운데(로컬)
	int32 HeldIndex = -1;
	FVector2D HeldOffset = FVector2D::ZeroVector;
	double LastHeldReleaseTime = -10.0;
	void UpdateHeld(const FGeometry& MyGeometry);

	TArray<FVector2D> CurrentTranslations;
	TArray<float> CurrentRotations;
	TArray<float> CurrentScales;
};