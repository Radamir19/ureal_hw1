#include "SprintDemoSetupCommandlet.h"

#include "SprintDemoAnimInstance.h"
#include "SprintCharacter.h"
#include "SprintDemoHUD.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateAliasNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/GameModeBase.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogSprintDemoSetup, Log, All);

namespace
{
constexpr const TCHAR* CharacterPath = TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter");
constexpr const TCHAR* AnimPath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed");
constexpr const TCHAR* ContextPath = TEXT("/Game/Input/IMC_Default.IMC_Default");
constexpr const TCHAR* SprintPath = TEXT("/Game/Input/Actions/IA_Sprint");
constexpr const TCHAR* InteractPath = TEXT("/Game/Input/Actions/IA_Interact");

template <typename T>
T* AddNode(UEdGraph* Graph, int32 X, int32 Y)
{
    T* Node = NewObject<T>(Graph, NAME_None, RF_Transactional);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->NodePosX = X;
    Node->NodePosY = Y;
    return Node;
}

bool Connect(UEdGraphPin* Output, UEdGraphPin* Input)
{
    if (!Output || !Input || !Output->GetOwningNode()->GetSchema()->TryCreateConnection(Output, Input))
    {
        UE_LOG(LogSprintDemoSetup, Error, TEXT("Could not connect graph pins %s -> %s"),
            Output ? *Output->PinName.ToString() : TEXT("<null>"),
            Input ? *Input->PinName.ToString() : TEXT("<null>"));
        return false;
    }
    return true;
}

bool SaveAsset(UObject* Asset)
{
    UPackage* Package = Asset->GetOutermost();
    Package->MarkPackageDirty();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, Args);
    UE_LOG(LogSprintDemoSetup, Display, TEXT("%s %s"), bSaved ? TEXT("Saved") : TEXT("FAILED to save"), *Filename);
    return bSaved;
}

bool Compile(UBlueprint* Blueprint)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
    UE_LOG(LogSprintDemoSetup, Display, TEXT("Compiled %s: %d errors, %d warnings"), *Blueprint->GetName(), Results.NumErrors, Results.NumWarnings);
    return Results.NumErrors == 0 && Blueprint->Status != BS_Error;
}

UInputAction* CreateAction(const TCHAR* PackageName)
{
    UPackage* Package = CreatePackage(PackageName);
    Package->FullyLoad();
    const FName Name(*FPackageName::GetLongPackageAssetName(PackageName));
    UInputAction* Action = FindObject<UInputAction>(Package, *Name.ToString());
    if (!Action)
    {
        Action = NewObject<UInputAction>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
        FAssetRegistryModule::AssetCreated(Action);
    }
    Action->ValueType = EInputActionValueType::Boolean;
    return Action;
}

bool SetObjectProperty(UObject* Target, FName Name, UObject* Value)
{
    FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Target->GetClass(), Name);
    if (!Property)
    {
        UE_LOG(LogSprintDemoSetup, Error, TEXT("Missing property %s on %s"), *Name.ToString(), *Target->GetName());
        return false;
    }
    Property->SetObjectPropertyValue_InContainer(Target, Value);
    return true;
}

