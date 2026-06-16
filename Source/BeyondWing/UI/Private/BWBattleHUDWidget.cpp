#include "BeyondWing/UI/Public/BWBattleHUDWidget.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/DeckComponent.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/UI/Public/BWTooltipWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"

UBWBattleHUDWidget::UBWBattleHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AllyZone_Tooltip.bEnableTooltip = true;
	AllyZone_Tooltip.Title = TEXT("선택 영역");
	AllyZone_Tooltip.Description = TEXT("클릭하여 배치");

	MoveRange_Tooltip.bEnableTooltip = true;
	MoveRange_Tooltip.Title = TEXT("이동 가능");
	MoveRange_Tooltip.Description = TEXT("클릭 시 해당 위치로 이동합니다.\n잔여 이동 거리: {V}");
}

void UBWBattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// 블루프린트 기본값이 비어있을 경우 강제로 채워넣음 (빈 상자 출력 방지)
	if (AllyZone_Tooltip.Title.IsEmpty())
	{
		AllyZone_Tooltip.bEnableTooltip = true;
		AllyZone_Tooltip.Title = TEXT("선택 영역");
		AllyZone_Tooltip.Description = TEXT("클릭하여 배치");
	}

	if (MoveRange_Tooltip.Title.IsEmpty())
	{
		MoveRange_Tooltip.bEnableTooltip = true;
		MoveRange_Tooltip.Title = TEXT("이동 가능");
		MoveRange_Tooltip.Description = TEXT("클릭 시 해당 위치로 이동합니다.\n잔여 이동 거리: {V}");
	}

	K2_UpdateAP(3, 3);
	
	if (BTN_BasicAttack)
	{
		BTN_BasicAttack->OnClicked.AddDynamic(this, &UBWBattleHUDWidget::OnBasicAttackClicked);
	}

	if (BTN_EndTurn)
	{
		BTN_EndTurn->OnClicked.AddDynamic(this, &UBWBattleHUDWidget::OnEndTurnClicked);
	}

	if (WBP_ToolTipHUD)
	{
		WBP_ToolTipHUD->SetVisibility(ESlateVisibility::Hidden);
	}

	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->SetBattleHUDWidget(this);
		if (PC->PossessedBWCharacter) InitializeBattleHUD(PC->PossessedBWCharacter);
	}
}

void UBWBattleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (!PC) return;

	// --- 0. 플레이어 캐릭터 탐색 ---
	if (!CachedPlayerChar)
	{
		TArray<AActor*> FoundPlayerActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundPlayerActors);
		for (AActor* Actor : FoundPlayerActors)
		{
			ABWCharacter* Char = Cast<ABWCharacter>(Actor);
			if (Char && !Char->bIsEnemy) { InitializeBattleHUD(Char); break; }
		}
	}

	// 기본 공격 버튼 활성화 상태
	if (CachedPlayerChar && CachedPlayerChar->DeckComponent && BTN_BasicAttack)
	{
		BTN_BasicAttack->SetIsEnabled(CachedPlayerChar->DeckComponent->bCanFreeAttack);
	}

	// --- 1. HUD 요소 호버 감지 및 툴팁 표시 ---
	bool bAnyHUDHovered = false;
	
	// [수정] 가장 안전한 좌표 변환 방식 사용 (UE 5.5 빌드 에러 해결)
	FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();
	FVector2D LocalMousePos = MyGeometry.AbsoluteToLocal(AbsoluteMousePos);
	
	// 1) AP 정보 (유저 WBP의 AP_Group 우선 체크, 없으면 AP_Text)
	if (AP_Tooltip.bEnableTooltip)
	{
		UWidget* APTarget = AP_Group ? Cast<UWidget>(AP_Group) : Cast<UWidget>(AP_Text);
		if (APTarget && APTarget->GetCachedGeometry().IsUnderLocation(AbsoluteMousePos))
		{
			int32 CurAP = CachedPlayerChar ? CachedPlayerChar->DeckComponent->CurrentAP : 0;
			int32 MaxAP = CachedPlayerChar ? CachedPlayerChar->DeckComponent->MaxAP : 0;
			ShowTooltip(AP_Tooltip.Title, AP_Tooltip.Description, CurAP, 0, MaxAP);
			bAnyHUDHovered = true;
		}
	}
	// 2) 스탯 정보
	else if (Stat_Tooltip.bEnableTooltip && TXT_PlayerStats && TXT_PlayerStats->GetCachedGeometry().IsUnderLocation(AbsoluteMousePos))
	{
		ShowTooltip(Stat_Tooltip.Title, Stat_Tooltip.Description);
		bAnyHUDHovered = true;
	}
	// 3) 카드 장수
	else if (CardCount_Tooltip.bEnableTooltip && TXT_CardCount && TXT_CardCount->GetCachedGeometry().IsUnderLocation(AbsoluteMousePos))
	{
		int32 DeckSize = CachedPlayerChar ? CachedPlayerChar->DeckComponent->Deck.Num() : 0;
		ShowTooltip(CardCount_Tooltip.Title, CardCount_Tooltip.Description, DeckSize);
		bAnyHUDHovered = true;
	}
	// 4) 골드
	else if (Gold_Tooltip.bEnableTooltip && TXT_Gold && TXT_Gold->GetCachedGeometry().IsUnderLocation(AbsoluteMousePos))
	{
		int32 Gold = GI ? GI->CurrentGold : 0;
		ShowTooltip(Gold_Tooltip.Title, Gold_Tooltip.Description, Gold);
		bAnyHUDHovered = true;
	}
	// 5) 층수
	else if (Floor_Tooltip.bEnableTooltip && TXT_Floor && TXT_Floor->GetCachedGeometry().IsUnderLocation(AbsoluteMousePos))
	{
		int32 Floor = GI ? GI->CurrentStep + 1 : 1;
		ShowTooltip(Floor_Tooltip.Title, Floor_Tooltip.Description, Floor);
		bAnyHUDHovered = true;
	}

	// 호버가 끝나면 즉시 숨김
	if (!bAnyHUDHovered) 
	{
		if (PC && PC->GetTileUnderCursor() == nullptr)
		{
			HideTooltip();
		}
	}

	// [수정] 툴팁이 켜져 있다면 (HUD든 그리드든) 마우스를 따라다니게 함
	if (WBP_ToolTipHUD && WBP_ToolTipHUD->GetVisibility() != ESlateVisibility::Hidden)
	{
		// [좌표 및 정렬 보정]
		UCanvasPanelSlot* TooltipSlot = Cast<UCanvasPanelSlot>(WBP_ToolTipHUD->Slot);
		if (TooltipSlot)
		{
			TooltipSlot->SetPosition(LocalMousePos);
			
			// 기본 정렬: 마우스 "왼쪽 아래" (Alignment X=1.1)
			FVector2D TargetAlignment(1.1f, -0.1f);

			// 화면 왼쪽 끝이면 "오른쪽 아래"로 반전
			if (LocalMousePos.X < 400.0f)
			{
				TargetAlignment.X = -0.1f;
			}

			TooltipSlot->SetAlignment(TargetAlignment);
		}
	}

	// --- 정보 갱신 (HP, 층수 등) ---
	if (CachedPlayerChar)
	{
		if (PB_PlayerHP) PB_PlayerHP->SetPercent(FMath::Clamp((float)CachedPlayerChar->CurrentHP / (float)CachedPlayerChar->MaxHP, 0.0f, 1.0f));
		if (TXT_PlayerHP) TXT_PlayerHP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CachedPlayerChar->CurrentHP, CachedPlayerChar->MaxHP)));

		if (CachedPlayerChar->DeckComponent && TXT_CardCount)
			TXT_CardCount->SetText(FText::FromString(FString::Printf(TEXT("Cards: %d"), CachedPlayerChar->DeckComponent->Deck.Num())));

		if (TXT_PlayerStats)
		{
			FString StatString = FString::Printf(TEXT("PHA: %d   INT: %d   FTH: %d   TEC: %d   ATK: %d   ASP: %d"), 
				CachedPlayerChar->PHA, CachedPlayerChar->INT, CachedPlayerChar->FTH, CachedPlayerChar->TEC,
				CachedPlayerChar->GetEffectiveATK(), CachedPlayerChar->GetEffectiveASP());
			TXT_PlayerStats->SetText(FText::FromString(StatString));
		}
	}
	else if (GI)
	{
		if (PB_PlayerHP) PB_PlayerHP->SetPercent((float)GI->PlayerCurrentHP / (float)GI->PlayerMaxHP);
		if (TXT_PlayerHP) TXT_PlayerHP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), GI->PlayerCurrentHP, GI->PlayerMaxHP)));
		if (TXT_CardCount) TXT_CardCount->SetText(FText::FromString(FString::Printf(TEXT("Cards: %d"), GI->PlayerDeckCount)));
		if (TXT_PlayerStats)
		{
			FString StatString = FString::Printf(TEXT("PHA: %d   INT: %d   FTH: %d   TEC: %d   ATK: %d   ASP: %d"), 
				GI->PlayerPHA, GI->PlayerINT, GI->PlayerFTH, GI->PlayerTEC,
				(GI->PlayerPHA + GI->PlayerTEC), (1 + (GI->PlayerPHA / 20)));
			TXT_PlayerStats->SetText(FText::FromString(StatString));
		}
	}

	if (GI)
	{
		if (TXT_Floor) TXT_Floor->SetText(FText::FromString(FString::Printf(TEXT("%dF"), GI->CurrentStep + 1)));
		if (TXT_Gold) TXT_Gold->SetText(FText::FromString(FString::Printf(TEXT("%d G"), GI->CurrentGold)));
	}

	// Slot 1
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABWCharacter::StaticClass(), FoundActors);
	TArray<ABWCharacter*> Enemies;
	for (AActor* Actor : FoundActors)
	{
		ABWCharacter* Char = Cast<ABWCharacter>(Actor);
		if (Char && Char->bIsEnemy && Char->CurrentHP > 0 && !Char->IsActorBeingDestroyed()) Enemies.Add(Char);
	}
	auto GetEnemyName = [](ABWCharacter* C) -> FString { return C->EnemyDisplayName.IsEmpty() ? C->GetName() : C->EnemyDisplayName; };
	bool bHasEnemy1 = Enemies.IsValidIndex(0);
	ESlateVisibility Vis1 = bHasEnemy1 ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	if (PB_Enemy1) { PB_Enemy1->SetVisibility(Vis1); if (bHasEnemy1) PB_Enemy1->SetPercent((float)Enemies[0]->CurrentHP / (float)Enemies[0]->MaxHP); }
	if (TXT_Enemy1Name) { TXT_Enemy1Name->SetVisibility(Vis1); if (bHasEnemy1) TXT_Enemy1Name->SetText(FText::FromString(GetEnemyName(Enemies[0]))); }
	if (TXT_Enemy1HP) { TXT_Enemy1HP->SetVisibility(Vis1); if (bHasEnemy1) TXT_Enemy1HP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Enemies[0]->CurrentHP, Enemies[0]->MaxHP))); }
	if (IMG_Enemy1Portrait) IMG_Enemy1Portrait->SetVisibility(Vis1);
	bool bHasEnemy2 = Enemies.IsValidIndex(1);
	ESlateVisibility Vis2 = bHasEnemy2 ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	if (PB_Enemy2) { PB_Enemy2->SetVisibility(Vis2); if (bHasEnemy2) PB_Enemy2->SetPercent((float)Enemies[1]->CurrentHP / (float)Enemies[1]->MaxHP); }
	if (TXT_Enemy2Name) { TXT_Enemy2Name->SetVisibility(Vis2); if (bHasEnemy2) TXT_Enemy2Name->SetText(FText::FromString(GetEnemyName(Enemies[1]))); }
	if (TXT_Enemy2HP) { TXT_Enemy2HP->SetVisibility(Vis2); if (bHasEnemy2) TXT_Enemy2HP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Enemies[1]->CurrentHP, Enemies[1]->MaxHP))); }
	if (IMG_Enemy2Portrait) IMG_Enemy2Portrait->SetVisibility(Vis2);
	bool bHasEnemy3 = Enemies.IsValidIndex(2);
	ESlateVisibility Vis3 = bHasEnemy3 ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	if (PB_Enemy3) { PB_Enemy3->SetVisibility(Vis3); if (bHasEnemy3) PB_Enemy3->SetPercent((float)Enemies[2]->CurrentHP / (float)Enemies[2]->MaxHP); }
	if (TXT_Enemy3Name) { TXT_Enemy3Name->SetVisibility(Vis3); if (bHasEnemy3) TXT_Enemy3Name->SetText(FText::FromString(GetEnemyName(Enemies[2]))); }
	if (TXT_Enemy3HP) { TXT_Enemy3HP->SetVisibility(Vis3); if (bHasEnemy3) TXT_Enemy3HP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Enemies[2]->CurrentHP, Enemies[2]->MaxHP))); }
	if (IMG_Enemy3Portrait) IMG_Enemy3Portrait->SetVisibility(Vis3);
}

