#pragma once
#include "CoreMinimal.h"
class UUserWidget;
class UTileView;
namespace NUGameMenuMotion
{
 void Start(UUserWidget* Menu, UTileView* Rows);
 void Stop(UUserWidget* Menu);
 // True: exit is pending and owns Completion. False: caller may close now.
 bool DeferClose(UUserWidget* Menu, FSimpleDelegate Completion);
}
