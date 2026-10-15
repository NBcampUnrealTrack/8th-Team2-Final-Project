#include "UI/NUGameMenuMotion.h"
#include "Blueprint/UserWidget.h"
#include "Components/TileView.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"

namespace NUGameMenuMotion
{
namespace
{
 struct FPart
 {
  TWeakObjectPtr<UWidget> Widget;
  FVector2D Rest, Offset;
  float Opacity=1, Delay=0, Duration=.25f;
  FVector2D ExitStart;
  float ExitOpacity=1;
 };
 struct FState
 {
  TWeakObjectPtr<UUserWidget> Menu;
  TWeakObjectPtr<UTileView> Rows;
  TArray<FPart> Parts;
  float Time=0;
  bool Closing=false, Ready=false, Enabled=true;
  FSimpleDelegate Completion;
 };
 TMap<TWeakObjectPtr<UUserWidget>,TSharedPtr<FState>> States;
 FDelegateHandle TickHandle;
 float Ease(float X) { X=FMath::Clamp(X,0.f,1.f);return 1-FMath::Pow(1-X,3.f); }
 void Add(FState& S,UWidget* W,FVector2D Offset,float Delay,float Duration)
 {
  if(!W || S.Parts.ContainsByPredicate([W](const FPart& P){return P.Widget==W;}))return;
  FPart P;P.Widget=W;P.Rest=W->GetRenderTransform().Translation;P.Opacity=W->GetRenderOpacity();P.Offset=Offset;P.Delay=Delay;P.Duration=Duration;
  S.Parts.Add(P);
  W->SetRenderTranslation(P.Rest+Offset);W->SetRenderOpacity(0);
 }
 void GatherRows(FState& S)
 {
  if(auto* Rows=S.Rows.Get())
  {
   int32 Index=0;
   for(UUserWidget* Row:Rows->GetDisplayedEntryWidgets())
   {
    Add(S,Row,FVector2D(-6,0),.04f+.02f*Index,.18f);++Index;
   }
  }
 }
 void Tick(float Delta)
 {
  TArray<TSharedPtr<FState>> Snapshot;States.GenerateValueArray(Snapshot);
  bool Active=false;
  for(auto& S:Snapshot)
  {
   auto* Menu=S->Menu.Get();
   if(!Menu || !Menu->IsInViewport()) { Stop(Menu);continue; }
   if(S->Ready)continue;
   S->Time+=FMath::Max(Delta,0.f);
   if(!S->Closing)GatherRows(*S);
   for(auto& P:S->Parts)
   {
    auto* W=P.Widget.Get();if(!W)continue;
    if(S->Closing)
    {
     float T=Ease(S->Time/.15f);
     W->SetRenderTranslation(FMath::Lerp(P.ExitStart,P.Rest+P.Offset,T));
     W->SetRenderOpacity(P.ExitOpacity*(1-T));
    }
    else
    {
     float T=Ease((S->Time-P.Delay)/P.Duration);
     W->SetRenderTranslation(P.Rest+P.Offset*(1-T));W->SetRenderOpacity(P.Opacity*T);
    }
   }
   if(S->Closing && S->Time>=.15f)
   {
    S->Ready=true;FSimpleDelegate Done=MoveTemp(S->Completion);Done.ExecuteIfBound();
   }
   else if(S->Closing || S->Time<.5f)Active=true;
  }
  if(!Active && TickHandle.IsValid()) { FSlateApplication::Get().OnPreTick().Remove(TickHandle);TickHandle.Reset(); }
 }
 void EnsureTick()
 {
  if(!TickHandle.IsValid() && FSlateApplication::IsInitialized())TickHandle=FSlateApplication::Get().OnPreTick().AddStatic(&Tick);
 }
}
void Stop(UUserWidget* Menu)
{
 auto* Found=States.Find(Menu);
 if(Found)
 {
  auto S=*Found;
  for(auto& P:S->Parts)if(auto* W=P.Widget.Get()){W->SetRenderTranslation(P.Rest);W->SetRenderOpacity(P.Opacity);}
  if(Menu)Menu->SetIsEnabled(S->Enabled);
  States.Remove(Menu);
 }
 for(auto It=States.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
 if(States.IsEmpty() && TickHandle.IsValid() && FSlateApplication::IsInitialized()) {FSlateApplication::Get().OnPreTick().Remove(TickHandle);TickHandle.Reset();}
}
void Start(UUserWidget* Menu,UTileView* Rows)
{
 Stop(Menu);if(!Menu || !FSlateApplication::IsInitialized())return;
 auto S=MakeShared<FState>();S->Menu=Menu;S->Rows=Rows;S->Enabled=Menu->GetIsEnabled();
 Add(*S,Menu->GetWidgetFromName(TEXT("HubLeftFit")),FVector2D(-20,0),0,.25f);
 Add(*S,Menu->GetWidgetFromName(TEXT("HubFit")),FVector2D(16,0),.03f,.25f);
 Add(*S,Menu->GetWidgetFromName(TEXT("HubDeskFit")),FVector2D(0,10),.08f,.30f);
 Add(*S,Menu->GetWidgetFromName(TEXT("HubDepthForeground")),FVector2D::ZeroVector,.08f,.30f);
 GatherRows(*S);States.Add(Menu,S);EnsureTick();
}
bool DeferClose(UUserWidget* Menu,FSimpleDelegate Completion)
{
 auto* Found=States.Find(Menu);
 if(!Found || (*Found)->Ready || !Menu->GetWorld() || Menu->GetWorld()->bIsTearingDown)return false;
 auto S=*Found;if(S->Closing)return true;
 S->Closing=true;S->Time=0;S->Completion=MoveTemp(Completion);Menu->SetIsEnabled(false);
 for(auto& P:S->Parts)if(auto* W=P.Widget.Get()){P.ExitStart=W->GetRenderTransform().Translation;P.ExitOpacity=W->GetRenderOpacity();}
 EnsureTick();return true;
}
}
