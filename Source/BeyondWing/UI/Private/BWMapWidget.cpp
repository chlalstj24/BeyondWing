#include "BeyondWing/UI/Public/BWMapWidget.h"
#include "BeyondWing/UI/Public/BWMapNodeWidget.h"
#include "BeyondWing/UI/Public/BWAugmentSelectWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/Core/Public/BWMapRules.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
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

	FString NodeHelp(EMapNodeType Type)
	{
		switch (Type)
		{
		case EMapNodeType::Battle:         return TEXT("일반 적과 싸운다. 이기면 골드와 카드 보상(3장 중 1장)");
		case EMapNodeType::Elite:          return TEXT("강한 적. 골드 2배 + 카드 보상 (유물·장비는 준비 중)");
		case EMapNodeType::Mystery:        return TEXT("선택형 이벤트 (차원마다 다름)");
		case EMapNodeType::Rest:           return TEXT("HP 회복 또는 카드 강화");
		case EMapNodeType::Shop:           return TEXT("카드 구매, 카드 삭제");
		case EMapNodeType::Chest:          return TEXT("전투 없이 골드 + 카드 보상");
		case EMapNodeType::Boss:           return TEXT("막의 끝. 네 줄이 합류하고, 마지막으로 서 있던 차원의 기믹이 보스전에 적용된다");
		case EMapNodeType::DimensionAltar: return TEXT("과부하와 모든 차원의 보너스 내성을 초기화");
		case EMapNodeType::Rift:           return TEXT("밟으면 다음 칸에서 두 줄 떨어진 차원으로 점프 가능");
		default:                           return FString();
		}
	}

	FString Concept(EDimensionType Dim)
	{
		switch (Dim)
		{
		case EDimensionType::Patrovas: return TEXT("마법");
		case EDimensionType::Berend:   return TEXT("상남자");
		case EDimensionType::Geleon:   return TEXT("기술");
		case EDimensionType::Taiga:    return TEXT("암살");
		default:                       return FString();
		}
	}

	FString Gimmick(EDimensionType Dim)
	{
		switch (Dim)
		{
		case EDimensionType::Patrovas: return TEXT("매 턴 스킬 숫자 범위가 무작위로 커지거나 좁아짐");
		case EDimensionType::Berend:   return TEXT("적을 벽에 밀치면 추가 피해 + 기절");
		case EDimensionType::Geleon:   return TEXT("피해량 +50%, 공격카드/효과카드 중 한 종류만");
		case EDimensionType::Taiga:    return TEXT("최초 3회 공격이 방어 무시 + 치명타");
		default:                       return FString();
		}
	}

	FString OverloadTwist(EDimensionType Dim)
	{
		switch (Dim)
		{
		case EDimensionType::Patrovas: return TEXT("숫자 범위가 커질 확률은 줄고 좁아질 확률은 늘어남");
		case EDimensionType::Berend:   return TEXT("적도 나를 벽에 밀치면 추가 피해");
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
			Text = FString::Printf(TEXT("카드 %d장 삭제 + %s"), B.CardRemovals, B.EquipmentGrade >= 2 ? TEXT("전설 장비") : TEXT("희귀 이상 장비"));
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
	if (UBWMapWidget* Map = Owner.Get()) Map->ChooseModalOption(Index);
}

// ---------------------------------------------------------------------------------------------
// 화면 만들기
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (bUseCodeLayout && WidgetTree) BuildCodeLayout();
}

void UBWMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bUseCodeLayout) return;
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

	// 위쪽: 막·진행 (왼쪽), 능력치·받아 둔 보너스 (오른쪽)
	ActText = AddText(RootCanvas, TEXT(""), FVector2D(40.0f, 24.0f), 34, BWMapUI::Gold, true);
	ProgressText = AddText(RootCanvas, TEXT(""), FVector2D(42.0f, 78.0f), 17, BWMapUI::TextDim);
	StatsText = AddText(RootCanvas, TEXT(""), FVector2D(1880.0f, 30.0f), 22, FLinearColor::White, true, FVector2D(1.0f, 0.0f));
	BonusText = AddText(RootCanvas, TEXT(""), FVector2D(1880.0f, 76.0f), 16, BWMapUI::Gold, false, FVector2D(1.0f, 0.0f));

	// 덱 보기 · 효과카드 사용 버튼 (막 제목 옆)
	{
		UButton* DeckButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DeckButton"));
		FButtonStyle Style = DeckButton->GetStyle();
		Style.SetNormal(BWMapUI::RoundBrush(FLinearColor(0.09f, 0.1f, 0.15f, 1.0f), 10.0f, BWMapUI::Gold * FLinearColor(1, 1, 1, 0.45f), 1.0f));
		Style.SetHovered(BWMapUI::RoundBrush(FLinearColor(0.2f, 0.17f, 0.08f, 1.0f), 10.0f, BWMapUI::Gold, 2.0f));
		Style.SetPressed(BWMapUI::RoundBrush(FLinearColor(0.3f, 0.24f, 0.08f, 1.0f), 10.0f, BWMapUI::Gold, 2.0f));
		Style.SetNormalPadding(FMargin(16.0f, 6.0f));
		Style.SetPressedPadding(FMargin(16.0f, 7.0f, 16.0f, 5.0f));
		DeckButton->SetStyle(Style);
		DeckButton->SetContent(MakeText(TEXT("덱 · 카드 사용"), 16, FLinearColor::White, true));
		DeckButton->OnClicked.AddDynamic(this, &UBWMapWidget::OnDeckButtonClicked);
		if (UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(DeckButton))
		{
			ButtonSlot->SetAutoSize(true);
			ButtonSlot->SetPosition(FVector2D(260.0f, 30.0f));
		}
	}

	// 지도 (상태가 바뀔 때마다 다시 그린다)
	MapLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapLayer"));
	MapLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Fill(RootCanvas->AddChildToCanvas(MapLayer));

	// 아래쪽 정보판: 칸 위에 올리면 그 칸에 도착했을 때 일어날 일
	AddBox(RootCanvas, FVector2D(960.0f, 965.0f), FVector2D(1860.0f, 170.0f), FLinearColor(0.03f, 0.035f, 0.06f, 0.92f), 14.0f, FLinearColor(1, 1, 1, 0.08f), 1.0f);
	InfoTitle = AddText(RootCanvas, TEXT(""), FVector2D(64.0f, 896.0f), 21, FLinearColor::White, true);
	InfoBody = AddText(RootCanvas, TEXT(""), FVector2D(64.0f, 932.0f), 16, FLinearColor(0.85f, 0.86f, 0.9f), false, FVector2D(0.0f, 0.0f), 1780.0f);

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
		PanelSize->SetMaxDesiredHeight(900.0f);
		ModalLayer->SetContent(PanelSize);
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrush(BWMapUI::RoundBrush(FLinearColor(0.035f, 0.04f, 0.07f, 0.98f), 18.0f, BWMapUI::Gold * FLinearColor(1, 1, 1, 0.6f), 2.0f));
		Panel->SetPadding(FMargin(40.0f, 28.0f, 40.0f, 30.0f));
		PanelSize->SetContent(Panel);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		ModalTitle = MakeText(TEXT(""), 30, BWMapUI::Gold, true);
		if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(ModalTitle)) TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		ModalBody = MakeText(TEXT(""), 17, FLinearColor(0.88f, 0.88f, 0.92f));
		ModalBody->SetWrapTextAt(780.0f); // AutoWrap은 첫 프레임 높이를 한 줄로 잡아 아래 버튼과 겹친다
		if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(ModalBody)) BodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
		UScrollBox* OptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ScrollSize->SetMaxDesiredHeight(600.0f);
		ScrollSize->SetContent(OptionScroll);
		Column->AddChildToVerticalBox(ScrollSize);
		ModalOptionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		OptionScroll->AddChild(ModalOptionBox);
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
	if (InfoTitle && !bModalOpen) ShowInfoFor(INDEX_NONE, INDEX_NONE);
}

