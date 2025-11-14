"""
Filename: node_properties.py
Description: Python wrapper for Blueprint node property modification
Updated: 2025-11-03 - Extended to support semantic node editing (add_pin, remove_pin, type modifications, etc.)

================================================================================
IMPORTANT NOTE FOR FUTURE AI DEVELOPERS
================================================================================

This module implements SEMANTIC NODE EDITING for UE5.5 Blueprint nodes.
Two completely different approaches are used depending on the operation:

APPROACH 1: PUBLIC API METHODS (for pin management)
====================================================
Used by: add_pin(), remove_pin()
Nodes: ExecutionSequence, MakeArray, Switch, SwitchInteger

DO NOT use FindFProperty to modify internal properties like "NumOutputs" or "NumInputs"
Instead, use PUBLIC API METHODS defined in the K2Node classes:

  ExecutionSequence: GetThenPinGivenIndex, InsertPinIntoExecutionNode, RemovePinFromExecutionNode, CanRemoveExecutionPin
  MakeArray: AddInputPin, RemoveInputPin (from IK2Node_AddPinInterface)
  Switch: AddInputPin, RemoveInputPin (from IK2Node_AddPinInterface)

See: ExecutionSequenceEditor.cpp, MakeArrayEditor.cpp for working implementations

APPROACH 2: DIRECT PROPERTY ASSIGNMENT + ReconstructNode (for enum types)
===========================================================================
Used by: set_enum_type()
Nodes: SwitchEnum

DO access public properties directly: SwitchNode->Enum = TargetEnum
Then call: SwitchNode->ReconstructNode() to regenerate pins

See: SwitchEnumEditor.cpp for working implementation

COMMON MISTAKES TO AVOID:
=========================
❌ Using FindFProperty for pin management (returns nullptr in UE5.5)
❌ Manually manipulating pins without calling Modify() first
❌ Forgetting to call MarkBlueprintAsStructurallyModified() after changes
❌ Not calling Graph->NotifyGraphChanged() to update the UI
❌ Trying to use the same approach for all node types

CORRECT PATTERN ALWAYS INCLUDES:
================================
1. Cast node to correct type
2. Call Node->Modify() for undo/redo support
3. Perform the change using PUBLIC API methods
4. Update Blueprint: FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint)
5. Update Graph: Graph->NotifyGraphChanged()
6. Return true only if ALL steps succeeded

Each convenience function has detailed docstrings explaining the C++ implementation.
Read them before implementing new node types!
================================================================================
"""

import json
import logging
from typing import Dict, Any, Optional, List

logger = logging.getLogger("BlueprintGraph.NodeProperties")


