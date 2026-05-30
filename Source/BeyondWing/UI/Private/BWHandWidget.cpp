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

void UBWHandWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// 플레이어 컨트롤러에 자신을 등록 (명령을 받기 위함)
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->SetHandWidget(this);
		UE_LOG(LogTemp, Log, TEXT("[BWHandWidget] Registered to PlayerController."));
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

void UBWHandWidget::HandleHandChanged(const TArray<FCardData>& CurrentHand)
{
	RefreshHand(CurrentHand);
}

void UBWHandWidget::RefreshHand(const TArray<FCardData>& InHandCards)
{
	if (!HandCanvas || !CardWidgetClass) return;

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
		
		// 1. 유령 카드는 이제 마우스 클릭을 받지 않도록 가시성 변경
		DiscardGhost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		
		// 2. [물리적 격리] 실제 히트박스(슬롯)를 화면 밖 멀리 보내버림 (비주얼은 남아있음)
		if (UCanvasPanelSlot* GhostSlot = Cast<UCanvasPanelSlot>(DiscardGhost->Slot))
		{
			GhostSlot->SetPosition(FVector2D(-5000.0f, -5000.0f));
		}
		
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

	for (int32 i = 0; i < CardWidgets.Num(); ++i)
	{
		UBWCardWidget* Card = CardWidgets[i];
		if (!Card || !Card->Slot) continue;

		UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(Card->Slot);
		FVector2D TargetOffset = FVector2D::ZeroVector;
		float TargetRot = Card->TargetRotation;
		float TargetScale = 1.0f;

		// [중요] Z-Order 가중치 시스템: 오른쪽 카드가 항상 판정 우위를 점하게 함 (ABC 이동 해결)
		int32 BaseZOrder = i * 2; 

		if (i == SelectedCardIndex)
		{
			if (CardSlot) CardSlot->SetZOrder(500); // 선택된 카드는 독보적 최상단
			TargetOffset = SelectionScreenPos;
			TargetRot = 0.0f;
			TargetScale = HoverScale;
		}
		else if (SelectedCardIndex != -1)
		{
			if (CardSlot) CardSlot->SetZOrder(BaseZOrder);
			TargetOffset.Y = BackgroundCardsYOffset;
		}
		else if (Card == HoveredCard)
		{
			// 호버된 카드는 살짝만 높여서, 오른쪽 카드(BaseZOrder + 2)보다는 낮게 유지
			if (CardSlot) CardSlot->SetZOrder(BaseZOrder + 1);
			TargetOffset.Y = -HoverYOffset;
			TargetRot = 0.0f;
			TargetScale = HoverScale;
		}
		else
		{
			bool bIsReturning = CurrentTranslations.IsValidIndex(i) && CurrentTranslations[i].Size() > 20.0f;
			if (CardSlot) CardSlot->SetZOrder(bIsReturning ? -1 : BaseZOrder);
		}

		if (CurrentTranslations.IsValidIndex(i))
		{
			CurrentTranslations[i] = FMath::Vector2DInterpTo(CurrentTranslations[i], TargetOffset, InDeltaTime, ActualSpeed);
			CurrentRotations[i] = FMath::FInterpTo(CurrentRotations[i], TargetRot, InDeltaTime, ActualSpeed);
			CurrentScales[i] = FMath::FInterpTo(CurrentScales[i], TargetScale, InDeltaTime, ActualSpeed);

			// 중요: 회전은 전체 카드(히트박스 포함)에 적용
			Card->SetRenderTransformAngle(CurrentRotations[i]);

			// [핵심] 위치 이동과 크기 조절은 오직 Visual_Group에만 적용
			// 이렇게 하면 카드가 뾱 올라가도 히트박스(버튼)는 마우스 커서 위치에 그대로 남습니다.
			if (Card->Visual_Group)
			{
				Card->Visual_Group->SetRenderTranslation(CurrentTranslations[i]);
				Card->Visual_Group->SetRenderScale(FVector2D(CurrentScales[i], CurrentScales[i]));
				
				// 루트 위젯의 변환값은 초기화 (히트박스 고정)
				Card->SetRenderTranslation(FVector2D::ZeroVector);
				Card->SetRenderScale(FVector2D(1.0f, 1.0f));
			}
			else
			{
				// fallback: Visual_Group이 없으면 어쩔 수 없이 전체 이동 (꿈틀거림 발생 가능)
				Card->SetRenderTranslation(CurrentTranslations[i]);
				Card->SetRenderScale(FVector2D(CurrentScales[i], CurrentScales[i]));
			}
		}
	}

	if (DiscardGhost)
	{
		DiscardAnimTimer -= InDeltaTime;
		GhostTranslation = FMath::Vector2DInterpTo(GhostTranslation, DiscardTargetPos, InDeltaTime, 5.0f);
		GhostScale = FMath::FInterpTo(GhostScale, 0.0f, InDeltaTime, 5.0f);

		if (DiscardGhost->Visual_Group)
		{
			DiscardGhost->Visual_Group->SetRenderTranslation(GhostTranslation);
			DiscardGhost->Visual_Group->SetRenderScale(FVector2D(GhostScale, GhostScale));
		}
		else
		{
			DiscardGhost->SetRenderTranslation(GhostTranslation);
			DiscardGhost->SetRenderScale(FVector2D(GhostScale, GhostScale));
		}

		if (DiscardAnimTimer <= 0.0f)
		{
			DiscardGhost->RemoveFromParent();
			DiscardGhost = nullptr;
		}
	}
}
