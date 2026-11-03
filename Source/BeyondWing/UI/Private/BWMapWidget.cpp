#include "BeyondWing/UI/Public/BWMapWidget.h"
#include "BeyondWing/UI/Public/BWSkin.h"
#include "UObject/UObjectIterator.h"
#include "BeyondWing/UI/Public/BWSettingsWidget.h"
#include "BeyondWing/UI/Public/BWMapNodeWidget.h"
#include "BeyondWing/UI/Public/BWAugmentSelectWidget.h"
#include "BeyondWing/Core/Public/BWAutoPlayer.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/Core/Public/BWMapRules.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "BeyondWing/UI/Public/BWTooltipWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "BeyondWing/UI/Public/BWCardWidget.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"

namespace BWMapUI
{
	const FVector2D CanvasSize(1920.0f, 1080.0f);
	const FLinearColor Gold(1.0f, 0.8f, 0.35f);
	const FLinearColor GateOpen(0.35f, 0.95f, 1.0f);
	const FLinearColor GateClosed(1.0f, 0.3f, 0.28f);
	const FLinearColor TextDim(0.68f, 0.7f, 0.78f);

	FLinearColor DimColor(EDimensionType Dim)
	{
		switch (Dim)
		{
		case EDimensionType::Patrovas: return FLinearColor(0.62f, 0.4f, 1.0f);   // 마법: 보랏빛
		case EDimensionType::Berend:   return FLinearColor(1.0f, 0.55f, 0.26f);  // 상남자: 노을 흙빛
		case EDimensionType::Geleon:   return FLinearColor(0.66f, 0.74f, 0.8f);  // 기술: 금속
		case EDimensionType::Taiga:    return FLinearColor(0.3f, 0.48f, 0.95f);  // 암살: 밤
		default:                       return FLinearColor(0.8f, 0.8f, 0.8f);
		}
	}

	FLinearColor NodeColor(EMapNodeType Type)
	{
		switch (Type)
		{
		case EMapNodeType::Battle:         return FLinearColor(0.9f, 0.36f, 0.32f);
		case EMapNodeType::Elite:          return FLinearColor(1.0f, 0.25f, 0.6f);
		case EMapNodeType::Mystery:        return FLinearColor(0.45f, 0.78f, 1.0f);
		case EMapNodeType::Rest:           return FLinearColor(0.38f, 0.88f, 0.48f);
		case EMapNodeType::Shop:           return FLinearColor(1.0f, 0.84f, 0.32f);
		case EMapNodeType::Chest:          return FLinearColor(1.0f, 0.62f, 0.18f);
		case EMapNodeType::Boss:           return FLinearColor(1.0f, 0.2f, 0.14f);
		case EMapNodeType::DimensionAltar: return FLinearColor(0.8f, 0.6f, 1.0f);
		case EMapNodeType::Rift:           return FLinearColor(0.3f, 1.0f, 0.88f);
		default:                           return FLinearColor::White;
		}
	}

	FString NodeIconName(EMapNodeType Type)
	{
		switch (Type)
		{
		case EMapNodeType::Battle:         return TEXT("Node_Battle");
		case EMapNodeType::Elite:          return TEXT("Node_Elite");
		case EMapNodeType::Mystery:        return TEXT("Node_Mystery");
		case EMapNodeType::Rest:           return TEXT("Node_Rest");
		case EMapNodeType::Shop:           return TEXT("Node_Shop");
		case EMapNodeType::Chest:          return TEXT("Node_Chest");
		case EMapNodeType::Boss:           return TEXT("Node_Boss");
		case EMapNodeType::DimensionAltar: return TEXT("Node_Altar");
		case EMapNodeType::Rift:           return TEXT("Node_Rift");
		default:                           return FString();
		}
	}

	FString NodeHelp(EMapNodeType Type)
	{
		switch (Type)
		{
		case EMapNodeType::Battle:         return TEXT("탑의 몬스터와 싸웁니다. 이기면 골드와 카드 보상(3장 중 1장)을 받습니다");
		case EMapNodeType::Elite:          return TEXT("강한 몬스터입니다. 골드 2배 + 카드 보상 (유물은 준비 중)");
		case EMapNodeType::Mystery:        return TEXT("선택형 이벤트 (구역마다 다름)");
		case EMapNodeType::Rest:           return TEXT("HP 회복 또는 카드 강화");
		case EMapNodeType::Shop:           return TEXT("카드 구매, 카드 삭제");
		case EMapNodeType::Chest:          return TEXT("회수팀보다 먼저 찾은 물건입니다. 전투 없이 골드 + 카드 보상");
		case EMapNodeType::Boss:           return TEXT("층의 끝입니다. 네 구역이 합류하고, 마지막으로 서 있던 구역의 기믹이 보스전에 적용됩니다");
		case EMapNodeType::DimensionAltar: return TEXT("과부하와 모든 구역의 보너스 내성을 초기화");
		case EMapNodeType::Rift:           return TEXT("밟으면 다음 칸에서 두 구역 떨어진 구역으로 점프 가능");
		default:                           return FString();
		}
	}

	FString Concept(EDimensionType Dim)
	{
		switch (Dim)
		{
		case EDimensionType::Patrovas: return TEXT("마법");
		case EDimensionType::Berend:   return TEXT("상남자");
		case EDimensionType::Geleon:   return TEXT("사격");
		case EDimensionType::Taiga:    return TEXT("암살");
		default:                       return FString();
		}
	}

	FString Gimmick(EDimensionType Dim)
	{
		switch (Dim)
		{
		case EDimensionType::Patrovas: return TEXT("마력선: 턴마다 두 줄이 빛나고, 그 위의 말은 최댓값 ×1.4 (적도 같음)");
		case EDimensionType::Berend:   return TEXT("정면 승부: 격돌에서 지면 밀려나고, 벽에 부딪히면 피해. 적은 기절");
		case EDimensionType::Geleon:   return TEXT("활공: 이번 턴 움직인 칸 1당 피해 +15%, 최대 +60% (적은 돌진한 칸만큼)");
		case EDimensionType::Taiga:    return TEXT("은신: 격돌을 열지 않고 맞닿은 적 없이 턴을 끝내면 다음 공격이 치명타 + 방어 무시");
		default:                       return FString();
		}
	}

	FString OverloadTwist(EDimensionType Dim)
	{
		switch (Dim)
		{
		case EDimensionType::Patrovas: return TEXT("마력선 위에서 턴을 끝내면 고정 피해 3");
		case EDimensionType::Berend:   return TEXT("내가 벽에 부딪히는 피해 2배");
		case EDimensionType::Geleon:   return TEXT("내가 받는 피해 +20%");
		case EDimensionType::Taiga:    return TEXT("적의 첫 공격 1회가 방어 무시");
		default:                       return FString();
		}
	}

	FString DescribeBonus(const FBWNeglectBonus& B)
	{
		FString Text;
		switch (B.Dimension)
		{
		case EDimensionType::Patrovas:
			Text = B.NormalCardChoices > 0 ? FString::Printf(TEXT("다음 카드 보상이 강화 %d장 + 일반 %d장"), B.UpgradedCardChoices, B.NormalCardChoices)
				: FString::Printf(TEXT("다음 카드 보상이 강화 카드 %d장 중 선택"), B.UpgradedCardChoices);
			break;
		case EDimensionType::Berend: Text = FString::Printf(TEXT("다음 전투 승리 시 HP %d%% 회복"), FMath::RoundToInt(B.VictoryHealFraction * 100.0f)); break;
		case EDimensionType::Geleon: Text = FString::Printf(TEXT("다음 전투 피해량 +%d%%"), FMath::RoundToInt(B.NextBattleDamageBonus * 100.0f)); break;
		case EDimensionType::Taiga:
			Text = FString::Printf(TEXT("카드 %d장 삭제 + %s"), B.CardRemovals, B.EquipmentGrade >= 2 ? TEXT("전설 유물") : TEXT("희귀 이상 유물"));
			break;
		default: break;
		}
		if (B.Percent < 100) Text += FString::Printf(TEXT(" (내성 %d%%)"), B.Percent);
		if (B.Gold > 0) Text += FString::Printf(TEXT(" + 깊은 방치 골드 %d"), B.Gold);
		return Text;
	}

	FString StatusName(EStatusEffect Status)
	{
		switch (Status)
		{
		case EStatusEffect::Stun:       return TEXT("기절");
		case EStatusEffect::Slow:       return TEXT("둔화");
		case EStatusEffect::Weak:       return TEXT("약화");
		case EStatusEffect::Vulnerable: return TEXT("취약");
		case EStatusEffect::Bleed:      return TEXT("출혈");
		case EStatusEffect::Confusion:  return TEXT("혼란");
		case EStatusEffect::Focus:      return TEXT("집중");
		default:                        return FString();
		}
	}

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
}

void UBWMapOptionHandler::HandleClicked()
{
	UBWMapWidget* Map = Owner.Get();
	if (!Map) return;
	if (Index == ToggleUpgradeIndex) Map->ToggleUpgradePreview();
	else Map->ChooseModalOption(Index);
}

// ---------------------------------------------------------------------------------------------
// 화면 만들기
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (bUseCodeLayout && WidgetTree && !UBWAutoPlayer::IsActive()) BuildCodeLayout();
}

void UBWMapWidget::CloseModalByEscape()
{
	for (const TCHAR* Word : { TEXT("닫기"), TEXT("떠나기"), TEXT("취소"), TEXT("그만두기"), TEXT("건너뛰기"), TEXT("지나갑니다") })
	{
		for (int32 i = 0; i < ModalOptions.Num(); ++i)
		{
			if (ModalOptions[i].bEnabled && ModalOptions[i].Label.ToString().Contains(Word)) { ChooseModalOption(i); return; }
		}
	}
}

void UBWMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bUseCodeLayout) return;

	// [추가] 전투 중에 저장하고 나갔다면 지도를 거치지 않고 그 전투를 다시 시작한다
	UBWGameInstance* ResumeGI = Cast<UBWGameInstance>(GetGameInstance());
	if (ResumeGI && ResumeGI->bResumeBattle)
	{
		ResumeGI->bResumeBattle = false;
		ResumeGI->BeginBattleSnapshot();
		ResumeGI->PrepareEncounter(ResumeGI->CurrentEncounterType, ResumeGI->GetCurrentDimension());
		bBusy = true;
		UGameplayStatics::OpenLevel(GetWorld(), BattleLevelName);
		return;
	}
	RenderMap();
	ProcessPendingFlow();
}

UTextBlock* UBWMapWidget::MakeText(const FString& Text, int32 FontSize, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = FontSize;
	if (bBold) Font.TypefaceFontName = TEXT("Bold");
	Font.OutlineSettings.OutlineSize = FontSize >= 26 ? 1 : 0;
	Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.8f);
	Block->SetFont(Font);
	Block->SetColorAndOpacity(Color);
	Block->SetShadowOffset(FVector2D(0.0f, 1.5f));
	Block->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.65f));
	Block->SetText(FText::FromString(Text));
	Block->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Block;
}

UTextBlock* UBWMapWidget::AddText(UCanvasPanel* Parent, const FString& Text, const FVector2D& Pos, int32 FontSize, const FLinearColor& Color, bool bBold, const FVector2D& Alignment, float WrapAt)
{
	UTextBlock* Block = MakeText(Text, FontSize, Color, bBold);
	// 캔버스의 자동 크기 칸에서는 AutoWrap이 좁게 접히므로 고정 폭으로만 접는다
	if (WrapAt > 0.0f) Block->SetWrapTextAt(WrapAt);
	if (UCanvasPanelSlot* TextSlot = Parent->AddChildToCanvas(Block))
	{
		TextSlot->SetAutoSize(true);
		TextSlot->SetPosition(Pos);
		TextSlot->SetAlignment(Alignment);
	}
	return Block;
}

UBorder* UBWMapWidget::AddBox(UCanvasPanel* Parent, const FVector2D& Center, const FVector2D& Size, const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
{
	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrush(BWMapUI::RoundBrush(Fill, Radius, Outline, OutlineWidth));
	Box->SetPadding(FMargin(0.0f));
	Box->SetHorizontalAlignment(HAlign_Center);
	Box->SetVerticalAlignment(VAlign_Center);
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* BoxSlot = Parent->AddChildToCanvas(Box))
	{
		BoxSlot->SetPosition(Center - Size * 0.5f);
		BoxSlot->SetSize(Size);
	}
	return Box;
}

void UBWMapWidget::AddLine(UCanvasPanel* Parent, const FVector2D& From, const FVector2D& To, float Thickness, const FLinearColor& Color)
{
	const FVector2D Delta = To - From;
	const float Length = Delta.Size();
	if (Length < 1.0f) return;
	UImage* Line = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Line->SetColorAndOpacity(Color);
	Line->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* LineSlot = Parent->AddChildToCanvas(Line))
	{
		const FVector2D Mid = (From + To) * 0.5f;
		LineSlot->SetPosition(Mid - FVector2D(Length * 0.5f, Thickness * 0.5f));
		LineSlot->SetSize(FVector2D(Length, Thickness));
	}
	Line->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Line->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
}

