#include "BeyondWing/UI/Public/BWUnitStatusWidget.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/Combat/Public/BWStatusRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UBWUnitStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!PB_HP && WidgetTree && !WidgetTree->RootWidget) BuildDefaultLayout();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBWUnitStatusWidget::BuildDefaultLayout()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	WidgetTree->RootWidget = Column;

	auto MakeText = [this](const TCHAR* Name, int32 Size) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0, 0, 0, 0.9f);
		Text->SetFont(Font);
		Text->SetJustification(ETextJustify::Center);
		return Text;
	};

	TXT_Intent = MakeText(TEXT("TXT_Intent"), 17);
	if (UVerticalBoxSlot* IntentSlot = Column->AddChildToVerticalBox(TXT_Intent))
	{
		IntentSlot->SetHorizontalAlignment(HAlign_Center);
		IntentSlot->SetPadding(FMargin(0, 0, 0, 2));
	}

	// HP 바: 흰 잔상 바 위에 빨간 바를 겹치고, 가운데에 숫자
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	BarBox->SetWidthOverride(150.0f);
	BarBox->SetHeightOverride(19.0f);
	UOverlay* Bars = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Bars"));
	BarBox->SetContent(Bars);
	PB_HPLag = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_HPLag"));
	PB_HPLag->SetFillColorAndOpacity(FLinearColor(1.0f, 0.95f, 0.85f, 0.9f));
	PB_HP = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_HP"));
	PB_HP->SetFillColorAndOpacity(FLinearColor(0.85f, 0.12f, 0.12f));
	// 위쪽 바의 배경은 투명해야 잔상이 비쳐 보인다
	FProgressBarStyle TopStyle = PB_HP->GetWidgetStyle();
	TopStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent);
	PB_HP->SetWidgetStyle(TopStyle);
	FProgressBarStyle LagStyle = PB_HPLag->GetWidgetStyle();
	LagStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.05f, 0.05f, 0.06f, 0.85f));
	PB_HPLag->SetWidgetStyle(LagStyle);
	TXT_HP = MakeText(TEXT("TXT_HP"), 12);
	for (UWidget* Child : TArray<UWidget*>{ PB_HPLag, PB_HP, TXT_HP })
	{
		if (UOverlaySlot* BarSlot = Bars->AddChildToOverlay(Child))
		{
			BarSlot->SetHorizontalAlignment(Child == TXT_HP ? HAlign_Center : HAlign_Fill);
			BarSlot->SetVerticalAlignment(Child == TXT_HP ? VAlign_Center : VAlign_Fill);
		}
	}
	if (UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(BarBox)) BarSlot->SetHorizontalAlignment(HAlign_Center);

	TXT_Status = MakeText(TEXT("TXT_Status"), 14);
	if (UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(TXT_Status))
	{
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
		StatusSlot->SetPadding(FMargin(0, 2, 0, 0));
	}
}

FString UBWUnitStatusWidget::DescribeIntent(const ABWCharacter* Enemy, FLinearColor& OutColor)
{
	OutColor = FLinearColor(1.0f, 0.45f, 0.4f);
	if (!Enemy) return FString();
	if (Enemy->IsStunned())
	{
		OutColor = FLinearColor(0.75f, 0.75f, 0.8f);
		return TEXT("기절 - 행동 없음");
	}
	const FBWEnemyPattern* Action = Enemy->GetCurrentAction();
	if (!Action) return FString();

	const FBWDiceRange Range = Enemy->MakeEnemyDuelSide().SkillRange;
	switch (Action->ActionType)
	{
	case EBWEnemyActionType::Heal:
		OutColor = FLinearColor(0.4f, 1.0f, 0.5f);
		return FString::Printf(TEXT("회복 %d"), Action->Value);
	case EBWEnemyActionType::BuffAttack:
		OutColor = FLinearColor(1.0f, 0.65f, 0.25f);
		return FString::Printf(TEXT("강화 +%d → 공격 %d~%d"), Action->Value, Range.Min + Action->Value, Range.Max + Action->Value);
	default:
		return FString::Printf(TEXT("공격 %d~%d"), Range.Min, Range.Max);
	}
}

void UBWUnitStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ABWCharacter* Char = OwnerChar.Get();
	if (!Char || Char->IsDying())
	{
		SetRenderOpacity(0.0f);
		return;
	}
	SetRenderOpacity(1.0f);

	const float Percent = Char->MaxHP > 0 ? FMath::Clamp((float)Char->CurrentHP / (float)Char->MaxHP, 0.0f, 1.0f) : 0.0f;
	if (PB_HP) PB_HP->SetPercent(Percent);

	// 잔상: 회복하면 바로 맞추고, 깎이면 잠깐 기다렸다가 천천히 따라 내려온다
	if (LastHP >= 0 && Char->CurrentHP < LastHP) LagWait = LagDelay;
	LastHP = Char->CurrentHP;
	if (ShownLag < 0.0f || ShownLag < Percent) ShownLag = Percent;
	else if (LagWait > 0.0f) LagWait -= InDeltaTime;
	else ShownLag = FMath::Max(Percent, ShownLag - LagSpeed * InDeltaTime);
	if (PB_HPLag) PB_HPLag->SetPercent(ShownLag);

	if (TXT_HP) TXT_HP->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Char->CurrentHP, Char->MaxHP)));

	if (TXT_Intent)
	{
		if (Char->bIsEnemy)
		{
			FLinearColor Color;
			TXT_Intent->SetText(FText::FromString(DescribeIntent(Char, Color)));
			TXT_Intent->SetColorAndOpacity(Color);
			TXT_Intent->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else TXT_Intent->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (TXT_Status)
	{
		TArray<FString> Parts;
		if (Char->bIsEnemy && Char->RolledSpeed > 0) Parts.Add(FString::Printf(TEXT("속도 %d"), Char->RolledSpeed));
		if (Char->Block > 0) Parts.Add(FString::Printf(TEXT("방어도 %d"), Char->Block));
		if (Char->Statuses.Num() > 0) Parts.Add(UBWStatusRules::DescribeStatuses(Char->Statuses));
		TXT_Status->SetText(FText::FromString(FString::Join(Parts, TEXT(" · "))));
		TXT_Status->SetColorAndOpacity(FLinearColor(0.85f, 0.9f, 1.0f));
		TXT_Status->SetVisibility(Parts.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