def set_node_property(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    property_name: str,
    property_value: Any,
    function_name: Optional[str] = None,
    action: Optional[str] = None,
    **kwargs
) -> Dict[str, Any]:
    """
    Set a property on a Blueprint node or perform semantic node editing.

    This function supports both simple property modifications and advanced semantic
    node editing operations (pin management, type modifications, reference updates).

    ================================================================================
    BEST PRACTICES FOR IMPLEMENTATION (for future AIs) - READ THIS FIRST!
    ================================================================================

    GOOD PRACTICES TO FOLLOW:

    1. ALWAYS use PUBLIC API METHODS for pin management (NOT FindFProperty)
       WHY: FindFProperty returns nullptr for internal properties like "NumOutputs"/"NumInputs"
       DO THIS:
         - ExecutionSequence: GetThenPinGivenIndex(), InsertPinIntoExecutionNode(), RemovePinFromExecutionNode(), CanRemoveExecutionPin()
         - MakeArray: AddInputPin(), RemoveInputPin() (from IK2Node_AddPinInterface)
         - Switch: AddInputPin(), RemoveInputPin()
       REFERENCE: ExecutionSequenceEditor.cpp, MakeArrayEditor.cpp

    2. ALWAYS call Node->Modify() BEFORE making any changes
       WHY: Required for undo/redo support
       DO THIS:
         Node->Modify();  // Before changing anything

    3. ALWAYS call BOTH Blueprint and Graph update functions AFTER changes
       WHY: UI won't show changes without notification
       DO THIS:
         FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
         Graph->NotifyGraphChanged();

    4. ALWAYS validate BEFORE removing pins (enforce minimum constraints)
       WHY: Nodes must have at least 1 pin of each type
       DO THIS:
         For ExecutionSequence: if (!SeqNode->CanRemoveExecutionPin()) return false;
         For MakeArray: Count remaining pins, ensure >= 2 before removing

    5. ALWAYS use intermediate double variable for JSON number parsing
       WHY: Direct casting interferes with TryGetNumberField parameter resolution
       DO THIS:
         double NumElementsDouble = 0.0;
         if (!Params->TryGetNumberField(TEXT("num_elements"), NumElementsDouble))
             return error;
         int32 NumElements = static_cast<int32>(NumElementsDouble);

    6. ALWAYS implement fallback dispatch for actions supporting multiple node types
       WHY: Some actions (add_pin, remove_pin) work on ExecutionSequence AND MakeArray
       DO THIS:
         bool bSuccess = FExecutionSequenceEditor::AddExecutionPin(Node, Graph);
         if (!bSuccess) {
             bSuccess = FMakeArrayEditor::AddArrayElementPin(Node, Graph);
         }

    7. ALWAYS check your logs by recompiling AFTER code changes
       WHY: Plugin DLL doesn't auto-reload - old code still runs in Unreal
       DO THIS:
         1. Edit C++ code
         2. Visual Studio: Build -> Build Solution
         3. Unreal will auto-reload the plugin DLL
         4. New UE_LOG statements will appear

    ================================================================================
    COMMON PROBLEMS ENCOUNTERED DURING IMPLEMENTATION (if practices above not followed):
    ================================================================================

    1. PROBLEM: FindFProperty returns nullptr for internal properties
       SYMPTOMS:
         - Error log: "NumOutputs property NOT FOUND"
         - Error log: "NumOutputs is NOT an FIntProperty"
         - Function returns false silently
       ROOT CAUSE:
         - Properties like "NumOutputs", "NumInputs" are NOT publicly exposed via reflection
         - FindFProperty only works for reflected properties with UPROPERTY()
         - These properties are managed through public API methods instead
       SOLUTION:
         - DO NOT use FindFProperty for pin management
         - Use public API methods: GetThenPinGivenIndex(), InsertPinIntoExecutionNode(), etc.
         - See add_pin() docstring for specific APIs per node type
       REFERENCE IMPLEMENTATIONS: ExecutionSequenceEditor.cpp, MakeArrayEditor.cpp

    2. PROBLEM: Parameter type mismatch when parsing number fields
       SYMPTOMS:
         - Error: "Missing 'num_elements' parameter"
         - Error occurs even when parameter IS provided
         - Happens with "set_num_elements" action
       ROOT CAUSE:
         - Calling TryGetNumberField with wrong reference type
         - Example: TryGetNumberField(TEXT("num_elements"), (double&)NumElements)
         - With NumElements as int32, the cast interferes with parameter parsing
       SOLUTION:
         - Use intermediate double variable for parsing:
           double NumElementsDouble = 0.0;
           if (!Params->TryGetNumberField(TEXT("num_elements"), NumElementsDouble))
               return error;
           int32 NumElements = static_cast<int32>(NumElementsDouble);
         - Never cast the target variable - use an intermediate

    3. PROBLEM: Fallback dispatch doesn't trigger on function failure
       SYMPTOMS:
         - Action succeeds on ExecutionSequence but fails on MakeArray
         - Error: "Failed to add execution pin" when trying add_pin on MakeArray
         - Only ExecutionSequence handler is called, MakeArray is ignored
       ROOT CAUSE:
         - NodePropertyManager doesn't have fallback logic for add_pin/remove_pin
         - Dispatch only tries ExecutionSequence API, not MakeArray API
         - Both node types support add_pin but with different APIs
       SOLUTION:
         - Implement dispatch fallback in NodePropertyManager:
           bool bSuccess = FExecutionSequenceEditor::AddExecutionPin(Node, Graph);
           if (!bSuccess) {
               bSuccess = FMakeArrayEditor::AddArrayElementPin(Node, Graph);
           }
         - Try primary node type first, fall back to secondary types
       REFERENCE: NodePropertyManager.cpp, lines 280-286

    4. PROBLEM: Graph changes not visible in UI after node modification
       SYMPTOMS:
         - Function returns success: true
         - Blueprint graph shows no visible changes
         - Changes only appear after manual refresh/close-reopen
       ROOT CAUSE:
         - Missing Graph->NotifyGraphChanged() call
         - Or missing MarkBlueprintAsStructurallyModified() call
         - UI not informed of structural changes to Blueprint
       SOLUTION:
         - Always include both update calls AFTER pin management:
           FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
           Graph->NotifyGraphChanged();
         - These must be called even if the operation "succeeded"
       REFERENCE: All editor files (ExecutionSequenceEditor.cpp, etc.)

    5. PROBLEM: Debug logs not appearing in console output
       SYMPTOMS:
         - UE_LOG statements show no output in Unreal console
         - Color expected (yellow for Warning, red for Error) not visible
         - Logs from previous compilation appear, but new ones don't
       ROOT CAUSE:
         - Plugin DLL not reloaded after code changes
         - Unreal is running old compiled code, not the new code
       SOLUTION:
         - Recompile the plugin in Visual Studio: Build -> Build Solution
         - Unreal will auto-reload the plugin DLL
         - Logs from new code will appear after reload
         - User must do manual recompilation (not automated)

    6. PROBLEM: Minimum pin constraints not enforced
       SYMPTOMS:
         - Can remove ALL pins from a node
         - Resulting in node with zero execution/input pins
         - Blueprint breaks or behaves unexpectedly
       ROOT CAUSE:
         - Missing validation check before removal
         - ExecutionSequence/MakeArray require minimum 1 pin
         - Need CanRemoveExecutionPin() check before removing
       SOLUTION:
         - Before removing a pin, validate:
           For ExecutionSequence: if (!SeqNode->CanRemoveExecutionPin()) return false;
           For MakeArray: Count remaining input pins, ensure >= 2 before removing
         - Always keep at least 1 pin of each type
       REFERENCE: ExecutionSequenceEditor.cpp (lines 102-105), MakeArrayEditor.cpp (lines 93-97)

    ================================================================================

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint to modify
        node_id: ID of the node to modify
        property_name: Name of the property to set (legacy, used if action not specified)
        property_value: Value to set (legacy, will be JSON serialized)
        function_name: Name of function graph (None = EventGraph)
        action: Semantic action to perform (None = use legacy property_name mode)
        **kwargs: Additional parameters for semantic actions

    Returns:
        Dictionary containing:
            - success (bool): Whether operation succeeded
            - updated_property (str): Name of updated property (legacy mode)
            - action (str): Action performed (semantic mode)
            - details (dict): Action-specific details
            - error (str): Error message if failed

    Supported legacy properties by node type:
        - Print nodes: "message", "duration", "text_color"
        - Variable nodes: "variable_name"
        - Event nodes: "event_name"
        - All nodes: "pos_x", "pos_y", "comment"

    Supported semantic actions (Phase 1 - Pin Management):
        - "add_pin": Add a pin to a node
            Parameters: pin_type (str), pin_name (optional)
        - "remove_pin": Remove a pin from a node
            Parameters: pin_name (str)
        - "set_enum_type": Set enum type on a node
            Parameters: enum_type (str)

    Supported semantic actions (Phase 2 - Type Modification):
        - "set_pin_type": Change pin type on comparison nodes
            Parameters: pin_name (str), new_type (str)
        - "set_value_type": Change value type on select nodes
            Parameters: new_type (str)
        - "set_cast_target": Change cast target type
            Parameters: target_type (str)

    Supported semantic actions (Phase 3 - Reference Updates):
        - "set_function_call": Change function being called (DESTRUCTIVE)
            Parameters: target_function (str), target_class (optional)
        - "set_event_type": Change event type (DESTRUCTIVE)
            Parameters: event_type (str)

    Example (legacy mode):
        >>> result = set_node_property(
        ...     unreal,
        ...     "MyActorBlueprint",
        ...     "K2Node_1234567890",
        ...     "message",
        ...     "Hello World!"
        ... )

    Example (semantic mode - add pin):
        >>> result = set_node_property(
        ...     unreal,
        ...     "MyActorBlueprint",
        ...     "K2Node_Switch_123",
        ...     action="add_pin",
        ...     pin_type="SwitchCase"
        ... )

    Example (semantic mode - set enum type):
        >>> result = set_node_property(
        ...     unreal,
        ...     "MyActorBlueprint",
        ...     "K2Node_SwitchEnum_456",
        ...     action="set_enum_type",
        ...     enum_type="ECardinalDirection"
        ... )

    TROUBLESHOOTING CHECKLIST:
        [ ] Using public API methods, not FindFProperty? (Problem #1)
        [ ] Using intermediate double variable for number parsing? (Problem #2)
        [ ] Fallback dispatch implemented for add_pin/remove_pin? (Problem #3)
        [ ] Both MarkBlueprintAsStructurallyModified AND NotifyGraphChanged called? (Problem #4)
        [ ] Recompiled plugin after code changes? (Problem #5)
        [ ] Minimum pin constraints enforced? (Problem #6)
    """
    try:
        # Build parameters
        params = {
            "blueprint_name": blueprint_name,
            "node_id": node_id,
        }

        # If action is specified, use semantic editing mode
        if action is not None:
            params["action"] = action
            # Add all kwargs as additional parameters for the action
            params.update(kwargs)
            logger.debug(
                f"Performing semantic action '{action}' on node '{node_id}' in {blueprint_name}"
            )
        else:
            # Legacy mode: simple property modification
            params["property_name"] = property_name
            params["property_value"] = property_value
            logger.debug(
                f"Setting property '{property_name}' on node '{node_id}' in {blueprint_name}"
            )

        if function_name is not None:
            params["function_name"] = function_name

        response = unreal_connection.send_command("set_node_property", params)

        if response.get("success"):
            if action is not None:
                logger.info(
                    f"Successfully performed action '{action}' on node '{node_id}' in {blueprint_name}"
                )
            else:
                logger.info(
                    f"Successfully set '{property_name}' on node '{node_id}' in {blueprint_name}"
                )
        else:
            logger.error(
                f"Failed to modify node: {response.get('error', 'Unknown error')}"
            )

        return response

    except Exception as e:
        logger.error(f"Exception in set_node_property: {e}", exc_info=True)
        return {
            "success": False,
            "error": str(e)
        }


