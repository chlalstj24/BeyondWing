#include "BeyondWing/UI/Public/BWGuardChoiceWidget.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/BWDuelRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

namespace BWGuardUI
{
	const FLinearColor Accent(0.4f, 0.85f, 1.0f);       // 가드에 건 MP (밝은 청록)
	const FLinearColor Kept(0.22f, 0.38f, 0.95f);       // 남는 MP (파랑)
	const FLinearColor Empty(0.12f, 0.12f, 0.16f, 0.9f); // 빈 칸
	const FLinearColor Threat(1.0f, 0.45f, 0.38f);
}

void UBWGuardChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget) BuildLayout();
}

void UBWGuardChoiceWidget::BuildLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// 전장이 보이도록 아주 옅게만 덮는다 (고르기 전에는 칸을 못 누른다)
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.02f, 0.28f));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	// 바깥 테두리(청록) 안에 어두운 판
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrushColor(BWGuardUI::Accent * FLinearColor(1, 1, 1, 0.85f));
	Frame->SetPadding(FMargin(0.0f, 2.0f));
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.05f, 0.95f));
	Panel->SetPadding(FMargin(34.0f, 20.0f, 34.0f, 22.0f));
	Frame->SetContent(Panel);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Frame))
	{
		PanelSlot->SetAnchors(FAnchors(0.73f, 0.42f)); // 가운데(전장)를 가리지 않게 오른쪽에
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}
	PanelWidget = Frame;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	auto MakeText = [this](int32 Size, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = Size >= 24 ? 1 : 0;
		Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetJustification(ETextJustify::Center);
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f));
		return Text;
	};
	auto AddCentered = [Column](UWidget* Widget, const FMargin& Pad)
	{
		if (UVerticalBoxSlot* VSlot = Column->AddChildToVerticalBox(Widget))
		{
			VSlot->SetHorizontalAlignment(HAlign_Center);
			VSlot->SetPadding(Pad);
		}
	};
	auto MakeButton = [this](UTextBlock* Label, const FLinearColor& Tint, float Width, float Height) -> TPair<USizeBox*, UButton*>
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(Width);
		Size->SetHeightOverride(Height);
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(Tint);
		Button->SetContent(Label);
		if (UButtonSlot* BSlot = Cast<UButtonSlot>(Label->Slot))
		{
			BSlot->SetHorizontalAlignment(HAlign_Center);
			BSlot->SetVerticalAlignment(VAlign_Center);
			BSlot->SetPadding(FMargin(4.0f, 0.0f));
		}
		Size->SetContent(Button);
		Buttons.Add(Button);
		return TPair<USizeBox*, UButton*>(Size, Button);
	};

	TXT_Title = MakeText(24, BWGuardUI::Accent);
	AddCentered(TXT_Title, FMargin(0.0f));
	TXT_Threat = MakeText(16, BWGuardUI::Threat);
	AddCentered(TXT_Threat, FMargin(0.0f, 4.0f, 0.0f, 14.0f));

	// MP 칸: 건 MP(청록) / 남는 MP(파랑) / 빈 칸
	CellRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddCentered(CellRow, FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	TXT_Mana = MakeText(14, FLinearColor(0.7f, 0.8f, 1.0f));
	AddCentered(TXT_Mana, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	// ◀ 걸 MP ▶
	UHorizontalBox* SpendRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddCentered(SpendRow, FMargin(0.0f));
	auto AddArrow = [&](const TCHAR* Glyph, bool bPlus)
	{
		UTextBlock* Label = MakeText(22, FLinearColor::White);
		Label->SetText(FText::FromString(Glyph));
		TPair<USizeBox*, UButton*> Made = MakeButton(Label, FLinearColor(0.25f, 0.45f, 0.75f), 56.0f, 56.0f);
		if (bPlus) Made.Value->OnClicked.AddDynamic(this, &UBWGuardChoiceWidget::OnPlusClicked);
		else Made.Value->OnClicked.AddDynamic(this, &UBWGuardChoiceWidget::OnMinusClicked);
		if (UHorizontalBoxSlot* HSlot = SpendRow->AddChildToHorizontalBox(Made.Key)) HSlot->SetVerticalAlignment(VAlign_Center);
	};
	AddArrow(TEXT("◀"), false);
	{
		UVerticalBox* Mid = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		USizeBox* MidSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		MidSize->SetWidthOverride(220.0f);
		MidSize->SetContent(Mid);
		TXT_Spend = MakeText(40, FLinearColor::White);
		if (UVerticalBoxSlot* S = Mid->AddChildToVerticalBox(TXT_Spend)) S->SetHorizontalAlignment(HAlign_Center);
		TXT_Block = MakeText(16, BWGuardUI::Accent);
		if (UVerticalBoxSlot* S = Mid->AddChildToVerticalBox(TXT_Block)) S->SetHorizontalAlignment(HAlign_Center);
		if (UHorizontalBoxSlot* HSlot = SpendRow->AddChildToHorizontalBox(MidSize)) HSlot->SetVerticalAlignment(VAlign_Center);
	}
	AddArrow(TEXT("▶"), true);

	TXT_Outcome = MakeText(15, FLinearColor(0.92f, 0.9f, 0.85f));
	AddCentered(TXT_Outcome, FMargin(0.0f, 12.0f, 0.0f, 12.0f));

	// 빠른 선택: 0 / 다 막기 / 최대
	UHorizontalBox* QuickRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddCentered(QuickRow, FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	auto AddQuick = [&](UTextBlock* Label) -> UButton*
	{
		TPair<USizeBox*, UButton*> Made = MakeButton(Label, FLinearColor(0.16f, 0.2f, 0.3f), 130.0f, 36.0f);
		if (UHorizontalBoxSlot* HSlot = QuickRow->AddChildToHorizontalBox(Made.Key)) HSlot->SetPadding(FMargin(5.0f, 0.0f));
		return Made.Value;
	};
	{
		UTextBlock* Zero = MakeText(14, FLinearColor::White); Zero->SetText(FText::FromString(TEXT("0")));
		AddQuick(Zero)->OnClicked.AddDynamic(this, &UBWGuardChoiceWidget::OnZeroClicked);
		TXT_CoverLabel = MakeText(14, FLinearColor::White);
		AddQuick(TXT_CoverLabel)->OnClicked.AddDynamic(this, &UBWGuardChoiceWidget::OnCoverClicked);
		UTextBlock* Max = MakeText(14, FLinearColor::White); Max->SetText(FText::FromString(TEXT("최대")));
		AddQuick(Max)->OnClicked.AddDynamic(this, &UBWGuardChoiceWidget::OnMaxClicked);
	}

	// 확정 버튼
	UHorizontalBox* ConfirmRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddCentered(ConfirmRow, FMargin(0.0f));
	{
		UTextBlock* Guard = MakeText(17, FLinearColor::White); Guard->SetText(FText::FromString(GuardButtonText));
		TPair<USizeBox*, UButton*> GuardBtn = MakeButton(Guard, FLinearColor(0.2f, 0.6f, 0.85f), 210.0f, 50.0f);
		GuardBtn.Value->OnClicked.AddDynamic(this, &UBWGuardChoiceWidget::OnGuardClicked);
		if (UHorizontalBoxSlot* HSlot = ConfirmRow->AddChildToHorizontalBox(GuardBtn.Key)) HSlot->SetPadding(FMargin(6.0f, 0.0f));

		UTextBlock* None = MakeText(15, FLinearColor(0.85f, 0.85f, 0.88f)); None->SetText(FText::FromString(NoGuardButtonText));
		TPair<USizeBox*, UButton*> NoneBtn = MakeButton(None, FLinearColor(0.3f, 0.3f, 0.34f), 170.0f, 50.0f);
		NoneBtn.Value->OnClicked.AddDynamic(this, &UBWGuardChoiceWidget::OnNoGuardClicked);
		if (UHorizontalBoxSlot* HSlot = ConfirmRow->AddChildToHorizontalBox(NoneBtn.Key)) HSlot->SetPadding(FMargin(6.0f, 0.0f));
	}

	UTextBlock* Hint = MakeText(12, FLinearColor(0.6f, 0.62f, 0.7f));
	Hint->SetText(FText::FromString(HintText));
	AddCentered(Hint, FMargin(0.0f, 12.0f, 0.0f, 0.0f));
}

void UBWGuardChoiceWidget::RebuildCells(int32 Count)
{
	if (!CellRow || Cells.Num() == Count) return;
	CellRow->ClearChildren();
	Cells.Reset();
	for (int32 i = 0; i < Count; ++i)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(16.0f);
		Box->SetHeightOverride(22.0f);
		UImage* Cell = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Box->SetContent(Cell);
		// 다섯 칸마다 조금 띄워 세기 쉽게
		if (UHorizontalBoxSlot* HSlot = CellRow->AddChildToHorizontalBox(Box)) HSlot->SetPadding(FMargin(i > 0 && i % 5 == 0 ? 7.0f : 2.0f, 0.0f, 0.0f, 0.0f));
		Cells.Add(Cell);
	}
}

void UBWGuardChoiceWidget::Setup(ABWCharacter* InPlayer, const FString& EnemyName, const FString& ActionName, const FBWDiceRange& IncomingRange, TFunction<void(int32)> InOnChosen)
{
	PlayerChar = InPlayer;
	OnChosen = MoveTemp(InOnChosen);
	Incoming = IncomingRange;
	bFinished = false;
	Age = 0.0f;

	MaxSpend = PlayerChar ? PlayerChar->GetMaxGuardMP() : 0;
	const FBWDefenseStats Defense = PlayerChar ? PlayerChar->Defense : FBWDefenseStats();
	// 적 최댓값을 다 막는 최소 MP (걸 수 있는 만큼까지)
	CoverSpend = 0;
	while (CoverSpend < MaxSpend && UBWDuelRules::GetShieldBlock(Defense, CoverSpend) < Incoming.Max) ++CoverSpend;

	if (TXT_Title) TXT_Title->SetText(FText::FromString(TitleText));
	if (TXT_Threat) TXT_Threat->SetText(FText::FromString(FString::Printf(TEXT("%s · %s  공격 %d ~ %d"), *EnemyName, *ActionName, Incoming.Min, Incoming.Max)));
	if (TXT_CoverLabel) TXT_CoverLabel->SetText(FText::FromString(FString::Printf(TEXT("다 막기 (%d)"), CoverSpend)));
	RebuildCells(PlayerChar ? FMath::Max(PlayerChar->MaxMP, PlayerChar->CurrentMP) : 0);

	// 처음에는 적 최댓값의 절반쯤을 막는 양을 제안한다
	int32 Suggest = 0;
	while (Suggest < MaxSpend && UBWDuelRules::GetShieldBlock(Defense, Suggest) * 2 < Incoming.Max) ++Suggest;
	Spend = -1;
	SetSpend(Suggest);
}

void UBWGuardChoiceWidget::SetSpend(int32 NewSpend)
{
	NewSpend = FMath::Clamp(NewSpend, 0, MaxSpend);
	if (NewSpend == Spend) return;
	Spend = NewSpend;
	Refresh();
}

void UBWGuardChoiceWidget::Refresh()
{
	if (!PlayerChar) return;
	const int32 MP = PlayerChar->CurrentMP;
	const int32 Blocked = UBWDuelRules::GetShieldBlock(PlayerChar->Defense, Spend);

	for (int32 i = 0; i < Cells.Num(); ++i)
	{
		if (!Cells[i]) continue;
		Cells[i]->SetColorAndOpacity(i < MP - Spend ? BWGuardUI::Kept : i < MP ? BWGuardUI::Accent : BWGuardUI::Empty);
	}
	if (TXT_Spend) TXT_Spend->SetText(FText::FromString(FString::Printf(TEXT("MP %d"), Spend)));
	if (TXT_Block) TXT_Block->SetText(FText::FromString(Spend > 0 ? FString::Printf(TEXT("피해 %d 막음"), Blocked) : FString(TEXT("가드 없음"))));
	if (TXT_Mana)
	{
		TXT_Mana->SetText(FText::FromString(FString::Printf(TEXT("보유 MP %d / %d  →  가드 후 %d   ·   MP 1당 %s 막음 · 한 번에 최대 %d"),
			MP, PlayerChar->MaxMP, MP - Spend, *FString::SanitizeFloat(PlayerChar->Defense.ShieldPerMP, 0), PlayerChar->Defense.MagicShieldMaxMP)));
	}
	if (TXT_Outcome)
	{
		const int32 Lo = FMath::Max(0, Incoming.Min - Blocked);
		const int32 Hi = FMath::Max(0, Incoming.Max - Blocked);
		FString Line = FString::Printf(TEXT("받는 피해  %d ~ %d"), Lo, Hi);
		if (PlayerChar->Block > 0) Line += FString::Printf(TEXT("  (방어도 %d가 먼저 받음)"), PlayerChar->Block);
		TXT_Outcome->SetText(FText::FromString(Line));
		TXT_Outcome->SetColorAndOpacity(Hi == 0 ? BWGuardUI::Accent : FLinearColor(0.92f, 0.9f, 0.85f));
	}
}

void UBWGuardChoiceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;

	// 나타날 때 살짝 커지며 밝아진다
	if (PanelWidget)
	{
		const float T = FMath::Clamp(Age / 0.18f, 0.0f, 1.0f);
		const float S = FMath::Lerp(0.92f, 1.0f, 1.0f - FMath::Square(1.0f - T));
		PanelWidget->SetRenderScale(FVector2D(S, S));
		PanelWidget->SetRenderOpacity(T);
	}
	if (bFinished || Age < 0.15f) return; // 열자마자 들어온 키는 무시

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	if (PC->WasInputKeyJustPressed(EKeys::Left) || PC->WasInputKeyJustPressed(EKeys::A)) SetSpend(Spend - 1);
	if (PC->WasInputKeyJustPressed(EKeys::Right) || PC->WasInputKeyJustPressed(EKeys::D)) SetSpend(Spend + 1);
	static const FKey NumKeys[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	static const FKey PadKeys[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour,
		EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	for (int32 i = 0; i < UE_ARRAY_COUNT(NumKeys); ++i)
	{
		if (PC->WasInputKeyJustPressed(NumKeys[i]) || PC->WasInputKeyJustPressed(PadKeys[i])) SetSpend(i);
	}
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Enter)) Confirm();
	else if (PC->WasInputKeyJustPressed(EKeys::Escape)) ConfirmNone();
}

void UBWGuardChoiceWidget::Confirm() { Finish(Spend); }
void UBWGuardChoiceWidget::ConfirmNone() { Finish(0); }

void UBWGuardChoiceWidget::Finish(int32 Chosen)
{
	if (bFinished) return;
	bFinished = true;
	TFunction<void(int32)> Callback = MoveTemp(OnChosen);
	RemoveFromParent();
	if (Callback) Callback(FMath::Clamp(Chosen, 0, MaxSpend));
}
