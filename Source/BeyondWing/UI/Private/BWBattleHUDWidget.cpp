#include "BeyondWing/UI/Public/BWBattleHUDWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/DeckComponent.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/Core/Public/BWMapRules.h"
#include "BeyondWing/Combat/Public/BWClassData.h"
#include "BeyondWing/Combat/Public/BWLifestyleRules.h"
#include "BeyondWing/Combat/Public/BWStatusRules.h"
#include "BeyondWing/UI/Public/BWTooltipWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "BeyondWing/UI/Public/BWHandWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "BeyondWing/UI/Public/BWSkin.h"

namespace BWHudUI
{
	FSlateBrush RoundBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float Width = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		return Brush;
	}
	FLinearColor Scaled(const FLinearColor& C, float K, float A) { return FLinearColor(C.R * K, C.G * K, C.B * K, A); }
	const FLinearColor Gold(1.0f, 0.8f, 0.35f);
	const FLinearColor TextDim(0.7f, 0.72f, 0.8f);
	const FLinearColor PlateFill(0.02f, 0.022f, 0.04f, 0.86f);
	FLinearColor DimColor(EDimensionType Dim)
	{
		switch (Dim)
		{
		case EDimensionType::Patrovas: return FLinearColor(0.62f, 0.4f, 1.0f);
		case EDimensionType::Berend:   return FLinearColor(1.0f, 0.55f, 0.26f);
		case EDimensionType::Geleon:   return FLinearColor(0.66f, 0.74f, 0.8f);
		case EDimensionType::Taiga:    return FLinearColor(0.3f, 0.48f, 0.95f);
		default:                       return FLinearColor(0.8f, 0.8f, 0.8f);
		}
	}
	const TCHAR* StatAbbrev(EClassType Class)
	{
		switch (Class)
		{
		case EClassType::PHA: return TEXT("PHA");
		case EClassType::TEC: return TEXT("TEC");
		case EClassType::INT: return TEXT("INT");
		case EClassType::FTH: return TEXT("FTH");
		case EClassType::HP:  return TEXT("HP");
		default:              return TEXT("-");
		}
	}
}

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

	// [추가] 임시 그림 정리: 예전 AP 구슬과 노란 구슬은 숨기고, 텍스처가 없는 이미지(흰 네모)도 숨긴다
	for (const TCHAR* Name : { TEXT("AP_Group"), TEXT("MP_Image"), TEXT("TXT_Gold_1") }) // TXT_Gold_1 = 아이콘 없는 덱 장수 (왼쪽 아래 DeckText가 대신함)
	{
		if (UWidget* Old = GetWidgetFromName(Name)) Old->SetVisibility(ESlateVisibility::Collapsed);
	}
	for (const TCHAR* Name : { TEXT("PlayerPortrait"), TEXT("Image_Floor"), TEXT("Image_Gold"), TEXT("Image_CardCount") })
	{
		UImage* Img = Cast<UImage>(GetWidgetFromName(Name));
		if (Img && !Img->GetBrush().GetResourceObject()) Img->SetVisibility(ESlateVisibility::Collapsed);
	}
	// [추가] HP 줄에는 "방어도" 같은 한글이 붙는데 디자인 글꼴엔 한글이 없어서, 크기만 두고 기본(한글 지원) 글꼴로 바꾼다
	if (TXT_PlayerHP)
	{
		FSlateFontInfo Font = GetDefault<UTextBlock>()->GetFont();
		Font.Size = TXT_PlayerHP->GetFont().Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
		TXT_PlayerHP->SetFont(Font);
	}
	// [추가] 코드 HUD (HP 바도 새로 만들어 PB_PlayerHP를 바꿔 끼우므로 방패 배지보다 먼저)
	if (bUseCodeLayout)
	{
		if (UCanvasPanel* RootCanvas = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr) BuildCodeLayout(RootCanvas);
	}
	BuildBlockBadge();
	if (MP_Text)
	{
		FSlateFontInfo Font = MP_Text->GetFont();
		Font.Size = 26;
		Font.OutlineSettings.OutlineSize = 2;
		Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
		MP_Text->SetFont(Font);
		MP_Text->SetColorAndOpacity(FLinearColor(0.55f, 1.0f, 0.7f));
	}

	// [추가] 덱·버린 덱 장수 (왼쪽 아래)
	if (!DeckText && WidgetTree && !bUseCodeLayout)
	{
		if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			DeckText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckText"));
			FSlateFontInfo Font = DeckText->GetFont();
			Font.Size = 18;
			Font.OutlineSettings.OutlineSize = 1;
			Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
			DeckText->SetFont(Font);
			DeckText->SetColorAndOpacity(FLinearColor(0.9f, 0.88f, 0.8f));
			DeckText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* DeckSlot = RootCanvas->AddChildToCanvas(DeckText))
			{
				DeckSlot->SetAnchors(FAnchors(0.0f, 1.0f));
				DeckSlot->SetAlignment(FVector2D(0.0f, 1.0f));
				DeckSlot->SetPosition(FVector2D(28.0f, -28.0f));
				DeckSlot->SetAutoSize(true);
			}
		}
	}

	// [수정] 안내판과 기본 공격 칸: 디자인에 없으면 루트 캔버스에 만든다
	if (UCanvasPanel* RootCanvas = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr)
	{
		if (!PromptText) BuildPromptBanner(RootCanvas);
		if (!TXT_AttackValue) BuildAttackPanel(RootCanvas);
	}
	// 기본 공격 버튼에 단축키 표시
	if (BTN_BasicAttack)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(BTN_BasicAttack->GetContent())) Label->SetText(FText::FromString(TEXT("기본 공격 (Q)")));
	}
	ShowPrompt(TEXT("파란 칸을 눌러 말을 배치하세요"), 0.0f, EBWPromptStyle::Highlight);

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

	// [수정] 남은 이동 칸 / 이번 턴 굴린 속도 (블루프린트 바인딩 대신 C++에서 직접 갱신)
	if (MP_Text)
	{
		MP_Text->SetText(CachedPlayerChar
			? FText::FromString(FString::Printf(TEXT("이동 %d/%d"), CachedPlayerChar->RemainingMove, CachedPlayerChar->RolledSpeed))
			: FText::FromString(TEXT("-")));
	}
	UpdateCodeLayout();
	if (DeckText)
	{
		const UDeckComponent* Deck = CachedPlayerChar ? CachedPlayerChar->DeckComponent : nullptr;
		// [수정] 이번 전투의 국가 기믹도 여기에 (스탯 칸 글꼴에는 한글이 없어 네모로 나왔다)
		FString Field = DescribeBattlefield();
		if (const ABWPlayerController* OwnerPC = Cast<ABWPlayerController>(GetOwningPlayer()))
		{
			const FString DimName = UBWMapRules::GetDimensionName(OwnerPC->Battlefield.Dimension).ToString();
			if (!Field.IsEmpty()) Field = DimName + TEXT(" · ") + Field;
		}
		const FString DeckLine = Deck ? FString::Printf(TEXT("손패 %d/%d  ·  덱 %d  ·  버린 카드 %d"), Deck->Hand.Num(), Deck->MaxHandSize, Deck->Deck.Num(), Deck->Discard.Num()) : FString();
		DeckText->SetText(FText::FromString(Field.IsEmpty() ? DeckLine : Field + TEXT("\n") + DeckLine));
	}

	// 안내판 연출과 만료 (만료되면 흐려지며 사라진다)
	if (PromptText && PromptExpireTime > 0.0f && GetWorld() && GetWorld()->GetTimeSeconds() > PromptExpireTime)
	{
		PromptExpireTime = -1.0f;
		PromptFadeOut = 0.0f;
	}
	UpdatePromptBanner(InDeltaTime);
	UpdateAttackPanel();
	UpdateManaPanel(InDeltaTime);

	// [수정] 기본 공격 버튼: 합 횟수가 남았고, 기절하지 않았고, 맞닿은 적이 있을 때 (손패와 상관없음)
	if (BTN_BasicAttack)
	{
		BTN_BasicAttack->SetIsEnabled(PC->CanUseBasicStrike());
	}

	// --- 1. HUD 요소 호버 감지 및 툴팁 표시 ---
	bool bAnyHUDHovered = false;
	
	// [수정] 가장 안전한 좌표 변환 방식 사용 (UE 5.5 빌드 에러 해결)
	FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();
	FVector2D LocalMousePos = MyGeometry.AbsoluteToLocal(AbsoluteMousePos);
	
	// 1) 스탯 정보
	if (Stat_Tooltip.bEnableTooltip && TXT_PlayerStats && TXT_PlayerStats->GetCachedGeometry().IsUnderLocation(AbsoluteMousePos))
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

	UpdateEnemyCards();

	// 6) 세트
	if (!bAnyHUDHovered)
	{
		for (const FSetRow& Row : SetRows)
		{
			if (!Row.Widget.IsValid() || !Row.Widget->GetCachedGeometry().IsUnderLocation(AbsoluteMousePos)) continue;
			ShowTooltip(Row.Title, Row.Desc);
			bAnyHUDHovered = true;
			break;
		}
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
		UpdateBlockDisplay(CachedPlayerChar->Block, InDeltaTime);
		if (TXT_PlayerHP)
		{
			// [수정] 방어도와 (매직실드 직업이면) MP를 HP 옆에 표시
			FString HPString = FString::Printf(TEXT("%d / %d"), CachedPlayerChar->CurrentHP, CachedPlayerChar->MaxHP);
			// 방어도·상태이상은 말 머리 위에, MP는 왼쪽 아래 '마나' 칸에 (여기에 붙이면 위쪽 안내판과 겹친다)
			TXT_PlayerHP->SetText(FText::FromString(HPString));
		}

		if (CachedPlayerChar->DeckComponent && TXT_CardCount)
			TXT_CardCount->SetText(FText::FromString(FString::Printf(TEXT("Cards: %d"), CachedPlayerChar->DeckComponent->Deck.Num())));

		if (TXT_PlayerStats)
		{
			// [수정] ATK = 지금 기본 공격 숫자 범위 (이 글꼴엔 한글과 ~가 없어 "3-7"로 표기)
			const FBWDiceRange Basic = CachedPlayerChar->GetBasicAttackRange();
			FString StatString = FString::Printf(TEXT("PHA: %d   INT: %d   FTH: %d   TEC: %d   ATK: %d-%d"),
				CachedPlayerChar->PHA, CachedPlayerChar->INT, CachedPlayerChar->FTH, CachedPlayerChar->TEC, Basic.Min, Basic.Max);
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
	// [수정] 적의 이번 턴 속도는 내가 움직이기 전에 공개된다 (전투 기획서 3장)
	auto GetEnemyName = [](ABWCharacter* C) -> FString
	{
		FString Name = C->EnemyDisplayName.IsEmpty() ? C->GetName() : C->EnemyDisplayName;
		if (C->RolledSpeed > 0) Name += FString::Printf(TEXT("  · 속도 %d"), C->RolledSpeed);
		if (C->Block > 0) Name += FString::Printf(TEXT("  · 방어도 %d"), C->Block);
		if (C->Statuses.Num() > 0) Name += FString::Printf(TEXT("  · [%s]"), *UBWStatusRules::DescribeStatuses(C->Statuses));
		return Name;
	};
	bool bHasEnemy1 = bShowEnemyList && Enemies.IsValidIndex(0);
	ESlateVisibility Vis1 = bHasEnemy1 ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	if (PB_Enemy1) { PB_Enemy1->SetVisibility(Vis1); if (bHasEnemy1) PB_Enemy1->SetPercent((float)Enemies[0]->CurrentHP / (float)Enemies[0]->MaxHP); }
	if (TXT_Enemy1Name) { TXT_Enemy1Name->SetVisibility(Vis1); if (bHasEnemy1) TXT_Enemy1Name->SetText(FText::FromString(GetEnemyName(Enemies[0]))); }
	if (TXT_Enemy1HP) { TXT_Enemy1HP->SetVisibility(Vis1); if (bHasEnemy1) TXT_Enemy1HP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Enemies[0]->CurrentHP, Enemies[0]->MaxHP))); }
	if (IMG_Enemy1Portrait) IMG_Enemy1Portrait->SetVisibility(Vis1);
	bool bHasEnemy2 = bShowEnemyList && Enemies.IsValidIndex(1);
	ESlateVisibility Vis2 = bHasEnemy2 ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	if (PB_Enemy2) { PB_Enemy2->SetVisibility(Vis2); if (bHasEnemy2) PB_Enemy2->SetPercent((float)Enemies[1]->CurrentHP / (float)Enemies[1]->MaxHP); }
	if (TXT_Enemy2Name) { TXT_Enemy2Name->SetVisibility(Vis2); if (bHasEnemy2) TXT_Enemy2Name->SetText(FText::FromString(GetEnemyName(Enemies[1]))); }
	if (TXT_Enemy2HP) { TXT_Enemy2HP->SetVisibility(Vis2); if (bHasEnemy2) TXT_Enemy2HP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Enemies[1]->CurrentHP, Enemies[1]->MaxHP))); }
	if (IMG_Enemy2Portrait) IMG_Enemy2Portrait->SetVisibility(Vis2);
	bool bHasEnemy3 = bShowEnemyList && Enemies.IsValidIndex(2);
	ESlateVisibility Vis3 = bHasEnemy3 ? ESlateVisibility::Visible : ESlateVisibility::Hidden;
	if (PB_Enemy3) { PB_Enemy3->SetVisibility(Vis3); if (bHasEnemy3) PB_Enemy3->SetPercent((float)Enemies[2]->CurrentHP / (float)Enemies[2]->MaxHP); }
	if (TXT_Enemy3Name) { TXT_Enemy3Name->SetVisibility(Vis3); if (bHasEnemy3) TXT_Enemy3Name->SetText(FText::FromString(GetEnemyName(Enemies[2]))); }
	if (TXT_Enemy3HP) { TXT_Enemy3HP->SetVisibility(Vis3); if (bHasEnemy3) TXT_Enemy3HP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Enemies[2]->CurrentHP, Enemies[2]->MaxHP))); }
	if (IMG_Enemy3Portrait) IMG_Enemy3Portrait->SetVisibility(Vis3);
}