bool AddInteractionGraph(UBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    UK2Node_Event* BeginPlay = nullptr;
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node))
            {
                if (Event->CustomFunctionName == TEXT("HandleInteraction"))
                {
                    return true; // The commandlet can safely be run again.
                }
            }
            if (UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
            {
                if (Event->EventReference.GetMemberName() == TEXT("ReceiveBeginPlay"))
                {
                    BeginPlay = Event;
                }
            }
        }
    }

    UEdGraph* Graph = BeginPlay ? BeginPlay->GetGraph() :
        (Blueprint->UbergraphPages.Num() ? Blueprint->UbergraphPages[0].Get() : nullptr);
    if (!Graph)
    {
        Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("EventGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
    }
    if (!BeginPlay)
    {
        BeginPlay = AddNode<UK2Node_Event>(Graph, 0, 0);
        BeginPlay->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"), AActor::StaticClass());
        BeginPlay->bOverrideFunction = true;
        BeginPlay->AllocateDefaultPins();
    }

    FMulticastDelegateProperty* Delegate = FindFProperty<FMulticastDelegateProperty>(ASprintCharacter::StaticClass(), TEXT("OnInteract"));
    if (!Delegate)
    {
        UE_LOG(LogSprintDemoSetup, Error, TEXT("ASprintCharacter.OnInteract delegate is missing"));
        return false;
    }
    UK2Node_AddDelegate* Bind = AddNode<UK2Node_AddDelegate>(Graph, BeginPlay->NodePosX + 320, BeginPlay->NodePosY);
    Bind->SetFromProperty(Delegate, true, ASprintCharacter::StaticClass());
    Bind->AllocateDefaultPins();

    UK2Node_CustomEvent* Event = AddNode<UK2Node_CustomEvent>(Graph, BeginPlay->NodePosX, BeginPlay->NodePosY + 280);
    Event->CustomFunctionName = TEXT("HandleInteraction");
    Event->AllocateDefaultPins();
    Event->SetDelegateSignature(Delegate->SignatureFunction);

    UK2Node_CallFunction* Print = AddNode<UK2Node_CallFunction>(Graph, BeginPlay->NodePosX + 360, BeginPlay->NodePosY + 280);
    Print->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)));
    Print->AllocateDefaultPins();
    Print->FindPinChecked(TEXT("InString"))->DefaultValue = TEXT("Interaction triggered!");
    Print->FindPinChecked(TEXT("bPrintToScreen"))->DefaultValue = TEXT("true");
    Print->FindPinChecked(TEXT("bPrintToLog"))->DefaultValue = TEXT("true");
    Print->FindPinChecked(TEXT("Duration"))->DefaultValue = TEXT("2.0");

    // Insert the binding ahead of any existing BeginPlay logic without removing it.
    UEdGraphPin* BeginThen = BeginPlay->FindPinChecked(UEdGraphSchema_K2::PN_Then);
    const TArray<UEdGraphPin*> ExistingLinks = BeginThen->LinkedTo;
    BeginThen->BreakAllPinLinks();
    for (UEdGraphPin* Link : ExistingLinks)
    {
        if (!Connect(Bind->FindPinChecked(UEdGraphSchema_K2::PN_Then), Link)) return false;
    }
    return Connect(BeginThen, Bind->FindPinChecked(UEdGraphSchema_K2::PN_Execute)) &&
        Connect(Event->FindPinChecked(UK2Node_Event::DelegateOutputName), Bind->GetDelegatePin()) &&
        Connect(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then), Print->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
}

bool AddBoolTransition(UAnimStateNode* From, UAnimStateNode* To, bool bInvert)
{
    UAnimStateTransitionNode* Transition = AddNode<UAnimStateTransitionNode>(From->GetGraph(), (From->NodePosX + To->NodePosX) / 2, From->NodePosY + (bInvert ? 80 : -80));
    Transition->AllocateDefaultPins();
    Transition->PostPlacedNewNode();
    Transition->CreateConnections(From, To);
    Transition->CrossfadeDuration = 0.2f;

    UEdGraph* Rule = Transition->BoundGraph;
    UK2Node_VariableGet* GetSprint = AddNode<UK2Node_VariableGet>(Rule, -360, 0);
    GetSprint->VariableReference.SetSelfMember(TEXT("bIsSprinting"));
    GetSprint->AllocateDefaultPins();
    UEdGraphPin* Value = GetSprint->FindPinChecked(TEXT("bIsSprinting"));
    if (bInvert)
    {
        UK2Node_CallFunction* Not = AddNode<UK2Node_CallFunction>(Rule, -180, 0);
        Not->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool)));
        Not->AllocateDefaultPins();
        if (!Connect(Value, Not->FindPinChecked(TEXT("A")))) return false;
        Value = Not->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);
    }
    UAnimationTransitionGraph* TransitionGraph = CastChecked<UAnimationTransitionGraph>(Rule);
    return Connect(Value, TransitionGraph->GetResultNode()->FindPinChecked(TEXT("bCanEnterTransition")));
}

