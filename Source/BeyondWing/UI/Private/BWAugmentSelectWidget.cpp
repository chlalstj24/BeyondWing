#include "BeyondWing/UI/Public/BWAugmentSelectWidget.h"
#include "BeyondWing/UI/Public/BWSkin.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ButtonSlot.h"
#include "Engine/Texture2D.h"
#include "Components/Spacer.h"
#include "Components/OverlaySlot.h"
#include "Components/Overlay.h"
#include "Components/Image.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UBWAugmentSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 블루프린트 디자인이 없으면 기본 화면을 코드로 만든다
	if (!BTN_Choice0 && WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();

	if (BTN_Choice0) BTN_Choice0->OnClicked.AddDynamic(this, &UBWAugmentSelectWidget::OnChoice0Clicked);
	if (BTN_Choice1) BTN_Choice1->OnClicked.AddDynamic(this, &UBWAugmentSelectWidget::OnChoice1Clicked);
	if (BTN_Choice2) BTN_Choice2->OnClicked.AddDynamic(this, &UBWAugmentSelectWidget::OnChoice2Clicked);
	if (BTN_Reroll) BTN_Reroll->OnClicked.AddDynamic(this, &UBWAugmentSelectWidget::OnRerollClicked);
}

void UBWAugmentSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshChoices();
}

void UBWAugmentSelectWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// [추가] 지도 위에 뜰 때 뒤를 어둡게 덮어 고르는 동안 지도를 누르지 않게 한다
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.01f, 0.6f));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.96f));
	Panel->SetPadding(FMargin(40.0f, 34.0f));
	BWSkin::StylePanel(Panel);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}

	// 제목과 설명이 한 줄에 들어가도록 최소 너비를 준다
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Width"));
	Width->SetMinDesiredWidth(460.0f);
	Panel->SetContent(Width);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Width->SetContent(Column);

	auto AddText = [this, Column](const TCHAR* Name, int32 Size) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		return Text;
	};
	auto AddButton = [this, Column](const TCHAR* ButtonName, UTextBlock* Label) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		// [추가] 어두운 버튼 + 흰 글씨 (기본 회색 버튼은 흰 글씨가 잘 안 보였다)
		auto Brush = [](const FLinearColor& Fill, const FLinearColor& Outline, float Width)
		{
			FSlateBrush B;
			B.DrawAs = ESlateBrushDrawType::RoundedBox;
			B.TintColor = FSlateColor(Fill);
			B.OutlineSettings.CornerRadii = FVector4(10.0f, 10.0f, 10.0f, 10.0f);
			B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			B.OutlineSettings.Color = FSlateColor(Outline);
			B.OutlineSettings.Width = Width;
			return B;
		};
		const FLinearColor Gold(1.0f, 0.8f, 0.35f);
		FButtonStyle Style = Button->GetStyle();
		Style.SetNormal(Brush(FLinearColor(0.09f, 0.1f, 0.15f), FLinearColor(1, 1, 1, 0.12f), 1.0f));
		Style.SetHovered(Brush(FLinearColor(0.2f, 0.17f, 0.08f), Gold, 2.0f));
		Style.SetPressed(Brush(FLinearColor(0.3f, 0.24f, 0.08f), Gold, 2.0f));
		Style.SetDisabled(Brush(FLinearColor(0.05f, 0.05f, 0.07f), FLinearColor(1, 1, 1, 0.05f), 1.0f));
		Style.SetNormalPadding(FMargin(18.0f, 10.0f));
		Style.SetPressedPadding(FMargin(18.0f, 11.0f, 18.0f, 9.0f));
		// 석판 툴팁 판: 평소 / 올림(밝게) / 누름(어둡게)
		FSlateBrush Card;
		if (BWSkin::Tooltip(Card))
		{
			Style.SetNormal(Card);
			Card.TintColor = FSlateColor(FLinearColor(1.25f, 1.25f, 1.25f)); Style.SetHovered(Card);
			Card.TintColor = FSlateColor(FLinearColor(0.8f, 0.8f, 0.82f)); Style.SetPressed(Card);
			Card.TintColor = FSlateColor(FLinearColor(0.45f, 0.45f, 0.48f, 0.85f)); Style.SetDisabled(Card);
			Style.SetNormalPadding(FMargin(24.0f, 18.0f));
			Style.SetPressedPadding(FMargin(24.0f, 19.0f, 24.0f, 17.0f));
		}
		Button->SetStyle(Style);
		Label->SetColorAndOpacity(FLinearColor::White);
		Button->SetContent(Label);
		if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(Button)) ButtonSlot->SetPadding(FMargin(0.0f, 6.0f));
		return Button;
	};

	TXT_Title = AddText(TEXT("TXT_Title"), 24);
	TXT_Title->SetColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.35f));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TXT_Title)) TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	TXT_Choice0 = AddText(TEXT("TXT_Choice0"), 16);
	TXT_Choice1 = AddText(TEXT("TXT_Choice1"), 16);
	TXT_Choice2 = AddText(TEXT("TXT_Choice2"), 16);
	// 버튼 안에서는 AutoWrap이 짧은 설명도 이상하게 접어 고정 폭으로만 접는다
	for (UTextBlock* Choice : { TXT_Choice0, TXT_Choice1, TXT_Choice2 })
	{
		Choice->SetAutoWrapText(false);
		Choice->SetWrapTextAt(CardWidth - 40.0f);
	}
	BTN_Choice0 = AddButton(TEXT("BTN_Choice0"), TXT_Choice0);
	BTN_Choice1 = AddButton(TEXT("BTN_Choice1"), TXT_Choice1);
	BTN_Choice2 = AddButton(TEXT("BTN_Choice2"), TXT_Choice2);

	// [추가] 후보는 왼쪽부터 안정·변화·도박, 가로로 나란히
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ChoiceRow"));
	Column->AddChildToVerticalBox(Row);
	for (UButton* Choice : { BTN_Choice0, BTN_Choice1, BTN_Choice2 })
	{
		Choice->RemoveFromParent();
		USizeBox* Card = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Card->SetWidthOverride(CardWidth);
		Card->SetHeightOverride(CardHeight);
		Card->SetContent(Choice);
		if (UHorizontalBoxSlot* CardSlot = Row->AddChildToHorizontalBox(Card)) CardSlot->SetPadding(FMargin(8.0f));
	}

	// [추가] 카드 안을 칸으로 나눈다: 위 종류 배지 → 아이콘 → 이름 → 효과(TXT_Choice) → 아래 세트 칸
	auto MakeLabel = [this](int32 Size, bool bBold, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		if (bBold) Font.TypefaceFontName = TEXT("Bold");
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetJustification(ETextJustify::Center);
		return Text;
	};
	UButton* ChoiceButtons[] = { BTN_Choice0, BTN_Choice1, BTN_Choice2 };
	UTextBlock* Bodies[] = { TXT_Choice0, TXT_Choice1, TXT_Choice2 };
	// 칸 높이는 고정: 가장 긴 설명(대가 포함 약 5줄)·세트 글(약 7줄)이 들어가는 크기
	static constexpr float BadgeH = 32.0f, IconH = 104.0f, NameH = 72.0f, BodyH = 150.0f, Gap = 10.0f;
	const float SetH = FMath::Max(120.0f, CardHeight - 36.0f - (BadgeH + IconH + NameH + BodyH + Gap * 4.0f));
	auto PlaceFixed = [this](UVerticalBox* Box, UWidget* Content, float Height, EVerticalAlignment VAlign, EHorizontalAlignment HAlign, float Top)
	{
		USizeBox* Fixed = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Fixed->SetHeightOverride(Height);
		if (USizeBoxSlot* In = Cast<USizeBoxSlot>(Fixed->AddChild(Content))) { In->SetVerticalAlignment(VAlign); In->SetHorizontalAlignment(HAlign); }
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Fixed)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0.0f, Top, 0.0f, 0.0f)); }
	};
	for (int32 i = 0; i < 3; ++i)
	{
		if (!ChoiceButtons[i] || !Bodies[i]) continue;
		UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UButtonSlot* InnerSlot = Cast<UButtonSlot>(ChoiceButtons[i]->SetContent(Inner)))
		{
			// 카드 위에서부터 채우고, 세트 칸은 맨 아래로
			InnerSlot->SetHorizontalAlignment(HAlign_Fill);
			InnerSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UBorder* Badge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Badge->SetPadding(FMargin(16.0f, 3.0f));
		UTextBlock* KindText = MakeLabel(15, true, FLinearColor(0.06f, 0.06f, 0.08f));
		Badge->SetContent(KindText);
		PlaceFixed(Inner, Badge, BadgeH, VAlign_Center, HAlign_Center, 0.0f);

		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconSize->SetWidthOverride(IconH);
		IconSize->SetHeightOverride(IconH);
		UBorder* IconFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		IconFrame->SetHorizontalAlignment(HAlign_Fill);
		IconFrame->SetVerticalAlignment(VAlign_Fill);
		IconFrame->SetPadding(FMargin(6.0f));
		IconSize->SetContent(IconFrame);
		UOverlay* IconStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		IconFrame->SetContent(IconStack);
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = IconStack->AddChildToOverlay(IconImage)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
		UTextBlock* Glyph = MakeLabel(48, true, FLinearColor::White);
		if (UOverlaySlot* S = IconStack->AddChildToOverlay(Glyph)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
		PlaceFixed(Inner, IconSize, IconH, VAlign_Center, HAlign_Center, Gap);

		UTextBlock* Name = MakeLabel(24, true, FLinearColor::White);
		Name->SetAutoWrapText(false);
		Name->SetWrapTextAt(CardWidth - 70.0f);
		PlaceFixed(Inner, Name, NameH, VAlign_Center, HAlign_Center, Gap);

		Bodies[i]->SetJustification(ETextJustify::Center);
		Bodies[i]->SetWrapTextAt(CardWidth - 80.0f);
		PlaceFixed(Inner, Bodies[i], BodyH, VAlign_Top, HAlign_Center, Gap);

		UBorder* SetBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SetBox->SetBrush(BWSkin::Plain(FLinearColor(0.08f, 0.1f, 0.13f, 0.85f), FLinearColor(1, 1, 1, 0.18f), 1.0f));
		SetBox->SetPadding(FMargin(14.0f, 10.0f));
		UTextBlock* SetText = MakeLabel(14, false, FLinearColor(0.85f, 0.87f, 0.9f));
		SetText->SetJustification(ETextJustify::Left);
		SetText->SetAutoWrapText(false);
		SetText->SetWrapTextAt(CardWidth - 110.0f);
		SetBox->SetContent(SetText);
		SetBox->SetVerticalAlignment(VAlign_Top);
		PlaceFixed(Inner, SetBox, SetH, VAlign_Fill, HAlign_Fill, Gap);

		KindBadges.Add(Badge); KindTexts.Add(KindText); IconFrames.Add(IconFrame); IconImages.Add(IconImage); IconGlyphs.Add(Glyph);
		NameTexts.Add(Name); SetBoxes.Add(SetBox); SetTexts.Add(SetText);
	}

	TXT_Reroll = AddText(TEXT("TXT_Reroll"), 14);
	BTN_Reroll = AddButton(TEXT("BTN_Reroll"), TXT_Reroll);
	BWSkin::StyleButton(BTN_Reroll); // 리롤은 장식 없는 작은 버튼
}

