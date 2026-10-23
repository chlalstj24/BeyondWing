#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BeyondWing/Core/Public/BWTypes.h"
#include "BeyondWing/Combat/Public/BWClassData.h"
#include "BWClassSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class USizeBox;
class UTexture2D;
class ABWCharacter;

// 삶의 방식(직업) 선택 화면. 타이틀의 '게임 시작'에서 열리고, 고르면 새 판을 시작해 지도로 간다.
// 카드 내용(문구·색·일러스트·능력 막대·수치·시작 덱)은 데이터 테이블 /Game/HUD/Data/DT_Classes (행 구조 FBWClassData)에서 읽는다.
// 화면 전체 문구(제목·버튼·안내)는 아래 '문구' 항목 — 위젯 블루프린트 WBP_ClassSelect의 Class Defaults에서 바꾼다.
// 위젯 블루프린트 없이도 쓸 수 있도록 화면을 코드로 만든다. 블루프린트로 꾸밀 때는 같은 이름의 위젯
// (BTN_Class0~4, TXT_Class0~4, TXT_Detail, BTN_Start, BTN_Back)을 두면 그대로 연결된다.
// 기본 화면: 직업을 누르면 그 카드가 넓어지며 일러스트·긴 설명이 커지고, 나머지 카드는 좁아져 옆으로 밀린다.
UCLASS()
class BEYONDWING_API UBWClassSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Class0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Class1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Class2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Class3;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Class4;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Class0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Class1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Class2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Class3;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Class4;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* TXT_Detail;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Start;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton* BTN_Back;

	// 카드 이름을 찾을 카드 데이터 테이블이 붙은 플레이어 말 블루프린트 (테이블에 시작 덱이 없으면 이 말의 직업별 배열을 쓴다)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select")
	TSoftClassPtr<ABWCharacter> PlayerCharacterClass;

	// 고르고 '시작'을 누르면 열 레벨 (타이틀이 넘겨 준다)
	UPROPERTY(BlueprintReadWrite, Category = "BW Class Select")
	FName TargetLevelName = TEXT("MapLevel");

	// ── 화면 문구 ── (카드 하나하나의 문구는 DT_Classes에서)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") FText ScreenTitle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") FText ScreenSubtitle;
	// 아직 안 골랐을 때 카드 아래 안내
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") FText PromptText;
	// 고른 뒤 카드 아래 줄. {Title}=카드 맨 위 글자, {Name}=이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") FText SelectedFormat;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") FText StartButtonText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") FText BackButtonText;
	// 펼친 카드의 시작 덱 줄 머리말
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") FText StartingDeckLabel;
	// 일러스트가 없을 때 빈 칸 글자
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") FText IllustrationPlaceholder;
	// 능력 막대 이름 4개 (DT_Classes의 RatingFirepower·Survival·Mobility·Growth 순서)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Class Select|문구") TArray<FText> RatingNames;

	// 카드 크기: 아무것도 안 골랐을 때 / 고른 카드 / 밀려난 카드
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float CardWidthNormal = 280.0f;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float CardWidthExpanded = 860.0f;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float CardWidthShrunk = 200.0f;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float CardHeight = 760.0f;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float ArtHeightNormal = 260.0f;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float ArtHeightExpanded = 400.0f;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float ArtHeightShrunk = 190.0f;
	// 펼치고 접히는 빠르기 (클수록 빠름)
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float ExpandSpeed = 12.0f;
	// 능력 막대 한 칸 크기·이름 칸 폭·글씨 크기: 평소 / 펼친 카드 (펼치는 동안 사이 값으로 커진다)
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") FVector2D BarCellSizeNormal = FVector2D(18.0f, 7.0f);
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") FVector2D BarCellSizeExpanded = FVector2D(28.0f, 11.0f);
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float BarLabelWidthNormal = 40.0f;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") float BarLabelWidthExpanded = 50.0f;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") int32 BarFontNormal = 12;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") int32 BarFontExpanded = 15;
	// 펼친 카드 오른쪽 설명 글씨 크기
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Layout") int32 LongTextFontSize = 15;

	// 배경 빛 번짐·카드 후광·떠다니는 불티에 쓰는 부드러운 원, 화면 가장자리 어둠
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Look") TSoftObjectPtr<UTexture2D> GlowTexture;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Look") TSoftObjectPtr<UTexture2D> VignetteTexture;
	UPROPERTY(EditAnywhere, Category = "BW Class Select|Look") int32 NumParticles = 36;

	UFUNCTION(BlueprintCallable, Category = "BW Class Select")
	void SelectClass(EClassType ClassType);

	// 고른 직업으로 새 판을 시작하고 지도로 간다
	UFUNCTION(BlueprintCallable, Category = "BW Class Select")
	void StartRun();

	// DT_Classes의 bSelectable (행이 없으면 고를 수 없음)
	UFUNCTION(BlueprintPure, Category = "BW Class Select")
	bool IsClassAvailable(EClassType ClassType) const;

	UBWClassSelectWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultLayout();
	void RefreshCards();
	void ApplyIllustrations();
	void UpdateCardSizes(float DeltaTime);
	void UpdateLook(const FGeometry& MyGeometry, float DeltaTime);
	FString DescribeStartingDeck(EClassType ClassType) const;

	EClassType Selected = EClassType::Common; // Common = 아직 안 고름

	// 카드 순서대로 DT_Classes의 행 (최대 5개). 테이블이 없으면 C++ 기본값으로 채운다
	UPROPERTY(Transient) TArray<FBWClassData> CardData;
	void LoadCardData();
	const FBWClassData* FindCard(EClassType ClassType) const;

	// 기본 화면이 만든 카드 부품 (카드 순서대로, 블루프린트 디자인이면 비어 있음)
	UPROPERTY(Transient) TArray<TObjectPtr<USizeBox>> CardBoxes;
	UPROPERTY(Transient) TArray<TObjectPtr<USizeBox>> ArtBoxes;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> ArtImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ArtPlaceholders;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TitleTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TagTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> LongTexts;
	TArray<float> CardWidths;
	TArray<float> ArtHeights;

	// 꾸밈: 카드 후광, 능력 막대 칸(카드마다 BarsPerCard×5), 배경 빛, 불티
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> CardGlows;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> BarCells;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> BarLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<USizeBox>> BarCellBoxes;
	UPROPERTY(Transient) TArray<TObjectPtr<USizeBox>> BarLabelBoxes;
	UPROPERTY(Transient) TArray<TObjectPtr<class UHorizontalBoxSlot>> InfoLeftSlots; // 정보 줄 왼쪽 칸 (펼치면 Fill → Auto)
	UPROPERTY(Transient) TObjectPtr<UImage> BackGlow;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Particles;
	TArray<FVector4f> ParticleState; // x, y, 오르는 속도, 위상
	FLinearColor ThemeColor = FLinearColor(0.9f, 0.75f, 0.4f);
	float OpenTime = 0.0f;
	bool bParticlesPlaced = false;

	UFUNCTION() void OnClass0Clicked();
	UFUNCTION() void OnClass1Clicked();
	UFUNCTION() void OnClass2Clicked();
	UFUNCTION() void OnClass3Clicked();
	UFUNCTION() void OnClass4Clicked();
	UFUNCTION() void OnStartClicked() { StartRun(); }
	UFUNCTION() void OnBackClicked() { RemoveFromParent(); }
};