bool AddSprintState(UAnimBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    UAnimStateNode* WalkRun = nullptr;
    UAnimStateNode* Idle = nullptr;
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
            {
                UE_LOG(LogSprintDemoSetup, Display, TEXT("Animation state: %s / %s"), *Graph->GetName(), *State->GetStateName());
                if (State->GetStateName() == TEXT("Sprint")) return true;
                if (State->GetStateName() == TEXT("Walk / Run")) WalkRun = State;
            }
        }
    }
    if (!WalkRun)
    {
        UE_LOG(LogSprintDemoSetup, Error, TEXT("The standard ABP_Unarmed Walk / Run state was not found"));
        return false;
    }
    UEdGraph* Machine = WalkRun->GetGraph();
    for (UEdGraphNode* Node : Machine->Nodes)
    {
        if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
        {
            if (State->GetStateName() == TEXT("Idle")) Idle = State;
        }
    }
    UAnimSequence* Run = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"));
    if (!Run) return false;

    UAnimStateNode* Sprint = AddNode<UAnimStateNode>(Machine, WalkRun->NodePosX + 450, WalkRun->NodePosY);
    Sprint->AllocateDefaultPins();
    Sprint->PostPlacedNewNode();
    FBlueprintEditorUtils::RenameGraph(Sprint->BoundGraph, TEXT("Sprint"));

    UAnimGraphNode_SequencePlayer* Player = AddNode<UAnimGraphNode_SequencePlayer>(Sprint->BoundGraph, -300, 0);
    Player->Node.SetSequence(Run);
    Player->Node.SetLoopAnimation(true);
    Player->Node.SetPlayRate(1.5f);
    Player->AllocateDefaultPins();
    if (!Connect(Player->FindPinChecked(TEXT("Pose")), Sprint->GetPoseSinkPinInsideState())) return false;

    // UE 5.8 uses a Locomotion alias for jumps. Include Sprint in that alias so
    // the unchanged jump/fall/land rules also apply while sprinting.
    const TArray<TObjectPtr<UEdGraphNode>> ExistingNodes = Machine->Nodes;
    for (UEdGraphNode* Node : ExistingNodes)
    {
        if (UAnimStateAliasNode* Alias = Cast<UAnimStateAliasNode>(Node))
        {
            if (Alias->GetAliasedStates().Contains(WalkRun))
            {
                Alias->GetAliasedStates().Add(Sprint);
            }
        }
        // Keep direct jump/fall exits too, if this template uses them instead of aliases.
        if (UAnimStateTransitionNode* Existing = Cast<UAnimStateTransitionNode>(Node))
        {
            UAnimStateNodeBase* Next = Existing->GetNextState();
            if (Existing->GetPreviousState() == WalkRun && Next && Next != Idle && Next != Sprint)
            {
                UAnimStateTransitionNode* Copy = AddNode<UAnimStateTransitionNode>(Machine, Sprint->NodePosX, Sprint->NodePosY - 160);
                Copy->AllocateDefaultPins();
                Copy->BoundGraph = DuplicateObject<UEdGraph>(Existing->BoundGraph, Copy);
                Machine->SubGraphs.Add(Copy->BoundGraph);
                Copy->CrossfadeDuration = Existing->CrossfadeDuration;
                Copy->PriorityOrder = Existing->PriorityOrder;
                Copy->CreateConnections(Sprint, Next);
            }
        }
    }
    if (!AddBoolTransition(WalkRun, Sprint, false) || !AddBoolTransition(Sprint, WalkRun, true)) return false;
    if (Idle && !AddBoolTransition(Idle, Sprint, false)) return false;
    return true;
}

bool Validate(UBlueprint* Character, UAnimBlueprint* Animation, UInputMappingContext* Context)
{
    bool bValid = Character && Animation && Context && Character->ParentClass == ASprintCharacter::StaticClass() &&
        Animation->ParentClass == USprintDemoAnimInstance::StaticClass() && Character->Status != BS_Error && Animation->Status != BS_Error;
    bool bSprintMapped = false;
    bool bInteractMapped = false;
    UObject* CharacterDefaults = Character->GeneratedClass ? Character->GeneratedClass->GetDefaultObject() : nullptr;
    for (const FName PropertyName : {FName(TEXT("SprintAction")), FName(TEXT("InteractAction"))})
    {
        FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(ASprintCharacter::StaticClass(), PropertyName);
        const UInputAction* Action = CharacterDefaults && Property ? Cast<UInputAction>(Property->GetObjectPropertyValue_InContainer(CharacterDefaults)) : nullptr;
        bValid &= Action && Action->ValueType == EInputActionValueType::Boolean;
    }
    for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
    {
        bSprintMapped |= Mapping.Action && Mapping.Action->GetName() == TEXT("IA_Sprint") && Mapping.Key == EKeys::LeftShift;
        bInteractMapped |= Mapping.Action && Mapping.Action->GetName() == TEXT("IA_Interact") && Mapping.Key == EKeys::E;
    }
    bool bPrintFound = false;
    bool bDelegateFound = false;
    TArray<UEdGraph*> Graphs;
    Character->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_AddDelegate* Bind = Cast<UK2Node_AddDelegate>(Node))
                bDelegateFound |= Bind->GetPropertyName() == TEXT("OnInteract") && Bind->GetDelegatePin()->LinkedTo.Num() > 0;
            if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
            {
                UEdGraphPin* Message = Call->FindPin(TEXT("InString"));
                bPrintFound |= Call->FunctionReference.GetMemberName() == TEXT("PrintString") && Message && Message->DefaultValue == TEXT("Interaction triggered!");
            }
        }
    }
    bool bSprintState = false;
    bool bEnter = false;
    bool bExit = false;
    Graphs.Reset();
    Animation->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UAnimStateNode* State = Cast<UAnimStateNode>(Node)) bSprintState |= State->GetStateName() == TEXT("Sprint");
            if (UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node))
            {
                bEnter |= Transition->GetNextState() && Transition->GetNextState()->GetStateName() == TEXT("Sprint");
                bExit |= Transition->GetPreviousState() && Transition->GetPreviousState()->GetStateName() == TEXT("Sprint");
            }
        }
    }
    bValid &= bSprintMapped && bInteractMapped && bPrintFound && bDelegateFound && bSprintState && bEnter && bExit;
    UE_LOG(LogSprintDemoSetup, Display, TEXT("Validation: mappings=%d/%d, delegate=%d, PrintString=%d, Sprint=%d, transitions=%d/%d, result=%s"),
        bSprintMapped, bInteractMapped, bDelegateFound, bPrintFound, bSprintState, bEnter, bExit, bValid ? TEXT("PASS") : TEXT("FAIL"));
    return bValid;
}
}

