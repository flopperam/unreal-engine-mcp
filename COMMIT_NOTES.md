# Commit Notes - Blueprint Graph Tools & TCP Fix

**Author**: Zoscran
**Date**: 2025-10-19
**Branch**: BlueprintGraph
**Type**: Feature Addition + Critical Bug Fix

---

## 📋 Summary

This commit adds **Blueprint Graph manipulation and inspection tools** to the Unreal Engine MCP server, enabling AI-driven programmatic creation, connection, and analysis of Blueprint nodes. Also includes a **critical TCP communication fix** for reliable transmission of large JSON responses.

**New Capabilities**:
- ✅ Create Blueprint nodes (Events, Print, Variables)
- ✅ Connect nodes to build execution flow
- ✅ Create and manage Blueprint variables
- ✅ Inspect Blueprint content (variables, functions, components)
- ✅ Analyze Blueprint graph structure and connections
- ✅ Reliable transmission of large JSON responses (TCP partial send bug fixed)

---

## 🎯 What Was Added

### 1. New MCP Tools (7 tools total)

#### Creation Tools (3 tools)
| Tool | Description | Implementation |
|------|-------------|----------------|
| `add_node` | Add nodes to Blueprint graphs | C++: `EpicUnrealMCPBlueprintGraphCommands.cpp`, Python: `node_manager.py` |
| `connect_nodes` | Connect Blueprint nodes | C++: `EpicUnrealMCPBlueprintGraphCommands.cpp`, Python: `connector_manager.py` |
| `create_variable` | Create Blueprint variables | C++: `EpicUnrealMCPBlueprintGraphCommands.cpp`, Python: `variable_manager.py` |

#### Inspection Tools (4 tools - integrated from GuBee33)
| Tool | Description | Implementation |
|------|-------------|----------------|
| `read_blueprint_content` | Read complete Blueprint content | C++: GuBee33's code, Python: `graph_inspector.py` |
| `analyze_blueprint_graph` | Analyze Blueprint graph structure | C++: GuBee33's code, Python: `graph_inspector.py` |
| `get_blueprint_variable_details` | Get variable details | C++: GuBee33's code, Python: `graph_inspector.py` |
| `get_blueprint_function_details` | Get function details | C++: GuBee33's code, Python: `graph_inspector.py` |

### 2. C++ Implementation

**New Files**:
```
FlopperamUnrealMCP/Plugins/UnrealMCP/Source/UnrealMCP/
├── Private/Commands/
│   └── EpicUnrealMCPBlueprintGraphCommands.cpp    (All 7 Blueprint Graph handlers)
└── Public/Commands/
    └── EpicUnrealMCPBlueprintGraphCommands.h       (Blueprint Graph interface)
```

**Modified Files**:
- `EpicUnrealMCPBridge.h` - Added BlueprintGraphCommands declaration
- `EpicUnrealMCPBridge.cpp` - Added BlueprintGraphCommands instantiation and routing
- `MCPServerRunnable.cpp` - **CRITICAL TCP FIX** (2 locations)
- `EpicUnrealMCPBlueprintCommands.cpp` - Removed temporary handlers

### 3. Python MCP Server Integration

**New Files**:
```
Python/helpers/blueprint_graph/
├── __init__.py
├── node_manager.py
├── connector_manager.py
├── variable_manager.py
└── graph_inspector.py          (4 inspection tool wrappers)
```

---

## 🐛 Critical Bug Fix: TCP Partial Send

### Problem Discovered
During testing of `analyze_blueprint_graph`, the tool was timing out with responses between 600-4096 bytes. Investigation revealed a fundamental TCP socket bug in the C++ plugin.

**Root Cause:**
```cpp
// OLD CODE (BUGGY) - MCPServerRunnable.cpp
int32 BytesSent = 0;
ClientSocket->Send((uint8*)TCHAR_TO_UTF8(*Response), Response.Len(), BytesSent);
// ⚠️ PROBLEM: No verification that BytesSent == Response.Len()
// ⚠️ PROBLEM: TCHAR_TO_UTF8(*Response) creates temporary buffer that may be destroyed
```

**Why it failed:**
- TCP `Send()` can return `true` but only send a partial amount of data
- Example: Request to send 3500 bytes → `Send()` returns `true` but `BytesSent = 2048`
- Remaining 1452 bytes are NEVER sent
- Python MCP server waits for complete JSON → timeout after 60 seconds

**Why some functions still worked:**
- Small messages (< 600 bytes): Fit in single TCP packet → sent completely
- Very large messages (> 10KB): Socket blocks until kernel buffer space available → sent completely
- Medium messages (600-4096 bytes): **CRITICAL ZONE** → partial send when buffer partially full → timeout

### Solution Implemented
Added proper loop to guarantee complete data transmission:

