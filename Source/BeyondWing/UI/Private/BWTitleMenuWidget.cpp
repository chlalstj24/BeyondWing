#include "BeyondWing/UI/Public/BWTitleMenuWidget.h"
#include "BeyondWing/Core/Public/BWGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UBWTitleMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// [강제 포커스] 타이틀 화면이 켜지면 무조건 키보드/마우스 포커스를 받도록 설정
	SetIsFocusable(true);
}

void UBWTitleMenuWidget::StartNewGame(FName LevelName)
{
	UBWGameInstance* GI = Cast<UBWGameInstance>(GetGameInstance());
	if (GI)
	{
		// 1. 세계 지도 생성 (15층)
		GI->GenerateWorldMap(15);
		
		// 2. 게임 레벨로 이동
		UGameplayStatics::OpenLevel(GetWorld(), LevelName);
		
		UE_LOG(LogTemp, Warning, TEXT("[Title] Map Generated and Moving to Level: %s"), *LevelName.ToString());
	}
}

void UBWTitleMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayer(), EQuitPreference::Quit, false);
}