void UBWMapWidget::RebuildMapLayer()
{
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

		AddText(MapLayer, UBWMapRules::GetDimensionName(Dim).ToString(), FVector2D(46.0f, Y - 46.0f), 22, DimColor, true);
		AddText(MapLayer, BWMapUI::Concept(Dim), FVector2D(48.0f, Y - 16.0f), 13, BWMapUI::TextDim);

		FString NeglectLine = FString::Printf(TEXT("방치 %d / %d"), Neglect, Condition);
		if (View.GlowLevel == 2) NeglectLine += TEXT(" · 깊은 방치");
		else if (View.GlowLevel == 1) NeglectLine += TEXT(" · 보너스 활성");
		if (bHere)
		{
			const int32 Consecutive = GI->ConsecutiveCounts.FindRef(Dim);
			const int32 Level = UBWMapRules::GetOverloadLevel(Consecutive);
			NeglectLine = FString::Printf(TEXT("여기 · 연속 %d턴%s"), Consecutive, Level == 2 ? TEXT(" · 과부하 II") : Level == 1 ? TEXT(" · 과부하 I") : TEXT(""));
		}
		View.Neglect = AddText(MapLayer, NeglectLine, FVector2D(48.0f, Y + 8.0f), 14, View.GlowLevel > 0 && !bHere ? BWMapUI::Gold : FLinearColor(0.85f, 0.85f, 0.9f));
		RowViews.Add(View);
	}

	// 칸 번호 (고정 칸은 금색)
	for (int32 Step = 0; Step < UBWMapRules::StepsPerAct; ++Step)
	{
		EMapNodeType Fixed;
		const bool bFixed = UBWMapRules::GetFixedNodeType(Step, Fixed);
		AddText(MapLayer, FString::FromInt(Step + 1), FVector2D(MapLeft + Step * ColumnWidth, FirstRowY - 96.0f), 14,
			Step == Cur ? FLinearColor::White : (bFixed ? BWMapUI::Gold : BWMapUI::TextDim), Step == Cur, FVector2D(0.5f, 0.0f));
	}

	// 2. 길: 같은 줄 직진선, 줄 경계 차원문(열림 ◆ / 닫힘 ×). 가시거리 안만 보인다
	auto IsMoveFromHere = [&](int32 FromStep, int32 FromRow, int32 ToRow)
	{
		return FromStep == Cur && FromRow == CurRow && GI->CanMoveToNode(FromStep + 1, ToRow);
	};
	for (int32 Step = 0; Step < UBWMapRules::StepsPerAct - 1; ++Step)
	{
		if (!GI->IsStepRevealed(Step + 1)) break;
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
	if (GI->IsStepRevealed(UBWMapRules::BossStep))
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
			View.Box->SetContent(MakeText(Label, bBoss ? 26 : (Label.Len() >= 3 ? 13 : 15), LabelColor, true));
			if (View.bCurrent) AddText(MapLayer, TEXT("현재"), View.Center - FVector2D(0.0f, View.Size * 0.5f + 6.0f), 12, FLinearColor::White, true, FVector2D(0.5f, 1.0f));
			NodeViews.Add(View);
		}
	}
}

