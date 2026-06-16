#include "BeyondWing/UI/Public/BWHandWidget.h"
#include "BeyondWing/UI/Public/BWCardWidget.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/DeckComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Blueprint/SlateBlueprintLibrary.h"

void UBWHandWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// [강력한 조치] 태어나자마자 플레이어 컨트롤러를 찾아가서 자기를 강제로 등록함
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->SetHandWidget(this);
		UE_LOG(LogTemp, Warning, TEXT("[HandUI] Proactively registered to PlayerController: %s"), *GetName());
		
		// 만약 이미 캐릭터가 소환되어 있다면 즉시 연결
		if (PC->PossessedBWCharacter)
		{
			LinkToPlayerDeck(PC->PossessedBWCharacter);
		}
	}
}

void UBWHandWidget::LinkToPlayerDeck(ABWCharacter* TargetCharacter)
{
	if (TargetCharacter && TargetCharacter->DeckComponent)
	{
		// 중복 바인딩 방지
		TargetCharacter->DeckComponent->OnHandChanged.RemoveDynamic(this, &UBWHandWidget::HandleHandChanged);
		TargetCharacter->DeckComponent->OnHandChanged.AddDynamic(this, &UBWHandWidget::HandleHandChanged);
		
		RefreshHand(TargetCharacter->DeckComponent->GetCurrentHand());
		UE_LOG(LogTemp, Warning, TEXT("[BWHandWidget] Deck Linked via LinkToPlayerDeck."));
	}
}

void UBWHandWidget::HandleHandChanged(const TArray<FBWCardData>& CurrentHand)
{
	RefreshHand(CurrentHand);
}

void UBWHandWidget::RefreshHand(const TArray<FBWCardData>& InHandCards)
{
	UE_LOG(LogTemp, Warning, TEXT("[HandUI] RefreshHand called with %d cards."), InHandCards.Num());
	
	if (!HandCanvas)
	{
		UE_LOG(LogTemp, Error, TEXT("[HandUI] RefreshHand FAILED: HandCanvas is NULL!"));
		return;
	}
	if (!CardWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[HandUI] RefreshHand FAILED: CardWidgetClass is NOT ASSIGNED!"));
		return;
	}

	TArray<UWidget*> Children = HandCanvas->GetAllChildren();
	for (UWidget* Child : Children)
	{
		if (Child != DiscardGhost) Child->RemoveFromParent();
	}

	CardWidgets.Empty();
	CurrentTranslations.Empty();
	CurrentRotations.Empty();
	CurrentScales.Empty();
	HoveredCard = nullptr;

	for (int32 i = 0; i < InHandCards.Num(); ++i)
	{
		UBWCardWidget* NewCard = CreateWidget<UBWCardWidget>(this, CardWidgetClass);
		if (NewCard)
		{
			NewCard->CardData = InHandCards[i];
			NewCard->HandIndex = i;
			NewCard->OwningHand = this;
			
			UCanvasPanelSlot* CardSlot = HandCanvas->AddChildToCanvas(NewCard);
			if (CardSlot)
			{
				CardSlot->SetSize(CardSize);
				CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				CardSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			}
			
			NewCard->SetVisibility(ESlateVisibility::Visible);
			CardWidgets.Add(NewCard);
			CurrentTranslations.Add(FVector2D::ZeroVector);
			CurrentRotations.Add(0.0f);
			CurrentScales.Add(1.0f);
			NewCard->K2_InitCardUI();
		}
	}
	UpdateCardLayout();
}

void UBWHandWidget::PlayDiscardAnimation(int32 Index)
{
	if (CardWidgets.IsValidIndex(Index))
	{
		DiscardGhost = CardWidgets[Index];
		
		// 유령 카드는 이제 마우스 클릭을 받지 않도록 가시성만 변경하고 위치는 유지
		DiscardGhost->SetVisibility(ESlateVisibility::HitTestInvisible);
		
		GhostTranslation = CurrentTranslations[Index];
		GhostScale = CurrentScales[Index];
		DiscardAnimTimer = 1.0f;
		
		CardWidgets.RemoveAt(Index);
		if (CurrentTranslations.IsValidIndex(Index)) CurrentTranslations.RemoveAt(Index);
		if (CurrentRotations.IsValidIndex(Index)) CurrentRotations.RemoveAt(Index);
		if (CurrentScales.IsValidIndex(Index)) CurrentScales.RemoveAt(Index);

		UpdateCardLayout();
	}
}