FString UBWBattleHUDWidget::DescribeBattlefield() const
{
	const ABWPlayerController* OwnerPC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (!OwnerPC) return FString();
	const FBWBattlefieldState& BF = OwnerPC->Battlefield;
	FString Field;
	switch (BF.Dimension)
	{
	case EDimensionType::Taiga:    Field = BF.TaigaChargesLeft > 0 ? TEXT("은신 중: 다음 공격 치명타 + 방어 무시") : TEXT("맞닿은 적 없이 턴을 끝내면 은신"); break;
	case EDimensionType::Berend:   Field = TEXT("격돌에서 지면 밀려난다. 벽에 부딪히면 피해"); break;
	case EDimensionType::Patrovas: Field = TEXT("보랏빛 마력선 위의 말은 최댓값 ×1.4"); break;
	case EDimensionType::Geleon:   Field = TEXT("활공: 이번 턴 움직인 칸 1당 피해 +15%"); break;
	default: break;
	}
	if (!Field.IsEmpty() && BF.bOverloaded) Field += TEXT(" (과부하)");
	return Field;
}

UTextBlock* UBWBattleHUDWidget::MakeHUDText(int32 Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = GetDefault<UTextBlock>()->GetFont(); // 기본 글꼴 = 한글 지원
	Font.Size = Size;
	if (bBold) Font.TypefaceFontName = TEXT("Bold");
	Font.OutlineSettings.OutlineSize = Size >= 22 ? 1 : 0;
	Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.85f);
	Text->SetFont(Font);
	Text->SetColorAndOpacity(Color);
	Text->SetShadowOffset(FVector2D(0.0f, 1.5f));
	Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f));
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

void UBWBattleHUDWidget::StyleButton(UButton* Button, const FLinearColor& Accent, int32 FontSize, const FVector2D& Size, const FVector2D& Position)
{
	if (!Button) return;
	using namespace BWHudUI;
	FButtonStyle Style = Button->GetStyle();
	const bool bRed = Accent.R > 0.8f && Accent.G < 0.6f;
	// 이동·기본 공격·턴 종료: 모서리 장식 없이 석판색 + 은빛 테두리 (사용자 요청 2026-11-03)
	{
		const FLinearColor Fill = bRed ? FLinearColor(0.36f, 0.13f, 0.12f, 1.0f) : FLinearColor(0.17f, 0.21f, 0.25f, 1.0f);
		const FLinearColor Edge(0.68f, 0.7f, 0.74f, 0.85f), EdgeLit(0.92f, 0.94f, 0.97f, 1.0f);
		Style.SetNormal(BWSkin::Plain(Fill, Edge, 1.5f));
		Style.SetHovered(BWSkin::Plain(Fill * 1.35f, EdgeLit, 2.0f));
		Style.SetPressed(BWSkin::Plain(Fill * 0.75f, EdgeLit, 2.0f));
		Style.SetDisabled(BWSkin::Plain(FLinearColor(0.1f, 0.11f, 0.13f, 0.9f), FLinearColor(1, 1, 1, 0.12f), 1.0f));
	}
	Style.SetNormalPadding(FMargin(12.0f, 4.0f));
	Style.SetPressedPadding(FMargin(12.0f, 6.0f, 12.0f, 2.0f));
	Button->SetStyle(Style);
	Button->SetBackgroundColor(FLinearColor::White);
	Button->SetColorAndOpacity(FLinearColor::White);
	if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
	{
		FSlateFontInfo Font = GetDefault<UTextBlock>()->GetFont();
		Font.Size = FontSize;
		Font.TypefaceFontName = TEXT("Bold");
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FLinearColor::White);
		Label->SetJustification(ETextJustify::Center);
		Label->SetShadowOffset(FVector2D(0.0f, 1.5f));
		Label->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f));
	}
	if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(Button->Slot))
	{
		ButtonSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		ButtonSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		ButtonSlot->SetAutoSize(false);
		ButtonSlot->SetPosition(Position);
		ButtonSlot->SetSize(Size);
	}
}

UWidget* UBWBattleHUDWidget::MakePile(const FString& Label, bool bRight, UTextBlock*& OutCount)
{
	// 덱·버린 카드 더미: 동그란 판에 큰 숫자 + 이름 (슬레이 더 스파이어처럼 양쪽 아래 구석)
	using namespace BWHudUI;
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(104.0f);
	Size->SetHeightOverride(104.0f);
	UBorder* Disc = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FSlateBrush RoundSkin;
	if (BWSkin::MakeImage(TEXT("slot_round"), FVector2D(104.0f, 104.0f), RoundSkin)) Disc->SetBrush(RoundSkin);
	else Disc->SetBrush(RoundBrush(FLinearColor(0.04f, 0.045f, 0.08f, 0.94f), 52.0f, bRight ? FLinearColor(0.6f, 0.62f, 0.72f, 0.7f) : Scaled(Gold, 1.0f, 0.75f), 2.5f));
	Disc->SetHorizontalAlignment(HAlign_Center);
	Disc->SetVerticalAlignment(VAlign_Center);
	Disc->SetVisibility(ESlateVisibility::HitTestInvisible);
	Size->SetContent(Disc);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Disc->SetContent(Column);
	OutCount = MakeHUDText(32, FLinearColor::White, true);
	OutCount->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(OutCount)) S->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Name = MakeHUDText(12, bRight ? TextDim : Gold);
	Name->SetText(FText::FromString(Label));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Name)) S->SetHorizontalAlignment(HAlign_Center);
	return Size;
}