void UBWMapWidget::BuildCodeLayout()
{
	// 바깥: 화면 비율이 달라도 1920×1080 판을 통째로 맞춰 넣는다
	UCanvasPanel* Outer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapOuter"));
	WidgetTree->RootWidget = Outer;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.018f, 0.02f, 0.04f, 1.0f));
	Backdrop->SetVisibility(ESlateVisibility::Visible); // 마우스를 받아 지도 클릭이 이 위젯으로 올라오게
	if (UCanvasPanelSlot* BackSlot = Outer->AddChildToCanvas(Backdrop))
	{
		BackSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackSlot->SetOffsets(FMargin(0.0f));
	}

	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("MapScale"));
	Scale->SetStretch(EStretch::ScaleToFit);
	Scale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* ScaleSlot = Outer->AddChildToCanvas(Scale))
	{
		ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ScaleSlot->SetOffsets(FMargin(0.0f));
	}
	USizeBox* Board = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MapBoard"));
	Board->SetWidthOverride(BWMapUI::CanvasSize.X);
	Board->SetHeightOverride(BWMapUI::CanvasSize.Y);
	Board->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Scale->SetContent(Board);
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Board->SetContent(RootCanvas);

	auto Fill = [](UPanelSlot* PanelSlot)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(0.0f));
		}
	};

	// 가장자리 어둠
	if (UTexture2D* Vignette = LoadObject<UTexture2D>(nullptr, TEXT("/Game/HUD/Texture/UI/T_UI_Vignette.T_UI_Vignette")))
	{
		UImage* VignetteImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		VignetteImage->SetBrushFromTexture(Vignette);
		VignetteImage->SetColorAndOpacity(FLinearColor(1, 1, 1, 0.7f));
		VignetteImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		Fill(RootCanvas->AddChildToCanvas(VignetteImage));
	}

	// 위쪽 HUD: 글자가 배경에 떠 있지 않게 모두 석판 칸 안에 (자세한 건 마우스를 올리면 설명 창)
	// 1줄: [계단 + 1F + 진행 눈금] [덱] ……… [HP] [골드]
	UHorizontalBox* TopLeft = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopLeft"));
	if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(TopLeft)) { S->SetAutoSize(true); S->SetPosition(FVector2D(40.0f, 22.0f)); }
	FloorPlate = MakeChip(nullptr, FMargin(14.0f, 6.0f, 18.0f, 6.0f));
	FloorPlate->SetVisibility(ESlateVisibility::Visible);
	if (UHorizontalBoxSlot* S = TopLeft->AddChildToHorizontalBox(FloorPlate)) S->SetVerticalAlignment(VAlign_Fill);
	{
		UButton* DeckButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DeckButton"));
		BWSkin::StyleButton(DeckButton);
		FButtonStyle Low = DeckButton->GetStyle();
		Low.SetNormalPadding(FMargin(14.0f, 6.0f));
		Low.SetPressedPadding(FMargin(14.0f, 7.0f, 14.0f, 5.0f));
		DeckButton->SetStyle(Low);
		DeckButton->OnClicked.AddDynamic(this, &UBWMapWidget::OnDeckButtonClicked);
		DeckButtonWidget = DeckButton;
		if (UHorizontalBoxSlot* S = TopLeft->AddChildToHorizontalBox(DeckButton)) { S->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f)); S->SetVerticalAlignment(VAlign_Fill); }
	}
	TopRightBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopRight"));
	if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(TopRightBar)) { S->SetAutoSize(true); S->SetPosition(FVector2D(1880.0f, 22.0f)); S->SetAlignment(FVector2D(1.0f, 0.0f)); }

	// 2줄: [증강 칸] [세트 칸] [유물 칸] (칸 수 고정)
	UHorizontalBox* SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SlotRow"));
	if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(SlotRow)) { S->SetAutoSize(true); S->SetPosition(FVector2D(40.0f, 104.0f)); }
	for (int32 Part = 0; Part < 3; ++Part)
	{
		UHorizontalBox* Strip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder* Panel = MakeChip(Strip, FMargin(12.0f, 10.0f));
		if (UHorizontalBoxSlot* S = SlotRow->AddChildToHorizontalBox(Panel)) S->SetPadding(FMargin(Part == 0 ? 0.0f : 14.0f, 0.0f, 0.0f, 0.0f));
		(Part == 0 ? AugmentStrip : Part == 1 ? SetStrip : RelicStrip) = Strip;
	}

	// 3줄: 받아 둔 효과 (있을 때만)
	BonusStrip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BonusStrip"));
	if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(BonusStrip)) { S->SetAutoSize(true); S->SetPosition(FVector2D(40.0f, 204.0f)); }

	// 지도 (상태가 바뀔 때마다 다시 그린다)
	MapLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapLayer"));
	MapLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Fill(RootCanvas->AddChildToCanvas(MapLayer));
	// 차원문 스와이프 연출 (세로로 쓸고 지나간다)
	SwipeOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Swipe"));
	SwipeOverlay->SetBrushColor(FLinearColor::White);
	SwipeOverlay->SetHorizontalAlignment(HAlign_Center);
	SwipeOverlay->SetVerticalAlignment(VAlign_Center);
	SwipeOverlay->SetContent(MakeText(TEXT(""), 54, FLinearColor::White, true));
	SwipeOverlay->SetVisibility(ESlateVisibility::Collapsed);
	Fill(RootCanvas->AddChildToCanvas(SwipeOverlay));

	// 알림 창 (보상·휴식·상점·이벤트): 화면을 어둡게 덮고 가운데 판
	ModalLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Modal"));
	ModalLayer->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.01f, 0.72f));
	ModalLayer->SetHorizontalAlignment(HAlign_Center);
	ModalLayer->SetVerticalAlignment(VAlign_Center);
	ModalLayer->SetVisibility(ESlateVisibility::Collapsed);
	Fill(RootCanvas->AddChildToCanvas(ModalLayer));
	{
		USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PanelSize->SetWidthOverride(860.0f);
		ModalPanelSize = PanelSize;
		PanelSize->SetMaxDesiredHeight(900.0f);
		ModalLayer->SetContent(PanelSize);
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrush(BWMapUI::RoundBrush(FLinearColor(0.035f, 0.04f, 0.07f, 0.98f), 18.0f, BWMapUI::Gold * FLinearColor(1, 1, 1, 0.6f), 2.0f));
		ModalPanel = Panel;
		BWSkin::StylePanel(Panel);
		Panel->SetPadding(FMargin(48.0f, 36.0f, 48.0f, 40.0f));
		PanelSize->SetContent(Panel);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleRow)) TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		ModalTitle = MakeText(TEXT(""), 30, BWMapUI::Gold, true);
		if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(ModalTitle))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
		ModalHeaderRight = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = TitleRow->AddChildToHorizontalBox(ModalHeaderRight)) S->SetVerticalAlignment(VAlign_Center);
		ModalBody = MakeText(TEXT(""), 17, FLinearColor(0.88f, 0.88f, 0.92f));
		ModalBody->SetWrapTextAt(780.0f); // AutoWrap은 첫 프레임 높이를 한 줄로 잡아 아래 버튼과 겹친다
		if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(ModalBody)) BodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
		UScrollBox* OptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ScrollSize->SetMaxDesiredHeight(600.0f);
		ModalScrollSize = ScrollSize;
		ScrollSize->SetContent(OptionScroll);
		Column->AddChildToVerticalBox(ScrollSize);
		ModalOptionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		OptionScroll->AddChild(ModalOptionBox);
	}

	// 설명 창: 전투 HUD와 같은 WBP_ToolTipHUD (맨 위, 마우스를 따라다닌다)
	if (UClass* TipClass = LoadClass<UBWTooltipWidget>(nullptr, TEXT("/Game/HUD/BP/WBP_ToolTipHUD.WBP_ToolTipHUD_C")))
	{
		MapTooltip = CreateWidget<UBWTooltipWidget>(this, TipClass);
		if (MapTooltip)
		{
			MapTooltip->SetVisibility(ESlateVisibility::Collapsed);
			if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(MapTooltip)) { S->SetAutoSize(true); S->SetZOrder(100); }
		}
	}
}

UWidget* UBWMapWidget::MakeIcon(const FString& Name, float Size, const FString& Fallback, const FLinearColor& FallbackColor)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
	if (UTexture2D* Tex = BWSkin::LoadIcon(Name))
	{
		UImage* Pic = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Pic->SetBrushFromTexture(Tex);
		Pic->SetVisibility(ESlateVisibility::HitTestInvisible);
		Box->SetContent(Pic);
	}
	else if (!Fallback.IsEmpty())
	{
		// 그림이 아직 없으면 글자 하나로 (자리 크기는 같게)
		UTextBlock* Glyph = MakeText(Fallback, FMath::RoundToInt(Size * 0.5f), FallbackColor, true);
		Glyph->SetJustification(ETextJustify::Center);
		if (USizeBoxSlot* GlyphSlot = Cast<USizeBoxSlot>(Box->SetContent(Glyph)))
		{
			GlyphSlot->SetHorizontalAlignment(HAlign_Center);
			GlyphSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Box;
}

UBorder* UBWMapWidget::MakeChip(UWidget* Content, const FMargin& ChipPadding)
{
	// 작은 판: 장식 없이 석판색 + 은빛 테두리
	UBorder* Chip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Chip->SetBrush(BWSkin::Plain(FLinearColor(0.1f, 0.12f, 0.15f, 0.94f), FLinearColor(0.68f, 0.7f, 0.74f, 0.55f), 1.5f));
	Chip->SetPadding(ChipPadding);
	Chip->SetVerticalAlignment(VAlign_Center);
	if (Content) Chip->SetContent(Content);
	Chip->SetVisibility(ESlateVisibility::Visible);
	return Chip;
}

UBorder* UBWMapWidget::MakeSlot(UWidget* Content, float Size, const FLinearColor& Edge, float EdgeWidth)
{
	UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Cell->SetBrush(BWSkin::Plain(FLinearColor(0.06f, 0.07f, 0.09f, 1.0f), Edge, EdgeWidth));
	Cell->SetPadding(FMargin(4.0f));
	Cell->SetHorizontalAlignment(HAlign_Center);
	Cell->SetVerticalAlignment(VAlign_Center);
	if (Content) Cell->SetContent(Content);
	Cell->SetVisibility(ESlateVisibility::Visible);
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
	Box->SetContent(Cell);
	return Cell;
}

void UBWMapWidget::UpdateTooltip()
{
	if (!MapTooltip) return;
	FString Title, Desc;
	const FVector2D Abs = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCursorPos() : FVector2D::ZeroVector;
	auto Find = [&](const TArray<FHoverTip>& Tips)
	{
		for (const FHoverTip& Tip : Tips)
		{
			UWidget* W = Tip.Widget.Get();
			if (!W || !W->IsVisible() || !W->GetCachedWidget().IsValid() || !W->GetCachedGeometry().IsUnderLocation(Abs)) continue;
			Title = Tip.Title; Desc = Tip.Desc;
			return true;
		}
		return false;
	};
	if (bModalOpen) Find(ModalTips);
	else if (!NodeTipTitle.IsEmpty()) { Title = NodeTipTitle; Desc = NodeTipDesc; }
	else Find(TopTips);

	if (Title.IsEmpty() && Desc.IsEmpty())
	{
		MapTooltip->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	MapTooltip->SetTooltipTitle(Title);
	MapTooltip->SetTooltipDescription(Desc, 0, 0, 0);
	MapTooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(MapTooltip->Slot))
	{
		// 마우스 오른쪽 아래, 화면 끝에 가까우면 반대쪽으로
		const FVector2D Pos = CursorToCanvas();
		S->SetPosition(Pos);
		S->SetAlignment(FVector2D(Pos.X > BWMapUI::CanvasSize.X * 0.6f ? 1.05f : -0.05f, Pos.Y > BWMapUI::CanvasSize.Y * 0.62f ? 1.05f : -0.08f));
	}
}

FVector2D UBWMapWidget::GetNodeCenter(int32 Step, int32 Row) const
{
	if (Step >= UBWMapRules::BossStep)
	{
		return FVector2D(MapLeft + UBWMapRules::BossStep * ColumnWidth + 40.0f, FirstRowY + RowHeight * (UBWMapRules::NumRows - 1) * 0.5f);
	}
	return FVector2D(MapLeft + Step * ColumnWidth, FirstRowY + Row * RowHeight);
}

void UBWMapWidget::RenderMap()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	// [강력한 조치] UI 전용 모드에서 탈출하여 게임 월드 입력을 허용
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
	SetIsFocusable(true);

	// [수정] 옛 저장(15칸)이나 빈 지도면 지금 막의 지도를 새로 만든다
	if (GI->WorldMapData.Num() != UBWMapRules::BossStep + 1) GI->GenerateWorldMap();

	if (!bUseCodeLayout || !RootCanvas)
	{
		RenderLegacyMap();
		return;
	}
	RebuildMapLayer();
	RefreshTopBar();
	if (!bModalOpen) ShowInfoFor(INDEX_NONE, INDEX_NONE);
}