# ============================================================================
# Convenience functions for common semantic actions
# ============================================================================

def add_pin(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    pin_type: str,
    pin_name: Optional[str] = None,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Add a pin to a node (Phase 1: Pin Management).

    Supported on: Switch, SwitchInteger, ExecutionSequence, MakeArray, SwitchEnum

    IMPLEMENTATION PATTERN (for future AIs):
    ==========================================
    DO NOT attempt to modify internal properties like "NumOutputs", "NumInputs" via FindFProperty.
    Instead, use the PUBLIC API METHODS provided by each node type:

    For ExecutionSequence nodes:
      - GetThenPinGivenIndex(int32 Index) - Get pin at specific index
      - InsertPinIntoExecutionNode(UEdGraphPin* PinToInsertBefore, EPinInsertPosition) - Add new pin
      - RemovePinFromExecutionNode(UEdGraphPin* TargetPin) - Remove existing pin
      - CanRemoveExecutionPin() - Check if pin can be removed
      Implementation file: ExecutionSequenceEditor.cpp

    For MakeArray/MakeContainer nodes:
      - AddInputPin() - Add a new input element pin (from IK2Node_AddPinInterface)
      - RemoveInputPin(UEdGraphPin* Pin) - Remove an existing input pin
      Implementation file: MakeArrayEditor.cpp

    For Switch/SwitchInteger nodes:
      - AddInputPin() - Add a new switch case pin (from IK2Node_AddPinInterface)
      - RemoveInputPin(UEdGraphPin* Pin) - Remove a switch case pin
      Implementation file: SwitchEditor.cpp (if exists)

    For SwitchEnum nodes:
      - Set the Enum property directly: SwitchNode->Enum = TargetEnum
      - Call ReconstructNode() to regenerate pins based on enum values
      Implementation file: SwitchEnumEditor.cpp

    Critical steps ALWAYS required:
      1. Call Node->Modify() to mark for undo/redo
      2. Use ONLY public API methods (never FindFProperty for pin management)
      3. After structural changes, call:
         - FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint)
         - Graph->NotifyGraphChanged()
      4. Return true only if all steps succeeded

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        node_id: ID of the node
        pin_type: Type of pin to add ("SwitchCase", "ExecutionOutput", "ArrayElement", "EnumValue")
                  - "ExecutionOutput" for ExecutionSequence nodes
                  - "ArrayElement" for MakeArray nodes
                  - "SwitchCase" for Switch/SwitchInteger nodes
        pin_name: Optional name for the pin
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary with success status and pin details

    Example:
        >>> # Add a new execution pin to ExecutionSequence
        >>> result = add_pin(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "K2Node_ExecutionSequence_123",
        ...     "ExecutionOutput"
        ... )
        >>> if result["success"]:
        ...     print(f"New pin created")
        ... else:
        ...     print(f"Error: {result['error']}")

    Example:
        >>> # Add a new element to MakeArray
        >>> result = add_pin(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "K2Node_MakeArray_456",
        ...     "ArrayElement"
        ... )
    """
    kwargs = {"pin_type": pin_type}
    if pin_name is not None:
        kwargs["pin_name"] = pin_name

    return set_node_property(
        unreal_connection,
        blueprint_name,
        node_id,
        action="add_pin",
        function_name=function_name,
        **kwargs
    )


def remove_pin(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    pin_name: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Remove a pin from a node (Phase 1: Pin Management).

    IMPLEMENTATION PATTERN (for future AIs):
    ==========================================
    Same pattern as add_pin - use PUBLIC API METHODS, NOT FindFProperty:

    For ExecutionSequence nodes:
      - Use RemovePinFromExecutionNode(UEdGraphPin* TargetPin)
      - Always check CanRemoveExecutionPin() before removing
      - Minimum: must keep at least 1 execution pin
      Implementation file: ExecutionSequenceEditor.cpp

    For MakeArray/MakeContainer nodes:
      - Use RemoveInputPin(UEdGraphPin* Pin)
      - Minimum: must keep at least 1 input element pin
      - Always call BreakAllPinLinks() before removing
      Implementation file: MakeArrayEditor.cpp

    For Switch/SwitchInteger nodes:
      - Use RemoveInputPin(UEdGraphPin* Pin)
      - Minimum validation may apply
      Implementation file: SwitchEditor.cpp (if exists)

    Critical implementation steps:
      1. Find the pin by name: Node->FindPin(*PinName)
      2. Validate the pin exists
      3. Check if removal is allowed (e.g., CanRemoveExecutionPin())
      4. Call Node->Modify() to mark for undo/redo
      5. Break all connections: Pin->BreakAllPinLinks()
      6. Remove the pin using public API method
      7. Update Blueprint and Graph:
         - FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint)
         - Graph->NotifyGraphChanged()

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        node_id: ID of the node
        pin_name: Name of the pin to remove (must exist and be removable)
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary with success status

    Example:
        >>> # Remove an execution pin from ExecutionSequence
        >>> result = remove_pin(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "K2Node_ExecutionSequence_123",
        ...     "Then_2"
        ... )
        >>> if result["success"]:
        ...     print("Pin removed successfully")
        ... else:
        ...     print(f"Error: {result['error']}")

    Example:
        >>> # Remove an element from MakeArray
        >>> result = remove_pin(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "K2Node_MakeArray_456",
        ...     "[0]"
        ... )
    """
    return set_node_property(
        unreal_connection,
        blueprint_name,
        node_id,
        action="remove_pin",
        function_name=function_name,
        pin_name=pin_name
    )