USprintDemoSetupCommandlet::USprintDemoSetupCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 USprintDemoSetupCommandlet::Main(const FString& Params)
{
    UBlueprint* Character = LoadObject<UBlueprint>(nullptr, CharacterPath);
    UAnimBlueprint* Animation = LoadObject<UAnimBlueprint>(nullptr, AnimPath);
    UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
    if (!Character || !Animation || !Context)
    {
        UE_LOG(LogSprintDemoSetup, Error, TEXT("Required Third Person template assets are missing"));
        return 1;
    }
    if (FParse::Param(*Params, TEXT("ValidateOnly"))) return Validate(Character, Animation, Context) ? 0 : 1;

    UInputAction* Sprint = CreateAction(SprintPath);
    UInputAction* Interact = CreateAction(InteractPath);
    Context->UnmapAllKeysFromAction(Sprint);
    Context->UnmapAllKeysFromAction(Interact);
    Context->MapKey(Sprint, EKeys::LeftShift);
    Context->MapKey(Interact, EKeys::E);

    Character->ParentClass = ASprintCharacter::StaticClass();
    Animation->ParentClass = USprintDemoAnimInstance::StaticClass();
    FBlueprintEditorUtils::RefreshAllNodes(Animation);
    if (!Compile(Animation) || !AddSprintState(Animation) || !Compile(Animation) || !AddInteractionGraph(Character) || !Compile(Character)) return 1;
    UObject* Defaults = Character->GeneratedClass->GetDefaultObject();
    if (!SetObjectProperty(Defaults, TEXT("SprintAction"), Sprint) || !SetObjectProperty(Defaults, TEXT("InteractAction"), Interact)) return 1;
    for (const auto& Pair : TArray<TPair<FName, FString>>{
        {TEXT("JumpAction"), TEXT("/Game/Input/Actions/IA_Jump.IA_Jump")},
        {TEXT("MoveAction"), TEXT("/Game/Input/Actions/IA_Move.IA_Move")},
        {TEXT("LookAction"), TEXT("/Game/Input/Actions/IA_Look.IA_Look")},
        {TEXT("MouseLookAction"), TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook")}})
    {
        UObject* Action = LoadObject<UInputAction>(nullptr, *Pair.Value);
        if (!Action || !SetObjectProperty(Defaults, Pair.Key, Action)) return 1;
    }
    if (!SaveAsset(Sprint) || !SaveAsset(Interact) || !SaveAsset(Context) || !SaveAsset(Animation) || !SaveAsset(Character)) return 1;

    const TCHAR* RedirectedBlueprints[] =
    {
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode"),
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController")
    };
    for (const TCHAR* Path : RedirectedBlueprints)
    {
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
        if (!Blueprint || !Compile(Blueprint)) return 1;
        if (AGameModeBase* GameModeDefaults = Cast<AGameModeBase>(Blueprint->GeneratedClass->GetDefaultObject()))
        {
            GameModeDefaults->HUDClass = ASprintDemoHUD::StaticClass();
        }
        if (!SaveAsset(Blueprint)) return 1;
    }
    return Validate(Character, Animation, Context) ? 0 : 1;
}
