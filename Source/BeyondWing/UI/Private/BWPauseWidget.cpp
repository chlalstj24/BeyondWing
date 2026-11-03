#include "BeyondWing/UI/Public/BWPauseWidget.h"
#include "BeyondWing/UI/Public/BWSkin.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "BeyondWing/UI/Public/BWSettingsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"

void UBWPauseWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		S->SetOffsets(FMargin(0.0f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.03f, 0.035f, 0.06f, 0.96f));
	Panel->SetPadding(FMargin(44.0f, 36.0f));
	BWSkin::StylePanel(Panel);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
	{
		S->SetAnchors(FAnchors(0.5f));
		S->SetAlignment(FVector2D(0.5f));
		S->SetAutoSize(true);
	}
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Panel->SetContent(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(FText::FromString(TEXT("일시 정지")));
	FSlateFontInfo Font = Title->GetFont();
	Font.Size = 30;
	Title->SetFont(Font);
	Title->SetColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.35f));
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Title))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	AddButton(Column, TEXT("계속하기"))->OnClicked.AddDynamic(this, &UBWPauseWidget::OnResumeClicked);
	AddButton(Column, TEXT("환경설정"))->OnClicked.AddDynamic(this, &UBWPauseWidget::OnSettingsClicked);
	AddButton(Column, TEXT("진행 저장 후 타이틀로"))->OnClicked.AddDynamic(this, &UBWPauseWidget::OnSaveQuitClicked);
}

UButton* UBWPauseWidget::AddButton(UVerticalBox* Column, const FString& Text)
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(340.0f);
	Size->SetHeightOverride(60.0f);
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = 20;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(FLinearColor(0.08f, 0.08f, 0.1f));
	Button->SetContent(Label);
	if (BWSkin::StyleButton(Button)) Label->SetColorAndOpacity(FLinearColor::White);
	Size->SetContent(Button);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Size)) S->SetPadding(FMargin(0.0f, 6.0f));
	return Button;
}

void UBWPauseWidget::Close()
{
	UGameplayStatics::SetGamePaused(this, false);
	RemoveFromParent();
}

void UBWPauseWidget::OnSettingsClicked()
{
	if (UBWSettingsWidget* Settings = CreateWidget<UBWSettingsWidget>(GetOwningPlayer(), UBWSettingsWidget::StaticClass())) Settings->AddToViewport(1000);
}

void UBWPauseWidget::OnSaveQuitClicked()
{
	if (UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance())) GI->SaveRun();
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(GetWorld(), TitleLevelName);
}