void UBWMapWidget::RebuildMapLayer()
{
	if (UBWAutoPlayer::IsActive()) return;
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || !MapLayer) return;
	MapLayer->ClearChildren();
	NodeViews.Reset();
	RowViews.Reset();
	HoveredNode = INDEX_NONE;

	const TArray<FMapStep>& Map = GI->WorldMapData;
	const int32 Cur = GI->CurrentStep;
	const int32 CurRow = GI->CurrentDimIdx;
	const float Half = NodeBoxSize * 0.5f;

	// 1. 줄 띠: 차원 색, 지금 줄은 진하게. 왼쪽에 이름·콘셉트·방치 카운터 (방치 빛은 가시거리와 무관하게 항상)
	for (int32 Row = 0; Row < UBWMapRules::NumRows; ++Row)
	{
		const EDimensionType Dim = GI->GetRowDimension(Row);
		const FLinearColor DimColor = BWMapUI::DimColor(Dim);
		const float Y = FirstRowY + Row * RowHeight;
		const bool bHere = Row == CurRow && Cur >= 0 && !GI->IsBossStep(Cur);

		FRowView View;
		View.Band = AddBox(MapLayer, FVector2D(MapLeft + 7.0f * ColumnWidth - 70.0f, Y), FVector2D(15.0f * ColumnWidth + 340.0f, 124.0f),
			DimColor * FLinearColor(0.16f, 0.16f, 0.16f, bHere ? 0.85f : 0.5f), 18.0f, DimColor * FLinearColor(1, 1, 1, bHere ? 0.55f : 0.12f), bHere ? 2.0f : 1.0f);

		const int32 Neglect = GI->NeglectedCounts.FindRef(Dim);
		const int32 Condition = UBWMapRules::GetNeglectCondition(Dim);
		View.GlowLevel = Neglect >= UBWMapRules::GetNeglectCap(Dim) ? 2 : (Neglect >= Condition ? 1 : 0);
		View.Glow = AddBox(MapLayer, FVector2D(140.0f, Y), FVector2D(216.0f, 112.0f), FLinearColor(0, 0, 0, 0.0f), 14.0f, BWMapUI::Gold, 0.0f);

		AddText(MapLayer, UBWMapRules::GetDimensionName(Dim).ToString(), FVector2D(46.0f, Y - 58.0f), 21, DimColor, true);
		AddText(MapLayer, BWMapUI::Concept(Dim), FVector2D(238.0f, Y - 50.0f), 12, BWMapUI::TextDim, false, FVector2D(1.0f, 0.0f));

		// 방치 눈금: 채운 칸 = 지금 방치, 금테 = 보너스가 켜지는 칸, 끝 칸 = 깊은 방치
		const int32 Cap = UBWMapRules::GetNeglectCap(Dim);
		for (int32 i = 0; i < Cap; ++i)
		{
			const bool bFilled = i < Neglect;
			const bool bMark = i + 1 == Condition || i + 1 == Cap;
			AddBox(MapLayer, FVector2D(55.0f + i * 20.0f, Y - 12.0f), FVector2D(16.0f, 16.0f),
				bFilled ? BWMapUI::Gold * FLinearColor(1, 1, 1, View.GlowLevel > 0 ? 0.95f : 0.55f) : FLinearColor(1, 1, 1, 0.07f), 4.0f,
				bMark ? BWMapUI::Gold : FLinearColor(1, 1, 1, 0.2f), bMark ? 2.0f : 1.0f);
		}

		const FBWNeglectBonus Next = UBWMapRules::MakeNeglectBonus(Dim, GI->BonusClaimCounts.FindRef(Dim) + 1, View.GlowLevel == 2);
		FString NeglectLine;
		FLinearColor LineColor = FLinearColor(0.85f, 0.85f, 0.9f);
		if (bHere)
		{
			const int32 Consecutive = GI->ConsecutiveCounts.FindRef(Dim);
			const int32 Level = UBWMapRules::GetOverloadLevel(Consecutive);
			NeglectLine = FString::Printf(TEXT("여기 · 연속 %d턴"), Consecutive);
			if (Level >= 1) NeglectLine = FString::Printf(TEXT("과부하 %s: %s%s"), Level == 2 ? TEXT("II") : TEXT("I"), *BWMapUI::OverloadTwist(Dim), Level == 2 ? TEXT(", 카드 보상 2장") : TEXT(""));
			if (Level >= 1) LineColor = BWMapUI::GateClosed;
		}
		else if (View.GlowLevel > 0)
		{
			NeglectLine = FString::Printf(TEXT("지금 가면%s: %s"), View.GlowLevel == 2 ? TEXT(" (깊은 방치)") : TEXT(""), *BWMapUI::DescribeBonus(Next));
			LineColor = BWMapUI::Gold;
		}
		else
		{
			NeglectLine = FString::Printf(TEXT("%d턴 더 비우면: %s"), Condition - Neglect, *BWMapUI::DescribeBonus(Next));
		}
		View.Neglect = AddText(MapLayer, NeglectLine, FVector2D(46.0f, Y + 2.0f), 11, LineColor, false, FVector2D(0.0f, 0.0f), 205.0f);
		RowViews.Add(View);
	}

	// 칸 번호 줄은 없앴다 (진행은 위쪽 층 판의 눈금으로, 칸 번호는 칸 설명 창에)

	// 2. 길: 같은 줄 직진선, 줄 경계 차원문(열림 ◆ / 닫힘 ×). 어디서 줄을 옮길 수 있는지는 처음부터 모두 보인다 (칸의 내용만 안개에 가린다)
	auto IsMoveFromHere = [&](int32 FromStep, int32 FromRow, int32 ToRow)
	{
		return FromStep == Cur && FromRow == CurRow && GI->CanMoveToNode(FromStep + 1, ToRow);
	};
	for (int32 Step = 0; Step < UBWMapRules::StepsPerAct - 1; ++Step)
	{
		for (int32 Row = 0; Row < UBWMapRules::NumRows; ++Row)
		{
			const bool bHot = IsMoveFromHere(Step, Row, Row);
			AddLine(MapLayer, GetNodeCenter(Step, Row) + FVector2D(Half, 0.0f), GetNodeCenter(Step + 1, Row) - FVector2D(Half, 0.0f),
				bHot ? 4.0f : 2.0f, bHot ? BWMapUI::Gold : FLinearColor(1, 1, 1, Step < Cur ? 0.07f : 0.18f));
		}
		for (int32 Boundary = 0; Boundary < UBWMapRules::NumRows - 1; ++Boundary)
		{
			const bool bOpen = UBWMapRules::IsGateOpen(Map, Step, Boundary, Boundary + 1);
			const FVector2D Mid = (GetNodeCenter(Step, Boundary) + GetNodeCenter(Step + 1, Boundary + 1)) * 0.5f;
			const float Fade = Step < Cur ? 0.35f : 1.0f;
			if (bOpen)
			{
				const bool bHotDown = IsMoveFromHere(Step, Boundary, Boundary + 1);
				const bool bHotUp = IsMoveFromHere(Step, Boundary + 1, Boundary);
				AddLine(MapLayer, GetNodeCenter(Step, Boundary) + FVector2D(Half * 0.7f, Half * 0.7f), GetNodeCenter(Step + 1, Boundary + 1) - FVector2D(Half * 0.7f, Half * 0.7f),
					bHotDown ? 4.0f : 2.0f, bHotDown ? BWMapUI::Gold : BWMapUI::GateOpen * FLinearColor(1, 1, 1, 0.28f * Fade));
				AddLine(MapLayer, GetNodeCenter(Step, Boundary + 1) + FVector2D(Half * 0.7f, -Half * 0.7f), GetNodeCenter(Step + 1, Boundary) - FVector2D(Half * 0.7f, -Half * 0.7f),
					bHotUp ? 4.0f : 2.0f, bHotUp ? BWMapUI::Gold : BWMapUI::GateOpen * FLinearColor(1, 1, 1, 0.28f * Fade));
				UBorder* Diamond = AddBox(MapLayer, Mid, FVector2D(15.0f, 15.0f), BWMapUI::GateOpen * FLinearColor(1, 1, 1, 0.9f * Fade), 3.0f);
				Diamond->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				Diamond->SetRenderTransformAngle(45.0f);
			}
			else
			{
				AddText(MapLayer, TEXT("×"), Mid, 26, BWMapUI::GateClosed * FLinearColor(1, 1, 1, Fade), true, FVector2D(0.5f, 0.5f));
			}
		}
	}
	// 마지막 칸(15칸) → 보스 합류선
	{
		for (int32 Row = 0; Row < UBWMapRules::NumRows; ++Row)
		{
			const bool bHot = Cur == UBWMapRules::RestStep && Row == CurRow;
			AddLine(MapLayer, GetNodeCenter(UBWMapRules::RestStep, Row) + FVector2D(Half, 0.0f), GetNodeCenter(UBWMapRules::BossStep, 0) - FVector2D(BossBoxSize * 0.45f, 0.0f),
				bHot ? 4.0f : 2.0f, bHot ? BWMapUI::Gold : FLinearColor(1.0f, 0.3f, 0.25f, 0.3f));
		}
	}

	// 3. 노드
	for (int32 Step = 0; Step < Map.Num(); ++Step)
	{
		for (int32 Row = 0; Row < Map[Step].Nodes.Num(); ++Row)
		{
			const FMapNode& Node = Map[Step].Nodes[Row];
			const bool bBoss = GI->IsBossStep(Step);
			bool bVisible = true, bFogged = false;
			GI->GetNodeVisibilityState(Step, Node.NodeType, bVisible, bFogged);

			FNodeView View;
			View.Step = Step;
			View.Row = Row;
			View.Center = GetNodeCenter(Step, Row);
			View.Size = bBoss ? BossBoxSize : NodeBoxSize;
			View.bSelectable = !bFogged && GI->CanMoveToNode(Step, Row);
			View.bCurrent = Step == Cur && (bBoss || Row == CurRow);

			const FLinearColor TypeColor = BWMapUI::NodeColor(Node.NodeType);
			FString Label = UBWMapRules::GetNodeTypeName(Node.NodeType).ToString();
			FLinearColor LabelColor = FLinearColor::White;
			if (bFogged)
			{
				View.Fill = FLinearColor(0.06f, 0.06f, 0.08f, 0.9f);
				View.Outline = FLinearColor(1, 1, 1, 0.08f);
				Label = TEXT("?");
				LabelColor = FLinearColor(1, 1, 1, 0.25f);
			}
			else if (Step < Cur && !Node.bIsVisited)
			{
				View.Fill = TypeColor * FLinearColor(0.12f, 0.12f, 0.12f, 0.5f);
				View.Outline = TypeColor * FLinearColor(1, 1, 1, 0.12f);
				LabelColor = FLinearColor(1, 1, 1, 0.25f);
			}
			else if (Node.bIsVisited && !View.bCurrent)
			{
				View.Fill = FLinearColor(0.2f, 0.2f, 0.22f, 0.9f);
				View.Outline = FLinearColor(1, 1, 1, 0.35f);
				LabelColor = FLinearColor(1, 1, 1, 0.55f);
			}
			else
			{
				View.Fill = TypeColor * FLinearColor(0.32f, 0.32f, 0.32f, 0.95f);
				View.Outline = TypeColor * FLinearColor(1, 1, 1, View.bSelectable ? 1.0f : 0.55f);
			}
			if (View.bCurrent) View.Outline = FLinearColor::White;

			View.Box = AddBox(MapLayer, View.Center, FVector2D(View.Size, View.Size), View.Fill, View.Size * 0.24f, View.Outline, View.bCurrent || View.bSelectable ? 3.0f : 2.0f);
			// 칸 종류는 아이콘으로 (무엇인지는 마우스를 올리면 설명 창). 그림이 없으면 예전 글자
			{
				const FString IconName = bFogged ? FString(TEXT("Mark_Fog")) : BWMapUI::NodeIconName(Node.NodeType);
				UWidget* Icon = MakeIcon(IconName, View.Size * (bBoss ? 0.7f : 0.74f), Label, LabelColor);
				// 안개 칸은 흐리게만 (너무 흐리면 안 보인다), 지난 칸은 더 흐리게
				Icon->SetRenderOpacity(bFogged ? 0.5f : FMath::Max(LabelColor.A, 0.3f));
				View.Box->SetContent(Icon);
			}
			// 지금 있는 칸: 위에 흰 마름모 표시
			if (View.bCurrent)
			{
				UBorder* Here = AddBox(MapLayer, View.Center - FVector2D(0.0f, View.Size * 0.5f + 14.0f), FVector2D(13.0f, 13.0f), FLinearColor::White, 2.0f, FLinearColor(0, 0, 0, 0.6f), 1.0f);
				Here->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				Here->SetRenderTransformAngle(45.0f);
			}
			// 갈 수 있는 칸: 도착하면 보너스를 받거나 과부하가 걸리는지 표시
			if (View.bSelectable && !bBoss)
			{
				const EDimensionType RowDim = GI->GetRowDimension(Row);
				const bool bSameRow = Cur >= 0 && Row == CurRow;
				const int32 Level = UBWMapRules::GetOverloadLevel(bSameRow ? GI->ConsecutiveCounts.FindRef(RowDim) + 1 : 1);
				// 도착하면 받는 보너스 / 걸리는 과부하: 칸 오른쪽 아래 작은 표시 (자세한 건 칸 설명 창)
				const bool bBonus = GI->NeglectedCounts.FindRef(RowDim) >= UBWMapRules::GetNeglectCondition(RowDim);
				if (bBonus || Level >= 1)
				{
					UBorder* Mark = AddBox(MapLayer, View.Center + FVector2D(View.Size * 0.42f, View.Size * 0.42f), FVector2D(30.0f, 30.0f),
						FLinearColor(0.06f, 0.07f, 0.09f, 1.0f), 15.0f, bBonus ? BWMapUI::Gold : BWMapUI::GateClosed, 1.5f);
					Mark->SetContent(MakeIcon(bBonus ? TEXT("Mark_Bonus") : TEXT("Mark_Overload"), 22.0f, bBonus ? TEXT("★") : TEXT("!"), bBonus ? BWMapUI::Gold : BWMapUI::GateClosed));
				}
			}
			NodeViews.Add(View);
		}
	}
}

void UBWMapWidget::RefreshTopBar()
{
	if (UBWAutoPlayer::IsActive()) return;
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || !FloorPlate) return;
	TopTips.Reset();
	auto AddTip = [this](UWidget* W, const FString& Title, const FString& Desc) { TopTips.Add({ W, Title, Desc }); };
	auto Row = [this]() { return WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); };
	auto Put = [](UHorizontalBox* Box, UWidget* W, float Left = 0.0f)
	{
		if (UHorizontalBoxSlot* S = Box->AddChildToHorizontalBox(W)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(Left, 0.0f, 0.0f, 0.0f)); }
	};

	// 층: 계단 + 1F + 진행 눈금 (15칸 + 보스)
	{
		UHorizontalBox* Line = Row();
		Put(Line, MakeIcon(TEXT("HUD_Floor"), 40.0f, TEXT("▲"), FLinearColor::White));
		Put(Line, MakeText(FString::Printf(TEXT("%dF"), GI->CurrentStage), 30, FLinearColor::White, true), 8.0f);
		UHorizontalBox* Pips = Row();
		for (int32 Step = 0; Step <= UBWMapRules::BossStep; ++Step)
		{
			const bool bBossPip = Step == UBWMapRules::BossStep;
			const bool bDone = Step < GI->CurrentStep, bHere = Step == GI->CurrentStep;
			EMapNodeType Fixed;
			const bool bFixed = UBWMapRules::GetFixedNodeType(Step, Fixed);
			const float Size = bBossPip ? 16.0f : 11.0f;
			USizeBox* PipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			PipBox->SetWidthOverride(Size);
			PipBox->SetHeightOverride(Size);
			UBorder* Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			const FLinearColor Fill = bHere ? FLinearColor::White : bDone ? BWMapUI::Gold * FLinearColor(1, 1, 1, 0.55f) : FLinearColor(1, 1, 1, 0.06f);
			const FLinearColor Edge = bBossPip ? BWMapUI::GateClosed : bFixed ? BWMapUI::Gold : FLinearColor(1, 1, 1, bHere ? 1.0f : 0.3f);
			Pip->SetBrush(BWMapUI::RoundBrush(Fill, Size * 0.5f, Edge, 1.5f));
			PipBox->SetContent(Pip);
			Put(Pips, PipBox, Step == 0 ? 0.0f : 5.0f);
		}
		Put(Line, Pips, 18.0f);
		FloorPlate->SetContent(Line);

		FString Desc = FString::Printf(TEXT("%d층 / %d층\n"), GI->CurrentStage, UBWMapRules::NumActs);
		if (GI->CurrentStep < 0) Desc += TEXT("아직 출발 전 · 1칸은 모든 구역이 일반 전투이고, 고른 구역이 첫 전투의 기믹이 됩니다");
		else if (GI->IsBossStep(GI->CurrentStep)) Desc += FString::Printf(TEXT("보스 · %s 기믹"), *UBWMapRules::GetDimensionName(GI->GetCurrentDimension()).ToString());
		else Desc += FString::Printf(TEXT("%d칸 / %d칸 · 보스까지 %d칸 · 지금 %s"), GI->CurrentStep + 1, UBWMapRules::StepsPerAct,
			UBWMapRules::BossStep - GI->CurrentStep, *UBWMapRules::GetDimensionName(GI->GetCurrentDimension()).ToString());
		Desc += TEXT("\n금테 눈금 = 고정 칸(보물·휴식), 붉은 눈금 = 보스");
		AddTip(FloorPlate, TEXT("층 · 진행"), Desc);
	}

	// 덱 버튼: 카드 아이콘 + 장수
	const int32 DeckCount = GI->RunDeck.Num() > 0 ? GI->RunDeck.Num() : GI->PlayerDeckCount;
	if (DeckButtonWidget)
	{
		UHorizontalBox* Line = Row();
		Put(Line, MakeIcon(TEXT("HUD_Deck"), 34.0f, TEXT("▤"), FLinearColor::White));
		USizeBox* NumBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NumBox->SetMinDesiredWidth(44.0f); // 세 자리까지 자리 고정
		NumBox->SetContent(MakeText(FString::FromInt(DeckCount), 24, FLinearColor::White, true));
		Put(Line, NumBox, 8.0f);
		DeckButtonWidget->SetContent(Line);
		AddTip(DeckButtonWidget, FString::Printf(TEXT("덱 %d장"), DeckCount), TEXT("누르면 덱을 보고, 지도에서 쓸 수 있는 카드를 씁니다"));
	}

	// HP · 골드 (네 자리까지 들어가게 폭 고정)
	if (TopRightBar)
	{
		TopRightBar->ClearChildren();
		auto Stat = [&](const TCHAR* Icon, const TCHAR* Glyph, const FLinearColor& GlyphColor, const FString& Value, float Width, const FString& TipTitle, const FString& TipDesc, float Left)
		{
			UHorizontalBox* Line = Row();
			Put(Line, MakeIcon(Icon, 34.0f, Glyph, GlyphColor));
			USizeBox* NumBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			NumBox->SetWidthOverride(Width);
			UTextBlock* Num = MakeText(Value, 24, FLinearColor::White, true);
			Num->SetJustification(ETextJustify::Right);
			NumBox->SetContent(Num);
			Put(Line, NumBox, 10.0f);
			UBorder* Chip = MakeChip(Line, FMargin(14.0f, 6.0f, 16.0f, 6.0f));
			Put(TopRightBar, Chip, Left);
			AddTip(Chip, TipTitle, TipDesc);
		};
		Stat(TEXT("HUD_HP"), TEXT("♥"), FLinearColor(0.95f, 0.35f, 0.38f), FString::Printf(TEXT("%d / %d"), GI->PlayerCurrentHP, GI->PlayerMaxHP), 150.0f,
			TEXT("체력"), FString::Printf(TEXT("HP %d / %d"), GI->PlayerCurrentHP, GI->PlayerMaxHP), 0.0f);
		Stat(TEXT("HUD_Gold"), TEXT("●"), BWMapUI::Gold, FString::FromInt(GI->CurrentGold), 70.0f,
			TEXT("골드"), FString::Printf(TEXT("%d골드 · 상점에서 카드를 사거나 지웁니다"), GI->CurrentGold), 12.0f);
	}

	RefreshAugmentStrip();

	// 받아 둔 효과: 효과마다 작은 칸
	if (BonusStrip)
	{
		BonusStrip->ClearChildren();
		TArray<FString> Bonuses;
		const FBWMapBonusState& B = GI->MapBonus;
		if (B.UpgradedCardChoices > 0) Bonuses.Add(FString::Printf(TEXT("다음 카드 보상 강화 %d장"), B.UpgradedCardChoices));
		if (B.VictoryHealFraction > 0.0f) Bonuses.Add(FString::Printf(TEXT("승리 시 HP %d%% 회복"), FMath::RoundToInt(B.VictoryHealFraction * 100.0f)));
		if (B.NextBattleDamageBonus > 0.0f) Bonuses.Add(FString::Printf(TEXT("다음 전투 피해 +%d%%"), FMath::RoundToInt(B.NextBattleDamageBonus * 100.0f)));
		if (B.EquipmentGrade > 0) Bonuses.Add(B.EquipmentGrade >= 2 ? TEXT("다음 전투 전설 유물") : TEXT("다음 전투 희귀 유물"));
		if (GI->bRiftJumpReady) Bonuses.Add(TEXT("균열: 이번에 두 구역 점프 가능"));
		for (int32 i = 0; i < Bonuses.Num(); ++i)
		{
			UHorizontalBox* Line = Row();
			Put(Line, MakeIcon(TEXT("Mark_Bonus"), 24.0f, TEXT("★"), BWMapUI::Gold));
			Put(Line, MakeText(Bonuses[i], 15, BWMapUI::Gold, true), 8.0f);
			UBorder* Chip = MakeChip(Line, FMargin(10.0f, 4.0f, 14.0f, 4.0f));
			Put(BonusStrip, Chip, i == 0 ? 0.0f : 10.0f);
			AddTip(Chip, TEXT("받아 둔 효과"), Bonuses[i]);
		}
	}
}