void UBWHandWidget::UpdateCardLayout()
{
	int32 CardCount = CardWidgets.Num();
	if (CardCount == 0) return;

	float TotalAngle = (CardCount - 1) * AngleStep;
	float StartAngle = -TotalAngle / 2.0f;

	for (int32 i = 0; i < CardCount; ++i)
	{
		float CurrentAngle = StartAngle + (i * AngleStep);
		float RadAngle = FMath::DegreesToRadians(CurrentAngle - 90.0f);

		FVector2D Pos;
		Pos.X = FMath::Cos(RadAngle) * ArcRadius;
		Pos.Y = FMath::Sin(RadAngle) * ArcRadius + ArcRadius; 
		Pos += HandCenterOffset;

		if (UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(CardWidgets[i]->Slot))
		{
			CardSlot->SetPosition(Pos);
			CardSlot->SetZOrder(i);
		}
		CardWidgets[i]->TargetRotation = CurrentAngle;
		CardWidgets[i]->HandIndex = i;

		if (CurrentRotations.IsValidIndex(i) && CurrentRotations[i] == 0.0f)
		{
			CurrentRotations[i] = CurrentAngle;
		}
	}
}

void UBWHandWidget::OnCardHovered(UBWCardWidget* InCard)
{
	if (HoveredCard && HoveredCard != InCard) HoveredCard->SetHovered(false);
	HoveredCard = InCard;
}

void UBWHandWidget::OnCardUnhovered(UBWCardWidget* InCard)
{
	if (HoveredCard == InCard) HoveredCard = nullptr;
}

void UBWHandWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	float ActualSpeed = (InterpSpeed <= 0.0f) ? 10.0f : InterpSpeed;

	// [수동 히트 테스트: ACE 버그 완전 종결]
	// UMG의 기본 Z-Order 판정은 '그리는 순서'와 '판정 순서'가 묶여있어 유저님의 요구를 충족할 수 없습니다.
	// 따라서 C++이 직접 마우스 위치를 가져와 "오른쪽 카드부터(역순)" 마우스 충돌을 직접 검사합니다.
	// 이렇게 하면 A카드가 Z-Order 100으로 맨 위에 그려져도, 마우스가 B카드 영역에 닿는 순간 
	// 역순 루프에 의해 B카드가 무조건 우선권을 뺏어오게 됩니다. (시각과 판정의 완벽한 분리)
	
	APlayerController* PC = GetOwningPlayer();
	if (PC && SelectedCardIndex == -1)
	{
		// [좌표 오차 완벽 해결] IsUnderLocation은 모니터 기준의 '절대 좌표'를 사용합니다.
		// SlateApplication에서 직접 가져온 커서 위치는 에디터 창의 위치나 DPI 배율과 상관없이 
		// 항상 위젯의 실제 위치와 100% 일치합니다.
		FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();

		UBWCardWidget* NewHovered = nullptr;
		// 오른쪽(가장 나중에 그려지는) 카드부터 검사하여 판정 우선권을 부여
		for (int32 i = CardWidgets.Num() - 1; i >= 0; --i)
		{
			if (CardWidgets[i]->GetCachedGeometry().IsUnderLocation(AbsoluteMousePos))
			{
				NewHovered = CardWidgets[i];
				break; 
			}
		}

		if (NewHovered != HoveredCard)
		{
			if (HoveredCard) HoveredCard->SetHovered(false);
			if (NewHovered) NewHovered->SetHovered(true);
			// HoveredCard 변수는 SetHovered 내부의 콜백을 통해 자동으로 업데이트됩니다.
		}
	}

	int32 HoverIdx = HoveredCard ? HoveredCard->HandIndex : -1;

	for (int32 i = 0; i < CardWidgets.Num(); ++i)
	{
		UBWCardWidget* Card = CardWidgets[i];
		if (!Card || !Card->Slot) continue;

		UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(Card->Slot);
		FVector2D TargetOffset = FVector2D::ZeroVector;
		float TargetRot = Card->TargetRotation;
		float TargetScale = 1.0f;

		int32 BaseZOrder = i * 2; 

		if (i == SelectedCardIndex)
		{
			if (CardSlot) CardSlot->SetZOrder(500);
			FVector2D CurrentSlotPos = CardSlot ? CardSlot->GetPosition() : FVector2D::ZeroVector;
			TargetOffset = SelectionScreenPos - CurrentSlotPos;
			TargetRot = 0.0f;
			TargetScale = HoverScale;
		}
		else if (SelectedCardIndex != -1)
		{
			if (CardSlot) CardSlot->SetZOrder(BaseZOrder);
			TargetOffset.Y = BackgroundCardsYOffset;
		}
		else if (i == HoverIdx)
		{
			if (CardSlot) CardSlot->SetZOrder(100);
			TargetOffset.Y = -HoverYOffset;
			TargetRot = 0.0f;
			TargetScale = HoverScale; 
		}
		else
		{
			if (CardSlot) CardSlot->SetZOrder(BaseZOrder);
		}

		if (CurrentTranslations.IsValidIndex(i))
		{
			CurrentTranslations[i] = FMath::Vector2DInterpTo(CurrentTranslations[i], TargetOffset, InDeltaTime, ActualSpeed);
			CurrentRotations[i] = FMath::FInterpTo(CurrentRotations[i], TargetRot, InDeltaTime, ActualSpeed);
			
			if (!CurrentScales.IsValidIndex(i)) CurrentScales.Add(1.0f);
			CurrentScales[i] = FMath::FInterpTo(CurrentScales[i], TargetScale, InDeltaTime, ActualSpeed);

			// [깜빡임(Flicker) 해결: 판정부 각도 고정]
			// 히트박스(Root)는 부채꼴 모양 그대로 각도와 위치를 "징그럽게 고정"합니다.
			// 애니메이션(CurrentRotations)을 판정부에 적용하면 각도가 변할 때마다 마우스 판정이 미세하게 어긋나 깜빡임이 생깁니다.
			Card->SetRenderTransformAngle(Card->TargetRotation); 
			Card->SetRenderTranslation(FVector2D::ZeroVector);
			Card->SetRenderScale(FVector2D(1.0f, 1.0f));

			UWidget* VisualToScale = Card->Visual_Group;
			
			if (!VisualToScale)
			{
				UWidget* RootW = Card->GetRootWidget();
				if (UPanelWidget* Panel = Cast<UPanelWidget>(RootW)) 
				{
					if (Panel->GetChildrenCount() > 0) VisualToScale = Panel->GetChildAt(0);
				}
			}

			if (VisualToScale)
			{
				// 오직 비주얼만 팝업 및 확대
				VisualToScale->SetRenderScale(FVector2D(CurrentScales[i], CurrentScales[i]));
				VisualToScale->SetRenderTranslation(CurrentTranslations[i]);
				
				// 비주얼은 수평(0도)으로 돌아오려는 애니메이션을 적용하되, 
				// 부모(Root)가 이미 꺾여있으므로 상대적인 각도를 보정해줍니다.
				VisualToScale->SetRenderTransformAngle(CurrentRotations[i] - Card->TargetRotation);
			}
		}
	}

	if (DiscardGhost)
	{
		DiscardAnimTimer -= InDeltaTime;
		GhostTranslation = FMath::Vector2DInterpTo(GhostTranslation, DiscardTargetPos, InDeltaTime, 5.0f);

		DiscardGhost->SetRenderTranslation(GhostTranslation);

		if (DiscardAnimTimer <= 0.0f)
		{
			DiscardGhost->RemoveFromParent();
			DiscardGhost = nullptr;
		}
	}
}