void UBWBattleHUDWidget::BuildCodeLayout(UCanvasPanel* RootCanvas)
{
	BuildEnemyCards(RootCanvas);
	using namespace BWHudUI;
	// 옛 디자이너 그림(임시 초상화·디자인 글꼴 숫자)은 숨기고, 같은 정보는 아래 판들이 보여 준다
	for (const TCHAR* Name : { TEXT("PlayerPortrait"), TEXT("TXT_Floor"), TEXT("TXT_HP"), TEXT("TXT_Gold"), TEXT("TXT_PlayerStats"), TEXT("TXT_PlayerHP"), TEXT("PB_PlayerHP"), TEXT("TXT_CardCount") })
	{
		if (UWidget* Old = GetWidgetFromName(Name)) Old->SetVisibility(ESlateVisibility::Collapsed);
	}
	TXT_Floor = nullptr;
	TXT_Gold = nullptr;
	TXT_PlayerStats = nullptr;
	TXT_CardCount = nullptr;

	// --- 왼쪽 위: 삶의 방식 판 ---
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlayerPlate"));
	FSlateBrush PlateSkin;
	if (BWSkin::Panel(PlateSkin)) Plate->SetBrush(PlateSkin);
	else Plate->SetBrush(RoundBrush(PlateFill, 14.0f, FLinearColor(1, 1, 1, 0.1f), 1.0f));
	Plate->SetPadding(FMargin(32.0f, 24.0f, 32.0f, 32.0f));
	Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Plate->SetContent(Column);

	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Head);
	USizeBox* ChipSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ChipSize->SetWidthOverride(46.0f);
	ChipSize->SetHeightOverride(46.0f);
	ClassChip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ClassChip->SetBrush(RoundBrush(FLinearColor(0.5f, 0.5f, 0.5f), 10.0f, FLinearColor(1, 1, 1, 0.35f), 1.5f));
	ClassChip->SetHorizontalAlignment(HAlign_Center);
	ClassChip->SetVerticalAlignment(VAlign_Center);
	TXT_ClassChip = MakeHUDText(15, FLinearColor::White, true);
	ClassChip->SetContent(TXT_ClassChip);
	ChipSize->SetContent(ClassChip);
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(ChipSize)) S->SetVerticalAlignment(VAlign_Center);
	UVerticalBox* NameColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(NameColumn)) { S->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f)); S->SetVerticalAlignment(VAlign_Center); }
	TXT_ClassName = MakeHUDText(22, FLinearColor::White, true);
	NameColumn->AddChildToVerticalBox(TXT_ClassName);
	TXT_RunInfo = MakeHUDText(14, TextDim);
	NameColumn->AddChildToVerticalBox(TXT_RunInfo);

	// HP 바 (왼쪽 끝에 방어도 방패가 걸리도록 조금 들여서)
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarSize->SetWidthOverride(380.0f);
	BarSize->SetHeightOverride(28.0f);
	UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HPOverlay"));
	BarSize->SetContent(BarOverlay);
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_PlayerHP_Code"));
	FProgressBarStyle BarStyle = Bar->GetWidgetStyle();
	BarStyle.SetBackgroundImage(RoundBrush(FLinearColor(0.1f, 0.025f, 0.03f, 1.0f), 8.0f, FLinearColor(0, 0, 0, 0.8f), 1.5f));
	BarStyle.SetFillImage(RoundBrush(FLinearColor::White, 8.0f));
	Bar->SetWidgetStyle(BarStyle);
	Bar->SetFillColorAndOpacity(FLinearColor(0.82f, 0.13f, 0.12f));
	Bar->SetPercent(1.0f);
	if (UOverlaySlot* S = BarOverlay->AddChildToOverlay(Bar)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	UTextBlock* HPText = MakeHUDText(17, FLinearColor::White, true);
	{
		FSlateFontInfo Font = HPText->GetFont();
		Font.OutlineSettings.OutlineSize = 1;
		HPText->SetFont(Font);
	}
	if (UOverlaySlot* S = BarOverlay->AddChildToOverlay(HPText)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(BarSize)) S->SetPadding(FMargin(24.0f, 12.0f, 0.0f, 0.0f));
	PB_PlayerHP = Bar;
	TXT_PlayerHP = HPText;

	TXT_LifeLine = MakeHUDText(15, FLinearColor(0.9f, 0.86f, 1.0f));
	TXT_LifeLine->SetWrapTextAt(400.0f);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TXT_LifeLine)) S->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	ResourceBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResourceBox"));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(ResourceBox)) S->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	TXT_StatLine = MakeHUDText(13, TextDim);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TXT_StatLine)) S->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(Plate))
	{
		S->SetAnchors(FAnchors(0.0f, 0.0f));
		S->SetPosition(FVector2D(20.0f, 16.0f));
		S->SetAutoSize(true);
		S->SetZOrder(20);
	}

	// --- 오른쪽 위: 골드 + 이번 전투의 차원 ---
	UVerticalBox* RightTop = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightTop"));
	RightTop->SetVisibility(ESlateVisibility::HitTestInvisible);
	UBorder* GoldPill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	// 작은 칸은 장식 없이 석판 색 + 은빛 테두리만 (큰 판의 모서리 삼각형을 쓰지 않는다)
	GoldPill->SetBrush(RoundBrush(FLinearColor(0.16f, 0.19f, 0.23f, 0.96f), 8.0f, FLinearColor(0.72f, 0.74f, 0.78f, 0.9f), 2.0f));
	GoldPill->SetPadding(FMargin(16.0f, 7.0f, 22.0f, 7.0f));
	GoldPill->SetHorizontalAlignment(HAlign_Right);
	UHorizontalBox* GoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	GoldPill->SetContent(GoldRow);
	USizeBox* CoinSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CoinSize->SetWidthOverride(22.0f);
	CoinSize->SetHeightOverride(22.0f);
	UBorder* Coin = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Coin->SetBrush(RoundBrush(FLinearColor(1.0f, 0.78f, 0.25f), 11.0f, FLinearColor(0.55f, 0.35f, 0.05f), 2.0f));
	CoinSize->SetContent(Coin);
	if (UHorizontalBoxSlot* S = GoldRow->AddChildToHorizontalBox(CoinSize)) S->SetVerticalAlignment(VAlign_Center);
	TXT_GoldValue = MakeHUDText(22, FLinearColor(1.0f, 0.88f, 0.5f), true);
	if (UHorizontalBoxSlot* S = GoldRow->AddChildToHorizontalBox(TXT_GoldValue)) { S->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f)); S->SetVerticalAlignment(VAlign_Center); }
	// 골드는 네 자리(9999)까지 들어가게 폭을 잡아 둔다
	USizeBox* GoldSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	GoldSize->SetMinDesiredWidth(170.0f);
	GoldSize->SetContent(GoldPill);
	if (UVerticalBoxSlot* S = RightTop->AddChildToVerticalBox(GoldSize)) S->SetHorizontalAlignment(HAlign_Right);

	DimPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FSlateBrush DimSkin;
	if (BWSkin::Panel(DimSkin)) DimPlate->SetBrush(DimSkin);
	else DimPlate->SetBrush(RoundBrush(PlateFill, 12.0f, FLinearColor(1, 1, 1, 0.2f), 1.5f));
	DimPlate->SetPadding(FMargin(28.0f, 22.0f, 30.0f, 32.0f));
	UVerticalBox* DimColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DimPlate->SetContent(DimColumn);
	UHorizontalBox* DimHead = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DimColumn->AddChildToVerticalBox(DimHead);
	USizeBox* DotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DotSize->SetWidthOverride(12.0f);
	DotSize->SetHeightOverride(12.0f);
	DimDot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DimDot->SetBrush(RoundBrush(FLinearColor::White, 6.0f));
	DotSize->SetContent(DimDot);
	if (UHorizontalBoxSlot* S = DimHead->AddChildToHorizontalBox(DotSize)) S->SetVerticalAlignment(VAlign_Center);
	TXT_DimName = MakeHUDText(18, FLinearColor::White, true);
	if (UHorizontalBoxSlot* S = DimHead->AddChildToHorizontalBox(TXT_DimName)) { S->SetPadding(FMargin(9.0f, 0.0f, 0.0f, 0.0f)); S->SetVerticalAlignment(VAlign_Center); }
	TXT_DimDetail = MakeHUDText(14, TextDim);
	TXT_DimDetail->SetWrapTextAt(330.0f);
	SetBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SetBox"));
	if (UVerticalBoxSlot* S = DimColumn->AddChildToVerticalBox(TXT_DimDetail)) S->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
	if (UVerticalBoxSlot* S = DimColumn->AddChildToVerticalBox(SetBox)) S->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	if (UVerticalBoxSlot* S = RightTop->AddChildToVerticalBox(DimPlate)) { S->SetHorizontalAlignment(HAlign_Right); S->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f)); }

	if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(RightTop))
	{
		S->SetAnchors(FAnchors(1.0f, 0.0f));
		S->SetAlignment(FVector2D(1.0f, 0.0f));
		S->SetPosition(FVector2D(-20.0f, 16.0f));
		S->SetAutoSize(true);
		S->SetZOrder(20);
	}

	// --- 아래 구석: 덱 더미(왼쪽) · 버린 카드 더미(오른쪽) ---
	UHorizontalBox* LeftBottom = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DrawPileRow"));
	LeftBottom->SetVisibility(ESlateVisibility::HitTestInvisible);
	UTextBlock* DrawCount = nullptr;
	LeftBottom->AddChildToHorizontalBox(MakePile(TEXT("덱"), false, DrawCount));
	TXT_DrawCount = DrawCount;
	TXT_HandCount = MakeHUDText(15, TextDim);
	if (UHorizontalBoxSlot* S = LeftBottom->AddChildToHorizontalBox(TXT_HandCount)) { S->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 8.0f)); S->SetVerticalAlignment(VAlign_Bottom); }
	if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(LeftBottom))
	{
		S->SetAnchors(FAnchors(0.0f, 1.0f));
		S->SetAlignment(FVector2D(0.0f, 1.0f));
		S->SetPosition(FVector2D(24.0f, -24.0f));
		S->SetAutoSize(true);
	}
	UTextBlock* DiscardCount = nullptr;
	UWidget* DiscardPile = MakePile(TEXT("버린 카드"), true, DiscardCount);
	TXT_DiscardCount = DiscardCount;
	if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(DiscardPile))
	{
		S->SetAnchors(FAnchors(1.0f, 1.0f));
		S->SetAlignment(FVector2D(1.0f, 1.0f));
		S->SetPosition(FVector2D(-24.0f, -24.0f));
		S->SetAutoSize(true);
	}

	// --- 오른쪽 버튼 줄 (버린 카드 더미 위): 이동 칸 수 · 이동 · 기본 공격 · 턴 종료 ---
	StyleButton(BTN_EndTurn, Gold, 22, FVector2D(220.0f, 62.0f), FVector2D(-24.0f, -136.0f));
	StyleButton(BTN_BasicAttack, FLinearColor(1.0f, 0.42f, 0.34f), 18, FVector2D(220.0f, 50.0f), FVector2D(-24.0f, -210.0f));
	StyleButton(Cast<UButton>(GetWidgetFromName(TEXT("Move_Button"))), FLinearColor(0.35f, 0.85f, 0.95f), 18, FVector2D(220.0f, 50.0f), FVector2D(-24.0f, -272.0f));
	if (UWidget* MoveGroup = GetWidgetFromName(TEXT("MP_Group")))
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(MoveGroup->Slot))
		{
			S->SetAnchors(FAnchors(1.0f, 1.0f));
			S->SetAlignment(FVector2D(1.0f, 1.0f));
			S->SetAutoSize(true);
			S->SetPosition(FVector2D(-30.0f, -332.0f));
		}
	}
}