void UBWMapWidget::RefreshAugmentStrip()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || !AugmentStrip || !SetStrip || !RelicStrip) return;
	constexpr float Cell = 52.0f, Gap = 6.0f;
	const FLinearColor EmptyEdge(1, 1, 1, 0.1f);
	auto Header = [this](UHorizontalBox* Strip, const TCHAR* Icon, const TCHAR* Glyph, const FString& TipTitle, const FString& TipDesc)
	{
		Strip->ClearChildren();
		UWidget* W = MakeIcon(Icon, 38.0f, Glyph, FLinearColor::White);
		W->SetVisibility(ESlateVisibility::Visible);
		if (UHorizontalBoxSlot* S = Strip->AddChildToHorizontalBox(W)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f)); }
		TopTips.Add({ W, TipTitle, TipDesc });
	};
	auto AddCell = [&](UHorizontalBox* Strip, UWidget* Content, const FLinearColor& Edge, float EdgeWidth) -> UBorder*
	{
		UBorder* C = MakeSlot(Content, Cell, Edge, EdgeWidth);
		if (UHorizontalBoxSlot* S = Strip->AddChildToHorizontalBox(C->GetParent())) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.0f, 0.0f, Gap, 0.0f)); }
		return C;
	};
	auto MoreText = [this](int32 N) { return MakeText(FString::Printf(TEXT("+%d"), N), 18, FLinearColor::White, true); };

	// 증강: 18칸 고정, 넘치면 마지막 칸 +N
	constexpr int32 AugSlots = 18;
	Header(AugmentStrip, TEXT("HUD_Augment"), TEXT("◆"), FString::Printf(TEXT("증강 %d개"), GI->OwnedAugments.Num()), TEXT("칸마다 마우스를 올리면 효과가 보입니다"));
	TArray<TPair<FName, FBWAugmentData>> Owned;
	for (const FName& RowName : GI->OwnedAugments)
	{
		FBWAugmentData Data;
		if (GI->GetAugmentData(RowName, Data)) Owned.Add({ RowName, Data });
	}
	const bool bAugOverflow = Owned.Num() > AugSlots;
	const int32 AugShown = bAugOverflow ? AugSlots - 1 : Owned.Num();
	for (int32 i = 0; i < AugSlots; ++i)
	{
		if (i < AugShown)
		{
			const FBWAugmentData& Augment = Owned[i].Value;
			const FLinearColor Tint = BWSkin::AugmentAccent((uint8)Augment.Kind);
			UWidget* Pic;
			if (UTexture2D* Tex = BWSkin::LoadAugmentIcon(Owned[i].Key, Augment.Icon))
			{
				UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Img->SetBrushFromTexture(Tex);
				Img->SetDesiredSizeOverride(FVector2D(Cell - 10.0f));
				Img->SetVisibility(ESlateVisibility::HitTestInvisible);
				Pic = Img;
			}
			else Pic = MakeText(Augment.AugmentName.Left(1), 22, Tint, true);
			UBorder* C = AddCell(AugmentStrip, Pic, Tint, Augment.Set != EBWAugmentSet::None ? 2.5f : 1.5f);
			FString Desc = Augment.Description;
			if (!Augment.CostText.IsEmpty()) Desc += FString::Printf(TEXT("\n대가: %s"), *Augment.CostText);
			if (Augment.Set != EBWAugmentSet::None) Desc += FString::Printf(TEXT("\n세트: %s"), *UBWAugmentRules::GetSetName(Augment.Set));
			TopTips.Add({ C, FString::Printf(TEXT("%s · %s"), *Augment.AugmentName, *UBWAugmentRules::GetKindName(Augment.Kind)), Desc });
		}
		else if (i == AugSlots - 1 && bAugOverflow)
		{
			UBorder* C = AddCell(AugmentStrip, MoreText(Owned.Num() - AugShown), FLinearColor(0.68f, 0.7f, 0.74f, 0.85f), 1.5f);
			TArray<FString> Rest;
			for (int32 k = AugShown; k < Owned.Num(); ++k) Rest.Add(Owned[k].Value.AugmentName);
			TopTips.Add({ C, FString::Printf(TEXT("증강 %d개 더"), Owned.Num() - AugShown), FString::Join(Rest, TEXT(" · ")) });
		}
		else AddCell(AugmentStrip, nullptr, EmptyEdge, 1.0f);
	}

	// 세트: 모은 세트마다 아이콘 + 모은 개수 (6칸 고정)
	constexpr int32 SetSlots = 6;
	Header(SetStrip, TEXT("HUD_Set"), TEXT("✦"), TEXT("세트"), TEXT("같은 세트 증강을 2·3·4개 모으면 효과가 켜집니다. 4개면 기적"));
	const FBWAugmentTotals Totals = GI->GetAugmentTotals();
	TArray<int32> Sets;
	for (int32 i = 1; i < Totals.SetCounts.Num(); ++i) if (Totals.SetCounts[i] > 0) Sets.Add(i);
	Sets.Sort([&Totals](int32 A, int32 B) { return Totals.SetCounts[A] > Totals.SetCounts[B]; });
	const bool bSetOverflow = Sets.Num() > SetSlots;
	const int32 SetShown = bSetOverflow ? SetSlots - 1 : Sets.Num();
	for (int32 i = 0; i < SetSlots; ++i)
	{
		if (i < SetShown)
		{
			const EBWAugmentSet Set = (EBWAugmentSet)Sets[i];
			const int32 Count = Totals.SetCounts[Sets[i]];
			// 아이콘 위 오른쪽 아래에 모은 개수
			UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			if (UOverlaySlot* S = Stack->AddChildToOverlay(MakeIcon(BWSkin::SetIconName((uint8)Set), Cell - 12.0f, UBWAugmentRules::GetSetName(Set).Left(1), BWMapUI::Gold)))
			{
				S->SetHorizontalAlignment(HAlign_Center);
				S->SetVerticalAlignment(VAlign_Center);
			}
			UBorder* Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Badge->SetBrush(BWMapUI::RoundBrush(Count >= 4 ? BWMapUI::Gold : FLinearColor(0.1f, 0.12f, 0.15f, 1.0f), 7.0f, BWMapUI::Gold, 1.0f));
			Badge->SetPadding(FMargin(4.0f, 0.0f));
			Badge->SetContent(MakeText(FString::FromInt(Count), 12, Count >= 4 ? FLinearColor::Black : FLinearColor::White, true));
			if (UOverlaySlot* S = Stack->AddChildToOverlay(Badge))
			{
				S->SetHorizontalAlignment(HAlign_Right);
				S->SetVerticalAlignment(VAlign_Bottom);
				S->SetPadding(FMargin(0.0f, 0.0f, -4.0f, -4.0f));
			}
			UBorder* C = AddCell(SetStrip, Stack, Count >= 2 ? BWMapUI::Gold : FLinearColor(0.68f, 0.7f, 0.74f, 0.6f), Count >= 2 ? 2.0f : 1.2f);
			TopTips.Add({ C, FString::Printf(TEXT("%s 세트 (%d개)"), *UBWAugmentRules::GetSetName(Set), Count), UBWAugmentRules::DescribeSet(Set, Count, GI->SelectedClass) });
		}
		else if (i == SetSlots - 1 && bSetOverflow)
		{
			UBorder* C = AddCell(SetStrip, MoreText(Sets.Num() - SetShown), FLinearColor(0.68f, 0.7f, 0.74f, 0.85f), 1.5f);
			TArray<FString> Rest;
			for (int32 k = SetShown; k < Sets.Num(); ++k) Rest.Add(FString::Printf(TEXT("%s %d"), *UBWAugmentRules::GetSetName((EBWAugmentSet)Sets[k]), Totals.SetCounts[Sets[k]]));
			TopTips.Add({ C, FString::Printf(TEXT("세트 %d개 더"), Sets.Num() - SetShown), FString::Join(Rest, TEXT(" · ")) });
		}
		else AddCell(SetStrip, nullptr, EmptyEdge, 1.0f);
	}

	// 유물: 아직 시스템이 없어 빈 칸 3개
	Header(RelicStrip, TEXT("HUD_Relic"), TEXT("◎"), TEXT("유물"), TEXT("아직 가진 유물이 없습니다 (유물은 준비 중)"));
	for (int32 i = 0; i < 3; ++i) AddCell(RelicStrip, nullptr, EmptyEdge, 1.0f);
}

FVector2D UBWMapWidget::CursorToCanvas() const
{
	if (!RootCanvas || !FSlateApplication::IsInitialized()) return FVector2D(-1.0f, -1.0f);
	return RootCanvas->GetCachedGeometry().AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
}

int32 UBWMapWidget::FindNodeAt(const FVector2D& CanvasPos) const
{
	for (int32 i = 0; i < NodeViews.Num(); ++i)
	{
		const FVector2D D = CanvasPos - NodeViews[i].Center;
		const float Reach = NodeViews[i].Size * 0.5f + 6.0f;
		if (FMath::Abs(D.X) <= Reach && FMath::Abs(D.Y) <= Reach) return i;
	}
	return INDEX_NONE;
}

void UBWMapWidget::UpdateHover(const FVector2D& CanvasPos)
{
	const int32 Found = FindNodeAt(CanvasPos);
	if (Found == HoveredNode) return;
	HoveredNode = Found;
	if (NodeViews.IsValidIndex(Found)) ShowInfoFor(NodeViews[Found].Step, NodeViews[Found].Row);
	else ShowInfoFor(INDEX_NONE, INDEX_NONE);
}

void UBWMapWidget::ShowInfoFor(int32 Step, int32 Row)
{
	if (UBWAutoPlayer::IsActive()) return;
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	NodeTipTitle.Reset();
	NodeTipDesc.Reset();
	if (!GI) return;
	// 칸에 마우스를 올렸을 때만 설명 창 (평소엔 안내 문구 없음)
	if (!GI->WorldMapData.IsValidIndex(Step) || !GI->WorldMapData[Step].Nodes.IsValidIndex(Row)) return;

	const FMapNode& Node = GI->WorldMapData[Step].Nodes[Row];
	const bool bBoss = GI->IsBossStep(Step);
	const EDimensionType Dim = bBoss ? GI->GetCurrentDimension() : GI->GetRowDimension(Row);
	bool bVisible = true, bFogged = false;
	GI->GetNodeVisibilityState(Step, Node.NodeType, bVisible, bFogged);

	const FString DimName = UBWMapRules::GetDimensionName(Dim).ToString();
	const FString HeadLine = bBoss ? FString::Printf(TEXT("보스 — 네 구역 합류 (%s 기믹)"), *DimName)
		: FString::Printf(TEXT("%s · %s · %d칸"), bFogged ? TEXT("안개 속 칸") : *UBWMapRules::GetNodeTypeName(Node.NodeType).ToString(), *DimName, Step + 1);

	TArray<FString> Lines;
	Lines.Add(HeadLine);
	Lines.Add(bFogged ? FString(TEXT("가까이 가야 무엇이 있는지 보입니다")) : BWMapUI::NodeHelp(Node.NodeType));

	const bool bSelectable = !bFogged && GI->CanMoveToNode(Step, Row);
	if (bSelectable)
	{
		TArray<FString> Arrive;
		const bool bFirst = GI->CurrentStep < 0;
		const bool bSameRow = !bFirst && (bBoss || Row == GI->CurrentDimIdx);
		if (!bFirst && !bSameRow) Arrive.Add(FMath::Abs(Row - GI->CurrentDimIdx) == 2 ? TEXT("균열로 두 구역 점프") : TEXT("차원문 통과"));
		if (!bBoss)
		{
			const int32 Neglect = GI->NeglectedCounts.FindRef(Dim);
			const int32 Condition = UBWMapRules::GetNeglectCondition(Dim);
			if (Neglect >= Condition)
			{
				const FBWNeglectBonus Bonus = UBWMapRules::MakeNeglectBonus(Dim, GI->BonusClaimCounts.FindRef(Dim) + 1, Neglect >= UBWMapRules::GetNeglectCap(Dim));
				Arrive.Add(FString::Printf(TEXT("방치 보너스(방치 %d턴): %s"), Neglect, *BWMapUI::DescribeBonus(Bonus)));
			}
			else if (!bSameRow) Arrive.Add(FString::Printf(TEXT("방치 %d/%d — 보너스 없음"), Neglect, Condition));
		}
		const int32 Consecutive = bSameRow ? GI->ConsecutiveCounts.FindRef(Dim) + 1 : 1;
		const int32 Level = UBWMapRules::GetOverloadLevel(Consecutive);
		if (Level >= 1) Arrive.Add(FString::Printf(TEXT("연속 %d턴 → 과부하 %s: %s%s"), Consecutive, Level == 2 ? TEXT("II") : TEXT("I"), *BWMapUI::OverloadTwist(Dim), Level == 2 ? TEXT(" + 카드 보상 3→2장") : TEXT("")));
		else Arrive.Add(FString::Printf(TEXT("연속 %d턴 (안정)"), Consecutive));
		Lines.Append(Arrive);
	}
	else if (!bFogged && Step == GI->CurrentStep + 1 && GI->CurrentStep >= 0)
	{
		const int32 Distance = FMath::Abs(Row - GI->CurrentDimIdx);
		Lines.Add(Distance == 1 ? TEXT("갈 수 없음: 이 경계의 차원문이 닫혀 있습니다") : TEXT("갈 수 없음: 한 번에 한 구역만 옮길 수 있습니다 (균열을 밟으면 두 구역)"));
	}
	NodeTipTitle = Lines[0];
	Lines.RemoveAt(0);
	NodeTipDesc = FString::Join(Lines, TEXT("\n"));
}

