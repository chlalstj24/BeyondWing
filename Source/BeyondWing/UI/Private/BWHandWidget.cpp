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

// [추가] 카드 좌측 하단 사거리 그림: T_CardRange_0 = 나 자신(효과카드), 1~MaxReach = 상하좌우 칸 수. 더 긴 사거리는 MaxReach 그림
namespace BWRangeArt
{
	static const TCHAR* Folder = TEXT("/Game/HUD/Texture/CardRange");
	static constexpr int32 MaxReach = 6;
}

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
	CancelHeldCard(); // 손패가 바뀌면 집고 있던 카드는 내려놓는다
	DragIndex = -1;
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
			// [수정] 공격카드에는 기본 공격에 더해 주는 몫(카드 범위 × 삶의 방식 배율, 스탯 반영)을 "+a~b"로 보여 준다
			if (const ABWCharacter* OwnerChar = Cast<ABWCharacter>(GetOwningPlayerPawn()); OwnerChar && NewCard->CardData.CardKind == ECardKind::Attack)
			{
				const FBWDiceRange Shown = OwnerChar->GetCardBoostRange(NewCard->CardData);
				NewCard->CardData.SkillMin = Shown.Min;
				NewCard->CardData.SkillMax = Shown.Max;
				NewCard->bShowAsBoost = true;
			}
			// [추가] 좌측 하단 그림 = 사거리 칸 그림 (공격카드 = Range, 효과카드 = 나 자신). 표의 Range를 바꾸면 그림도 따라 바뀐다
			{
				const int32 Reach = NewCard->CardData.CardKind == ECardKind::Attack ? FMath::Clamp(NewCard->CardData.Range, 1, BWRangeArt::MaxReach) : 0;
				NewCard->CardData.AboveImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%s/T_CardRange_%d.T_CardRange_%d"), BWRangeArt::Folder, Reach, Reach)));
			}
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
			// 겔레온에서 고르지 않은 종류의 카드는 흐리게 (써지지는 않고 안내만 뜬다)
			if (const ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer()); PC && !UBWDimensionRules::CanUseCard(PC->Battlefield, NewCard->CardData))
			{
				// 카드 그림이 색 보정을 받지 않는 재질이라 어둡게 대신 반투명으로 표시한다
				NewCard->SetRenderOpacity(0.45f);
			}
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

	// [수정] 손패가 많아지면(최대 10장) 부채꼴 전체 각도를 MaxFanAngle 안으로 줄여 카드가 겹치게 한다
	const float Step = CardCount > 1 ? FMath::Min(AngleStep, MaxFanAngle / (CardCount - 1)) : AngleStep;
	float TotalAngle = (CardCount - 1) * Step;
	float StartAngle = -TotalAngle / 2.0f;

	for (int32 i = 0; i < CardCount; ++i)
	{
		float CurrentAngle = StartAngle + (i * Step);
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

void UBWHandWidget::UpdateDrag(const FGeometry& MyGeometry)
{
	const bool bMouseDown = FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
	const FVector2D MouseLocal = MyGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());

	// 누르는 순간 마우스 아래 카드를 잡는다 (숫자 키로 집은 카드가 있으면 그 클릭은 컨트롤러가 놓기로 처리)
	if (bMouseDown && !bWasMouseDown && HoveredCard && SelectedCardIndex == -1 && HeldIndex == -1)
	{
		DragIndex = HoveredCard->HandIndex;
		DragStartLocal = MouseLocal;
		DragOffset = FVector2D::ZeroVector;
	}

	if (DragIndex != -1)
	{
		if (bMouseDown)
		{
			// 조금이라도 움직였을 때만 끌기로 본다 (그냥 클릭은 카드 버튼이 처리)
			const FVector2D Delta = MouseLocal - DragStartLocal;
			DragOffset = Delta.SizeSquared() > 25.0f ? Delta : FVector2D::ZeroVector;
		}
		else
		{
			// 놓았을 때 충분히 위로 끌었으면 사용
			const float Lift = DragStartLocal.Y - MouseLocal.Y;
			const int32 UsedIndex = DragIndex;
			DragIndex = -1;
			DragOffset = FVector2D::ZeroVector;
			if (Lift >= DragUseDistance)
			{
				if (ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer())) PC->UseCardShortcut(UsedIndex);
			}
		}
	}
	bWasMouseDown = bMouseDown;
}