void UBWMapWidget::RefreshTopBar()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || !ActText) return;

	ActText->SetText(FText::FromString(FString::Printf(TEXT("%d막 / %d막"), GI->CurrentStage, UBWMapRules::NumActs)));

	FString Progress;
	if (GI->CurrentStep < 0) Progress = TEXT("시작할 줄을 고르세요 · 1칸은 모든 줄이 일반 전투, 시작 줄이 곧 첫 전투의 차원 기믹");
	else if (GI->IsBossStep(GI->CurrentStep)) Progress = FString::Printf(TEXT("보스 · %s 기믹"), *UBWMapRules::GetDimensionName(GI->GetCurrentDimension()).ToString());
	else Progress = FString::Printf(TEXT("%d칸 / %d칸 · 보스까지 %d칸 · 지금 %s"), GI->CurrentStep + 1, UBWMapRules::StepsPerAct,
		UBWMapRules::BossStep - GI->CurrentStep, *UBWMapRules::GetDimensionName(GI->GetCurrentDimension()).ToString());
	ProgressText->SetText(FText::FromString(Progress));

	const int32 DeckCount = GI->RunDeck.Num() > 0 ? GI->RunDeck.Num() : GI->PlayerDeckCount;
	StatsText->SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d      골드 %d      덱 %d장      증강 %d개"),
		GI->PlayerCurrentHP, GI->PlayerMaxHP, GI->CurrentGold, DeckCount, GI->OwnedAugments.Num())));

	TArray<FString> Bonuses;
	const FBWMapBonusState& B = GI->MapBonus;
	if (B.UpgradedCardChoices > 0) Bonuses.Add(FString::Printf(TEXT("다음 카드 보상 강화 %d장"), B.UpgradedCardChoices));
	if (B.VictoryHealFraction > 0.0f) Bonuses.Add(FString::Printf(TEXT("승리 시 HP %d%% 회복"), FMath::RoundToInt(B.VictoryHealFraction * 100.0f)));
	if (B.NextBattleDamageBonus > 0.0f) Bonuses.Add(FString::Printf(TEXT("다음 전투 피해 +%d%%"), FMath::RoundToInt(B.NextBattleDamageBonus * 100.0f)));
	if (B.EquipmentGrade > 0) Bonuses.Add(B.EquipmentGrade >= 2 ? TEXT("다음 전투 전설 장비") : TEXT("다음 전투 희귀 장비"));
	if (GI->bRiftJumpReady) Bonuses.Add(TEXT("균열: 이번에 두 줄 점프 가능"));
	BonusText->SetText(FText::FromString(Bonuses.Num() > 0 ? TEXT("받아 둔 효과: ") + FString::Join(Bonuses, TEXT("  ·  ")) : FString()));
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
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI || !InfoTitle) return;

	if (!GI->WorldMapData.IsValidIndex(Step) || !GI->WorldMapData[Step].Nodes.IsValidIndex(Row))
	{
		InfoTitle->SetText(FText::FromString(GI->CurrentStep < 0 ? TEXT("시작할 줄을 고르세요") : TEXT("다음 칸을 고르세요")));
		InfoBody->SetText(FText::FromString(TEXT(
			"금색 테두리 = 갈 수 있는 칸 · 직진, 또는 열린 차원문(◆)으로 위·아래 한 줄. 붉은 × = 닫힌 차원문\n"
			"노드와 차원문은 2칸 앞까지만 보입니다. 비워 둔 줄은 방치 카운터가 쌓이고, 조건이 차면 왼쪽 이름판이 빛납니다 — 그 줄로 넘어가는 순간 보너스를 받습니다.\n"
			"한 줄에 4턴 이상 머물면 과부하 I(기믹이 불리하게 뒤틀림), 6턴 이상이면 과부하 II(+ 카드 보상 3→2장).")));
		return;
	}

	const FMapNode& Node = GI->WorldMapData[Step].Nodes[Row];
	const bool bBoss = GI->IsBossStep(Step);
	const EDimensionType Dim = bBoss ? GI->GetCurrentDimension() : GI->GetRowDimension(Row);
	bool bVisible = true, bFogged = false;
	GI->GetNodeVisibilityState(Step, Node.NodeType, bVisible, bFogged);

	const FString DimName = UBWMapRules::GetDimensionName(Dim).ToString();
	InfoTitle->SetText(FText::FromString(bBoss ? FString::Printf(TEXT("보스 — 네 줄 합류 (%s 기믹)"), *DimName)
		: FString::Printf(TEXT("%s · %d칸 · %s"), *DimName, Step + 1, bFogged ? TEXT("?") : *UBWMapRules::GetNodeTypeName(Node.NodeType).ToString())));

	TArray<FString> Lines;
	if (bFogged) Lines.Add(TEXT("아직 보이지 않는 칸입니다 (가시거리 2칸)."));
	else Lines.Add(BWMapUI::NodeHelp(Node.NodeType));
	if (Dim != EDimensionType::None) Lines.Add(FString::Printf(TEXT("차원 기믹: %s"), *BWMapUI::Gimmick(Dim)));

	const bool bSelectable = !bFogged && GI->CanMoveToNode(Step, Row);
	if (bSelectable)
	{
		TArray<FString> Arrive;
		const bool bFirst = GI->CurrentStep < 0;
		const bool bSameRow = !bFirst && (bBoss || Row == GI->CurrentDimIdx);
		if (!bFirst && !bSameRow) Arrive.Add(FMath::Abs(Row - GI->CurrentDimIdx) == 2 ? TEXT("균열로 두 줄 점프") : TEXT("차원문 통과"));
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
		Lines.Add(TEXT("도착하면: ") + FString::Join(Arrive, TEXT(" · ")));
	}
	else if (!bFogged && Step == GI->CurrentStep + 1 && GI->CurrentStep >= 0)
	{
		const int32 Distance = FMath::Abs(Row - GI->CurrentDimIdx);
		Lines.Add(Distance == 1 ? TEXT("갈 수 없음: 이 경계의 차원문이 닫혀 있습니다") : TEXT("갈 수 없음: 한 번에 한 줄만 옮길 수 있습니다 (균열을 밟으면 두 줄)"));
	}
	InfoBody->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