// ---------------------------------------------------------------------------------------------
// 매 프레임: 빛 맥박, 마우스 올림, 스와이프, 증강 선택 끝
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bUseCodeLayout || !RootCanvas) return;
	Time += InDeltaTime;
	if (ModalLayer && bModalOpen) ModalLayer->SetRenderOpacity(FMath::Clamp((Time - ModalOpenedAt) / 0.25f, 0.0f, 1.0f));
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 4.0f);

	for (int32 i = 0; i < NodeViews.Num(); ++i)
	{
		FNodeView& View = NodeViews[i];
		if (!View.Box || !(View.bSelectable || View.bCurrent)) continue;
		const bool bHover = i == HoveredNode;
		const FLinearColor Outline = View.bCurrent ? FLinearColor::White : FMath::Lerp(BWMapUI::Gold * FLinearColor(1, 1, 1, 0.45f), BWMapUI::Gold, bHover ? 1.0f : Pulse);
		View.Box->SetBrush(BWMapUI::RoundBrush(bHover ? View.Fill * 1.6f : View.Fill, View.Size * 0.24f, Outline, bHover ? 4.0f : 3.0f));
		View.Box->SetRenderScale(FVector2D(bHover ? 1.1f : 1.0f));
	}
	for (FRowView& Row : RowViews)
	{
		if (!Row.Glow || Row.GlowLevel == 0) continue;
		const float Strength = Row.GlowLevel == 2 ? 0.6f + 0.4f * Pulse : 0.25f + 0.3f * Pulse;
		Row.Glow->SetBrush(BWMapUI::RoundBrush(BWMapUI::Gold * FLinearColor(1, 1, 1, Strength * 0.18f), 14.0f, BWMapUI::Gold * FLinearColor(1, 1, 1, Strength), Row.GlowLevel == 2 ? 3.0f : 2.0f));
	}

	if (!bModalOpen && !bBusy) UpdateHover(CursorToCanvas());
	else { HoveredNode = INDEX_NONE; NodeTipTitle.Reset(); NodeTipDesc.Reset(); }
	UpdateTooltip();

	// 차원문 스와이프: 도착 차원 색이 위에서 아래로 쓸고 지나간다
	if (SwipeElapsed >= 0.0f && SwipeOverlay)
	{
		SwipeElapsed += InDeltaTime;
		const float T = FMath::Clamp(SwipeElapsed / FMath::Max(0.05f, SwipeTime), 0.0f, 1.0f);
		const float Eased = T < 0.5f ? 2.0f * T * T : 1.0f - FMath::Square(-2.0f * T + 2.0f) * 0.5f;
		SwipeOverlay->SetRenderTranslation(FVector2D(0.0f, FMath::Lerp(-BWMapUI::CanvasSize.Y, BWMapUI::CanvasSize.Y, Eased)));
		if (T >= 1.0f)
		{
			SwipeElapsed = -1.0f;
			SwipeOverlay->SetVisibility(ESlateVisibility::Collapsed);
			ShowArrival();
		}
	}

	CheckAugmentClosed();
}

void UBWMapWidget::FinishAutoAugment()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	bAutoAugmentPending = false;
	if (GI) (GI->bActCleared ? GI->AugmentBossPickedAct : GI->AugmentPickedAct) = GI->CurrentStage;
	ProcessPendingFlow();
}

void UBWMapWidget::CheckAugmentClosed()
{
	// 증강 화면이 닫혔으면(골랐으면) 이번 막은 고른 것으로 저장
	if (AugmentWidget && !AugmentWidget->IsInViewport())
	{
		AugmentWidget = nullptr;
		if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance()))
		{
			(GI->bActCleared ? GI->AugmentBossPickedAct : GI->AugmentPickedAct) = GI->CurrentStage;
			GI->SaveRun();
		}
		RefreshTopBar();
		ProcessPendingFlow();
	}
}

FReply UBWMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bUseCodeLayout)
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !bModalOpen && !bBusy && !AugmentWidget)
		{
			const int32 Found = FindNodeAt(CursorToCanvas());
			if (NodeViews.IsValidIndex(Found)) TryMoveTo(NodeViews[Found].Step, NodeViews[Found].Row);
		}
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDraggingMap = true;
		LastMousePos = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UBWMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bUseCodeLayout && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDraggingMap = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UBWMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDraggingMap && MapScrollBox)
	{
		FVector2D CurrentMousePos = InMouseEvent.GetScreenSpacePosition();
		FVector2D Delta = LastMousePos - CurrentMousePos;
		MapScrollBox->SetScrollOffset(MapScrollBox->GetScrollOffset() + Delta.X);
		LastMousePos = CurrentMousePos;
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// ---------------------------------------------------------------------------------------------
// 이동 → 도착 → 노드
// ---------------------------------------------------------------------------------------------

bool UBWMapWidget::TryMoveTo(int32 Step, int32 Row)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || bModalOpen || bBusy) return false;
	bool bVisible = true, bFogged = false;
	if (GI->WorldMapData.IsValidIndex(Step) && GI->WorldMapData[Step].Nodes.IsValidIndex(Row))
	{
		GI->GetNodeVisibilityState(Step, GI->WorldMapData[Step].Nodes[Row].NodeType, bVisible, bFogged);
	}
	if (bFogged || !GI->SelectMapNode(Step, Row)) return false;

	bBusy = true;
	RebuildMapLayer();
	RefreshTopBar();
	if (GI->LastArrival.bChangedRow && RootCanvas && !UBWAutoPlayer::IsActive()) BeginSwipe(GI->LastArrival.Dimension);
	else ShowArrival();
	return true;
}

void UBWMapWidget::BeginSwipe(EDimensionType Dimension)
{
	SwipeColor = BWMapUI::DimColor(Dimension);
	SwipeOverlay->SetBrushColor(SwipeColor * FLinearColor(0.45f, 0.45f, 0.45f, 0.92f));
	if (UTextBlock* Label = Cast<UTextBlock>(SwipeOverlay->GetContent())) Label->SetText(UBWMapRules::GetDimensionName(Dimension));
	SwipeOverlay->SetRenderTranslation(FVector2D(0.0f, -BWMapUI::CanvasSize.Y));
	SwipeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	SwipeElapsed = 0.0f;
}

void UBWMapWidget::ShowArrival()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	const FBWArrivalReport& R = GI->LastArrival;
	const EDimensionType Dim = R.Dimension;

	TArray<FString> Lines;
	if (R.bClaimedBonus)
	{
		Lines.Add(FString::Printf(TEXT("방치 보너스 — 방치 %d턴%s, 효과 %d%%"), R.NeglectCount, R.bDeepNeglect ? TEXT(" (깊은 방치)") : TEXT(""), R.BonusPercent));
		for (const FString& Line : R.BonusLines) Lines.Add(TEXT("  · ") + Line);
	}
	if (R.bOverloadChanged)
	{
		if (R.OverloadLevel == 0) Lines.Add(TEXT("과부하가 풀렸습니다."));
		else if (R.OverloadLevel == 1) Lines.Add(FString::Printf(TEXT("과부하 I (연속 %d턴): %s"), R.Consecutive, *BWMapUI::OverloadTwist(Dim)));
		else Lines.Add(FString::Printf(TEXT("과부하 II (연속 %d턴, 상한): %s + 카드 보상 선택지 3→2장"), R.Consecutive, *BWMapUI::OverloadTwist(Dim)));
	}

	auto Continue = [this]()
	{
		UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance());
		if (G && G->PendingCardRemovals > 0) ShowCardRemoval(0, true, [this](int32) { EnterCurrentNode(); });
		else EnterCurrentNode();
	};

	if (Lines.Num() == 0)
	{
		Continue();
		return;
	}
	TArray<FModalOption> Options;
	Options.Add({ FText::FromString(TEXT("확인")), FText::GetEmpty(), true, Continue });
	OpenModal(FString::Printf(TEXT("%s 도착"), *UBWMapRules::GetDimensionName(Dim).ToString()), FString::Join(Lines, TEXT("\n")), MoveTemp(Options));
}

void UBWMapWidget::EnterCurrentNode()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	const FMapNode& Node = GI->GetCurrentNode();
	const EDimensionType Dim = GI->GetCurrentDimension();

	switch (Node.NodeType)
	{
	case EMapNodeType::Battle:
	case EMapNodeType::Elite:
	case EMapNodeType::Boss:
	{
		// 전투는 저장하지 않고 들어간다 (전투 중 종료하면 마지막 저장인 지도로 돌아온다)
		GI->BeginBattleSnapshot();
		GI->PrepareEncounter(Node.NodeType, Dim);
		bBusy = true;
		if (UBWAutoPlayer::IsActive()) { bAutoBattlePending = true; return; }
		if (InfoTitle) InfoTitle->SetText(FText::FromString(TEXT("전투 준비 중…")));
		UGameplayStatics::OpenLevel(GetWorld(), BattleLevelName);
		return;
	}
	case EMapNodeType::Rest:
		ShowRest();
		return;
	case EMapNodeType::Chest:
		GI->GenerateTreasureRewards();
		ShowRewards(TEXT("보물 상자"));
		return;
	case EMapNodeType::Shop:
		ShopCards = GI->RollCardChoices(GI->ShopCardCount);
		ShopSold.Init(false, ShopCards.Num());
		ShopSaleIndex = ShopCards.Num() > 0 ? FMath::RandRange(0, ShopCards.Num() - 1) : INDEX_NONE;
		ShowShop();
		return;
	case EMapNodeType::Mystery:
		ShowMystery();
		return;
	case EMapNodeType::DimensionAltar:
	{
		GI->UseDimensionAltar();
		TArray<FModalOption> Options;
		Options.Add({ FText::FromString(TEXT("확인")), FText::GetEmpty(), true, [this]() { FinishNode(); } });
		OpenModal(TEXT("차원 제단"), TEXT("제단이 뒤엉킨 차원의 흐름을 가라앉힙니다.\n과부하(연속 체류)와 모든 구역의 보너스 내성이 초기화됐습니다."), MoveTemp(Options));
		return;
	}
	case EMapNodeType::Rift:
	{
		TArray<FModalOption> Options;
		Options.Add({ FText::FromString(TEXT("확인")), FText::GetEmpty(), true, [this]() { FinishNode(); } });
		OpenModal(TEXT("균열"), TEXT("구역 사이에 금이 가 있습니다. 숲에서 사람이 사라진다는 아케니아의 소문이 떠오르네요.\n다음 칸에서는 두 구역 떨어진 곳으로 바로 점프할 수 있습니다 (차원문 개폐와 무관, 다음 칸 한 번)."), MoveTemp(Options));
		return;
	}
	default:
		FinishNode();
		return;
	}
}

void UBWMapWidget::FinishNode()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	bBusy = false;
	if (GI) GI->SaveRun();
	RebuildMapLayer();
	RefreshTopBar();
	ShowInfoFor(INDEX_NONE, INDEX_NONE);
	ProcessPendingFlow();
}

// ---------------------------------------------------------------------------------------------
// 보상 · 카드 고르기 · 막 넘어가기
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::ProcessPendingFlow()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || bModalOpen || bBusy || AugmentWidget || bAutoAugmentPending) return;

	if (GI->PendingStatPoints > 0 && !UBWAutoPlayer::IsActive() && !GI->IsRunCleared())
	{
		ShowStatAllocation();
		return;
	}
	if (GI->PendingRewards.Num() > 0)
	{
		const FString Title = GI->CurrentEncounterType == EMapNodeType::Boss ? TEXT("보스 격파") : GI->CurrentEncounterType == EMapNodeType::Elite ? TEXT("엘리트 처치") : TEXT("B.W 정산 완료");
		bBusy = true;
		ShowRewards(Title);
		return;
	}
	if (GI->PendingCardRemovals > 0)
	{
		bBusy = true;
		ShowCardRemoval(0, true, [this](int32) { FinishNode(); });
		return;
	}
	if (GI->bActCleared)
	{
		// [추가] 증강기획 1-3: 층 보스를 잡으면 증강 하나 (마지막 층 보스는 입탑이 끝나므로 없음)
		if (!GI->IsRunCleared() && GI->AugmentBossPickedAct < GI->CurrentStage)
		{
			OpenAugmentPick();
			if (AugmentWidget || bAutoAugmentPending) return;
		}
		TArray<FModalOption> Options;
		if (GI->IsRunCleared())
		{
			Options.Add({ FText::FromString(TEXT("타이틀로")), FText::GetEmpty(), true, [this]()
			{
				if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->DeleteSavedRun();
				bBusy = true;
				if (UBWAutoPlayer::IsActive()) return;
				UGameplayStatics::OpenLevel(GetWorld(), TitleLevelName);
			} });
			OpenModal(TEXT("탑 정복"), FString::Printf(TEXT("%d층의 보스를 모두 쓰러뜨렸습니다. 어디에서 무력을 쓰더라도 여기보다 많이 벌 곳은 없겠죠. 이번 입탑은 여기까지입니다."), UBWMapRules::NumActs), MoveTemp(Options));
		}
		else
		{
			Options.Add({ FText::FromString(TEXT("다음 층으로")), FText::GetEmpty(), true, [this]()
			{
				if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->AdvanceToNextAct();
				RenderMap();
				ProcessPendingFlow();
			} });
			OpenModal(FString::Printf(TEXT("%d층 돌파"), GI->CurrentStage),
				TEXT("다음 층으로 올라갑니다.\n구역 순서가 새로 섞이고, 방치·연속 체류·보너스 내성과 받아 둔 방치 보너스는 모두 초기화됩니다.\n층을 시작하며 증강을 하나 고릅니다."), MoveTemp(Options));
		}
		return;
	}
	if (GI->AugmentPickedAct < GI->CurrentStage && GI->CurrentStep < 0) OpenAugmentPick();
}

