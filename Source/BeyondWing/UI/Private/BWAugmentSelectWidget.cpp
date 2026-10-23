#include "BeyondWing/UI/Public/BWAugmentSelectWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f));
	Panel->SetPadding(FMargin(24.0f));
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
		Button->SetContent(Label);
		if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(Button)) ButtonSlot->SetPadding(FMargin(0.0f, 6.0f));
		return Button;
	};

	TXT_Title = AddText(TEXT("TXT_Title"), 22);
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TXT_Title)) TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	TXT_Choice0 = AddText(TEXT("TXT_Choice0"), 14);
	TXT_Choice1 = AddText(TEXT("TXT_Choice1"), 14);
	TXT_Choice2 = AddText(TEXT("TXT_Choice2"), 14);
	BTN_Choice0 = AddButton(TEXT("BTN_Choice0"), TXT_Choice0);
	BTN_Choice1 = AddButton(TEXT("BTN_Choice1"), TXT_Choice1);
	BTN_Choice2 = AddButton(TEXT("BTN_Choice2"), TXT_Choice2);

	TXT_Reroll = AddText(TEXT("TXT_Reroll"), 14);
	BTN_Reroll = AddButton(TEXT("BTN_Reroll"), TXT_Reroll);
}

void UBWAugmentSelectWidget::RefreshChoices()
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (!GI) return;

	if (TXT_Title) TXT_Title->SetText(FText::FromString(FString::Printf(TEXT("%d막 증강 선택 (3개 중 1개)"), GI->AugmentPickAct)));

	UButton* Buttons[] = { BTN_Choice0, BTN_Choice1, BTN_Choice2 };
	UTextBlock* Texts[] = { TXT_Choice0, TXT_Choice1, TXT_Choice2 };
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bHas = GI->AugmentChoices.IsValidIndex(i);
		if (Buttons[i]) Buttons[i]->SetVisibility(bHas ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bHas || !Texts[i]) continue;

		FBWAugmentData Data;
		const FString Label = GI->GetAugmentData(GI->AugmentChoices[i], Data)
			? FString::Printf(TEXT("%s\n%s"), *Data.AugmentName, *Data.Description)
			: GI->AugmentChoices[i].ToString();
		Texts[i]->SetText(FText::FromString(Label));
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