void UBWBattleHUDWidget::UpdateCodeLayout()
{
	if (!bUseCodeLayout || !TXT_ClassName) return;
	using namespace BWHudUI;
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	const ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());

	// 삶의 방식 이름·색 (DT_Classes)
	const EClassType Class = CachedPlayerChar ? CachedPlayerChar->CharacterClass : GI ? GI->SelectedClass : EClassType::Common;
	const FBWClassData* Info = GI ? GI->FindClassData(Class) : nullptr;
	const FLinearColor ClassColor = Info ? Info->Color : FLinearColor(0.5f, 0.5f, 0.5f);
	TXT_ClassName->SetText(Info && !Info->DisplayName.IsEmpty() ? Info->DisplayName : FText::FromString(StatAbbrev(Class)));
	if (ClassChip) ClassChip->SetBrush(RoundBrush(ClassColor, 10.0f, FLinearColor(1, 1, 1, 0.35f), 1.5f));
	if (TXT_ClassChip)
	{
		TXT_ClassChip->SetText(FText::FromString(StatAbbrev(Class)));
		TXT_ClassChip->SetColorAndOpacity(Info && Info->bDarkText ? FLinearColor(0.08f, 0.08f, 0.1f) : FLinearColor::White);
	}

	// 막 · 칸 · 차원
	const EDimensionType Dim = PC ? PC->Battlefield.Dimension : EDimensionType::None;
	if (TXT_RunInfo && GI)
	{
		const FString DimName = UBWMapRules::GetDimensionName(Dim).ToString();
		TXT_RunInfo->SetText(FText::FromString(DimName.IsEmpty() ? FString::Printf(TEXT("%d층 · %d번째 칸"), GI->CurrentStage, GI->CurrentStep + 1)
			: FString::Printf(TEXT("%d층 · %d번째 칸 · %s"), GI->CurrentStage, GI->CurrentStep + 1, *DimName)));
	}

	// 고유 자원 (광기·장전·해방·어둠 …)과 상태이상. 없으면 이 삶의 방식의 자원 소개
	// [수정] 전투 중에는 고유 자원 판이 대신한다
	UpdateSetPanel();
	if (CachedPlayerChar && ResourceBox)
	{
		if (TXT_LifeLine) TXT_LifeLine->SetVisibility(ESlateVisibility::Collapsed);
		UpdateResourcePanel(ClassColor);
	}
	else if (TXT_LifeLine)
	{
		FString Line = CachedPlayerChar ? CachedPlayerChar->GetLifestyleStatusText() : FString();
		if (CachedPlayerChar && CachedPlayerChar->Statuses.Num() > 0)
		{
			const FString Status = UBWStatusRules::DescribeStatuses(CachedPlayerChar->Statuses);
			Line = Line.IsEmpty() ? Status : Line + TEXT("  ·  ") + Status;
		}
		const bool bIdle = Line.IsEmpty();
		if (bIdle && Info) Line = Info->ResourceText.ToString();
		TXT_LifeLine->SetText(FText::FromString(Line));
		TXT_LifeLine->SetColorAndOpacity(bIdle ? FLinearColor(0.75f, 0.74f, 0.82f, 0.8f) : FLinearColor(ClassColor * 0.35f + FLinearColor(0.65f, 0.65f, 0.65f, 0.65f)));
		TXT_LifeLine->SetVisibility(Line.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (TXT_StatLine && CachedPlayerChar)
	{
		TXT_StatLine->SetText(FText::FromString(FString::Printf(TEXT("PHA %d · TEC %d · INT %d · FTH %d · %s"),
			CachedPlayerChar->PHA, CachedPlayerChar->TEC, CachedPlayerChar->INT, CachedPlayerChar->FTH,
			Cast<ABWPlayerController>(GetOwningPlayer()) ? *Cast<ABWPlayerController>(GetOwningPlayer())->DescribePlayerDefense(CachedPlayerChar) : TEXT(""))));
	}

	if (TXT_GoldValue && GI) TXT_GoldValue->SetText(FText::AsNumber(GI->CurrentGold));

	if (DimPlate)
	{
		const bool bHasDim = Dim != EDimensionType::None;
		// 차원이 없어도(탑 내부) 증강·세트가 있으면 판을 보여 주고 차원 줄만 숨긴다
		const bool bHasAugments = GI && GI->OwnedAugments.Num() > 0;
		DimPlate->SetVisibility(bHasDim || bHasAugments ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const ESlateVisibility DimLines = bHasDim ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (TXT_DimName && TXT_DimName->GetParent()) TXT_DimName->GetParent()->SetVisibility(DimLines);
		if (TXT_DimDetail) TXT_DimDetail->SetVisibility(DimLines);
		if (!bHasDim)
		{
			FSlateBrush PlainSkin;
			if (BWSkin::Panel(PlainSkin)) DimPlate->SetBrush(PlainSkin);
		}
		if (bHasDim)
		{
			const FLinearColor DC = DimColor(Dim);
			const bool bOver = PC && PC->Battlefield.bOverloaded;
			FSlateBrush DimSkin;
			if (BWSkin::Panel(DimSkin)) { DimSkin.TintColor = FSlateColor(bOver ? FLinearColor(1.0f, 0.7f, 0.68f) : FLinearColor::White); DimPlate->SetBrush(DimSkin); }
			else DimPlate->SetBrush(RoundBrush(PlateFill, 12.0f, bOver ? FLinearColor(1.0f, 0.3f, 0.25f, 0.9f) : Scaled(DC, 1.0f, 0.7f), 1.5f));
			if (DimDot) DimDot->SetBrushColor(DC);
			if (TXT_DimName) TXT_DimName->SetText(UBWMapRules::GetDimensionName(Dim));
			if (TXT_DimDetail) TXT_DimDetail->SetText(FText::FromString(DescribeBattlefield()));
		}
	}

	const UDeckComponent* Deck = CachedPlayerChar ? CachedPlayerChar->DeckComponent : nullptr;
	if (TXT_DrawCount) TXT_DrawCount->SetText(FText::AsNumber(Deck ? Deck->Deck.Num() : 0));
	if (TXT_DiscardCount) TXT_DiscardCount->SetText(FText::AsNumber(Deck ? Deck->Discard.Num() : 0));
	if (TXT_HandCount) TXT_HandCount->SetText(FText::FromString(Deck ? FString::Printf(TEXT("손패 %d / %d"), Deck->Hand.Num(), Deck->MaxHandSize) : FString()));
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

namespace BWPrompt
{
	struct FLook { FLinearColor Accent; FLinearColor Text; };
	static FLook GetLook(EBWPromptStyle Style)
	{
		switch (Style)
		{
		case EBWPromptStyle::Highlight: return { FLinearColor(0.35f, 0.95f, 0.9f), FLinearColor(0.88f, 1.0f, 1.0f) };
		case EBWPromptStyle::Warning:   return { FLinearColor(1.0f, 0.32f, 0.28f), FLinearColor(1.0f, 0.84f, 0.8f) };
		case EBWPromptStyle::Result:    return { FLinearColor(0.82f, 0.82f, 0.92f), FLinearColor::White };
		default:                        return { FLinearColor(0.95f, 0.78f, 0.42f), FLinearColor(1.0f, 0.97f, 0.9f) };
		}
	}
}

void UBWBattleHUDWidget::ShowPrompt(const FString& Message, float Duration, EBWPromptStyle Style)
{
	if (!PromptText) return;
	PromptText->SetText(FText::FromString(Message));
	PromptExpireTime = (Duration > 0.0f && GetWorld()) ? GetWorld()->GetTimeSeconds() + Duration : -1.0f;

	// 새 문구마다 다시 나타나는 연출 (같은 문구가 계속 오면 흔들림만 다시)
	PromptStyle = Style;
	PromptAge = 0.0f;
	PromptFadeOut = -1.0f;
	const BWPrompt::FLook Look = BWPrompt::GetLook(Style);
	PromptText->SetColorAndOpacity(Look.Text);
	if (PromptFrame && !bPromptSkinned) PromptFrame->SetBrushColor(Look.Accent * FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
	if (PromptIcon) PromptIcon->SetColorAndOpacity(Look.Accent);
	if (PromptPanel) PromptPanel->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UBWBattleHUDWidget::BuildPromptBanner(UCanvasPanel* RootCanvas)
{
	// 안내판: 위아래 색 테두리(바깥 Border) 안에 어두운 판(안쪽 Border), 왼쪽에 마름모 표시와 글
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptFrame"));
	Frame->SetPadding(FMargin(0.0f, 2.0f));
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptPlate"));
	Plate->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.03f, 0.82f));
	Plate->SetPadding(FMargin(26.0f, 9.0f, 30.0f, 10.0f));
	FSlateBrush BannerSkin;
	if (BWSkin::Banner(BannerSkin))
	{
		// 석판 띠가 테두리를 맡으므로 바깥 색 줄은 숨긴다
		Frame->SetPadding(FMargin(0.0f));
		Frame->SetBrushColor(FLinearColor::Transparent);
		bPromptSkinned = true;
		Plate->SetBrush(BannerSkin);
		Plate->SetBrushColor(FLinearColor::White);
		Plate->SetPadding(FMargin(40.0f, 14.0f, 44.0f, 15.0f));
	}
	Frame->SetContent(Plate);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(10.0f);
	IconBox->SetHeightOverride(10.0f);
	PromptIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	PromptIcon->SetRenderTransformAngle(45.0f);
	IconBox->SetContent(PromptIcon);
	if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconBox))
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
	}
	PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
	FSlateFontInfo Font = PromptText->GetFont();
	Font.Size = 20;
	PromptText->SetFont(Font);
	PromptText->SetJustification(ETextJustify::Center);
	PromptText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	PromptText->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
	// 위쪽 줄의 HP 숫자와 골드 사이에 들어가도록 PromptMaxWidth에서 줄을 바꾼다 (짧은 글은 짧은 판)
	PromptText->SetAutoWrapText(false);
	PromptText->SetWrapTextAt(PromptMaxWidth);
	if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(PromptText)) TextSlot->SetVerticalAlignment(VAlign_Center);
	Plate->SetContent(Row);

	Frame->SetVisibility(ESlateVisibility::Collapsed);
	Frame->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (UCanvasPanelSlot* PromptSlot = RootCanvas->AddChildToCanvas(Frame))
	{
		PromptSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		PromptSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		PromptSlot->SetPosition(FVector2D(0.0f, 12.0f)); // 위쪽 줄 가운데 (HP와 골드 사이, 적 머리 위 표시와 겹치지 않게)
		PromptSlot->SetAutoSize(true);
		PromptSlot->SetZOrder(50);
	}
	PromptPanel = Frame;
	PromptFrame = Frame;
}

void UBWBattleHUDWidget::UpdatePromptBanner(float DeltaTime)
{
	if (!PromptPanel || PromptPanel->GetVisibility() == ESlateVisibility::Collapsed) return;
	PromptAge += DeltaTime;

	// 나타남: 0.18초 동안 위에서 내려오며 밝아진다
	const float In = FMath::Clamp(PromptAge / 0.18f, 0.0f, 1.0f);
	const float InEase = 1.0f - FMath::Pow(1.0f - In, 3.0f);
	float Opacity = InEase;
	FVector2D Offset(0.0f, (1.0f - InEase) * -14.0f);
	float Scale = 1.0f;

	if (PromptStyle == EBWPromptStyle::Warning && PromptAge < 0.4f)
	{
		// 할 수 없는 일: 좌우로 짧게 흔들린다
		Offset.X = FMath::Sin(PromptAge * 70.0f) * 9.0f * (1.0f - PromptAge / 0.4f);
	}
	else if (PromptStyle == EBWPromptStyle::Highlight)
	{
		// 눈여겨볼 일: 살짝 커졌다 돌아오고, 테두리가 은은하게 숨 쉰다
		Scale = 1.0f + 0.08f * FMath::Max(0.0f, 1.0f - PromptAge / 0.25f);
		if (PromptFrame)
		{
			const FLinearColor Accent = BWPrompt::GetLook(PromptStyle).Accent;
			if (!bPromptSkinned) PromptFrame->SetBrushColor(Accent * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f + 0.35f * (0.5f + 0.5f * FMath::Sin(PromptAge * 4.0f))));
		}
	}

	// 사라짐: 0.25초 동안 흐려진다
	if (PromptFadeOut >= 0.0f)
	{
		PromptFadeOut += DeltaTime;
		Opacity *= FMath::Clamp(1.0f - PromptFadeOut / 0.25f, 0.0f, 1.0f);
		if (PromptFadeOut >= 0.25f)
		{
			PromptFadeOut = -1.0f;
			if (PromptText) PromptText->SetText(FText::GetEmpty());
			PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
	}
	PromptPanel->SetRenderOpacity(Opacity);
	PromptPanel->SetRenderTranslation(Offset);
	PromptPanel->SetRenderScale(FVector2D(Scale));
}