void UBWBattleHUDWidget::ShowTooltip(const FString& Title, const FString& RawDesc, int32 V, int32 D, int32 R)
{
	if (WBP_ToolTipHUD)
	{
		WBP_ToolTipHUD->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WBP_ToolTipHUD->SetTooltipTitle(Title);
		WBP_ToolTipHUD->SetTooltipDescription(RawDesc, V, D, R);
	}
}

void UBWBattleHUDWidget::HideTooltip()
{
	if (WBP_ToolTipHUD) WBP_ToolTipHUD->SetVisibility(ESlateVisibility::Hidden);
}

void UBWBattleHUDWidget::OnBasicAttackClicked()
{
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC) PC->EnterBasicAttackMode();
}

void UBWBattleHUDWidget::OnEndTurnClicked()
{
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC) PC->RequestEndTurn();
}

void UBWBattleHUDWidget::HandleAPChanged(int32 CurrentAP, int32 MaxAP) { K2_UpdateAP(CurrentAP, MaxAP); }
UDeckComponent* UBWBattleHUDWidget::GetPlayerDeckComponent() { return CachedPlayerChar ? CachedPlayerChar->DeckComponent : nullptr; }
void UBWBattleHUDWidget::InitializeBattleHUD(ABWCharacter* InCharacter)
{
	CachedPlayerChar = InCharacter;
	if (InCharacter && InCharacter->DeckComponent)
	{
		InCharacter->DeckComponent->OnAPChanged.RemoveDynamic(this, &UBWBattleHUDWidget::HandleAPChanged);
		InCharacter->DeckComponent->OnAPChanged.AddDynamic(this, &UBWBattleHUDWidget::HandleAPChanged);
		K2_UpdateAP(InCharacter->DeckComponent->CurrentAP, InCharacter->DeckComponent->MaxAP);
	}
}
void UBWBattleHUDWidget::OnTargetHovered(ABWCharacter* TargetChar) { CurrentHoveredEnemy = TargetChar; }