// ---------------------------------------------------------------------------------------------
// 매 프레임: 빛 맥박, 마우스 올림, 스와이프, 증강 선택 끝
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bUseCodeLayout || !RootCanvas) return;
	Time += InDeltaTime;
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

	// 증강 화면이 닫혔으면(골랐으면) 이번 막은 고른 것으로 저장
	if (AugmentWidget && !AugmentWidget->IsInViewport())
	{
		AugmentWidget = nullptr;
		if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance()))
		{
			GI->AugmentPickedAct = GI->CurrentStage;
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
	if (GI->LastArrival.bChangedRow && RootCanvas) BeginSwipe(GI->LastArrival.Dimension);
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
		GI->PrepareEncounter(Node.NodeType, Dim);
		bBusy = true;
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
		ShopCards = GI->RollCardChoices(3);
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
		OpenModal(TEXT("차원 제단"), TEXT("제단이 뒤엉킨 차원의 흐름을 가라앉힙니다.\n과부하(연속 체류)와 모든 차원의 보너스 내성이 초기화됐습니다."), MoveTemp(Options));
		return;
	}
	case EMapNodeType::Rift:
	{
		TArray<FModalOption> Options;
		Options.Add({ FText::FromString(TEXT("확인")), FText::GetEmpty(), true, [this]() { FinishNode(); } });
		OpenModal(TEXT("균열"), TEXT("차원 사이에 금이 가 있습니다.\n다음 칸에서는 두 줄 떨어진 차원으로 바로 점프할 수 있습니다 (차원문 개폐와 무관, 다음 칸 한 번)."), MoveTemp(Options));
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
	if (!GI || bModalOpen || bBusy || AugmentWidget) return;

	if (GI->PendingRewards.Num() > 0)
	{
		const FString Title = GI->CurrentEncounterType == EMapNodeType::Boss ? TEXT("보스 격파") : GI->CurrentEncounterType == EMapNodeType::Elite ? TEXT("엘리트 처치") : TEXT("전투 승리");
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
		TArray<FModalOption> Options;
		if (GI->IsRunCleared())
		{
			Options.Add({ FText::FromString(TEXT("타이틀로")), FText::GetEmpty(), true, [this]()
			{
				if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->DeleteSavedRun();
				bBusy = true;
				UGameplayStatics::OpenLevel(GetWorld(), TitleLevelName);
			} });
			OpenModal(TEXT("탑 정복"), FString::Printf(TEXT("%d막의 보스를 모두 쓰러뜨렸습니다. 이번 판은 여기까지입니다."), UBWMapRules::NumActs), MoveTemp(Options));
		}
		else
		{
			Options.Add({ FText::FromString(TEXT("다음 막으로")), FText::GetEmpty(), true, [this]()
			{
				if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->AdvanceToNextAct();
				RenderMap();
				ProcessPendingFlow();
			} });
			OpenModal(FString::Printf(TEXT("%d막 돌파"), GI->CurrentStage),
				TEXT("다음 막으로 올라갑니다.\n줄 순서가 새로 섞이고, 방치·연속 체류·보너스 내성과 받아 둔 방치 보너스는 모두 초기화됩니다.\n막을 시작하며 증강을 하나 고릅니다."), MoveTemp(Options));
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
		GI->AugmentPickedAct = GI->CurrentStage; // 표가 비었으면 건너뛴다
		return;
	}
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
		Lines.Add(FString::Printf(TEXT("\n%s — 덱에 넣을 카드를 고르세요."), *CardReward.RewardName));
		for (int32 Choice = 0; Choice < CardReward.CardChoices.Num(); ++Choice)
		{
			FBWCardData Card;
			const bool bUpgraded = Choice < CardReward.UpgradedChoices;
			if (!GI->GetCardData(CardReward.CardChoices[Choice], bUpgraded, Card)) continue;
			Options.Add({ FText::FromString(Card.CardName + (bUpgraded ? TEXT("  (강화)") : TEXT(""))), FText::FromString(DescribeCard(Card)), true,
				[this, CardRewardIndex, Choice, Done]()
				{
					if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->ChooseRewardCard(CardRewardIndex, Choice);
					Done();
				} });
		}
		Options.Add({ FText::FromString(TEXT("건너뛰기")), FText::FromString(TEXT("카드를 받지 않는다")), true, Done });
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
		Options.Add({ FText::FromString(Card.CardName), FText::FromString(DescribeCard(Card)), true,
			[this, i, Count, bFromBonus, OnDone, RemovedSoFar]()
			{
				UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance());
				if (!G) return;
				const bool bRemoved = G->RemoveRunCard(i);
				if (bFromBonus) G->PendingCardRemovals = FMath::Max(0, G->PendingCardRemovals - 1);
				ShowCardRemoval(Count - 1, bFromBonus, OnDone, RemovedSoFar + (bRemoved ? 1 : 0));
			} });
	}
	Options.Add({ FText::FromString(TEXT("그만두기")), FText::FromString(TEXT("더 지우지 않는다")), true, [this, bFromBonus, OnDone, RemovedSoFar]()
	{
		if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance()); G && bFromBonus) G->PendingCardRemovals = 0;
		OnDone(RemovedSoFar);
	} });
	OpenModal(FString::Printf(TEXT("카드 삭제 (%d장 남음)"), Remaining),
		bFromBonus ? TEXT("타이가 방치 보너스: 덱에서 뺄 카드를 고르세요.") : TEXT("덱에서 뺄 카드를 고르세요."), MoveTemp(Options));
}