void UBWBattleHUDWidget::BuildAttackPanel(UCanvasPanel* RootCanvas)
{
	// 왼쪽 아래(덱 장수 위) '기본 공격' 칸: 왼쪽 금빛 막대 + 제목 / 숫자 범위 / 설명
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AttackPanel"));
	Plate->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.03f, 0.72f));
	Plate->SetPadding(FMargin(0.0f));
	FSlateBrush AttackSkin;
	if (BWSkin::Tooltip(AttackSkin)) { Plate->SetBrush(AttackSkin); Plate->SetBrushColor(FLinearColor::White); Plate->SetPadding(FMargin(6.0f, 4.0f)); }
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Plate->SetContent(Row);

	USizeBox* AccentBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	AccentBox->SetWidthOverride(4.0f);
	AttackAccent = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	AttackAccent->SetColorAndOpacity(FLinearColor(0.95f, 0.78f, 0.42f));
	AccentBox->SetContent(AttackAccent);
	if (UHorizontalBoxSlot* AccentSlot = Row->AddChildToHorizontalBox(AccentBox)) AccentSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* ColumnSlot = Row->AddChildToHorizontalBox(Column)) ColumnSlot->SetPadding(FMargin(16.0f, 8.0f, 22.0f, 10.0f));

	auto MakeText = [this, Column](int32 Size, const FLinearColor& Color, const TCHAR* Name) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = Size >= 24 ? 1 : 0;
		Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f));
		Column->AddChildToVerticalBox(Text);
		return Text;
	};
	UTextBlock* Title = MakeText(14, FLinearColor(0.95f, 0.78f, 0.42f), TEXT("TXT_AttackTitle"));
	Title->SetText(FText::FromString(TEXT("기본 공격")));
	TXT_AttackValue = MakeText(30, FLinearColor::White, TEXT("TXT_AttackValue"));
	TXT_AttackDetail = MakeText(13, FLinearColor(0.8f, 0.78f, 0.85f), TEXT("TXT_AttackDetail"));

	Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	AttackPanel = Plate;

	// [추가] 기본 공격 칸 위에 '마나' 칸 (MP를 쓰는 삶의 방식만 보인다)
	UBorder* ManaPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ManaPanel"));
	ManaPlate->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.045f, 0.75f));
	ManaPlate->SetPadding(FMargin(0.0f));
	FSlateBrush ManaSkin;
	if (BWSkin::Tooltip(ManaSkin)) { ManaPlate->SetBrush(ManaSkin); ManaPlate->SetBrushColor(FLinearColor::White); ManaPlate->SetPadding(FMargin(6.0f, 4.0f)); }
	UHorizontalBox* ManaRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ManaPlate->SetContent(ManaRow);
	USizeBox* ManaAccentBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ManaAccentBox->SetWidthOverride(4.0f);
	UImage* ManaAccent = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	ManaAccent->SetColorAndOpacity(FLinearColor(0.4f, 0.7f, 1.0f));
	ManaAccentBox->SetContent(ManaAccent);
	if (UHorizontalBoxSlot* AccentSlot = ManaRow->AddChildToHorizontalBox(ManaAccentBox)) AccentSlot->SetVerticalAlignment(VAlign_Fill);
	UVerticalBox* ManaColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UHorizontalBoxSlot* ColumnSlot = ManaRow->AddChildToHorizontalBox(ManaColumn)) ColumnSlot->SetPadding(FMargin(16.0f, 8.0f, 22.0f, 10.0f));
	{
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ManaColumn->AddChildToVerticalBox(TitleRow);
		auto MakeManaText = [this](int32 Size, const FLinearColor& Color) -> UTextBlock*
		{
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			FSlateFontInfo Font = Text->GetFont();
			Font.Size = Size;
			Font.OutlineSettings.OutlineSize = Size >= 24 ? 1 : 0;
			Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
			Text->SetFont(Font);
			Text->SetColorAndOpacity(Color);
			Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
			Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f));
			return Text;
		};
		TXT_ManaTitle = MakeManaText(14, FLinearColor(0.5f, 0.75f, 1.0f));
		if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(TXT_ManaTitle)) S->SetVerticalAlignment(VAlign_Bottom);
		TXT_ManaValue = MakeManaText(26, FLinearColor::White);
		if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(TXT_ManaValue)) { S->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f)); S->SetVerticalAlignment(VAlign_Bottom); }

		ManaCellRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* S = ManaColumn->AddChildToVerticalBox(ManaCellRow)) S->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 4.0f));
		TXT_ManaDetail = MakeManaText(13, FLinearColor(0.72f, 0.78f, 0.92f));
		ManaColumn->AddChildToVerticalBox(TXT_ManaDetail);
	}
	ManaPlate->SetVisibility(ESlateVisibility::Collapsed);
	ManaPanel = ManaPlate;

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftInfoStack"));
	if (UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(ManaPlate)) S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	Stack->AddChildToVerticalBox(Plate);
	Stack->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Stack))
	{
		PanelSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		PanelSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		PanelSlot->SetPosition(bUseCodeLayout ? FVector2D(24.0f, -140.0f) : FVector2D(28.0f, -86.0f)); // 덱 더미(덱 장수 줄) 위
		PanelSlot->SetAutoSize(true);
	}
}

void UBWBattleHUDWidget::UpdateManaPanel(float DeltaTime)
{
	if (!ManaPanel || !TXT_ManaValue) return;
	const bool bUsesMP = CachedPlayerChar && CachedPlayerChar->ClassProfile.bUsesMP;
	ManaPanel->SetVisibility(bUsesMP ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bUsesMP) return;

	const int32 MP = CachedPlayerChar->CurrentMP;
	const int32 MaxMP = FMath::Max(1, CachedPlayerChar->MaxMP);
	if (ShownMP >= 0 && MP != ShownMP) ManaFlash = 0.6f;
	ShownMP = MP;
	ManaFlash = FMath::Max(0.0f, ManaFlash - DeltaTime);

	// 집거나 올린 카드에 MP 비용이 있으면 쓰고 남을 MP를 칸 색으로 보여 준다
	int32 Cost = 0;
	const ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	const UBWHandWidget* Hand = PC ? PC->GetHandWidget() : nullptr;
	const UDeckComponent* Deck = CachedPlayerChar->DeckComponent;
	// 합 대상을 고르는 중이면 그 카드, 아니면 집거나 올린 카드
	const int32 Focus = PC && PC->bIsDuelTargetingMode ? PC->DuelCardIndex : Hand ? Hand->GetFocusedCardIndex() : -1;
	if (Deck && Deck->Hand.IsValidIndex(Focus)) Cost = FMath::Clamp(Deck->Hand[Focus].MPCost, 0, MP);

	if (ManaCellRow && ManaCells.Num() != MaxMP)
	{
		ManaCellRow->ClearChildren();
		ManaCells.Reset();
		for (int32 i = 0; i < MaxMP; ++i)
		{
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(10.0f);
			Box->SetHeightOverride(14.0f);
			UImage* Cell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Box->SetContent(Cell);
			if (UHorizontalBoxSlot* S = ManaCellRow->AddChildToHorizontalBox(Box)) S->SetPadding(FMargin(i > 0 && i % 5 == 0 ? 6.0f : 2.0f, 0.0f, 0.0f, 0.0f));
			ManaCells.Add(Cell);
		}
	}
	const float Pulse = 0.85f + 0.15f * FMath::Sin(GetWorld() ? GetWorld()->GetRealTimeSeconds() * 5.0f : 0.0f);
	for (int32 i = 0; i < ManaCells.Num(); ++i)
	{
		if (!ManaCells[i]) continue;
		FLinearColor Color = FLinearColor(0.12f, 0.12f, 0.16f, 0.9f);
		if (i < MP - Cost) Color = FLinearColor(0.25f, 0.45f, 1.0f);
		else if (i < MP) Color = FLinearColor(1.0f, 0.75f, 0.35f) * Pulse; // 이 카드에 쓸 MP
		ManaCells[i]->SetColorAndOpacity(Color);
	}

	// 어둠의 성자의 MP는 신성력이다
	const bool bFaith = CachedPlayerChar->CharacterClass == EClassType::FTH;
	if (TXT_ManaTitle) TXT_ManaTitle->SetText(FText::FromString(bFaith ? TEXT("신성력") : TEXT("마나")));
	TXT_ManaValue->SetText(FText::FromString(Cost > 0 ? FString::Printf(TEXT("%d / %d  (-%d)"), MP, MaxMP, Cost) : FString::Printf(TEXT("%d / %d"), MP, MaxMP)));
	TXT_ManaValue->SetColorAndOpacity(FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor(0.45f, 0.85f, 1.0f), ManaFlash / 0.6f));
	TXT_ManaValue->SetRenderScale(FVector2D(1.0f + 0.12f * (ManaFlash / 0.6f)));
	const FBWDefenseStats& Def = CachedPlayerChar->Defense;
	TXT_ManaDetail->SetText(FText::FromString(Def.Type == EDefenseType::MagicShield
		? FString::Printf(TEXT("턴마다 +%d · 가드: %s 1당 피해 %s 막음 (한 번 최대 %d)"), CachedPlayerChar->ClassProfile.MPPerTurn, bFaith ? TEXT("신성력") : TEXT("MP"), *FString::SanitizeFloat(Def.ShieldPerMP, 0), Def.MagicShieldMaxMP)
		: FString::Printf(TEXT("턴마다 +%d"), CachedPlayerChar->ClassProfile.MPPerTurn)));
}

