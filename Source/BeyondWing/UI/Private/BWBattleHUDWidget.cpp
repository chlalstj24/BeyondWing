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
#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "BeyondWing/UI/Public/BWHandWidget.h"
#include "Blueprint/WidgetTree.h"
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
	if (!DeckText && WidgetTree)
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
	if (DeckText)
	{
		const UDeckComponent* Deck = CachedPlayerChar ? CachedPlayerChar->DeckComponent : nullptr;
		// [수정] 이번 전투의 국가 기믹도 여기에 (스탯 칸 글꼴에는 한글이 없어 네모로 나왔다)
		FString Field;
		if (const ABWPlayerController* OwnerPC = Cast<ABWPlayerController>(GetOwningPlayer()))
		{
			const FBWBattlefieldState& BF = OwnerPC->Battlefield;
			switch (BF.Dimension)
			{
			case EDimensionType::Taiga:    Field = FString::Printf(TEXT("타이가 · 방어 무시+치명타 %d회 남음"), BF.TaigaChargesLeft); break;
			case EDimensionType::Berend:   Field = TEXT("베렌드 · 밀어서 벽에 박으면 추가 피해 + 기절"); break;
			case EDimensionType::Patrovas: Field = FString::Printf(TEXT("파트로바스 · 이번 턴 스킬 최댓값 x%.2f"), BF.PatrovasMaxMultiplier); break;
			case EDimensionType::Geleon:
				Field = !BF.bGeleonUseEffects ? FString(TEXT("겔레온 · 피해 +50%, 효과카드 안 씀"))
					: BF.GeleonAttackCardName.IsEmpty() ? FString(TEXT("겔레온 · 피해 +50%, 처음 쓰는 공격카드 1종만"))
					: FString::Printf(TEXT("겔레온 · 피해 +50%%, 강화 카드는 '%s'만"), *BF.GeleonAttackCardName);
				break;
			default: break;
			}
			if (!Field.IsEmpty() && BF.bOverloaded) Field += TEXT(" (과부하)");
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
	if (PromptFrame) PromptFrame->SetBrushColor(Look.Accent * FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
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
			PromptFrame->SetBrushColor(Accent * FLinearColor(1.0f, 1.0f, 1.0f, 0.55f + 0.35f * (0.5f + 0.5f * FMath::Sin(PromptAge * 4.0f))));
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
		UTextBlock* ManaTitle = MakeManaText(14, FLinearColor(0.5f, 0.75f, 1.0f));
		ManaTitle->SetText(FText::FromString(TEXT("마나")));
		if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(ManaTitle)) S->SetVerticalAlignment(VAlign_Bottom);
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
		PanelSlot->SetPosition(FVector2D(28.0f, -86.0f)); // 덱 장수 줄 위
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

	TXT_ManaValue->SetText(FText::FromString(Cost > 0 ? FString::Printf(TEXT("%d / %d  (-%d)"), MP, MaxMP, Cost) : FString::Printf(TEXT("%d / %d"), MP, MaxMP)));
	TXT_ManaValue->SetColorAndOpacity(FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor(0.45f, 0.85f, 1.0f), ManaFlash / 0.6f));
	TXT_ManaValue->SetRenderScale(FVector2D(1.0f + 0.12f * (ManaFlash / 0.6f)));
	const FBWDefenseStats& Def = CachedPlayerChar->Defense;
	TXT_ManaDetail->SetText(FText::FromString(Def.Type == EDefenseType::MagicShield
		? FString::Printf(TEXT("턴마다 +%d · 가드: MP 1당 피해 %s 막음 (한 번 최대 %d)"), CachedPlayerChar->ClassProfile.MPPerTurn, *FString::SanitizeFloat(Def.ShieldPerMP, 0), Def.MagicShieldMaxMP)
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