void UBWMapWidget::OpenAugmentPick()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	GI->BeginAugmentPick(GI->CurrentStage, GI->SelectedClass);
	if (GI->AugmentChoices.Num() == 0)
	{
		(GI->bActCleared ? GI->AugmentBossPickedAct : GI->AugmentPickedAct) = GI->CurrentStage; // 표가 비었으면 건너뛴다
		return;
	}
	if (UBWAutoPlayer::IsActive()) { bAutoAugmentPending = true; return; }
	AugmentWidget = CreateWidget<UBWAugmentSelectWidget>(GetOwningPlayer(), UBWAugmentSelectWidget::StaticClass());
	if (AugmentWidget) AugmentWidget->AddToViewport(600);
}

FString UBWMapWidget::DescribeCard(const FBWCardData& Card) const
{
	TArray<FString> Parts;
	if (Card.CardKind == ECardKind::Attack)
	{
		Parts.Add(FString::Printf(TEXT("공격 %d~%d"), Card.SkillMin, Card.SkillMax));
		if (Card.MPCost > 0) Parts.Add(FString::Printf(TEXT("MP %d"), Card.MPCost));
		if (Card.Range > 1) Parts.Add(FString::Printf(TEXT("사거리 %d"), Card.Range));
		if (Card.AppliedStatus != EStatusEffect::None) Parts.Add(BWMapUI::StatusName(Card.AppliedStatus));
		if (Card.bPushesTarget) Parts.Add(TEXT("밀치기"));
	}
	else
	{
		Parts.Add(TEXT("효과"));
		if (Card.MPCost > 0) Parts.Add(FString::Printf(TEXT("MP %d"), Card.MPCost));
		if (Card.EffectHeal > 0) Parts.Add(FString::Printf(TEXT("회복 %d"), Card.EffectHeal));
		if (Card.EffectBlock > 0) Parts.Add(FString::Printf(TEXT("방어도 %d"), Card.EffectBlock));
		if (Card.EffectMove > 0) Parts.Add(FString::Printf(TEXT("이동 +%d"), Card.EffectMove));
		if (Card.EffectMP > 0) Parts.Add(FString::Printf(TEXT("MP +%d"), Card.EffectMP));
		if (Card.EffectDraw > 0) Parts.Add(FString::Printf(TEXT("뽑기 %d"), Card.EffectDraw));
		if (Card.EffectAttackMaxBonus > 0) Parts.Add(FString::Printf(TEXT("공격 최댓값 +%d"), Card.EffectAttackMaxBonus));
		if (Card.SelfStatus != EStatusEffect::None) Parts.Add(FString::Printf(TEXT("%s %d턴"), *BWMapUI::StatusName(Card.SelfStatus), Card.SelfStatusDuration));
	}
	return FString::Join(Parts, TEXT(" · "));
}

void UBWMapWidget::ShowRewards(const FString& Title)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	TArray<FString> Lines;
	int32 CardRewardIndex = INDEX_NONE;
	for (int32 i = 0; i < GI->PendingRewards.Num(); ++i)
	{
		const FRewardItem& Reward = GI->PendingRewards[i];
		if (Reward.Type == ERewardType::CardSelection)
		{
			if (CardRewardIndex == INDEX_NONE) CardRewardIndex = i;
			continue;
		}
		Lines.Add(Reward.Description.IsEmpty() ? FString::Printf(TEXT("· %s"), *Reward.RewardName) : FString::Printf(TEXT("· %s — %s"), *Reward.RewardName, *Reward.Description));
	}

	auto Done = [this]()
	{
		if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->PendingRewards.Empty();
		FinishNode();
	};

	TArray<FModalOption> Options;
	if (CardRewardIndex != INDEX_NONE)
	{
		const FRewardItem& CardReward = GI->PendingRewards[CardRewardIndex];
		Lines.Add(FString::Printf(TEXT("\n%s — 덱에 넣을 카드를 누르세요."), *CardReward.RewardName));
		for (int32 Choice = 0; Choice < CardReward.CardChoices.Num(); ++Choice)
		{
			FBWCardData Card;
			const bool bUpgraded = Choice < CardReward.UpgradedChoices;
			if (!GI->GetCardData(CardReward.CardChoices[Choice], bUpgraded, Card)) continue;
			FModalOption Option{ FText::FromString(bUpgraded ? TEXT("강화") : TEXT("")), FText::FromString(DescribeCard(Card)), true,
				[this, CardRewardIndex, Choice, Done]()
				{
					if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->ChooseRewardCard(CardRewardIndex, Choice);
					Done();
				} };
			Option.bShowCard = true;
			Option.Card = Card;
			Options.Add(MoveTemp(Option));
		}
		Options.Add({ FText::FromString(TEXT("건너뛰기")), FText::FromString(TEXT("카드를 받지 않습니다")), true, Done });
	}
	else
	{
		Options.Add({ FText::FromString(TEXT("확인")), FText::GetEmpty(), true, Done });
	}
	OpenModal(Title, FString::Join(Lines, TEXT("\n")), MoveTemp(Options));
}

void UBWMapWidget::ShowCardRemoval(int32 Count, bool bFromBonus, TFunction<void(int32)> OnDone, int32 RemovedSoFar)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	const int32 Remaining = bFromBonus ? GI->PendingCardRemovals : Count;
	if (Remaining <= 0 || GI->RunDeck.Num() <= 1)
	{
		if (bFromBonus) GI->PendingCardRemovals = 0;
		OnDone(RemovedSoFar);
		return;
	}

	TArray<FModalOption> Options;
	const TArray<FBWRunCard> Deck = GI->RunDeck;
	for (int32 i = 0; i < Deck.Num(); ++i)
	{
		FBWCardData Card;
		if (!GI->GetCardData(Deck[i].Row, Deck[i].bUpgraded, Card)) continue;
		Options.Add(MakeCardOption(Card, Deck[i].bUpgraded ? TEXT("강화") : TEXT(""), true,
			[this, i, Count, bFromBonus, OnDone, RemovedSoFar]()
			{
				UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance());
				if (!G) return;
				const bool bRemoved = G->RemoveRunCard(i);
				if (bFromBonus) G->PendingCardRemovals = FMath::Max(0, G->PendingCardRemovals - 1);
				ShowCardRemoval(Count - 1, bFromBonus, OnDone, RemovedSoFar + (bRemoved ? 1 : 0));
			}));
	}
	Options.Add({ FText::FromString(TEXT("그만두기")), FText::FromString(TEXT("더 지우지 않습니다")), true, [this, bFromBonus, OnDone, RemovedSoFar]()
	{
		if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance()); G && bFromBonus) G->PendingCardRemovals = 0;
		OnDone(RemovedSoFar);
	} });
	OpenModal(FString::Printf(TEXT("카드 삭제 (%d장 남음)"), Remaining),
		bFromBonus ? TEXT("타이가 방치 보너스: 덱에서 뺄 카드를 누르세요.") : TEXT("덱에서 뺄 카드를 누르세요."), MoveTemp(Options));
}

void UBWMapWidget::ShowCardUpgrade(const FString& Title, TFunction<void()> OnDone, TFunction<void()> OnCancel)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	TArray<FModalOption> Options;
	for (int32 i = 0; i < GI->RunDeck.Num(); ++i)
	{
		if (GI->RunDeck[i].bUpgraded) continue;
		FBWCardData Card;
		if (!GI->GetCardData(GI->RunDeck[i].Row, false, Card)) continue;
		Options.Add(MakeCardOption(Card, FString(), true,
			[this, i, OnDone]()
			{
				if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->UpgradeRunCard(i);
				OnDone();
			}));
	}
	Options.Add({ FText::FromString(TEXT("취소")), FText::GetEmpty(), true, OnCancel });
	OpenModal(Title, TEXT("강화할 카드를 누르세요. 오른쪽 위 '강화 미리보기'를 켜면 모든 카드가 강화된 모습으로 보입니다."), MoveTemp(Options));
}

// ---------------------------------------------------------------------------------------------
// 휴식 · 상점 · 이벤트
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::ShowRest()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	bool bCanUpgrade = false;
	for (const FBWRunCard& Entry : GI->RunDeck) bCanUpgrade |= !Entry.bUpgraded;

	TArray<FModalOption> Options;
	Options.Add({ FText::FromString(FString::Printf(TEXT("HP 회복 +%d"), GI->GetRestHealAmount())),
		FText::FromString(FString::Printf(TEXT("최대 HP의 %d%%"), FMath::RoundToInt(GI->RestHealFraction * 100.0f))), true,
		[this]()
		{
			if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->RestHeal();
			FinishNode();
		} });
	Options.Add({ FText::FromString(TEXT("카드 강화")), FText::FromString(TEXT("덱의 카드 한 장을 강화합니다")), bCanUpgrade,
		[this]() { ShowCardUpgrade(TEXT("카드 강화"), [this]() { FinishNode(); }, [this]() { ShowRest(); }); } });
	OpenModal(TEXT("휴식"), FString::Printf(TEXT("모닥불 앞에서 숨을 고릅니다. 탑 안에서 이런 시간은 귀하죠. HP %d / %d\n하나를 골라 주세요."), GI->PlayerCurrentHP, GI->PlayerMaxHP), MoveTemp(Options));
}

void UBWMapWidget::ShowShop()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	TArray<FModalOption> Options;
	if (ShopSold.Num() != ShopCards.Num()) ShopSold.Init(false, ShopCards.Num());
	for (int32 i = 0; i < ShopCards.Num(); ++i)
	{
		FBWCardData Card;
		if (!GI->GetCardData(ShopCards[i], false, Card)) continue;
		const bool bSale = i == ShopSaleIndex;
		const int32 Price = GI->GetShopCardPrice(Card, bSale);
		FModalOption Option = MakeCardOption(Card, FString(), !ShopSold[i] && GI->CurrentGold >= Price,
			[this, i, Price]()
			{
				UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance());
				if (!G || !ShopCards.IsValidIndex(i) || ShopSold[i] || G->CurrentGold < Price) return;
				G->CurrentGold -= Price;
				G->AddRunCard(ShopCards[i]);
				ShopSold[i] = true;
				RefreshTopBar();
				ShowShop();
			});
		Option.Price = Price;
		Option.OriginalPrice = bSale ? GI->GetShopCardPrice(Card, false) : -1;
		Option.bSoldOut = ShopSold[i];
		Options.Add(MoveTemp(Option));
	}
	Options.Add({ FText::FromString(FString::Printf(TEXT("카드 제거  ·  %d골드"), GI->ShopRemovePrice)), FText::FromString(TEXT("덱에서 카드 한 장을 뺍니다")),
		GI->CurrentGold >= GI->ShopRemovePrice && GI->RunDeck.Num() > 1,
		[this]()
		{
			ShowCardRemoval(1, false, [this](int32 Removed)
			{
				UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance());
				if (G && Removed > 0) G->CurrentGold = FMath::Max(0, G->CurrentGold - G->ShopRemovePrice);
				RefreshTopBar();
				ShowShop();
			});
		} });
	Options.Add({ FText::FromString(TEXT("떠나기")), FText::FromString(TEXT("상점을 나갑니다")), true, [this]() { FinishNode(); } });
	OpenModal(TEXT("떠돌이 상인"), TEXT("\"다 입탑자분들이 두고 간 물건입니다. 어디 가서 이 값엔 못 구하실걸요?\"  (붉은 띠 = 반값 · 유물은 준비 중)"), MoveTemp(Options), EModalStyle::Shop);
}

void UBWMapWidget::ShowMystery()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	const EDimensionType Dim = GI->GetCurrentDimension();
	auto LoseHP = [this](int32 Amount)
	{
		if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->PlayerCurrentHP = FMath::Max(1, G->PlayerCurrentHP - Amount);
	};
	bool bCanUpgrade = false;
	for (const FBWRunCard& Entry : GI->RunDeck) bCanUpgrade |= !Entry.bUpgraded;

	// 차원별 이벤트 하나씩 (임시: 차원별 이벤트 풀은 준비 중)
	FString Title, Body;
	TArray<FModalOption> Options;
	switch (Dim)
	{
	case EDimensionType::Patrovas:
		Title = TEXT("뒤틀린 서고");
		Body = TEXT("책장이 제멋대로 다시 쓰이고 있습니다. 남의 연구를 슬쩍하는 건 파트로바스에선 일상이라지만… 손을 넣어 보시겠습니까?");
		Options.Add({ FText::FromString(TEXT("HP 6을 잃고 무작위 카드 1장 강화")), FText::GetEmpty(), bCanUpgrade, [this, LoseHP]()
		{
			UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance());
			if (!G) return;
			LoseHP(6);
			TArray<int32> Candidates;
			for (int32 i = 0; i < G->RunDeck.Num(); ++i) if (!G->RunDeck[i].bUpgraded) Candidates.Add(i);
			if (Candidates.Num() > 0) G->UpgradeRunCard(Candidates[FMath::RandRange(0, Candidates.Num() - 1)]);
			FinishNode();
		} });
		break;
	case EDimensionType::Berend:
		Title = TEXT("투기장의 함성");
		Body = TEXT("구경꾼들이 한 판 붙어 보라며 판돈을 겁니다. 아주 베렌드다운 일이죠. 말리는 사람은 없습니다.");
		Options.Add({ FText::FromString(TEXT("HP 8을 잃고 골드 60")), FText::GetEmpty(), true, [this, LoseHP]()
		{
			LoseHP(8);
			if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->CurrentGold += 60;
			FinishNode();
		} });
		break;
	case EDimensionType::Geleon:
		Title = TEXT("멈춘 공방");
		Body = TEXT("주인이 사라진 공방에서 연마기만 돌아가고 있습니다. 겔레온의 장인이 자리를 비울 리가 없는데 말이죠.");
		Options.Add({ FText::FromString(TEXT("골드 30을 내고 카드 1장 강화 (직접 고름)")), FText::GetEmpty(), bCanUpgrade && GI->CurrentGold >= 30, [this]()
		{
			ShowCardUpgrade(TEXT("멈춘 공방"), [this]()
			{
				if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->CurrentGold = FMath::Max(0, G->CurrentGold - 30);
				FinishNode();
			}, [this]() { ShowMystery(); });
		} });
		break;
	case EDimensionType::Taiga:
		Title = TEXT("그림자 거래");
		Body = TEXT("얼굴을 가린 자가 짐을 덜어 주겠다고 속삭입니다. 허가증은 보여 주지 않네요. 타이가에서라면 그리 좋은 징조는 아닙니다.");
		Options.Add({ FText::FromString(TEXT("HP 5를 잃고 카드 1장 삭제")), FText::GetEmpty(), GI->RunDeck.Num() > 1, [this, LoseHP]()
		{
			ShowCardRemoval(1, false, [this, LoseHP](int32 Removed)
			{
				if (Removed > 0) LoseHP(5);
				FinishNode();
			});
		} });
		break;
	default:
		Title = TEXT("떨어진 주머니");
		Body = TEXT("길가에 주머니가 떨어져 있습니다. 먼저 올라간 입탑자의 것이겠죠. B.W보다 먼저 주워도 괜찮을까요?");
		Options.Add({ FText::FromString(TEXT("줍습니다 (골드 25)")), FText::GetEmpty(), true, [this]()
		{
			if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->CurrentGold += 25;
			FinishNode();
		} });
		break;
	}
	Options.Add({ FText::FromString(TEXT("지나갑니다")), FText::GetEmpty(), true, [this]() { FinishNode(); } });
	OpenModal(Title, Body + TEXT("\n(임시 이벤트 — 구역별 이벤트 풀은 준비 중)"), MoveTemp(Options));
}