def set_enum_type(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    enum_type: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Set enum type on a SwitchEnum node (Phase 1: Pin Management).

    IMPLEMENTATION PATTERN (for future AIs):
    ==========================================
    For SwitchEnum nodes, use DIRECT PROPERTY ASSIGNMENT (different from add_pin/remove_pin):

    Steps to implement:
      1. Cast node to UK2Node_SwitchEnum
      2. Find the enum by path using FindObject<UEnum>()
         - Try direct path first: FindObject<UEnum>(nullptr, *EnumPath)
         - If not found, try with "/Script/" prefix
         - If not found, try with "/Game/" prefix
      3. Call Node->Modify() to mark for undo/redo
      4. Set the enum property directly: SwitchNode->Enum = TargetEnum
      5. Call ReconstructNode() to regenerate pins based on enum values
      6. Update Blueprint and Graph:
         - FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint)
         - Graph->NotifyGraphChanged()

    Why this pattern works:
      - SwitchEnum nodes store the enum as a public UEnum* property
      - ReconstructNode() automatically generates case pins for each enum value
      - No manual pin manipulation needed (unlike add_pin/remove_pin)
      - This is the simplest and most reliable approach

    Implementation file: SwitchEnumEditor.cpp

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        node_id: ID of the node
        enum_type: Full path to enum type - supported formats:
                   - "/Script/CoreUObject.EAxis" (engine enums)
                   - "/Game/Enums/ECardinalDirection" (project enums)
                   - "EMyEnum" (will try both /Script/ and /Game/ prefixes)
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary with success status and generated pins

    Example:
        >>> # Set enum type on SwitchEnum node
        >>> result = set_enum_type(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "K2Node_SwitchEnum_789",
        ...     "/Game/Enums/ECardinalDirection"
        ... )
        >>> if result["success"]:
        ...     print(f"Enum type set. Generated pins: {result.get('generated_pins')}")
        ... else:
        ...     print(f"Error: {result['error']}")

    Example:
        >>> # Set with short name (will auto-resolve)
        >>> result = set_enum_type(
        ...     unreal,
        ...     "MyBlueprint",
        ...     "K2Node_SwitchEnum_789",
        ...     "ECardinalDirection"
        ... )
    """
    return set_node_property(
        unreal_connection,
        blueprint_name,
        node_id,
        action="set_enum_type",
        function_name=function_name,
        enum_type=enum_type
    )


def set_pin_type(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    pin_name: str,
    new_type: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Change pin type on a comparison node (Phase 2: Type Modification).

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        node_id: ID of the node
        pin_name: Name of pin ("A" or "B" for comparison nodes)
        new_type: New type ("int", "float", "string", "bool", "vector", etc.)
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary with success status
    """
    return set_node_property(
        unreal_connection,
        blueprint_name,
        node_id,
        action="set_pin_type",
        function_name=function_name,
        pin_name=pin_name,
        new_type=new_type
    )