void UBWBattleHUDWidget::UpdateAttackPanel()
{
	if (!TXT_AttackValue) return;
	if (!CachedPlayerChar)
	{
		if (AttackPanel) AttackPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (AttackPanel) AttackPanel->SetVisibility(ESlateVisibility::HitTestInvisible);

	const FBWDiceRange Basic = CachedPlayerChar->GetBasicAttackRange();
	const int32 PowerPercent = FMath::RoundToInt(CachedPlayerChar->GetCardPower() * 100.0f);

	// 공격카드를 집거나 마우스를 올리면 그 카드로 강화했을 때를 미리 보여 준다
	const ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	const UBWHandWidget* Hand = PC ? PC->GetHandWidget() : nullptr;
	const UDeckComponent* Deck = CachedPlayerChar->DeckComponent;
	// 합 대상을 고르는 중이면 그 카드, 아니면 집거나 올린 카드
	const int32 Focus = PC && PC->bIsDuelTargetingMode ? PC->DuelCardIndex : Hand ? Hand->GetFocusedCardIndex() : -1;
	const FBWCardData* Card = Deck && Deck->Hand.IsValidIndex(Focus) && Deck->Hand[Focus].CardKind == ECardKind::Attack ? &Deck->Hand[Focus] : nullptr;
	if (Card)
	{
		const FBWDiceRange Boost = CachedPlayerChar->GetCardBoostRange(*Card);
		TXT_AttackValue->SetText(FText::FromString(FString::Printf(TEXT("%d~%d  +%d~%d  =  %d~%d"), Basic.Min, Basic.Max, Boost.Min, Boost.Max, Basic.Min + Boost.Min, Basic.Max + Boost.Max)));
		TXT_AttackValue->SetColorAndOpacity(FLinearColor(1.0f, 0.88f, 0.55f));
		TXT_AttackDetail->SetText(FText::FromString(FString::Printf(TEXT("'%s'(으)로 강화 · 사거리 %d · 카드 몫 %d%%"), *Card->CardName, CachedPlayerChar->GetAttackReach(Card), PowerPercent)));
		if (AttackAccent) AttackAccent->SetColorAndOpacity(FLinearColor(1.0f, 0.7f, 0.3f));
	}
	else
	{
		TXT_AttackValue->SetText(FText::FromString(FString::Printf(TEXT("%d ~ %d"), Basic.Min, Basic.Max)));
		TXT_AttackValue->SetColorAndOpacity(FLinearColor::White);
		const int32 Reach = CachedPlayerChar->GetAttackReach(nullptr);
		TXT_AttackDetail->SetText(FText::FromString(Reach > 1
			? FString::Printf(TEXT("사거리 %d · 적 클릭 또는 Q · 공격카드로 강화 (카드 몫 %d%%)"), Reach, PowerPercent)
			: FString::Printf(TEXT("맞닿은 적 클릭 또는 Q · 공격카드로 강화 (카드 몫 %d%%)"), PowerPercent)));
		if (AttackAccent) AttackAccent->SetColorAndOpacity(FLinearColor(0.95f, 0.78f, 0.42f));
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

UDeckComponent* UBWBattleHUDWidget::GetPlayerDeckComponent() { return CachedPlayerChar ? CachedPlayerChar->DeckComponent : nullptr; }
void UBWBattleHUDWidget::InitializeBattleHUD(ABWCharacter* InCharacter)
{
	CachedPlayerChar = InCharacter;
}
void UBWBattleHUDWidget::OnTargetHovered(ABWCharacter* TargetChar) { CurrentHoveredEnemy = TargetChar; }

void UBWBattleHUDWidget::BuildBlockBadge()
{
	if (!PB_PlayerHP || BlockBadge || !WidgetTree) return;
	OriginalHPBarStyle = PB_PlayerHP->GetWidgetStyle();
	OriginalHPFillColor = PB_PlayerHP->GetFillColorAndOpacity();

	// 방패 그림 위에 수치
	UOverlay* Badge = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BlockBadge"));
	UImage* Shield = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IMG_BlockShield"));
	if (UTexture2D* Tex = BlockShieldTexture.LoadSynchronous()) Shield->SetBrushFromTexture(Tex);
	else Shield->SetColorAndOpacity(BlockBarColor);
	Shield->SetDesiredSizeOverride(FVector2D(BlockBadgeSize, BlockBadgeSize));
	if (UOverlaySlot* S = Badge->AddChildToOverlay(Shield)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	TXT_BlockValue = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TXT_BlockValue"));
	FSlateFontInfo Font = GetDefault<UTextBlock>()->GetFont();
	Font.Size = FMath::RoundToInt(BlockBadgeSize * 0.42f);
	Font.OutlineSettings.OutlineSize = 2;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.05f, 0.2f, 1.0f);
	TXT_BlockValue->SetFont(Font);
	TXT_BlockValue->SetColorAndOpacity(FLinearColor::White);
	TXT_BlockValue->SetJustification(ETextJustify::Center);
	if (UOverlaySlot* S = Badge->AddChildToOverlay(TXT_BlockValue)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0, 0, 0, BlockBadgeSize * 0.08f)); }
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(BlockBadgeSize);
	Size->SetHeightOverride(BlockBadgeSize);
	Size->SetContent(Badge);
	Size->SetVisibility(ESlateVisibility::Collapsed);
	BlockBadge = Size;

	// HP 바가 놓인 곳에 따라 붙인다: 캔버스면 HP 바 왼쪽 끝 가운데, 가로 상자면 맨 앞
	UPanelWidget* Parent = PB_PlayerHP->GetParent();
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
	{
		if (UCanvasPanelSlot* BarSlot = Cast<UCanvasPanelSlot>(PB_PlayerHP->Slot))
		{
			const FVector2D Pos = BarSlot->GetPosition(), BarSize = BarSlot->GetSize(), Align = BarSlot->GetAlignment();
			if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(BlockBadge))
			{
				S->SetAnchors(BarSlot->GetAnchors());
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetAutoSize(true);
				S->SetPosition(FVector2D(Pos.X - Align.X * BarSize.X + BlockBadgeSize * 0.35f, Pos.Y - Align.Y * BarSize.Y + BarSize.Y * 0.5f));
				S->SetZOrder(BarSlot->GetZOrder() + 5);
			}
		}
	}
	else if (UHorizontalBox* Row = Cast<UHorizontalBox>(Parent))
	{
		Row->InsertChildAt(Row->GetChildIndex(PB_PlayerHP), BlockBadge);
	}
	else if (UOverlay* Over = Cast<UOverlay>(Parent))
	{
		if (UOverlaySlot* S = Over->AddChildToOverlay(BlockBadge)) { S->SetHorizontalAlignment(HAlign_Left); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(-BlockBadgeSize * 0.5f, 0, 0, 0)); }
	}
	UE_LOG(LogTemp, Log, TEXT("[HUD] Block badge attached to %s"), Parent ? *Parent->GetClass()->GetName() : TEXT("nothing"));
}