void UBWMapWidget::ShowCardUpgrade(const FString& Title, TFunction<void()> OnDone, TFunction<void()> OnCancel)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	TArray<FModalOption> Options;
	for (int32 i = 0; i < GI->RunDeck.Num(); ++i)
	{
		if (GI->RunDeck[i].bUpgraded) continue;
		FBWCardData Card, Upgraded;
		if (!GI->GetCardData(GI->RunDeck[i].Row, false, Card) || !GI->GetCardData(GI->RunDeck[i].Row, true, Upgraded)) continue;
		Options.Add({ FText::FromString(Card.CardName), FText::FromString(FString::Printf(TEXT("%s  →  %s"), *DescribeCard(Card), *DescribeCard(Upgraded))), true,
			[this, i, OnDone]()
			{
				if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->UpgradeRunCard(i);
				OnDone();
			} });
	}
	Options.Add({ FText::FromString(TEXT("취소")), FText::GetEmpty(), true, OnCancel });
	OpenModal(Title, TEXT("강화할 카드를 고르세요. 공격카드는 숫자 범위 최소 +1·최대 +2, 효과카드는 수치가 오릅니다."), MoveTemp(Options));
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
	Options.Add({ FText::FromString(TEXT("카드 강화")), FText::FromString(TEXT("덱의 카드 한 장을 강화한다")), bCanUpgrade,
		[this]() { ShowCardUpgrade(TEXT("카드 강화"), [this]() { FinishNode(); }, [this]() { ShowRest(); }); } });
	OpenModal(TEXT("휴식"), FString::Printf(TEXT("모닥불 앞에서 숨을 고릅니다. HP %d / %d\n하나를 고르세요."), GI->PlayerCurrentHP, GI->PlayerMaxHP), MoveTemp(Options));
}