def set_value_type(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    new_type: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Change value type on a Select node (Phase 2: Type Modification).

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        node_id: ID of the node
        new_type: New value type ("int", "float", "string", "bool", "vector", etc.)
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary with success status
    """
    return set_node_property(
        unreal_connection,
        blueprint_name,
        node_id,
        action="set_value_type",
        function_name=function_name,
        new_type=new_type
    )


def set_cast_target(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    target_type: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Change cast target type on a Cast node (Phase 2: Type Modification).

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        node_id: ID of the node
        target_type: Target class path or name (e.g., "ACharacter" or "/Game/Characters/MyCharacter.MyCharacter_C")
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary with success status
    """
    return set_node_property(
        unreal_connection,
        blueprint_name,
        node_id,
        action="set_cast_target",
        function_name=function_name,
        target_type=target_type
    )


def set_function_call(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    target_function: str,
    target_class: Optional[str] = None,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Change function being called on a CallFunction node (Phase 3: Reference Updates - DESTRUCTIVE).

    ⚠️ WARNING: This operation is destructive and will disconnect all pins connected to the
    affected pins. Only use when you know what you're doing.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        node_id: ID of the node
        target_function: Name of the function to call
        target_class: Optional class path containing the function
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary with success status and warning
    """
    kwargs = {"target_function": target_function}
    if target_class is not None:
        kwargs["target_class"] = target_class

    return set_node_property(
        unreal_connection,
        blueprint_name,
        node_id,
        action="set_function_call",
        function_name=function_name,
        **kwargs
    )


def set_event_type(
    unreal_connection,
    blueprint_name: str,
    node_id: str,
    event_type: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]:
    """
    Change event type on an Event node (Phase 3: Reference Updates - DESTRUCTIVE).

    ⚠️ WARNING: This operation is destructive and will disconnect all pins. Only use when
    you know what you're doing.

    Supported events: "BeginPlay", "Tick", "Destroyed", or any custom event name.

    Args:
        unreal_connection: Connection to Unreal Engine
        blueprint_name: Name of the Blueprint
        node_id: ID of the node
        event_type: Event type (e.g., "BeginPlay", "Tick", "Destroyed")
        function_name: Name of function graph (None = EventGraph)

    Returns:
        Dictionary with success status and warning
    """
    return set_node_property(
        unreal_connection,
        blueprint_name,
        node_id,
        action="set_event_type",
        function_name=function_name,
        event_type=event_type
    )