void UBWMapWidget::ShowStatAllocation()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	if (GI->PendingStatPoints <= 0)
	{
		ModalLockUntil = Time + 0.8f;
		FinishNode();
		return;
	}
	const FBWDefenseStats Defense = GI->GetClassCombatProfile(GI->SelectedClass).Defense;
	const FString DefenseEffect = Defense.Type == EDefenseType::Evade ? FString(TEXT("포인트마다 회피 +1%"))
		: Defense.Type == EDefenseType::MagicShield ? FString(TEXT("3포인트마다 가드 상한 +1"))
		: FString(TEXT("3포인트마다 방어 +1"));
	const TArray<TPair<FString, FString>> Rows = {
		{ FString::Printf(TEXT("PHA +1  (지금 %d)"), GI->PlayerPHA), TEXT("물리 공격 강화") },
		{ FString::Printf(TEXT("TEC +1  (지금 %d)"), GI->PlayerTEC), TEXT("기술 공격 강화, 치명타") },
		{ FString::Printf(TEXT("INT +1  (지금 %d)"), GI->PlayerINT), TEXT("마법 공격 강화") },
		{ FString::Printf(TEXT("FTH +1  (지금 %d)"), GI->PlayerFTH), TEXT("신앙 공격 강화") },
		{ FString::Printf(TEXT("최대 HP +%d  (지금 %d)"), UBWGameInstance::HPPerStatPoint, GI->PlayerMaxHP), TEXT("현재 HP도 함께 오름") },
		{ FString::Printf(TEXT("방어 +1  (지금 %d포인트)"), GI->PlayerDefensePoints), DefenseEffect },
	};
	TArray<FModalOption> Options;
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		Options.Add({ FText::FromString(Rows[i].Key), FText::FromString(Rows[i].Value), true, [this, i]()
		{
			if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance()))
			{
				G->SpendStatPoint(i);
				G->SaveRun();
			}
			ShowStatAllocation();
		} });
	}
	OpenModal(TEXT("스탯 배분"), FString::Printf(TEXT("남은 포인트 %d. 고르는 즉시 적용되며 되돌릴 수 없습니다."), GI->PendingStatPoints), MoveTemp(Options));
}

void UBWMapWidget::OnDeckButtonClicked()
{
	if (!bModalOpen && !bBusy && !AugmentWidget) ShowDeck();
}

void UBWMapWidget::ShowDeck()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	TArray<FModalOption> Options;
	for (int32 i = 0; i < GI->RunDeck.Num(); ++i)
	{
		FBWCardData Card;
		if (!GI->GetCardData(GI->RunDeck[i].Row, GI->RunDeck[i].bUpgraded, Card)) continue;
		FString Reason;
		const bool bUsable = GI->CanUseCardOnMap(i, Reason);
		FModalOption Option = MakeCardOption(Card, bUsable ? FString::Printf(TEXT("눌러서 사용 · HP +%d (이번 층 1회)"), Card.EffectHeal) : FString(), bUsable, [this, i]()
		{
			if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance()))
			{
				G->UseCardOnMap(i);
				G->SaveRun();
			}
			RefreshTopBar();
			ShowDeck();
		});
		Option.bDimWhenDisabled = false; // 덱 보기: 못 쓰는 카드도 그대로 보여 준다
		Option.Detail = FText::FromString(bUsable ? DescribeCard(Card) : FString::Printf(TEXT("%s — %s"), *DescribeCard(Card), *Reason));
		Options.Add(MoveTemp(Option));
	}
	Options.Add({ FText::FromString(TEXT("닫기")), FText::GetEmpty(), true, []() {} });
	OpenModal(FString::Printf(TEXT("덱 %d장 · HP %d / %d"), GI->RunDeck.Num(), GI->PlayerCurrentHP, GI->PlayerMaxHP),
		TEXT("지도에서는 HP를 회복하는 효과카드만 쓸 수 있고, 같은 카드는 층당 한 번입니다. 나머지 카드는 전투에서 씁니다."), MoveTemp(Options));
}

// ---------------------------------------------------------------------------------------------
// 알림 창
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::OpenModal(const FString& Title, const FString& Body, TArray<FModalOption> Options, EModalStyle Style)
{
	if (Title != ModalTitleString) bPreviewUpgrades = false; // 다른 창이 열리면 강화 미리보기는 꺼진 채로
	ModalOptions = MoveTemp(Options);
	ModalTitleString = Title;
	ModalBodyString = Body;
	ModalStyle = Style;
	bModalOpen = true;
	ModalOpenedAt = Time;
	ModalTips.Reset();
	if (!ModalLayer) return; // 코드 화면이 없으면 ChooseModalOption으로만 진행

	const bool bShop = Style == EModalStyle::Shop;
	if (ModalPanel)
	{
		// 상점은 따뜻한 나무빛 판, 나머지는 밤색 판
		// 석판 판 (상인은 따뜻한 빛을 살짝 입힌다)
		if (!BWSkin::StylePanel(ModalPanel, bShop ? FLinearColor(1.05f, 0.9f, 0.75f) : FLinearColor::White))
		ModalPanel->SetBrush(bShop ? BWMapUI::RoundBrush(FLinearColor(0.075f, 0.052f, 0.035f, 0.98f), 18.0f, FLinearColor(1.0f, 0.72f, 0.32f, 0.85f), 3.0f)
			: BWMapUI::RoundBrush(FLinearColor(0.035f, 0.04f, 0.07f, 0.98f), 18.0f, BWMapUI::Gold * FLinearColor(1, 1, 1, 0.6f), 2.0f));
	}
	ModalTitle->SetText(FText::FromString(Title));
	ModalBody->SetText(FText::FromString(Body));
	ModalBody->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ModalOptionBox->ClearChildren();
	OptionHandlers.Reset();

	// 제목 줄 오른쪽: 상점이면 가진 골드, 강화할 수 있는 카드가 보이면 '강화 미리보기'
	bool bAnyUpgradable = false;
	for (const FModalOption& Option : ModalOptions) bAnyUpgradable |= Option.bShowCard && !Option.bSoldOut && !Option.Card.CardName.EndsWith(TEXT("+"));
	if (ModalHeaderRight)
	{
		ModalHeaderRight->ClearChildren();
		if (bAnyUpgradable) ModalHeaderRight->AddChildToHorizontalBox(MakeUpgradeToggle());
		if (bShop)
		{
			const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
			if (UHorizontalBoxSlot* S = ModalHeaderRight->AddChildToHorizontalBox(MakePriceTag(GI ? GI->CurrentGold : 0, -1, true)))
			{
				S->SetPadding(FMargin(18.0f, 0.0f, 0.0f, 0.0f));
				S->SetVerticalAlignment(VAlign_Center);
			}
		}
	}

	// 카드 선택지: 카드를 그대로 늘어놓고 누르면 고른다. 카드가 있으면 창을 넓혀 한 줄에 3~4장
	int32 NumCards = 0;
	for (const FModalOption& Option : ModalOptions) NumCards += Option.bShowCard ? 1 : 0;
	const bool bManyCards = NumCards > 4 && !bShop; // 상점 카드는 다섯 장이어도 크게
	const FVector2D CardSize = bManyCards ? RewardCardSize * 0.8f : RewardCardSize;
	const int32 PerRow = bManyCards || bShop ? FMath::Clamp(NumCards, 3, 5) : FMath::Clamp(NumCards, 3, 4);
	const float PanelWidth = NumCards > 0 ? PerRow * (CardSize.X + 30.0f) + 120.0f + (bShop ? 40.0f : 0.0f) : 860.0f;
	if (ModalPanelSize) ModalPanelSize->SetWidthOverride(PanelWidth);
	if (ModalScrollSize) ModalScrollSize->SetMaxDesiredHeight(bManyCards ? 700.0f : 600.0f);
	UWrapBox* CardRow = nullptr;
	for (int32 i = 0; i < ModalOptions.Num(); ++i)
	{
		const FModalOption& Option = ModalOptions[i];
		if (!Option.bShowCard) continue;
		if (!CardRow)
		{
			CardRow = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
			CardRow->SetInnerSlotPadding(FVector2D(18.0f, bShop ? 24.0f : 14.0f));
			CardRow->SetExplicitWrapSize(true);
			CardRow->SetWrapSize(PanelWidth - 70.0f); // 알림 창 안쪽 폭. 이보다 넓으면 다음 줄로
			CardRow->SetHorizontalAlignment(HAlign_Center);
			UWidget* RowHolder = CardRow;
			if (bShop)
			{
				UBorder* Shelf = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				Shelf->SetBrush(BWMapUI::RoundBrush(FLinearColor(0.025f, 0.017f, 0.012f, 0.9f), 14.0f, FLinearColor(1.0f, 0.72f, 0.32f, 0.25f), 1.5f));
				Shelf->SetPadding(FMargin(20.0f, 26.0f, 20.0f, 18.0f));
				Shelf->SetContent(CardRow);
				RowHolder = Shelf;
			}
			if (UVerticalBoxSlot* RowSlot = ModalOptionBox->AddChildToVerticalBox(RowHolder))
			{
				RowSlot->SetHorizontalAlignment(HAlign_Center);
				RowSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 16.0f));
			}
		}
		CardRow->AddChildToWrapBox(MakeCardChoice(Option, i, CardSize));
	}

	// 글자 선택지: 보통은 세로로, 상점은 아래 한 줄로 (카드 제거 · 떠나기)
	UHorizontalBox* ShopRow = nullptr;
	if (bShop)
	{
		ShopRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* RowSlot = ModalOptionBox->AddChildToVerticalBox(ShopRow))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Center);
			RowSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}
	}
	for (int32 i = 0; i < ModalOptions.Num(); ++i)
	{
		const FModalOption& Option = ModalOptions[i];
		if (Option.bShowCard) continue;
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		Style.SetNormal(BWMapUI::RoundBrush(FLinearColor(0.09f, 0.1f, 0.15f, 1.0f), 10.0f, FLinearColor(1, 1, 1, 0.12f), 1.0f));
		Style.SetHovered(BWMapUI::RoundBrush(FLinearColor(0.2f, 0.17f, 0.08f, 1.0f), 10.0f, BWMapUI::Gold, 2.0f));
		Style.SetPressed(BWMapUI::RoundBrush(FLinearColor(0.3f, 0.24f, 0.08f, 1.0f), 10.0f, BWMapUI::Gold, 2.0f));
		Style.SetDisabled(BWMapUI::RoundBrush(FLinearColor(0.05f, 0.05f, 0.07f, 1.0f), 10.0f, FLinearColor(1, 1, 1, 0.05f), 1.0f));
		Style.SetNormalPadding(FMargin(18.0f, 10.0f));
		Style.SetPressedPadding(FMargin(18.0f, 11.0f, 18.0f, 9.0f));
		Button->SetStyle(Style);
		BWSkin::StyleButton(Button);
		Button->SetIsEnabled(Option.bEnabled);

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Content->AddChildToVerticalBox(MakeText(Option.Label.ToString(), 19, Option.bEnabled ? FLinearColor::White : FLinearColor(1, 1, 1, 0.55f), true));
		if (!Option.Detail.IsEmpty())
		{
			UTextBlock* Detail = MakeText(Option.Detail.ToString(), 14, Option.bEnabled ? BWMapUI::TextDim : FLinearColor(1, 1, 1, 0.45f));
			Detail->SetWrapTextAt(740.0f);
			Content->AddChildToVerticalBox(Detail);
		}
		Button->SetContent(Content);

		UBWMapOptionHandler* Handler = NewObject<UBWMapOptionHandler>(this);
		Handler->Index = i;
		Handler->Owner = this;
		Button->OnClicked.AddDynamic(Handler, &UBWMapOptionHandler::HandleClicked);
		OptionHandlers.Add(Handler);

		if (ShopRow)
		{
			if (UHorizontalBoxSlot* ButtonSlot = ShopRow->AddChildToHorizontalBox(Button)) ButtonSlot->SetPadding(FMargin(10.0f, 0.0f));
		}
		else if (UVerticalBoxSlot* ButtonSlot = ModalOptionBox->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 4.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
	ModalLayer->SetVisibility(ESlateVisibility::Visible);
}

void UBWMapWidget::ToggleUpgradePreview()
{
	if (!bModalOpen) return;
	bPreviewUpgrades = !bPreviewUpgrades;
	TArray<FModalOption> Same = ModalOptions;
	OpenModal(ModalTitleString, ModalBodyString, MoveTemp(Same), ModalStyle);
}

UWidget* UBWMapWidget::MakeUpgradeToggle()
{
	// 슬레이 더 스파이어의 '업그레이드 보기' 체크 상자: 네모 + 글
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = Button->GetStyle();
	const FLinearColor Edge = bPreviewUpgrades ? BWMapUI::Gold : FLinearColor(1, 1, 1, 0.25f);
	Style.SetNormal(BWMapUI::RoundBrush(FLinearColor(0.08f, 0.09f, 0.13f, 1.0f), 10.0f, Edge, 1.5f));
	Style.SetHovered(BWMapUI::RoundBrush(FLinearColor(0.18f, 0.15f, 0.07f, 1.0f), 10.0f, BWMapUI::Gold, 2.0f));
	Style.SetPressed(BWMapUI::RoundBrush(FLinearColor(0.28f, 0.22f, 0.07f, 1.0f), 10.0f, BWMapUI::Gold, 2.0f));
	Style.SetNormalPadding(FMargin(14.0f, 7.0f));
	Style.SetPressedPadding(FMargin(14.0f, 8.0f, 14.0f, 6.0f));
	Button->SetStyle(Style);
	BWSkin::StyleButton(Button);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* BoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BoxSize->SetWidthOverride(20.0f);
	BoxSize->SetHeightOverride(20.0f);
	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrush(BWMapUI::RoundBrush(FLinearColor(0.02f, 0.02f, 0.03f, 1.0f), 4.0f, BWMapUI::Gold, 2.0f));
	Box->SetPadding(FMargin(4.0f));
	if (bPreviewUpgrades)
	{
		UBorder* Check = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Check->SetBrush(BWMapUI::RoundBrush(BWMapUI::Gold, 2.0f));
		Box->SetContent(Check);
	}
	BoxSize->SetContent(Box);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BoxSize)) S->SetVerticalAlignment(VAlign_Center);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeText(TEXT("강화 미리보기"), 17, bPreviewUpgrades ? BWMapUI::Gold : FLinearColor::White, true)))
	{
		S->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	Button->SetContent(Row);
	ModalTips.Add({ Button, TEXT("강화 미리보기"), TEXT("누르면 모든 카드가 강화된 모습으로 보이고, 다시 누르면 돌아갑니다") });

	UBWMapOptionHandler* Handler = NewObject<UBWMapOptionHandler>(this);
	Handler->Index = UBWMapOptionHandler::ToggleUpgradeIndex;
	Handler->Owner = this;
	Button->OnClicked.AddDynamic(Handler, &UBWMapOptionHandler::HandleClicked);
	OptionHandlers.Add(Handler);
	return Button;
}