void UBWMapWidget::ShowShop()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;
	TArray<FModalOption> Options;
	for (int32 i = 0; i < ShopCards.Num(); ++i)
	{
		FBWCardData Card;
		if (!GI->GetCardData(ShopCards[i], false, Card)) continue;
		Options.Add({ FText::FromString(FString::Printf(TEXT("%s — %d골드"), *Card.CardName, GI->ShopCardPrice)), FText::FromString(DescribeCard(Card)), GI->CurrentGold >= GI->ShopCardPrice,
			[this, i]()
			{
				UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance());
				if (!G || !ShopCards.IsValidIndex(i) || G->CurrentGold < G->ShopCardPrice) return;
				G->CurrentGold -= G->ShopCardPrice;
				G->AddRunCard(ShopCards[i]);
				ShopCards.RemoveAt(i);
				RefreshTopBar();
				ShowShop();
			} });
	}
	Options.Add({ FText::FromString(FString::Printf(TEXT("카드 삭제 — %d골드"), GI->ShopRemovePrice)), FText::FromString(TEXT("덱에서 카드 한 장을 뺀다")),
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
	Options.Add({ FText::FromString(TEXT("떠나기")), FText::GetEmpty(), true, [this]() { FinishNode(); } });
	OpenModal(FString::Printf(TEXT("상점 — 골드 %d"), GI->CurrentGold), TEXT("카드를 사거나 덱에서 카드를 뺄 수 있습니다. (유물·장비는 준비 중)"), MoveTemp(Options));
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
		Body = TEXT("책장이 제멋대로 다시 쓰이고 있다. 손을 넣으면 무언가 달라질 것 같다.");
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
		Body = TEXT("구경꾼들이 한 판 붙어 보라며 판돈을 건다.");
		Options.Add({ FText::FromString(TEXT("HP 8을 잃고 골드 60")), FText::GetEmpty(), true, [this, LoseHP]()
		{
			LoseHP(8);
			if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->CurrentGold += 60;
			FinishNode();
		} });
		break;
	case EDimensionType::Geleon:
		Title = TEXT("멈춘 공방");
		Body = TEXT("주인이 사라진 공방에서 연마기만 돌아가고 있다.");
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
		Body = TEXT("얼굴을 가린 자가 짐을 덜어 주겠다고 속삭인다.");
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
		Body = TEXT("길가에 주머니가 떨어져 있다.");
		Options.Add({ FText::FromString(TEXT("줍는다 (골드 25)")), FText::GetEmpty(), true, [this]()
		{
			if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance())) G->CurrentGold += 25;
			FinishNode();
		} });
		break;
	}
	Options.Add({ FText::FromString(TEXT("지나간다")), FText::GetEmpty(), true, [this]() { FinishNode(); } });
	OpenModal(Title, Body + TEXT("\n(임시 이벤트 — 차원별 이벤트 풀은 준비 중)"), MoveTemp(Options));
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
		const FString Detail = bUsable ? FString::Printf(TEXT("%s — 눌러서 사용 (HP +%d, 이번 막 1회)"), *DescribeCard(Card), Card.EffectHeal)
			: FString::Printf(TEXT("%s — %s"), *DescribeCard(Card), *Reason);
		Options.Add({ FText::FromString(Card.CardName), FText::FromString(Detail), bUsable, [this, i]()
		{
			if (UBWGameInstance* G = Cast<UBWGameInstance>(GetGameInstance()))
			{
				G->UseCardOnMap(i);
				G->SaveRun();
			}
			RefreshTopBar();
			ShowDeck();
		} });
	}
	Options.Add({ FText::FromString(TEXT("닫기")), FText::GetEmpty(), true, []() {} });
	OpenModal(FString::Printf(TEXT("덱 %d장 · HP %d / %d"), GI->RunDeck.Num(), GI->PlayerCurrentHP, GI->PlayerMaxHP),
		TEXT("지도에서는 HP를 회복하는 효과카드만 쓸 수 있고, 같은 카드는 막당 한 번입니다. 나머지 카드는 전투에서 씁니다."), MoveTemp(Options));
}

// ---------------------------------------------------------------------------------------------
// 알림 창
// ---------------------------------------------------------------------------------------------

void UBWMapWidget::OpenModal(const FString& Title, const FString& Body, TArray<FModalOption> Options)
{
	ModalOptions = MoveTemp(Options);
	ModalTitleString = Title;
	bModalOpen = true;
	if (!ModalLayer) return; // 코드 화면이 없으면 ChooseModalOption으로만 진행

	ModalTitle->SetText(FText::FromString(Title));
	ModalBody->SetText(FText::FromString(Body));
	ModalBody->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ModalOptionBox->ClearChildren();
	OptionHandlers.Reset();

	for (int32 i = 0; i < ModalOptions.Num(); ++i)
	{
		const FModalOption& Option = ModalOptions[i];
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		Style.SetNormal(BWMapUI::RoundBrush(FLinearColor(0.09f, 0.1f, 0.15f, 1.0f), 10.0f, FLinearColor(1, 1, 1, 0.12f), 1.0f));
		Style.SetHovered(BWMapUI::RoundBrush(FLinearColor(0.2f, 0.17f, 0.08f, 1.0f), 10.0f, BWMapUI::Gold, 2.0f));
		Style.SetPressed(BWMapUI::RoundBrush(FLinearColor(0.3f, 0.24f, 0.08f, 1.0f), 10.0f, BWMapUI::Gold, 2.0f));
		Style.SetDisabled(BWMapUI::RoundBrush(FLinearColor(0.05f, 0.05f, 0.07f, 1.0f), 10.0f, FLinearColor(1, 1, 1, 0.05f), 1.0f));
		Style.SetNormalPadding(FMargin(18.0f, 10.0f));
		Style.SetPressedPadding(FMargin(18.0f, 11.0f, 18.0f, 9.0f));
		Button->SetStyle(Style);
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

		if (UVerticalBoxSlot* ButtonSlot = ModalOptionBox->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 4.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
	ModalLayer->SetVisibility(ESlateVisibility::Visible);
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