```cpp
// NEW CODE (FIXED) - MCPServerRunnable.cpp lines 333-347, 102-122
FTCHARToUTF8 UTF8Response(*Response);           // Persistent buffer
const uint8* DataToSend = (const uint8*)UTF8Response.Get();
int32 TotalDataSize = UTF8Response.Length();
int32 TotalBytesSent = 0;

// Loop until ALL data is sent
while (TotalBytesSent < TotalDataSize)
{
    int32 BytesSent = 0;
    Client->Send(DataToSend + TotalBytesSent,      // Offset pointer
                 TotalDataSize - TotalBytesSent,    // Remaining bytes
                 BytesSent);
    TotalBytesSent += BytesSent;

    UE_LOG(LogTemp, Display, TEXT("Sent %d bytes (%d/%d total)"),
           BytesSent, TotalBytesSent, TotalDataSize);
}
// ✅ Guaranteed: TotalBytesSent == TotalDataSize
```

**Fixed in 2 locations:**
1. `MCPServerRunnable.cpp` lines 102-122 (main thread handler)
2. `MCPServerRunnable.cpp` lines 333-347 (ProcessMessage function)

### Impact & Validation

**Before Fix:**
- ❌ `analyze_blueprint_graph` - Timeout
- ⚠️ Medium-sized responses unreliable

**After Fix:**
- ✅ All message sizes transmit reliably
- ✅ `analyze_blueprint_graph` works perfectly
- ✅ **Zero regressions** - All existing tools validated

**Regression Testing Results:**
- ✅ `get_actors_in_level`: 138 actors (~13,000 bytes JSON) - SUCCESS
- ✅ `get_available_materials`: 67 materials (~15-20KB JSON) - SUCCESS
- ✅ `read_blueprint_content`: Complex Blueprint data - SUCCESS
- ✅ `analyze_blueprint_graph`: Full graph analysis (was failing) - SUCCESS
- ✅ All 27 existing MCP tools tested - NO REGRESSIONS

---

## ✅ Testing Status

### Creation Tools
- ✅ `add_node` - Creates Print, Event, Variable nodes
- ✅ `connect_nodes` - Connects execution and data pins
- ✅ `create_variable` - Creates typed variables (bool, int, float, string, vector, rotator)

### Inspection Tools
- ✅ `read_blueprint_content` - Returns variables, functions, event graph, components
- ✅ `analyze_blueprint_graph` - Returns detailed graph with nodes and connections
- ✅ `get_blueprint_variable_details` - Returns variable types, defaults, metadata
- ✅ `get_blueprint_function_details` - Returns function signature and graph

### TCP Fix Validation
- ✅ Large responses tested (13KB, 15-20KB JSON)
- ✅ No regressions on existing 27 tools

**Test Blueprint**: `BP_TestGraphInspection`

---

## 📝 File Changes

### Added (3 files)
1. `EpicUnrealMCPBlueprintGraphCommands.cpp`
2. `EpicUnrealMCPBlueprintGraphCommands.h`
3. `Python/helpers/blueprint_graph/graph_inspector.py`

### Modified (6 files)
1. `EpicUnrealMCPBridge.h`
2. `EpicUnrealMCPBridge.cpp`
3. `MCPServerRunnable.cpp` - **TCP FIX**
4. `EpicUnrealMCPBlueprintCommands.cpp`
5. `Python/helpers/blueprint_graph/__init__.py`
6. `README.md`

---

## 🎯 Total MCP Tools

**Before**: 27 tools
**After**: 34 tools (+7 Blueprint Graph tools)

---

## 📞 Credits

- **Creation Tools**: Zoscran
- **Inspection Tools**: GuBee33 (cherry-picked)
- **TCP Bug Fix**: Zoscran

---

**Ready for Commit** ✅

---

# Commit Notes - Blueprint Node Creation, Deletion & Property Management

**Author**: Zoscran
**Date**: 2025-10-26
**Branch**: BlueprintGraph
**Type**: Feature Addition

---

## 📋 Summary

This commit adds three essential Blueprint node manipulation tools that complete the core node lifecycle for programmatic Blueprint development:
- **add_event_node**: Create event nodes (ReceiveBeginPlay, ReceiveTick, etc.) in Blueprint graphs
- **delete_node**: Remove nodes from Blueprint graphs (EventGraph or function graphs)
- **set_node_property**: Modify properties of existing nodes (message, position, comments, variable names)

These tools enable complete node lifecycle management: Creation → Connection → Deletion → Property Modification.

**Total MCP Tools**: 34 → 37 (+3 new tools)

---

## 🎯 What Was Added

### 1. New MCP Tools (3 tools)

