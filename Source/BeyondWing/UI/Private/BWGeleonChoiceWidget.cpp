#include "BeyondWing/UI/Public/BWGeleonChoiceWidget.h"
#include "BeyondWing/Core/Public/BWPlayerController.h"
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

void UBWGeleonChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 블루프린트 디자인이 없으면 기본 화면을 코드로 만든다
	if (!BTN_Attack && WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();

	if (BTN_Attack) BTN_Attack->OnClicked.AddDynamic(this, &UBWGeleonChoiceWidget::OnAttackClicked);
	if (BTN_Effect) BTN_Effect->OnClicked.AddDynamic(this, &UBWGeleonChoiceWidget::OnEffectClicked);
}

void UBWGeleonChoiceWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// 화면 전체를 살짝 어둡게 덮어 선택 전에는 전장을 못 누르게 한다
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.04f, 0.94f));
	Panel->SetPadding(FMargin(28.0f));
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.42f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	auto MakeText = [this](const TCHAR* Name, int32 Size, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
		return Text;
	};

	TXT_Title = MakeText(TEXT("TXT_Title"), 26, FLinearColor(0.8f, 0.9f, 1.0f));
	TXT_Title->SetText(FText::FromString(TEXT("겔레온 · 피해 +50%")));
	Column->AddChildToVerticalBox(TXT_Title);

	TXT_Desc = MakeText(TEXT("TXT_Desc"), 14, FLinearColor(0.75f, 0.75f, 0.78f));
	TXT_Desc->SetText(FText::FromString(TEXT("효과카드를 쓰면 기본 공격을 강화하는 공격카드는 처음 쓴 1종만 쓸 수 있습니다. 기본 공격은 언제나 됩니다. 고른 뒤에는 바꿀 수 없습니다.")));
	if (UVerticalBoxSlot* DescSlot = Column->AddChildToVerticalBox(TXT_Desc)) DescSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 18.0f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row)) RowSlot->SetHorizontalAlignment(HAlign_Center);

	auto MakeChoice = [this, Row](const TCHAR* ButtonName, UTextBlock* Label, const FLinearColor& Tint) -> UButton*
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(280.0f);
		Size->SetHeightOverride(120.0f);
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		Button->SetBackgroundColor(Tint);
		Button->SetContent(Label);
		Size->SetContent(Button);
		if (UHorizontalBoxSlot* ChoiceSlot = Row->AddChildToHorizontalBox(Size)) ChoiceSlot->SetPadding(FMargin(10.0f, 0.0f));
		return Button;
	};

	TXT_Attack = MakeText(TEXT("TXT_Attack"), 16, FLinearColor(1.0f, 0.85f, 0.7f));
	TXT_Effect = MakeText(TEXT("TXT_Effect"), 16, FLinearColor(0.7f, 1.0f, 0.85f));
	BTN_Attack = MakeChoice(TEXT("BTN_Attack"), TXT_Attack, FLinearColor(0.9f, 0.45f, 0.3f));
	BTN_Effect = MakeChoice(TEXT("BTN_Effect"), TXT_Effect, FLinearColor(0.3f, 0.75f, 0.55f));
	SetDeckCounts(0, 0);
}

void UBWGeleonChoiceWidget::SetDeckCounts(int32 AttackCount, int32 EffectCount)
{
	if (TXT_Attack) TXT_Attack->SetText(FText::FromString(FString::Printf(TEXT("효과카드 안 씀\n공격카드 %d장 모두 사용"), AttackCount)));
	if (TXT_Effect) TXT_Effect->SetText(FText::FromString(FString::Printf(TEXT("효과카드 %d장 사용\n강화 카드는 처음 쓴 1종만"), EffectCount)));
}

void UBWGeleonChoiceWidget::Choose(bool bUseEffects)
{
	ABWPlayerController* PC = Cast<ABWPlayerController>(GetOwningPlayer());
	if (PC) PC->ConfirmGeleonMode(bUseEffects); // 확정되면 컨트롤러가 이 화면을 닫는다
}
