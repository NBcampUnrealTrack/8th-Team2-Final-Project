#include "ProjectMPaperZDMigrationLibrary.h"

#include "Animation/NUPaperZDAnimInstance.h"
#include "AnimNodes/PaperZDAnimNode_PlaySequence.h"
#include "AnimSequences/PaperZDAnimSequence_Flipbook.h"
#include "AnimSequences/PaperZDFlipbookAnimDataSource.h"
#include "AnimSequences/Sources/PaperZDAnimationSource_Flipbook.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Modules/ModuleManager.h"
#include "PaperFlipbook.h"
#include "PaperZDAnimBP.h"
#include "PaperZDAnimBPGeneratedClass.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace ProjectMPaperZDMigration
{
	static const TCHAR* PlayerFolder = TEXT("/Game/Art/Animations/PaperZD/Player");
	static const TCHAR* SlimeFolder = TEXT("/Game/Art/Animations/PaperZD/Slime");

	static FString MakeObjectPath(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return PackageName + TEXT(".") + AssetName;
	}

	template <typename T>
	static T* LoadAsset(const FString& PackageName)
	{
		return LoadObject<T>(nullptr, *MakeObjectPath(PackageName));
	}

	template <typename T>
	static T* CreateAsset(const FString& PackageName)
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}

		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		T* Asset = NewObject<T>(Package, T::StaticClass(), AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		if (Asset)
		{
			FAssetRegistryModule::AssetCreated(Asset);
			Asset->MarkPackageDirty();
		}
		return Asset;
	}

	template <typename T>
	static T* LoadOrCreateAsset(const FString& PackageName, TArray<FString>& Errors)
	{
		if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *MakeObjectPath(PackageName)))
		{
			T* Typed = Cast<T>(Existing);
			if (!Typed)
			{
				Errors.Add(FString::Printf(TEXT("%s 에셋 타입이 예상과 다릅니다."), *PackageName));
			}
			return Typed;
		}

		T* Created = CreateAsset<T>(PackageName);
		if (!Created)
		{
			Errors.Add(FString::Printf(TEXT("%s 생성에 실패했습니다."), *PackageName));
		}
		return Created;
	}

	static UPaperFlipbook* LoadFlipbook(const TCHAR* ObjectPath, TArray<FString>& Errors)
	{
		UPaperFlipbook* Flipbook = LoadObject<UPaperFlipbook>(nullptr, ObjectPath);
		if (!Flipbook)
		{
			Errors.Add(FString::Printf(TEXT("원본 플립북을 찾지 못했습니다: %s"), ObjectPath));
		}
		return Flipbook;
	}

	static bool SetSequenceData(UPaperZDAnimSequence_Flipbook* Sequence,
		const TArray<UPaperFlipbook*>& Flipbooks, bool bDirectional, TArray<FString>& Errors)
	{
		if (!Sequence)
		{
			return false;
		}

		FArrayProperty* ArrayProperty = Sequence->GetAnimDataSourceProperty();
		if (!ArrayProperty)
		{
			Errors.Add(FString::Printf(TEXT("%s의 AnimData 프로퍼티를 찾지 못했습니다."), *Sequence->GetPathName()));
			return false;
		}

		Sequence->Modify();
		FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Sequence));
		Helper.EmptyAndAddValues(Flipbooks.Num());
		for (int32 Index = 0; Index < Flipbooks.Num(); ++Index)
		{
			auto* Entry = reinterpret_cast<FPaperZDFlipbookAnimDataSource*>(Helper.GetRawPtr(Index));
			Entry->Animation = Flipbooks[Index];
		}

		Sequence->bDirectionalSequence = bDirectional;
		Sequence->DirectionalAngleOffset = 0.0f;
		Sequence->Category = TEXT("Locomotion");
		Sequence->MarkPackageDirty();
		return true;
	}

	static UPaperZDAnimSequence_Flipbook* BuildSequence(const FString& PackageName,
		UPaperZDAnimationSource_Flipbook* Source, const TArray<UPaperFlipbook*>& Flipbooks,
		bool bDirectional, TArray<FString>& Errors)
	{
		UPaperZDAnimSequence_Flipbook* Sequence = LoadOrCreateAsset<UPaperZDAnimSequence_Flipbook>(PackageName, Errors);
		if (!Sequence || !Source)
		{
			return nullptr;
		}

		Sequence->SetAnimSource(Source);
		return SetSequenceData(Sequence, Flipbooks, bDirectional, Errors) ? Sequence : nullptr;
	}

	static UPaperZDAnimBP* LoadOrCreateAnimBP(const FString& PackageName,
		UPaperZDAnimationSource_Flipbook* Source, TArray<FString>& Errors)
	{
		const FString ObjectPath = MakeObjectPath(PackageName);
		if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath))
		{
			UPaperZDAnimBP* AnimBP = Cast<UPaperZDAnimBP>(Existing);
			if (!AnimBP)
			{
				Errors.Add(FString::Printf(TEXT("%s 에셋 타입이 PaperZD AnimBP가 아닙니다."), *PackageName));
				return nullptr;
			}
			if (AnimBP->ParentClass != UNUPaperZDAnimInstance::StaticClass())
			{
				Errors.Add(FString::Printf(TEXT("%s는 생성기 소유 AnimBP가 아니어서 덮어쓰지 않았습니다."), *PackageName));
				return nullptr;
			}
			AnimBP->SupportedAnimationSource = Source;
			return AnimBP;
		}

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			Errors.Add(FString::Printf(TEXT("%s 패키지 생성에 실패했습니다."), *PackageName));
			return nullptr;
		}

		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		UPaperZDAnimBP* AnimBP = Cast<UPaperZDAnimBP>(FKismetEditorUtilities::CreateBlueprint(
			UNUPaperZDAnimInstance::StaticClass(), Package, AssetName, BPTYPE_Normal,
			UPaperZDAnimBP::StaticClass(), UPaperZDAnimBPGeneratedClass::StaticClass(),
			TEXT("ProjectMPaperZDMigration")));
		if (!AnimBP)
		{
			Errors.Add(FString::Printf(TEXT("%s AnimBP 생성에 실패했습니다."), *PackageName));
			return nullptr;
		}

		AnimBP->SupportedAnimationSource = Source;
		AnimBP->CreateGraph();
		FAssetRegistryModule::AssetCreated(AnimBP);
		AnimBP->MarkPackageDirty();
		return AnimBP;
	}

	static UEdGraphPin* FindPin(UEdGraphNode* Node, FName PinName, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName == PinName && Pin->Direction == Direction)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	static UEdGraphNode* CreateNode(UEdGraph* Graph, const TCHAR* ClassPath, int32 X, int32 Y,
		TArray<FString>& Errors)
	{
		UClass* NodeClass = StaticLoadClass(UEdGraphNode::StaticClass(), nullptr, ClassPath);
		if (!NodeClass)
		{
			Errors.Add(FString::Printf(TEXT("PaperZD 그래프 노드 클래스를 찾지 못했습니다: %s"), ClassPath));
			return nullptr;
		}

		FGraphNodeCreator<UEdGraphNode> Creator(*Graph);
		UEdGraphNode* Node = Creator.CreateNode(false, TSubclassOf<UEdGraphNode>(NodeClass));
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Creator.Finalize();
		return Node;
	}

	static UK2Node_VariableGet* CreateVariableGet(UEdGraph* Graph, FName VariableName,
		int32 X, int32 Y)
	{
		FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
		UK2Node_VariableGet* Node = Creator.CreateNode(false);
		Node->VariableReference.SetSelfMember(VariableName);
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Creator.Finalize();
		return Node;
	}

	static bool ConfigurePlayNode(UEdGraphNode* Node, UPaperZDAnimSequence_Flipbook* Sequence,
		TArray<FString>& Errors)
	{
		FStructProperty* AnimNodeProperty = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("AnimNode"));
		if (!AnimNodeProperty || AnimNodeProperty->Struct != FPaperZDAnimNode_PlaySequence::StaticStruct())
		{
			Errors.Add(TEXT("PlaySequence 노드의 AnimNode 구조체를 찾지 못했습니다."));
			return false;
		}

		auto* RuntimeNode = AnimNodeProperty->ContainerPtrToValuePtr<FPaperZDAnimNode_PlaySequence>(Node);
		RuntimeNode->AnimSequence = Sequence;
		RuntimeNode->bLoopAnimation = true;
		RuntimeNode->PlayRate = 1.0f;
		return true;
	}

	static bool Connect(UEdGraph* Graph, UEdGraphPin* OutputPin, UEdGraphPin* InputPin,
		const TCHAR* Description, TArray<FString>& Errors)
	{
		if (!OutputPin || !InputPin || !Graph->GetSchema()->TryCreateConnection(OutputPin, InputPin))
		{
			Errors.Add(FString::Printf(TEXT("그래프 연결에 실패했습니다: %s"), Description));
			return false;
		}
		return true;
	}

	static UEdGraphNode* ResetGraph(UPaperZDAnimBP* AnimBP, TArray<FString>& Errors)
	{
		AnimBP->CreateGraph();
		UEdGraph* Graph = AnimBP->GetGraph();
		if (!Graph)
		{
			Errors.Add(FString::Printf(TEXT("%s AnimationGraph를 찾지 못했습니다."), *AnimBP->GetPathName()));
			return nullptr;
		}

		UEdGraphNode* Sink = nullptr;
		TArray<UEdGraphNode*> ToRemove;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->GetClass()->GetName() == TEXT("PaperZDAnimGraphNode_Sink") && !Sink)
			{
				Sink = Node;
			}
			else if (Node)
			{
				ToRemove.Add(Node);
			}
		}

		for (UEdGraphNode* Node : ToRemove)
		{
			Graph->RemoveNode(Node, true, true);
		}
		if (Sink)
		{
			Sink->BreakAllNodeLinks();
		}
		else
		{
			Errors.Add(FString::Printf(TEXT("%s AnimationGraph의 Sink 노드를 찾지 못했습니다."), *AnimBP->GetPathName()));
		}
		return Sink;
	}

	static bool CompileAndValidate(UPaperZDAnimBP* AnimBP, int32 ExpectedNodeCount,
		TArray<FString>& Errors)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
		FKismetEditorUtilities::CompileBlueprint(AnimBP);
		if (AnimBP->Status == BS_Error)
		{
			Errors.Add(FString::Printf(TEXT("%s 컴파일에 실패했습니다."), *AnimBP->GetPathName()));
			return false;
		}
		if (!AnimBP->GetGraph() || AnimBP->GetGraph()->Nodes.Num() != ExpectedNodeCount)
		{
			Errors.Add(FString::Printf(TEXT("%s 그래프 노드 수가 예상과 다릅니다."), *AnimBP->GetPathName()));
			return false;
		}
		if (!Cast<UPaperZDAnimBPGeneratedClass>(AnimBP->GeneratedClass))
		{
			Errors.Add(FString::Printf(TEXT("%s 생성 클래스가 PaperZD AnimBP 클래스가 아닙니다."), *AnimBP->GetPathName()));
			return false;
		}
		AnimBP->MarkPackageDirty();
		return true;
	}

	static bool BuildPlayerGraph(UPaperZDAnimBP* AnimBP, UPaperZDAnimSequence_Flipbook* Idle,
		UPaperZDAnimSequence_Flipbook* Walk, TArray<FString>& Errors)
	{
		UEdGraphNode* Sink = ResetGraph(AnimBP, Errors);
		UEdGraph* Graph = AnimBP ? AnimBP->GetGraph() : nullptr;
		if (!Sink || !Graph)
		{
			return false;
		}

		UEdGraphNode* IdleNode = CreateNode(Graph,
			TEXT("/Script/PaperZDEditor.PaperZDAnimGraphNode_PlaySequence"), -900, -180, Errors);
		UEdGraphNode* WalkNode = CreateNode(Graph,
			TEXT("/Script/PaperZDEditor.PaperZDAnimGraphNode_PlaySequence"), -900, 100, Errors);
		UEdGraphNode* SelectNode = CreateNode(Graph,
			TEXT("/Script/PaperZDEditor.PaperZDAnimGraphNode_SelectByBool"), -550, 0, Errors);
		UEdGraphNode* DirectionNode = CreateNode(Graph,
			TEXT("/Script/PaperZDEditor.PaperZDAnimGraphNode_SetDirectionality"), -250, 0, Errors);
		UK2Node_VariableGet* MovingGet = CreateVariableGet(Graph,
			GET_MEMBER_NAME_CHECKED(UNUPaperZDAnimInstance, bIsMoving), -850, 300);
		UK2Node_VariableGet* DirectionGet = CreateVariableGet(Graph,
			GET_MEMBER_NAME_CHECKED(UNUPaperZDAnimInstance, DirectionInput), -520, 300);

		if (!IdleNode || !WalkNode || !SelectNode || !DirectionNode || !MovingGet || !DirectionGet
			|| !ConfigurePlayNode(IdleNode, Idle, Errors) || !ConfigurePlayNode(WalkNode, Walk, Errors))
		{
			return false;
		}

		bool bConnected = true;
		bConnected &= Connect(Graph, FindPin(IdleNode, TEXT("Animation"), EGPD_Output),
			FindPin(SelectNode, TEXT("FalseAnimation"), EGPD_Input), TEXT("Idle -> Select.False"), Errors);
		bConnected &= Connect(Graph, FindPin(WalkNode, TEXT("Animation"), EGPD_Output),
			FindPin(SelectNode, TEXT("TrueAnimation"), EGPD_Input), TEXT("Walk -> Select.True"), Errors);
		bConnected &= Connect(Graph, MovingGet->GetValuePin(),
			FindPin(SelectNode, TEXT("bSelectValue"), EGPD_Input), TEXT("bIsMoving -> Select"), Errors);
		bConnected &= Connect(Graph, FindPin(SelectNode, TEXT("Animation"), EGPD_Output),
			FindPin(DirectionNode, TEXT("Animation"), EGPD_Input), TEXT("Select -> Directionality"), Errors);
		bConnected &= Connect(Graph, DirectionGet->GetValuePin(),
			FindPin(DirectionNode, TEXT("Input"), EGPD_Input), TEXT("DirectionInput -> Directionality"), Errors);
		bConnected &= Connect(Graph, FindPin(DirectionNode, TEXT("Animation"), EGPD_Output),
			FindPin(Sink, TEXT("Result"), EGPD_Input), TEXT("Directionality -> Sink"), Errors);

		return bConnected && CompileAndValidate(AnimBP, 7, Errors);
	}

	static bool BuildSlimeGraph(UPaperZDAnimBP* AnimBP, UPaperZDAnimSequence_Flipbook* Idle,
		TArray<FString>& Errors)
	{
		UEdGraphNode* Sink = ResetGraph(AnimBP, Errors);
		UEdGraph* Graph = AnimBP ? AnimBP->GetGraph() : nullptr;
		if (!Sink || !Graph)
		{
			return false;
		}

		UEdGraphNode* IdleNode = CreateNode(Graph,
			TEXT("/Script/PaperZDEditor.PaperZDAnimGraphNode_PlaySequence"), -350, 0, Errors);
		if (!IdleNode || !ConfigurePlayNode(IdleNode, Idle, Errors))
		{
			return false;
		}

		const bool bConnected = Connect(Graph, FindPin(IdleNode, TEXT("Animation"), EGPD_Output),
			FindPin(Sink, TEXT("Result"), EGPD_Input), TEXT("Slime Idle -> Sink"), Errors);
		return bConnected && CompileAndValidate(AnimBP, 2, Errors);
	}
}

