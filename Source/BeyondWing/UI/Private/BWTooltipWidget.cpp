#include "BeyondWing/UI/Public/BWTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"
#include "BeyondWing/Character/Public/BWCharacter.h"
#include "BeyondWing/UI/Public/BWSkin.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UBWTooltipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// 블루프린트 배치(고정 폭 상자 등) 대신, 제목·설명 글을 석판 판 하나에 다시 담는다
	// → 창 폭은 글 길이만큼 (MaxWrapWidth에서 줄바꿈), 지도·전투 어디서나 같은 모습
	if (!WidgetTree || !TXT_Title || !TXT_Description) return;
	TXT_Title->RemoveFromParent();
	TXT_Description->RemoveFromParent();
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BWTipFrame"));
	Frame->SetBrush(BWSkin::Plain(FLinearColor(0.07f, 0.085f, 0.105f, 0.97f), FLinearColor(0.68f, 0.7f, 0.74f, 0.75f), 1.5f));
	Frame->SetPadding(FMargin(16.0f, 10.0f, 18.0f, 12.0f));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BWTipColumn"));
	Frame->SetContent(Column);
	Column->AddChildToVerticalBox(TXT_Title);
	if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(TXT_Description)) S->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	TXT_Title->SetColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.5f));
	TXT_Description->SetColorAndOpacity(FLinearColor(0.92f, 0.93f, 0.96f));
	WidgetTree->RootWidget = Frame;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBWTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	for (UTextBlock* Text : { TXT_Title, TXT_Description })
	{
		if (!Text) continue;
		Text->SetAutoWrapText(false);
		Text->SetWrapTextAt(MaxWrapWidth);
	}
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot)) CanvasSlot->SetAutoSize(true);
}

void UBWTooltipWidget::SetTooltipTitle(const FString& InTitle)
{
	if (TXT_Title)
	{
		TXT_Title->SetText(FText::FromString(InTitle));
	}
}

void UBWTooltipWidget::SetTooltipDescription(const FString& RawDescription, int32 Value, int32 Dist, int32 Range)
{
	if (TXT_Description)
	{
		// 자동 줄넘김(부모 폭에 맞춰 접기)을 쓰면 창이 좁은 채로 굳는다 → 정해 둔 폭에서만 접어 내용만큼 넓어지게
		TXT_Description->SetAutoWrapText(false);
		TXT_Description->SetWrapTextAt(MaxWrapWidth);
		TXT_Description->SetJustification(ETextJustify::Left);
		
		FString Formatted = FormatTemplate(RawDescription, Value, Dist, Range);
		TXT_Description->SetText(FText::FromString(Formatted));
	}
}

void UBWTooltipWidget::SetupFromCardData(const FBWCardData& CardData, ABWCharacter* OwnerChar)
{
	SetTooltipTitle(CardData.CardName);
	
	int32 FinalDamage = 0;
	if (OwnerChar)
	{
		bool bCrit;
		FinalDamage = OwnerChar->CalculateFinalCardDamage(CardData, bCrit);
	}
	else
	{
		FinalDamage = FMath::RoundToInt(CardData.BaseDamage);
	}

	SetTooltipDescription(CardData.Description, FinalDamage, 0, CardData.Range);
}

void UBWTooltipWidget::SetupFromEnemyPattern(const FBWEnemyPattern& Pattern)
{
	SetTooltipTitle(Pattern.ActionName);
	// [수정] {V}=숫자 범위 최댓값, {R}=최솟값 (적 스킬은 합에서 이 범위로 굴린다)
	SetTooltipDescription(Pattern.Description, Pattern.SkillRange.Max, 0, Pattern.SkillRange.Min);
}

FString UBWTooltipWidget::FormatTemplate(const FString& Raw, int32 V, int32 D, int32 R)
{
	FString Result = Raw;
	Result = Result.Replace(TEXT("{V}"), *FString::FromInt(V));
	Result = Result.Replace(TEXT("{D}"), *FString::FromInt(D));
	Result = Result.Replace(TEXT("{R}"), *FString::FromInt(R));
	return Result;
}