UWidget* UBWMapWidget::MakePriceTag(int32 Price, int32 OriginalPrice, bool bAffordable)
{
	// 동전 + 값 (할인이면 원래 값을 흐리게 앞에). 살 수 없으면 값이 붉다
	UBorder* Tag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Tag->SetBrush(BWMapUI::RoundBrush(FLinearColor(0.02f, 0.015f, 0.01f, 0.92f), 14.0f, FLinearColor(1.0f, 0.75f, 0.3f, 0.55f), 1.5f));
	Tag->SetPadding(FMargin(12.0f, 4.0f, 16.0f, 4.0f));
	Tag->SetVisibility(ESlateVisibility::HitTestInvisible);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Tag->SetContent(Row);

	USizeBox* CoinSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CoinSize->SetWidthOverride(20.0f);
	CoinSize->SetHeightOverride(20.0f);
	UBorder* Coin = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Coin->SetBrush(BWMapUI::RoundBrush(FLinearColor(1.0f, 0.78f, 0.25f), 10.0f, FLinearColor(0.55f, 0.35f, 0.05f), 2.0f));
	CoinSize->SetContent(Coin);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CoinSize)) S->SetVerticalAlignment(VAlign_Center);

	if (OriginalPrice > Price)
	{
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeText(FString::FromInt(OriginalPrice), 15, FLinearColor(1, 1, 1, 0.4f))))
		{
			S->SetPadding(FMargin(9.0f, 0.0f, 0.0f, 0.0f));
			S->SetVerticalAlignment(VAlign_Center);
		}
	}
	const FLinearColor ValueColor = bAffordable ? FLinearColor(1.0f, 0.88f, 0.5f) : FLinearColor(1.0f, 0.38f, 0.34f);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(MakeText(FString::FromInt(Price), 20, ValueColor, true)))
	{
		S->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	return Tag;
}

UBWMapWidget::FModalOption UBWMapWidget::MakeCardOption(const FBWCardData& Card, const FString& Caption, bool bEnabled, TFunction<void()> OnChosen)
{
	FModalOption Option;
	Option.Label = FText::FromString(Caption);
	Option.bEnabled = bEnabled;
	Option.OnChosen = MoveTemp(OnChosen);
	Option.bShowCard = true;
	Option.Card = Card;
	return Option;
}

UWidget* UBWMapWidget::MakeCardChoice(const FModalOption& Option, int32 Index, const FVector2D& CardSize)
{
	// 버튼(테두리만) 안에 카드 위젯. 카드 위젯은 클릭을 받지 않게 해서 버튼이 받는다
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = Button->GetStyle();
	Style.SetNormal(BWMapUI::RoundBrush(FLinearColor(0, 0, 0, 0), 14.0f, FLinearColor(1, 1, 1, 0.0f), 0.0f));
	Style.SetHovered(BWMapUI::RoundBrush(FLinearColor(1.0f, 0.85f, 0.4f, 0.12f), 14.0f, BWMapUI::Gold, 3.0f));
	Style.SetPressed(BWMapUI::RoundBrush(FLinearColor(1.0f, 0.85f, 0.4f, 0.22f), 14.0f, BWMapUI::Gold, 3.0f));
	Style.SetNormalPadding(FMargin(6.0f));
	Style.SetPressedPadding(FMargin(6.0f, 8.0f, 6.0f, 4.0f));
	Button->SetStyle(Style);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	// 카드 위젯은 손패 크기(360×530)로 배치한 뒤 통째로 줄인다 (작게 배치하면 글자가 겹친다)
	USizeBox* OuterBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	OuterBox->SetWidthOverride(CardSize.X);
	OuterBox->SetHeightOverride(CardSize.Y);
	const bool bShopItem = Option.Price >= 0;
	if (Option.bSoldOut)
	{
		// 팔린 자리: 카드 크기의 빈 틀
		UBorder* Empty = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Empty->SetBrush(BWMapUI::RoundBrush(FLinearColor(0, 0, 0, 0.25f), 14.0f, FLinearColor(1, 1, 1, 0.14f), 2.0f));
		Empty->SetHorizontalAlignment(HAlign_Center);
		Empty->SetVerticalAlignment(VAlign_Center);
		Empty->SetContent(MakeText(TEXT("판매 완료"), 20, FLinearColor(1, 1, 1, 0.4f), true));
		OuterBox->SetContent(Empty);
		Column->AddChildToVerticalBox(OuterBox);
		USizeBox* TagSpace = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TagSpace->SetHeightOverride(40.0f);
		Column->AddChildToVerticalBox(TagSpace);
		Button->SetContent(Column);
		Button->SetIsEnabled(false);
		FButtonStyle Plain = Button->GetStyle();
		Plain.SetDisabled(Plain.Normal);
		Button->SetStyle(Plain);
		return Button;
	}
	// 강화 미리보기가 켜져 있으면 강화된 모습으로
	const FBWCardData ShownCard = bPreviewUpgrades ? UBWMapRules::MakeUpgradedCard(Option.Card) : Option.Card;
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Scale->SetStretch(EStretch::ScaleToFit);
	OuterBox->SetContent(Scale);
	USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CardBox->SetWidthOverride(360.0f);
	CardBox->SetHeightOverride(530.0f);
	Scale->SetContent(CardBox);
	Column->AddChildToVerticalBox(OuterBox);

	if (!CardWidgetClass) CardWidgetClass = LoadClass<UBWCardWidget>(nullptr, TEXT("/Game/HUD/BP/WBP_Card.WBP_Card_C"));
	if (UBWCardWidget* CardWidget = CardWidgetClass ? CreateWidget<UBWCardWidget>(this, CardWidgetClass) : nullptr)
	{
		CardWidget->CardData = ShownCard;
		// 좌측 하단 그림 = 사거리 칸 (손패와 같은 규칙)
		const int32 Reach = ShownCard.CardKind == ECardKind::Attack ? FMath::Clamp(ShownCard.Range, 1, 6) : 0;
		CardWidget->CardData.AboveImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/HUD/Texture/CardRange/T_CardRange_%d.T_CardRange_%d"), Reach, Reach)));
		CardWidget->HandIndex = INDEX_NONE;
		// 카드 그림은 소프트 참조라 미리 불러 둔다 (전투에서는 PreloadCombatAssets가 하는 일)
		CardWidget->CardData.BelowImage.LoadSynchronous();
		CardWidget->CardData.AboveImage.LoadSynchronous();
		CardWidget->CardData.BodyImage.LoadSynchronous();
		CardWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		CardBox->SetContent(CardWidget);
		CardWidget->InitCardUI();
	}
	else
	{
		CardBox->SetContent(MakeText(ShownCard.CardName, 20, FLinearColor::White, true));
	}

	// 할인 카드: 카드 위쪽에 붉은 띠
	if (bShopItem && Option.OriginalPrice > Option.Price)
	{
		UOverlay* Over = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Column->RemoveChild(OuterBox);
		Over->AddChildToOverlay(OuterBox);
		UBorder* Ribbon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Ribbon->SetBrush(BWMapUI::RoundBrush(FLinearColor(0.75f, 0.12f, 0.1f, 0.95f), 8.0f, FLinearColor(1.0f, 0.6f, 0.4f), 1.5f));
		Ribbon->SetPadding(FMargin(14.0f, 3.0f));
		Ribbon->SetContent(MakeText(TEXT("반값"), 16, FLinearColor::White, true));
		Ribbon->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* S = Over->AddChildToOverlay(Ribbon))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Top);
			S->SetPadding(FMargin(0.0f, -12.0f, 0.0f, 0.0f));
		}
		Column->AddChildToVerticalBox(Over);
	}

	// 카드 밑: 강화 표시와 효과 설명 한 줄
	FString Caption = Option.Label.ToString();
	// 큰 목록(덱 보기·삭제)과 상점에서는 카드에 이미 적힌 설명을 다시 쓰지 않는다
	const bool bCompact = CardSize.X < RewardCardSize.X || bShopItem;
	if (!bCompact && !ShownCard.Description.IsEmpty()) Caption = Caption.IsEmpty() ? ShownCard.Description : Caption + TEXT(" · ") + ShownCard.Description;
	if (bShopItem)
	{
		const UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
		if (UVerticalBoxSlot* TagSlot = Column->AddChildToVerticalBox(MakePriceTag(Option.Price, Option.OriginalPrice, GI && GI->CurrentGold >= Option.Price)))
		{
			TagSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
			TagSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}
	else if (!Caption.IsEmpty())
	{
		UTextBlock* Text = MakeText(Caption, 14, BWMapUI::TextDim);
		Text->SetWrapTextAt(CardSize.X);
		Text->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* TextSlot = Column->AddChildToVerticalBox(Text))
		{
			TextSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
			TextSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}
	Button->SetContent(Column);
	if (!Option.Detail.IsEmpty()) ModalTips.Add({ Button, Option.Label.ToString(), Option.Detail.ToString() });
	// 못 고르는 카드: 상점에서 골드가 모자라면 흐리게, 덱 보기에서는 그대로 두고 누르지만 못 하게
	Button->SetIsEnabled(Option.bEnabled);
	if (!Option.bEnabled && Option.bDimWhenDisabled) Button->SetRenderOpacity(bShopItem ? 0.62f : 0.45f);
	if (!Option.bEnabled && !Option.bDimWhenDisabled)
	{
		FButtonStyle Plain = Button->GetStyle();
		Plain.SetDisabled(Plain.Normal);
		Button->SetStyle(Plain);
	}

	UBWMapOptionHandler* Handler = NewObject<UBWMapOptionHandler>(this);
	Handler->Index = Index;
	Handler->Owner = this;
	Button->OnClicked.AddDynamic(Handler, &UBWMapOptionHandler::HandleClicked);
	OptionHandlers.Add(Handler);
	return Button;
}

void UBWMapWidget::CloseModal()
{
	bModalOpen = false;
	ModalTitleString.Reset();
	if (ModalLayer) ModalLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void UBWMapWidget::ChooseModalOption(int32 Index)
{
	if (!bModalOpen || !ModalOptions.IsValidIndex(Index) || !ModalOptions[Index].bEnabled) return;
	// 창이 뜨는 동안의 클릭은 무시한다 (연타로 다음 창까지 눌리는 것 방지)
	if (!UBWAutoPlayer::IsActive() && (Time - ModalOpenedAt < 0.25f || Time < ModalLockUntil)) return;
	// 콜백이 새 창을 열 수 있으니 먼저 꺼내 두고 닫는다
	TFunction<void()> Callback = ModalOptions[Index].OnChosen;
	ModalOptions.Reset();
	CloseModal();
	if (Callback) Callback();
}

TArray<FString> UBWMapWidget::GetModalOptionLabels() const
{
	TArray<FString> Labels;
	for (const FModalOption& Option : ModalOptions) Labels.Add(Option.bEnabled ? Option.Label.ToString() : TEXT("(불가) ") + Option.Label.ToString());
	return Labels;
}

// ---------------------------------------------------------------------------------------------
// 옛 디자이너 화면 (bUseCodeLayout = false)
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::RenderLegacyMap()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || !MapMainContainer || !NodeWidgetClass) return;

	if (MapScrollBox)
	{
		MapScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		MapScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		MapScrollBox->SetOrientation(Orient_Horizontal);
		MapScrollBox->SetAllowRightClickDragScrolling(true);
		MapScrollBox->SetWheelScrollMultiplier(ScrollSensitivity);
	}

	MapMainContainer->ClearChildren();
	MapMainContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	for (int32 step = 0; step < GI->WorldMapData.Num(); ++step)
	{
		UVerticalBox* ColumnBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (!ColumnBox) continue;
		ColumnBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		const FMapStep& StepData = GI->WorldMapData[step];
		for (int32 dim = 0; dim < StepData.Nodes.Num(); ++dim)
		{
			USizeBox* NodeWrapper = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			NodeWrapper->SetWidthOverride(NodeSize.X);
			NodeWrapper->SetHeightOverride(NodeSize.Y);
			NodeWrapper->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

			UBWMapNodeWidget* NewNode = CreateWidget<UBWMapNodeWidget>(this, NodeWidgetClass);
			if (!NewNode) continue;
			NewNode->InitNode(StepData.Nodes[dim]);
			NodeWrapper->AddChild(NewNode);
			if (UVerticalBoxSlot* NodeSlot = ColumnBox->AddChildToVerticalBox(NodeWrapper))
			{
				NodeSlot->SetHorizontalAlignment(HAlign_Center);
				NodeSlot->SetVerticalAlignment(VAlign_Center);
				NodeSlot->SetPadding(FMargin(0.0f, RowSpacing / 2.0f));
			}
		}
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(MapMainContainer->AddChild(ColumnBox)))
		{
			HSlot->SetPadding(FMargin(ColumnSpacing / 2.0f, 0.0f));
			HSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	ScrollToCurrentStep();
}

void UBWMapWidget::ScrollToCurrentStep()
{
	if (!MapScrollBox) return;
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	float ScrollOffset = (GI->CurrentStep) * (NodeSize.X + ColumnSpacing);
	ScrollOffset = FMath::Max(0.0f, ScrollOffset - 200.0f);
	MapScrollBox->SetScrollOffset(ScrollOffset);
}


void UBWMapWidget::DebugShowScreen(const FString& Which)
{
	if (Which == TEXT("shop"))
	{
		// 실제 상점 칸과 같이 카드를 먼저 뽑는다 (예전엔 뽑지 않아 카드 줄이 비어 보였음)
		if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance()))
		{
			ShopCards = GI->RollCardChoices(GI->ShopCardCount);
			ShopSold.Init(false, ShopCards.Num());
			ShopSaleIndex = ShopCards.Num() > 0 ? FMath::RandRange(0, ShopCards.Num() - 1) : INDEX_NONE;
		}
		ShowShop();
	}
	else if (Which == TEXT("rest")) ShowRest();
	else if (Which == TEXT("reward")) ShowRewards(TEXT("전투 보상"));
	else if (Which == TEXT("stat")) ShowStatAllocation();
	else if (Which == TEXT("augment")) OpenAugmentPick();
	else ShowDeck();
}

static FAutoConsoleCommandWithWorldAndArgs GBWUICommand(TEXT("bw.UI"), TEXT("화면 확인용 창 띄우기: shop|rest|reward|stat|augment|deck|pause|settings"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const FString Which = Args.Num() > 0 ? Args[0] : FString();
		if (Which == TEXT("pause")) { if (UBWGameInstance* GI = World ? Cast<UBWGameInstance>(World->GetGameInstance()) : nullptr) GI->HandleEscapeKey(); return; }
		if (Which == TEXT("settings"))
		{
			if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
				if (UBWSettingsWidget* W = CreateWidget<UBWSettingsWidget>(PC, UBWSettingsWidget::StaticClass())) W->AddToViewport(800);
			return;
		}
		for (TObjectIterator<UBWMapWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport()) { It->DebugShowScreen(Args.Num() > 0 ? Args[0] : FString()); return; }
		}
	}));