FString UProjectMPaperZDMigrationLibrary::RebuildPaperZDAssets()
{
	using namespace ProjectMPaperZDMigration;

	TArray<FString> Errors;
	FModuleManager::Get().LoadModule(TEXT("PaperZDEditor"));

	// Directional sequence entries are ordered around the top vector.
	TArray<UPaperFlipbook*> PlayerIdle = {
		LoadFlipbook(TEXT("/Game/Art/Sprites/Player/FB_Player_Idle_Up.FB_Player_Idle_Up"), Errors),
		LoadFlipbook(TEXT("/Game/Art/Sprites/Player/FB_Player_Idle_Right.FB_Player_Idle_Right"), Errors),
		LoadFlipbook(TEXT("/Game/Art/Sprites/Player/FB_Player_Idle_Down.FB_Player_Idle_Down"), Errors),
		LoadFlipbook(TEXT("/Game/Art/Sprites/Player/FB_Player_Idle_Left.FB_Player_Idle_Left"), Errors)
	};
	TArray<UPaperFlipbook*> PlayerWalk = {
		LoadFlipbook(TEXT("/Game/Art/Sprites/Player/FB_Player_Walk_Up.FB_Player_Walk_Up"), Errors),
		LoadFlipbook(TEXT("/Game/Art/Sprites/Player/FB_Player_Walk_Right.FB_Player_Walk_Right"), Errors),
		LoadFlipbook(TEXT("/Game/Art/Sprites/Player/FB_Player_Walk_Down.FB_Player_Walk_Down"), Errors),
		LoadFlipbook(TEXT("/Game/Art/Sprites/Player/FB_Player_Walk_Left.FB_Player_Walk_Left"), Errors)
	};
	UPaperFlipbook* SlimeFlipbook = LoadFlipbook(
		TEXT("/Game/Art/Sprites/Slime/FB_Slime_Idle.FB_Slime_Idle"), Errors);

	if (!Errors.IsEmpty())
	{
		return TEXT("FAILED\n") + FString::Join(Errors, TEXT("\n"));
	}

	UPaperZDAnimationSource_Flipbook* PlayerSource = LoadOrCreateAsset<UPaperZDAnimationSource_Flipbook>(
		FString(PlayerFolder) / TEXT("AS_Player"), Errors);
	UPaperZDAnimationSource_Flipbook* SlimeSource = LoadOrCreateAsset<UPaperZDAnimationSource_Flipbook>(
		FString(SlimeFolder) / TEXT("AS_Slime"), Errors);

	UPaperZDAnimSequence_Flipbook* PlayerIdleSequence = BuildSequence(
		FString(PlayerFolder) / TEXT("PZA_Player_Idle"), PlayerSource, PlayerIdle, true, Errors);
	UPaperZDAnimSequence_Flipbook* PlayerWalkSequence = BuildSequence(
		FString(PlayerFolder) / TEXT("PZA_Player_Walk"), PlayerSource, PlayerWalk, true, Errors);
	UPaperZDAnimSequence_Flipbook* SlimeIdleSequence = BuildSequence(
		FString(SlimeFolder) / TEXT("PZA_Slime_Idle"), SlimeSource, { SlimeFlipbook }, false, Errors);

	UPaperZDAnimBP* PlayerAnimBP = LoadOrCreateAnimBP(
		FString(PlayerFolder) / TEXT("ABPZD_Player"), PlayerSource, Errors);
	UPaperZDAnimBP* SlimeAnimBP = LoadOrCreateAnimBP(
		FString(SlimeFolder) / TEXT("ABPZD_Slime"), SlimeSource, Errors);

	if (Errors.IsEmpty())
	{
		BuildPlayerGraph(PlayerAnimBP, PlayerIdleSequence, PlayerWalkSequence, Errors);
		BuildSlimeGraph(SlimeAnimBP, SlimeIdleSequence, Errors);
	}

	if (!Errors.IsEmpty())
	{
		return TEXT("FAILED\n") + FString::Join(Errors, TEXT("\n"));
	}

	return TEXT("SUCCESS: PaperZD Source 2, Sequence 3, AnimBP 2, Player graph 7 nodes, Slime graph 2 nodes");
}
