#include "WowViewerPlayerController.h"

AWowViewerPlayerController::AWowViewerPlayerController() { bShowMouseCursor = false; }

void AWowViewerPlayerController::BeginPlay()
{
    Super::BeginPlay();
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
}
