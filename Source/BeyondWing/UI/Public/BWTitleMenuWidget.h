#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWTitleMenuWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;

// 타이틀(메인) 화면. bUseCodeLayout이면 블루프린트 디자이너 내용 대신 코드로 화면을 만든다:
// 배경 키 아트(천천히 흐름) + 떠오르는 불티 + 가장자리 어둠 + 제목 + 메뉴(이어하기·새 게임·환경설정·게임 종료, 마우스를 올리면 빛 막대와 함께 밀려남).
// 문구와 그림은 아래 UPROPERTY — WBP_TitleMenu의 Class Defaults에서 바꾼다.
UCLASS()
class BEYONDWING_API UBWTitleMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 게임 시작: 삶의 방식(직업) 선택 화면을 연다 (고르면 지도 생성 후 LevelName으로 이동)
	UFUNCTION(BlueprintCallable, Category = "BW Title")
	void StartNewGame(FName LevelName);

	// 저장된 판을 불러와 지도로 간다
	UFUNCTION(BlueprintCallable, Category = "BW Title")
	void ContinueGame();

	UFUNCTION(BlueprintCallable, Category = "BW Title")
	void OpenSettings();

	// 게임 종료
	UFUNCTION(BlueprintCallable, Category = "BW Title")
	void QuitGame();

	// 비워 두면 /Game/HUD/BP/WBP_ClassSelect, 그것도 없으면 C++ 기본 위젯(UBWClassSelectWidget)을 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title")
	TSubclassOf<class UBWClassSelectWidget> ClassSelectWidgetClass;

	// 환경설정 화면 (비우면 C++ 기본 위젯)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title")
	TSubclassOf<class UBWSettingsWidget> SettingsWidgetClass;

	// 끄면 블루프린트 디자이너에 그린 화면을 그대로 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title")
	bool bUseCodeLayout = true;

	// 코드 화면의 '새 게임'·'이어하기'가 여는 레벨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title")
	FName NewGameLevel = TEXT("MapLevel");

	// ── 문구 ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText TitleText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText SubtitleText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText ContinueText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText StartText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText SettingsText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText QuitText;
	// 오른쪽 아래 작은 글 (버전 등). 비우면 안 보인다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText FooterText;
	// 이어할 판이 있는데 '새 게임'을 누르면 묻는 글과 두 버튼
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구", meta = (MultiLine = true)) FText ConfirmNewGameText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText ConfirmYesText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BW Title|문구") FText ConfirmNoText;

	// ── 모양 ──
	// 배경 그림 (화면을 꽉 채우고 넘치는 부분은 잘림)
	UPROPERTY(EditAnywhere, Category = "BW Title|Look") TSoftObjectPtr<UTexture2D> BackdropTexture;
	UPROPERTY(EditAnywhere, Category = "BW Title|Look") TSoftObjectPtr<UTexture2D> GlowTexture;
	UPROPERTY(EditAnywhere, Category = "BW Title|Look") TSoftObjectPtr<UTexture2D> VignetteTexture;
	UPROPERTY(EditAnywhere, Category = "BW Title|Look") FLinearColor AccentColor = FLinearColor(1.0f, 0.82f, 0.55f);
	UPROPERTY(EditAnywhere, Category = "BW Title|Look") int32 NumParticles = 40;
	// 배경이 천천히 커졌다 작아지는 정도와 한 번 왕복하는 데 걸리는 시간(초)
	UPROPERTY(EditAnywhere, Category = "BW Title|Look") float BackdropZoom = 0.05f;
	UPROPERTY(EditAnywhere, Category = "BW Title|Look") float BackdropCycleSeconds = 40.0f;

	UBWTitleMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultLayout();
	void RefreshContinue();
	void ShowConfirm(bool bShow);

	UPROPERTY(Transient) TObjectPtr<UImage> BackdropImage;
	UPROPERTY(Transient) TObjectPtr<UImage> FadeImage;
	UPROPERTY(Transient) TObjectPtr<UWidget> TitleBlock;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> MenuButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> MenuBars;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MenuTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Particles;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ContinueSummary;
	UPROPERTY(Transient) TObjectPtr<UWidget> ConfirmPanel;
	TArray<FVector4f> ParticleState; // x, y, 오르는 속도, 위상
	TArray<float> MenuHover;         // 버튼별 마우스 올림 정도 0~1
	TArray<bool> MenuEnabled;        // 이어하기는 저장이 있을 때만
	bool bParticlesPlaced = false;
	float BackdropAspect = 16.0f / 9.0f;
	float OpenTime = 0.0f;

	UFUNCTION() void OnContinueClicked() { ContinueGame(); }
	UFUNCTION() void OnStartClicked();
	UFUNCTION() void OnSettingsClicked() { OpenSettings(); }
	UFUNCTION() void OnQuitClicked() { QuitGame(); }
	UFUNCTION() void OnConfirmYes();
	UFUNCTION() void OnConfirmNo() { ShowConfirm(false); }
};