void UBWHandWidget::ToggleHoldCard(int32 Index)
{
	if (HeldIndex == Index)
	{
		CancelHeldCard();
		return;
	}
	if (!CardWidgets.IsValidIndex(Index)) return;
	DragIndex = -1;
	DragOffset = FVector2D::ZeroVector;
	HeldIndex = Index;
	HeldOffset = FVector2D::ZeroVector;
	if (HoveredCard) HoveredCard->SetHovered(false);
}

int32 UBWHandWidget::GetFocusedCardIndex() const
{
	if (HeldIndex != -1) return HeldIndex;
	if (DragIndex != -1) return DragIndex;
	return HoveredCard ? HoveredCard->HandIndex : -1;
}

bool UBWHandWidget::ShouldIgnoreCardClick() const
{
	return HeldIndex != -1 || FPlatformTime::Seconds() - LastHeldReleaseTime < 0.4;
}

void UBWHandWidget::CancelHeldCard()
{
	if (HeldIndex != -1) LastHeldReleaseTime = FPlatformTime::Seconds();
	HeldIndex = -1;
	HeldOffset = FVector2D::ZeroVector;
}

bool UBWHandWidget::ReleaseHeldCard()
{
	if (!CardWidgets.IsValidIndex(HeldIndex))
	{
		CancelHeldCard();
		return false;
	}
	// 카드가 쉬던 자리(회전된 루트 판정 영역) 위에서 누르면 제자리에 내려놓는다
	const int32 Index = HeldIndex;
	const bool bOverHome = CardWidgets[Index]->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
	CancelHeldCard();
	if (bOverHome) return false;
	if (ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer())) PC->UseCardShortcut(Index);
	return true;
}

void UBWHandWidget::UpdateHeld(const FGeometry& MyGeometry)
{
	if (!CardWidgets.IsValidIndex(HeldIndex))
	{
		CancelHeldCard();
		return;
	}
	// 카드 가운데가 마우스에 오도록: 쉬던 자리 가운데에서 마우스까지의 거리를 카드 회전만큼 되돌려 비주얼에 준다
	const UBWCardWidget* Card = CardWidgets[HeldIndex];
	const FGeometry& CardGeo = Card->GetCachedGeometry();
	const FVector2D HomeCenter = MyGeometry.AbsoluteToLocal(CardGeo.LocalToAbsolute(CardGeo.GetLocalSize() * 0.5f));
	const FVector2D MouseLocal = MyGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	HeldOffset = (MouseLocal - HomeCenter).GetRotated(-Card->TargetRotation);
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

	// [추가] 카드를 위로 끌어 놓으면 사용
	UpdateDrag(MyGeometry);
	if (HeldIndex != -1) UpdateHeld(MyGeometry);

	// [수동 히트 테스트: ACE 버그 완전 종결]
	// UMG의 기본 Z-Order 판정은 '그리는 순서'와 '판정 순서'가 묶여있어 유저님의 요구를 충족할 수 없습니다.
	// 따라서 C++이 직접 마우스 위치를 가져와 "오른쪽 카드부터(역순)" 마우스 충돌을 직접 검사합니다.
	// 이렇게 하면 A카드가 Z-Order 100으로 맨 위에 그려져도, 마우스가 B카드 영역에 닿는 순간 
	// 역순 루프에 의해 B카드가 무조건 우선권을 뺏어오게 됩니다. (시각과 판정의 완벽한 분리)
	
	APlayerController* PC = GetOwningPlayer();
	if (PC && SelectedCardIndex == -1 && HeldIndex == -1)
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

		if (i == HeldIndex)
		{
			// 숫자 키로 집은 카드는 마우스를 따라다닌다
			if (CardSlot) CardSlot->SetZOrder(600);
			TargetOffset = HeldOffset;
			TargetRot = 0.0f;
			TargetScale = HoverScale;
			if (CurrentTranslations.IsValidIndex(i)) CurrentTranslations[i] = HeldOffset;
		}
		else if (i == DragIndex && !DragOffset.IsNearlyZero())
		{
			// 끄는 중인 카드는 마우스를 그대로 따라간다
			if (CardSlot) CardSlot->SetZOrder(600);
			TargetOffset = DragOffset;
			TargetRot = 0.0f;
			TargetScale = HoverScale;
			if (CurrentTranslations.IsValidIndex(i)) CurrentTranslations[i] = DragOffset;
		}
		else if (i == SelectedCardIndex)
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