void UBWBattleHUDWidget::UpdateBlockDisplay(int32 InBlock, float DeltaTime)
{
	if (!PB_PlayerHP) return;
	const bool bBlue = InBlock > 0;
	if (bBlue != bHPBarBlue)
	{
		bHPBarBlue = bBlue;
		if (bBlue)
		{
			// 원래 막대 그림 대신 푸른 막대 (원래 색이 틴트를 받지 않는 그림이라 브러시를 바꾼다)
			FProgressBarStyle Style = OriginalHPBarStyle;
			FSlateBrush Fill = Style.FillImage;
			Fill.SetResourceObject(nullptr);
			if (Fill.DrawAs == ESlateBrushDrawType::NoDrawType || Fill.DrawAs == ESlateBrushDrawType::Image) Fill.DrawAs = ESlateBrushDrawType::Box;
			Fill.TintColor = FSlateColor(BlockBarColor);
			Style.SetFillImage(Fill);
			PB_PlayerHP->SetWidgetStyle(Style);
			PB_PlayerHP->SetFillColorAndOpacity(FLinearColor::White);
		}
		else
		{
			PB_PlayerHP->SetWidgetStyle(OriginalHPBarStyle);
			PB_PlayerHP->SetFillColorAndOpacity(OriginalHPFillColor);
		}
	}
	if (BlockBadge)
	{
		BlockBadge->SetVisibility(bBlue ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (InBlock != ShownBlock)
		{
			if (InBlock > ShownBlock && ShownBlock >= 0) BlockPulse = 1.0f; // 방어도를 얻으면 방패가 한 번 커졌다 돌아온다
			ShownBlock = InBlock;
			if (TXT_BlockValue) TXT_BlockValue->SetText(FText::AsNumber(InBlock));
		}
		BlockPulse = FMath::Max(0.0f, BlockPulse - DeltaTime * 4.0f);
		const float Scale = 1.0f + 0.25f * FMath::Sin(BlockPulse * PI);
		BlockBadge->SetRenderScale(FVector2D(Scale, Scale));
	}
}

namespace BWHudResource
{
	struct FRow
	{
		FString Label;
		FString Value;
		FString Hint;
		bool bActive = false; // 켜져 있음 (밝게)
		bool bAlert = false;  // 위험 (붉게)
	};

	FString Pips(int32 Filled, int32 Total)
	{
		FString Out;
		for (int32 i = 0; i < Total; ++i) Out += i < Filled ? TEXT("●") : TEXT("○");
		return Out;
	}

	// 이번 전투 덱(덱·손패·버린 카드)에 그 특수 카드가 있는지. 없는 카드에 묶인 자원 줄은 감춘다
	bool OwnsSpecial(const ABWCharacter* C, EBWCardSpecial Special)
	{
		const UDeckComponent* D = C ? C->DeckComponent : nullptr;
		if (!D) return false;
		auto Has = [Special](const TArray<FBWCardData>& Cards) { return Cards.ContainsByPredicate([Special](const FBWCardData& Card) { return Card.Special == Special; }); };
		return Has(D->Deck) || Has(D->Hand) || Has(D->Discard);
	}

	// 삶의 방식마다 '지금 무엇이 쌓였고 다음에 무엇이 되는지'를 줄로 만든다
	void Collect(const ABWCharacter* C, const UBWGameInstance* GI, TArray<FRow>& Rows)
	{
		using L = UBWLifestyleRules;
		const FBWLifestyleState& S = C->Life;
		switch (C->CharacterClass)
		{
		case EClassType::PHA:
		{
			const int32 Threshold = FMath::FloorToInt(C->MaxHP * L::RageHPFraction);
			const FBWDiceRange Rage = L::RageBonus(C->PHA, S.bBloodRevenge);
			FRow& R = Rows.AddDefaulted_GetRef();
			R.Label = TEXT("광기");
			if (C->IsRaging())
			{
				R.Value = FString::Printf(TEXT("광기! 공격 +%d~%d"), Rage.Min, Rage.Max);
				R.Hint = TEXT("HP 60% 이하 · 카드 회복은 절반");
				R.bActive = true;
			}
			else
			{
				R.Value = FString::Printf(TEXT("HP %d 이하에서"), Threshold);
				R.Hint = FString::Printf(TEXT("광기가 되면 공격 +%d~%d (자해로 HP를 깎아 들어간다)"), Rage.Min, Rage.Max);
			}
			const FBWDiceRange Will = L::WillBonus(S.Wounds);
			FRow& W = Rows.AddDefaulted_GetRef();
			W.Label = TEXT("상처");
			W.Value = FString::FromInt(S.Wounds);
			// 상처는 철의 의지(희귀 공격카드)가 쓰는 자원. 덱에 없으면 무엇에 쓰는지만 알려 준다
			const bool bHasWill = OwnsSpecial(C, EBWCardSpecial::WillOfIron);
			W.Hint = bHasWill
				? FString::Printf(TEXT("자해·출혈로 쌓임 · 철의 의지 공격 +%d~%d, 쓰면 상처 0·HP 25%% 회복"), Will.Min, Will.Max)
				: FString(TEXT("자해·출혈로 쌓임 · 희귀 카드 '철의 의지'를 얻으면 3마다 공격 +1~2"));
			W.bActive = bHasWill && S.Wounds >= 3;
			// 격노도 희귀 카드. 덱에 있을 때만 줄을 보인다
			if (!OwnsSpecial(C, EBWCardSpecial::Rage)) break;
			FRow& G = Rows.AddDefaulted_GetRef();
			G.Label = TEXT("격노");
			G.Value = S.bRageUsed ? FString(TEXT("사용함")) : S.WillUses >= L::RageWillUses ? FString(TEXT("쓸 수 있음!")) : FString::Printf(TEXT("철의 의지 %s"), *Pips(S.WillUses, L::RageWillUses));
			G.Hint = S.bRageGuard ? FString(TEXT("이번 적 턴 받는 피해 -90%")) : FString(TEXT("이번 전투에 철의 의지를 2번 쓰면 격노 카드를 쓸 수 있다"));
			G.bActive = S.bRageGuard || (!S.bRageUsed && S.WillUses >= L::RageWillUses);
			break;
		}
		case EClassType::TEC:
		{
			FRow& R = Rows.AddDefaulted_GetRef();
			R.Label = TEXT("장전");
			R.Value = S.bLoaded ? TEXT("● 장전됨") : TEXT("○ 비었음");
			R.Hint = S.bLoaded ? TEXT("마탄을 쏠 수 있다") : TEXT("사격 카드로 격돌에서 이기면 장전");
			R.bActive = S.bLoaded;
			FRow& M = Rows.AddDefaulted_GetRef();
			M.Label = TEXT("마탄");
			M.Value = FString::Printf(TEXT("%d / %d"), S.MagicCount, L::SeventhShot);
			M.Hint = S.MagicCount + 1 >= L::SeventhShot ? FString(TEXT("다음이 일곱 번째: 치명타 3배, 같은 피해가 나에게"))
				: FString::Printf(TEXT("다음 마탄 최댓값 +%d · 치명타가 아니면 빙의"), L::MagicMaxBonus(S.MagicCount, false));
			M.bAlert = S.MagicCount + 1 >= L::SeventhShot;
			FRow& P = Rows.AddDefaulted_GetRef();
			P.Label = TEXT("빙의");
			if (S.bPossessed && !S.bFiredMagicThisTurn)
			{
				P.Value = TEXT("빙의!");
				P.Hint = FString::Printf(TEXT("이번 턴 마탄을 안 쏘면 턴 끝에 %d 피해"), S.MagicCount * L::PossessedPenaltyPerShot);
				P.bAlert = true;
			}
			else if (S.bPossessedNext)
			{
				P.Value = TEXT("다음 턴 빙의");
				P.Hint = TEXT("다음 턴에도 마탄을 쏴야 한다");
				P.bAlert = true;
			}
			else
			{
				P.Value = TEXT("없음");
				P.Hint = TEXT("치명타 없이 마탄을 쏘면 악마에게 빙의된다");
			}
			if (GI && GI->SoulBullets > 0)
			{
				FRow& B = Rows.AddDefaulted_GetRef();
				B.Label = TEXT("영혼탄");
				B.Value = FString::Printf(TEXT("%d발"), GI->SoulBullets);
				B.Hint = TEXT("일곱 번째 마탄의 반동을 대신 맞는다");
			}
			break;
		}
		case EClassType::INT:
		{
			FRow& R = Rows.AddDefaulted_GetRef();
			R.Label = TEXT("해방");
			if (S.bLiberated)
			{
				R.Value = TEXT("해방!");
				R.Hint = TEXT("마법 카드 ×1.4 · 최대 MP +10 (이번 전투)");
				R.bActive = true;
			}
			else
			{
				R.Value = FString::Printf(TEXT("명상 %s"), *Pips(S.Liberation, L::LiberationNeeded));
				R.Hint = FString::Printf(TEXT("명상을 %d번 더 하면 해방 (명상은 즉시 턴 종료)"), L::LiberationNeeded - S.Liberation);
			}
			FRow& C2 = Rows.AddDefaulted_GetRef();
			C2.Label = TEXT("영창");
			if (S.ChantReady > 0) { C2.Value = FString::Printf(TEXT("+%d 준비됨"), S.ChantReady); C2.Hint = TEXT("이번 턴 첫 마법의 최댓값이 오른다"); C2.bActive = true; }
			else if (S.ChantPending > 0) { C2.Value = FString::Printf(TEXT("+%d 영창 중"), S.ChantPending); C2.Hint = TEXT("적 턴에 맞으면 깨진다 · 다음 턴 첫 마법에"); C2.bAlert = true; }
			else { C2.Value = TEXT("없음"); C2.Hint = TEXT("영창 카드로 다음 턴 마법을 키운다"); }
			break;
		}
		case EClassType::FTH:
		{
			FRow& D = Rows.AddDefaulted_GetRef();
			D.Label = TEXT("어둠");
			D.Value = FString::Printf(TEXT("%d  %s"), S.Dark, *Pips(FMath::Min(S.Dark, L::TenebrisDarkNeeded), L::TenebrisDarkNeeded));
			D.Hint = S.bTenebris ? FString(TEXT("턴 끝마다 1씩 줄고 0이면 빛으로 돌아온다"))
				: S.Dark >= L::TenebrisDarkNeeded ? FString(TEXT("강림할 수 있다!")) : FString(TEXT("치유 카드를 쓸 때마다 +1 · 3이면 강림"));
			D.bActive = !S.bTenebris && S.Dark >= L::TenebrisDarkNeeded;
			FRow& T = Rows.AddDefaulted_GetRef();
			T.Label = TEXT("테네브리스");
			if (S.bTenebris)
			{
				T.Value = FString::Printf(TEXT("강림 중 · 턴마다 자해 %d"), S.Dark * L::TenebrisSelfDamagePerDark);
				T.Hint = FString::Printf(TEXT("치유 카드가 공격이 된다 · 공격 최댓값 +%d · 준 피해 %d%% 회복"), S.Dark * L::TenebrisMaxPerDark, FMath::RoundToInt(L::TenebrisLifesteal * 100.0f));
				T.bAlert = true;
			}
			else
			{
				T.Value = TEXT("빛");
				T.Hint = TEXT("치유 카드는 회복 · 강림하면 공격으로 바뀐다");
			}
			break;
		}
		case EClassType::HP:
		{
			FRow& B = Rows.AddDefaulted_GetRef();
			B.Label = TEXT("방어도");
			B.Value = FString::FromInt(C->Block);
			B.Hint = TEXT("전투 내내 쌓임 · 턴 종료 +1 · 격돌에서 이기면 +1");
			B.bActive = C->Block > 0;
			FRow& G = Rows.AddDefaulted_GetRef();
			G.Label = TEXT("질풍노도");
			if (S.bGaleGuard) { G.Value = TEXT("버티는 중"); G.Hint = TEXT("이번 적 턴 받는 피해 절반 → 다음 턴 폭풍 질주"); G.bActive = true; }
			else if (S.bStormReady) { G.Value = TEXT("폭풍 질주!"); G.Hint = TEXT("이번 턴 첫 기본 공격이 두 배"); G.bActive = true; }
			else if (S.bStormPending) { G.Value = TEXT("다음 턴 폭풍 질주"); G.Hint = TEXT("다음 턴 첫 기본 공격이 두 배"); G.bActive = true; }
			else if (S.GaleCooldown > 0) { G.Value = FString::Printf(TEXT("대기 %d턴"), S.GaleCooldown); G.Hint = TEXT("다시 쓰려면 기다려야 한다"); }
			else { G.Value = TEXT("쓸 수 있음"); G.Hint = TEXT("한 턴 격돌을 쉬고 피해 절반, 다음 턴 첫 공격 두 배"); }
			break;
		}
		default:
			break;
		}

		// 모든 삶의 방식 공통: 이번 적 턴 버프와 상태이상
		TArray<FString> Buffs;
		if (S.Counter > 0) Buffs.Add(FString::Printf(TEXT("반격 %d"), S.Counter));
		if (S.EvadeBonus > 0.0f) Buffs.Add(FString::Printf(TEXT("회피 +%d%%"), FMath::RoundToInt(S.EvadeBonus * 100.0f)));
		if (S.DefenseBonus > 0) Buffs.Add(FString::Printf(TEXT("방어 +%d"), S.DefenseBonus));
		if (C->CharacterClass != EClassType::HP && C->Block > 0) Buffs.Add(FString::Printf(TEXT("방어도 %d"), C->Block));
		if (C->CoverShield > 0) Buffs.Add(FString::Printf(TEXT("방벽 보호막 %d"), C->CoverShield));
		if (Buffs.Num() > 0)
		{
			FRow& R = Rows.AddDefaulted_GetRef();
			R.Label = TEXT("이번 적 턴");
			R.Value = FString::Join(Buffs, TEXT(" · "));
			R.bActive = true;
		}
		if (C->Statuses.Num() > 0)
		{
			FRow& R = Rows.AddDefaulted_GetRef();
			R.Label = TEXT("상태");
			R.Value = UBWStatusRules::DescribeStatuses(C->Statuses);
			R.Hint = UBWStatusRules::DescribeStatusDetails(C->Statuses);
			R.bAlert = true;
		}
	}
}

void UBWBattleHUDWidget::BuildEnemyCards(UCanvasPanel* RootCanvas)
{
	using namespace BWHudUI;
	for (int32 i = 0; i < 2; ++i)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		const FLinearColor Accent = i == 0 ? FLinearColor(0.5f, 0.75f, 1.0f) : FLinearColor(1.0f, 0.45f, 0.4f);
		Card->SetBrush(RoundBrush(FLinearColor(0.05f, 0.05f, 0.09f, 0.96f), 10.0f, Accent, 2.0f));
		Card->SetPadding(FMargin(12.0f, 10.0f));
		Card->SetVisibility(ESlateVisibility::Collapsed);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(EnemyCardWidth);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Size->SetContent(Column);
		Card->SetContent(Size);

		TXT_EnemyCardKind[i] = MakeHUDText(13, Accent, true);
		Column->AddChildToVerticalBox(TXT_EnemyCardKind[i]);
		TXT_EnemyCardName[i] = MakeHUDText(19, FLinearColor::White, true);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TXT_EnemyCardName[i])) S->SetPadding(FMargin(0.0f, 2.0f));
		TXT_EnemyCardValue[i] = MakeHUDText(30, Gold, true);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TXT_EnemyCardValue[i])) S->SetHorizontalAlignment(HAlign_Center);
		TXT_EnemyCardDesc[i] = MakeHUDText(13, TextDim);
		TXT_EnemyCardDesc[i]->SetWrapTextAt(EnemyCardWidth - 8.0f);
		Column->AddChildToVerticalBox(TXT_EnemyCardDesc[i]);

		EnemyCard[i] = Card;
		if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(Card)) S->SetAutoSize(true);
	}
}

void UBWBattleHUDWidget::UpdateEnemyCards()
{
	if (!EnemyCard[0] || !EnemyCard[1]) return;
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	const ABWCharacter* Enemy = PC ? PC->GetHoveredEnemy() : nullptr;
	const FBWEnemyPattern* Action = Enemy ? Enemy->GetCurrentAction() : nullptr;
	FVector2D Screen;
	if (!Action || !PC->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), Screen, true))
	{
		EnemyCard[0]->SetVisibility(ESlateVisibility::Collapsed);
		EnemyCard[1]->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
	Screen /= Scale;
	const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this) / Scale;

	// 왼쪽: 내 턴에 치면 내는 카드
	const FBWDuelSide Counter = Enemy->MakeEnemyDuelSide(true);
	TXT_EnemyCardKind[0]->SetText(FText::FromString(Counter.bGuard ? TEXT("내 턴에 치면 · 방어") : TEXT("내 턴에 치면 · 반격")));
	TXT_EnemyCardName[0]->SetText(FText::FromString(Action->CounterName.IsEmpty() ? Action->ActionName : Action->CounterName));
	TXT_EnemyCardValue[0]->SetText(FText::FromString(FString::Printf(TEXT("%d~%d"), Counter.SkillRange.Min, Counter.SkillRange.Max)));
	FString CounterDesc = Counter.bGuard ? TEXT("맞굴림. 이겨도 적이 굴린 수만큼 피해가 깎이고, 지면 아무 일도 없다.") : TEXT("맞굴림. 진 쪽이 맞는다.");
	FBWDuelForecast Forecast;
	FString Label;
	FBWDiceRange WinHP, LoseHP;
	if (PC->GetDuelForecast(Enemy, Forecast, Label, WinHP, LoseHP))
	{
		CounterDesc += FString::Printf(TEXT("\n\n%s\n승률 %d%% · 이기면 적 HP -%d~%d"), *Label, FMath::RoundToInt(Forecast.WinChance * 100.0f), WinHP.Min, WinHP.Max);
		if (!Forecast.bGuarded) CounterDesc += LoseHP.Max > 0 ? FString::Printf(TEXT("\n지면 당신의 HP -%d~%d"), LoseHP.Min, LoseHP.Max) : FString(TEXT("\n져도 피해 없음"));
	}
	if (Enemy->Statuses.Num() > 0) CounterDesc += FString::Printf(TEXT("\n\n%s"), *UBWStatusRules::DescribeStatusDetails(Enemy->Statuses));
	if (Enemy->CurrentAttackBuff > 0) CounterDesc += FString::Printf(TEXT("\n공격 +%d"), Enemy->CurrentAttackBuff);
	if (Enemy->Block > 0) CounterDesc += FString::Printf(TEXT("\n방어도 %d"), Enemy->Block);
	TXT_EnemyCardDesc[0]->SetText(FText::FromString(CounterDesc));

	// 오른쪽: 적 턴에 쓰는 카드
	const FBWDiceRange Range = Enemy->MakeEnemyDuelSide().SkillRange;
	FString Kind = TEXT("공격"), Value = FString::Printf(TEXT("%d~%d"), Range.Min, Range.Max), Desc = Action->Description;
	switch (Action->ActionType)
	{
	case EBWEnemyActionType::Heal: Kind = TEXT("효과"); Value = FString::Printf(TEXT("회복 %d"), Action->Value); break;
	case EBWEnemyActionType::Block: Kind = TEXT("효과"); Value = FString::Printf(TEXT("방어도 %d"), Action->Value); break;
	case EBWEnemyActionType::Hex: Kind = TEXT("효과"); Value = UBWStatusRules::DescribeStatuses({ FBWStatusInstance{ Action->AppliedStatus, Action->StatusDuration, Action->StatusValue } }); break;
	case EBWEnemyActionType::Summon: Kind = TEXT("효과"); Value = TEXT("소환"); break;
	case EBWEnemyActionType::Chant: Kind = TEXT("효과"); Value = TEXT("영창"); break;
	case EBWEnemyActionType::Devour: Kind = TEXT("효과"); Value = FString::Printf(TEXT("포식 %d"), Action->Value); break;
	case EBWEnemyActionType::Reckoning: Value = FString::Printf(TEXT("%d~%d"), Range.Min + Enemy->EatenCards * Action->Value, Range.Max + Enemy->EatenCards * Action->Value); break;
	case EBWEnemyActionType::BuffAttack: Value = FString::Printf(TEXT("%d~%d"), Range.Min + Action->Value, Range.Max + Action->Value); break;
	default: break;
	}
	if (Enemy->ActionRange > 1) Desc += FString::Printf(TEXT("\n사거리 %d"), Enemy->ActionRange);
	TXT_EnemyCardKind[1]->SetText(FText::FromString(FString::Printf(TEXT("적 턴에 · %s"), *Kind)));
	TXT_EnemyCardName[1]->SetText(FText::FromString(Action->ActionName));
	TXT_EnemyCardValue[1]->SetText(FText::FromString(Value));
	TXT_EnemyCardDesc[1]->SetText(FText::FromString(Desc));

	// 적 양 옆에 놓되 화면 밖으로 나가지 않게
	const float Gap = 70.0f, Top = FMath::Clamp(Screen.Y - 110.0f, 90.0f, FMath::Max(90.0f, View.Y - 420.0f));
	const float LeftX = Screen.X - Gap - EnemyCardWidth - 24.0f, RightX = Screen.X + Gap;
	for (int32 i = 0; i < 2; ++i)
	{
		EnemyCard[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(EnemyCard[i]->Slot))
		{
			S->SetPosition(FVector2D(FMath::Clamp(i == 0 ? LeftX : RightX, 8.0f, View.X - EnemyCardWidth - 32.0f), Top));
		}
	}
}