void UBWAugmentSelectWidget::RefreshChoices()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	if (TXT_Title) TXT_Title->SetText(FText::FromString(FString::Printf(TEXT("%d층 증강 선택 (3개 중 1개)"), GI->AugmentPickAct)));
	const FBWAugmentTotals Totals = GI->GetAugmentTotals();

	UButton* Buttons[] = { BTN_Choice0, BTN_Choice1, BTN_Choice2 };
	UTextBlock* Texts[] = { TXT_Choice0, TXT_Choice1, TXT_Choice2 };
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bHas = GI->AugmentChoices.IsValidIndex(i);
		if (Buttons[i]) Buttons[i]->SetVisibility(bHas ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bHas || !Texts[i]) continue;

		FBWAugmentData Data;
		if (!GI->GetAugmentData(GI->AugmentChoices[i], Data))
		{
			Texts[i]->SetText(FText::FromName(GI->AugmentChoices[i]));
			continue;
		}

		// 안정 파랑, 변화 금빛, 도박 붉음
		const FLinearColor Accent = BWSkin::AugmentAccent((uint8)Data.Kind);
		if (KindTexts.IsValidIndex(i))
		{
			// 위: 종류 배지
			KindTexts[i]->SetText(FText::FromString(UBWAugmentRules::GetKindName(Data.Kind)));
			KindBadges[i]->SetBrush(BWSkin::Plain(Accent, FLinearColor(1, 1, 1, 0.5f), 1.0f));
			// 아이콘: 그림이 있으면 그림, 없으면 이름 첫 글자
			IconFrames[i]->SetBrush(BWSkin::Plain(FLinearColor(0.1f, 0.12f, 0.15f, 1.0f), Accent, 2.5f));
			UTexture2D* IconTex = BWSkin::LoadAugmentIcon(GI->AugmentChoices[i], Data.Icon);
			IconImages[i]->SetVisibility(IconTex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (IconTex) IconImages[i]->SetBrushFromTexture(IconTex);
			IconGlyphs[i]->SetVisibility(IconTex ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
			IconGlyphs[i]->SetText(FText::FromString(Data.AugmentName.Left(1)));
			IconGlyphs[i]->SetColorAndOpacity(Accent);
			NameTexts[i]->SetText(FText::FromString(Data.AugmentName));
			// 가운데: 효과 (+ 대가)
			FString Body = Data.Description;
			if (!Data.CostText.IsEmpty()) Body += FString::Printf(TEXT("\n\n대가: %s"), *Data.CostText);
			Texts[i]->SetText(FText::FromString(Body));
			// 아래: 세트 칸 (세트가 없으면 빌드 이름, 둘 다 없으면 숨김)
			FString SetLine;
			if (Data.Set != EBWAugmentSet::None)
			{
				const int32 Have = Totals.SetCounts.IsValidIndex((int32)Data.Set) ? Totals.SetCounts[(int32)Data.Set] : 0;
				SetLine = FString::Printf(TEXT("세트 %s · 보유 %d/4\n%s"), *UBWAugmentRules::GetSetName(Data.Set), Have, *UBWAugmentRules::DescribeSet(Data.Set, Have + 1, GI->SelectedClass));
			}
			else SetLine = Data.BuildName;
			SetTexts[i]->SetText(FText::FromString(SetLine));
			SetBoxes[i]->SetVisibility(SetLine.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		}
		else
		{
			FString Label = FString::Printf(TEXT("[%s]\n\n%s\n\n%s"), *UBWAugmentRules::GetKindName(Data.Kind), *Data.AugmentName, *Data.Description);
			if (!Data.CostText.IsEmpty()) Label += FString::Printf(TEXT("\n\n대가: %s"), *Data.CostText);
			Texts[i]->SetText(FText::FromString(Label));
		}
		FButtonStyle Style = Buttons[i]->GetStyle();
		FSlateBrush Normal = Style.Normal;
		Normal.OutlineSettings.Color = FSlateColor(Accent);
		Normal.OutlineSettings.Width = 2.0f;
		if (Normal.DrawAs == ESlateBrushDrawType::Box) Normal.TintColor = FSlateColor(FLinearColor::LerpUsingHSV(FLinearColor::White, Accent, 0.22f));
		Style.SetNormal(Normal);
		Buttons[i]->SetStyle(Style);
	}

	if (BTN_Reroll) BTN_Reroll->SetIsEnabled(GI->AugmentRerollsLeft > 0);
	if (TXT_Reroll) TXT_Reroll->SetText(FText::FromString(FString::Printf(TEXT("리롤 (%d회 남음)"), GI->AugmentRerollsLeft)));
}

void UBWAugmentSelectWidget::Choose(int32 ChoiceIndex)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI && GI->ChooseAugment(ChoiceIndex)) RemoveFromParent();
}

void UBWAugmentSelectWidget::Reroll()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI && GI->RerollAugmentChoices()) RefreshChoices();
}
