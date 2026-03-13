#include "WowViewerGameMode.h"
#include "WowFlyCamera.h"
#include "WowViewerPlayerController.h"

AWowViewerGameMode::AWowViewerGameMode()
{
    DefaultPawnClass = AWowFlyCamera::StaticClass();
    PlayerControllerClass = AWowViewerPlayerController::StaticClass();
}