| Tool | Description |
|------|-------------|
| `add_event_node` | Create specialized event nodes in Blueprint graphs (ReceiveBeginPlay, ReceiveTick, ReceiveDestroyed, etc.) |
| `delete_node` | Remove nodes from Blueprint graphs with proper link disconnection |
| `set_node_property` | Modify node properties after creation (message text, position, comments, variable names) |

### 2. C++ Implementation

**New Files** (6 files):
- `NodeDeleter.h` / `NodeDeleter.cpp` - Node deletion with link management
- `NodePropertyManager.h` / `NodePropertyManager.cpp` - Node property modification
- BlueprintGraph/ directory structure established for node tools

**Modified Files** (3 files):
- `EpicUnrealMCPBlueprintGraphCommands.h` - Added handler declarations
- `EpicUnrealMCPBlueprintGraphCommands.cpp` - Added handler implementations
- `EpicUnrealMCPBridge.cpp` - Added command routing for new tools

### 3. Python MCP Server Integration

**New Files** (2 files):
- `node_deleter.py` - delete_node MCP tool wrapper
- `node_properties.py` - set_node_property MCP tool wrapper

**Modified Files** (1 file):
- `unreal_mcp_server_advanced.py` - Added tool imports and decorators

---

## 🔧 Tool Details

### add_event_node

**Purpose**: Create specialized event nodes in Blueprint graphs

**Function Signature**:
```python
add_event_node(
    blueprint_name: str,
    event_name: str,
    pos_x: float = 0,
    pos_y: float = 0
) -> Dict[str, Any]
```

**Supported Events**:
- ReceiveBeginPlay, ReceiveTick, ReceiveEndPlay
- ReceiveDestroyed, ReceiveAnyDamage
- ReceiveActorBeginOverlap, ReceiveActorEndOverlap

**Implementation Details**:
- Creates UK2Node_Event nodes with proper EventReference setup
- Prevents duplicate event nodes in same graph
- Properly allocates execution pins
- Marks Blueprint as modified

---

### delete_node

**Purpose**: Remove nodes from Blueprint graphs to enable iterative development and error correction

**Function Signature**:
```python
delete_node(
    blueprint_name: str,
    node_id: str,
    function_name: Optional[str] = None
) -> Dict[str, Any]
```

**Implementation Details**:
- Locates nodes by NodeGuid or node name
- Disconnects all node links (execution and data) before removal
- Supports both EventGraph and function-specific graphs
- Updates graph and notifies Blueprint editor of changes

---

### set_node_property

**Purpose**: Modify node properties after creation to update behavior and appearance

**Function Signature**:
```python
set_node_property(
    blueprint_name: str,
    node_id: str,
    property_name: str,
    property_value: Any,
    function_name: Optional[str] = None
) -> Dict[str, Any]
```

**Supported Properties**:
- **Print nodes**: "message", "duration" (via pin modification)
- **Variable nodes**: "variable_name" (Get/Set nodes)
- **All nodes**: "pos_x", "pos_y", "comment" (position and comments)

**Implementation Details**:
- Handles Print node properties via FindPin("InString")
- Handles Variable Get/Set nodes via VariableReference
- Handles position and comment for all node types
- Extensible pattern for node type-specific properties

---

## ✅ Testing Status

**All tools tested and functional**:
- ✅ add_event_node - Event node creation verified
- ✅ delete_node - Node removal and link disconnection verified
- ✅ set_node_property - Property modification (message, position, comment) verified
- ✅ Error handling for invalid nodes validated
- ✅ No regressions on existing 34 MCP tools

---

## 📝 File Changes Summary

### Added (8 files)
```
FlopperamUnrealMCP/Plugins/UnrealMCP/Private/Commands/BlueprintGraph/
  ├── NodeDeleter.cpp
  └── NodePropertyManager.cpp
FlopperamUnrealMCP/Plugins/UnrealMCP/Public/Commands/BlueprintGraph/
  ├── NodeDeleter.h
  └── NodePropertyManager.h
UnrealMCP/Source/UnrealMCP/... (synchronized copies)
Python/helpers/blueprint_graph/
  ├── node_deleter.py
  └── node_properties.py
```

### Modified (4 files)
```
EpicUnrealMCPBlueprintGraphCommands.h (handlers added)
EpicUnrealMCPBlueprintGraphCommands.cpp (implementations added)
EpicUnrealMCPBridge.cpp (routing added)
unreal_mcp_server_advanced.py (tool imports/decorators)
```

---

## 🎯 Node Lifecycle Tools Summary

**Complete node manipulation tools now available**:
1. add_node - Create generic nodes
2. add_event_node - Create event nodes
3. delete_node - Remove nodes
4. set_node_property - Modify node properties
5. connect_nodes - Connect nodes

**Total MCP Tools Summary**:
- Before: 34 tools
- After: 37 tools (+3 new)

---

## 📞 Credits

- **Implementation**: Zoscran

---

**Ready for Commit** ✅