void UBWBattleHUDWidget::UpdateSetPanel()
{
	using namespace BWHudUI;
	const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!SetBox || !GI) return;
	const FBWAugmentTotals Totals = GI->GetAugmentTotals();

	FString Key;
	for (int32 Count : Totals.SetCounts) Key += FString::FromInt(Count) + TEXT(",");
	Key += FString::FromInt(GI->OwnedAugments.Num());
	if (Key == ShownSetKey) return;
	ShownSetKey = Key;
	SetBox->ClearChildren();
	SetRows.Reset();

	for (int32 i = 1; i < Totals.SetCounts.Num(); ++i)
	{
		const int32 Count = Totals.SetCounts[i];
		if (Count <= 0) continue;
		const EBWAugmentSet Set = (EBWAugmentSet)i;
		FString Pips;
		for (int32 p = 0; p < 4; ++p) Pips += p < Count ? TEXT("●") : TEXT("○");

		// 세트 아이콘 + 이름 + 모은 개수 눈금
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UTexture2D* SetTex = BWSkin::LoadIcon(BWSkin::SetIconName((uint8)Set)))
		{
			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			IconBox->SetWidthOverride(24.0f);
			IconBox->SetHeightOverride(24.0f);
			UImage* SetIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			SetIcon->SetBrushFromTexture(SetTex);
			SetIcon->SetRenderOpacity(Count >= 2 ? 1.0f : 0.6f);
			IconBox->SetContent(SetIcon);
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconBox)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f)); }
		}
		UTextBlock* RowText = MakeHUDText(14, Count >= 4 ? Gold : Count >= 2 ? FLinearColor::White : TextDim, Count >= 3);
		RowText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"), *UBWAugmentRules::GetSetName(Set), *Pips)));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(RowText)) S->SetVerticalAlignment(VAlign_Center);
		if (UVerticalBoxSlot* S = SetBox->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

		SetRows.Add({ Row, FString::Printf(TEXT("%s (%d개)"), *UBWAugmentRules::GetSetName(Set), Count), UBWAugmentRules::DescribeSet(Set, Count, GI->SelectedClass) });
	}

	// 가진 증강: 작은 아이콘 줄 (최대 두 줄, 넘치면 마지막 칸에 +N), 마우스를 올리면 효과
	TArray<TPair<FName, FBWAugmentData>> Owned;
	for (const FName& RowName : GI->OwnedAugments)
	{
		FBWAugmentData Data;
		if (GI->GetAugmentData(RowName, Data)) Owned.Add({ RowName, Data });
	}
	if (Owned.Num() > 0)
	{
		constexpr float IconSize = 34.0f, IconGap = 4.0f, StripWidth = 330.0f;
		const int32 PerRow = FMath::Max(1, FMath::FloorToInt((StripWidth + IconGap) / (IconSize + IconGap)));
		const int32 MaxIcons = PerRow * 2;
		const bool bOverflow = Owned.Num() > MaxIcons;
		const int32 IconCount = bOverflow ? MaxIcons - 1 : Owned.Num();

		UWrapBox* Strip = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		Strip->SetInnerSlotPadding(FVector2D(IconGap, IconGap));
		Strip->SetWrapSize(StripWidth);
		Strip->SetExplicitWrapSize(true);
		if (UVerticalBoxSlot* S = SetBox->AddChildToVerticalBox(Strip)) S->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

		auto MakeCell = [&](const FLinearColor& Edge, float EdgeWidth) -> UBorder*
		{
			UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Cell->SetBrush(BWSkin::Plain(FLinearColor(0.1f, 0.12f, 0.15f, 1.0f), Edge, EdgeWidth));
			Cell->SetPadding(FMargin(3.0f));
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(IconSize);
			Box->SetHeightOverride(IconSize);
			Box->SetContent(Cell);
			Strip->AddChildToWrapBox(Box);
			return Cell;
		};

		for (int32 i = 0; i < IconCount; ++i)
		{
			const FName RowName = Owned[i].Key;
			const FBWAugmentData& Data = Owned[i].Value;
			const FLinearColor Accent = BWSkin::AugmentAccent((uint8)Data.Kind);
			UBorder* Cell = MakeCell(Accent, Data.Set != EBWAugmentSet::None ? 2.0f : 1.2f);
			if (UTexture2D* Tex = BWSkin::LoadAugmentIcon(RowName, Data.Icon))
			{
				UImage* Pic = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Pic->SetBrushFromTexture(Tex);
				Cell->SetContent(Pic);
			}
			else
			{
				Cell->SetHorizontalAlignment(HAlign_Center);
				Cell->SetVerticalAlignment(VAlign_Center);
				UTextBlock* Glyph = MakeHUDText(16, Accent, true);
				Glyph->SetText(FText::FromString(Data.AugmentName.Left(1)));
				Cell->SetContent(Glyph);
			}
			FString Desc = Data.Description;
			if (!Data.CostText.IsEmpty()) Desc += FString::Printf(TEXT("\n대가: %s"), *Data.CostText);
			if (Data.Set != EBWAugmentSet::None) Desc += FString::Printf(TEXT("\n세트: %s"), *UBWAugmentRules::GetSetName(Data.Set));
			SetRows.Add({ Cell, FString::Printf(TEXT("%s · %s"), *Data.AugmentName, *UBWAugmentRules::GetKindName(Data.Kind)), Desc });
		}
		if (bOverflow)
		{
			UBorder* Cell = MakeCell(FLinearColor(0.68f, 0.7f, 0.74f, 0.85f), 1.2f);
			Cell->SetHorizontalAlignment(HAlign_Center);
			Cell->SetVerticalAlignment(VAlign_Center);
			UTextBlock* More = MakeHUDText(13, FLinearColor::White, true);
			More->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Owned.Num() - IconCount)));
			Cell->SetContent(More);
			TArray<FString> Rest;
			for (int32 i = IconCount; i < Owned.Num(); ++i) Rest.Add(Owned[i].Value.AugmentName);
			SetRows.Add({ Cell, FString::Printf(TEXT("증강 %d개 더"), Owned.Num() - IconCount), FString::Join(Rest, TEXT(" · ")) });
		}
	}
}

void UBWBattleHUDWidget::UpdateResourcePanel(const FLinearColor& ClassColor)
{
	using namespace BWHudUI;
	if (!ResourceBox || !CachedPlayerChar) return;
	TArray<BWHudResource::FRow> Rows;
	BWHudResource::Collect(CachedPlayerChar, Cast<UBWGameInstance>(GetGameInstance()), Rows);

	// 내용이 바뀔 때만 다시 만든다
	FString Key;
	for (const BWHudResource::FRow& R : Rows) Key += R.Label + R.Value + R.Hint + (R.bActive ? TEXT("1") : TEXT("0")) + (R.bAlert ? TEXT("1") : TEXT("0")) + TEXT("|");
	if (Key == ShownResourceKey) return;
	ShownResourceKey = Key;
	ResourceBox->ClearChildren();

	const FLinearColor AlertColor(1.0f, 0.38f, 0.32f);
	for (const BWHudResource::FRow& R : Rows)
	{
		const FLinearColor Accent = R.bAlert ? AlertColor : ClassColor;
		const bool bLit = R.bActive || R.bAlert;
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// 이름표
		USizeBox* PillSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PillSize->SetWidthOverride(86.0f);
		UBorder* Pill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Pill->SetBrush(RoundBrush(Scaled(Accent, bLit ? 0.55f : 0.22f, 0.95f), 7.0f, Scaled(Accent, 1.0f, bLit ? 0.9f : 0.4f), 1.0f));
		Pill->SetPadding(FMargin(6.0f, 3.0f));
		Pill->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* LabelText = MakeHUDText(13, FLinearColor::White, true);
		LabelText->SetText(FText::FromString(R.Label));
		Pill->SetContent(LabelText);
		PillSize->SetContent(Pill);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(PillSize)) S->SetVerticalAlignment(VAlign_Top);

		// 값 + 설명
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		const FLinearColor ValueColor = R.bAlert ? AlertColor : R.bActive ? FLinearColor(ClassColor * 0.4f + FLinearColor(0.6f, 0.6f, 0.6f, 0.6f)) : FLinearColor(0.93f, 0.93f, 0.96f);
		UTextBlock* ValueText = MakeHUDText(17, ValueColor, true);
		ValueText->SetText(FText::FromString(R.Value));
		Texts->AddChildToVerticalBox(ValueText);
		if (!R.Hint.IsEmpty())
		{
			UTextBlock* HintText = MakeHUDText(12, TextDim);
			HintText->SetText(FText::FromString(R.Hint));
			HintText->SetWrapTextAt(290.0f);
			Texts->AddChildToVerticalBox(HintText);
		}
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Texts)) S->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
		if (UVerticalBoxSlot* S = ResourceBox->AddChildToVerticalBox(Line)) S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
	}
}
