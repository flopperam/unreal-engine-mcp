# Blueprint Graph Programming Guide

This guide explains how to programmatically create and manipulate Blueprint graphs using the MCP server. Build complex Blueprint logic through AI commands without opening the Blueprint editor.

## What is Blueprint Graph Programming?

Blueprint Graph Programming allows you to:
- **Add nodes** to Blueprint event graphs (Print, Events, Variables, etc.)
- **Connect nodes** to create execution and data flow
- **Create variables** with custom types and default values
- **Build complete Blueprint logic** through natural language commands

This is perfect for:
- Automating repetitive Blueprint setup
- Creating Blueprint templates programmatically
- Rapid prototyping of game logic
- Educational purposes and tutorials

---

## Available Tools

### Creation Tools
| Tool | Description | Use Case |
|------|-------------|----------|
| `add_node` | Add a node to a Blueprint graph | Create Print nodes, Events, Variable Get/Set |
| `connect_nodes` | Connect two nodes together | Build execution flow and data connections |
| `disconnect_nodes` | Disconnect two connected nodes | Remove connections between nodes |
| `create_variable` | Create a new Blueprint variable | Add properties to your Blueprints |

### Inspection Tools
| Tool | Description | Use Case |
|------|-------------|----------|
| `read_blueprint_content` | Read complete Blueprint structure | Get all variables, functions, components, event graph |
| `analyze_blueprint_graph` | Analyze graph nodes and connections | Detailed graph structure with pins and connections |
| `get_blueprint_variable_details` | Get variable information | Variable types, defaults, metadata |
| `get_blueprint_function_details` | Get function information | Function signatures, parameters, graphs |

---

## Tool Reference

### `add_node`

Add a node to a Blueprint graph (EventGraph or function graph).

Create various types of K2Nodes in a Blueprint's event graph or function graph.
Supports 23 node types organized by category.

**Parameters**:
```python
add_node(
    blueprint_name: str,      # Name of the Blueprint to modify
    node_type: str,           # Type of node to create. Supported types (23 total):

        CONTROL FLOW:
            "Branch" - Conditional execution (if/then/else)
            "Comparison" - Arithmetic/logical operators (==, !=, <, >, AND, OR, etc.)
                ℹ️ Types can be changed via set_node_property with action="set_pin_type"
            "Switch" - Switch on byte/enum value with cases
                ℹ️ Creates 1 pin at creation; add more via set_node_property with action="add_pin"
            "SwitchEnum" - Switch on enum type (auto-generates pins per enum value)
                ℹ️ Creates pins based on enum; change enum via set_node_property with action="set_enum_type"
            "SwitchInteger" - Switch on integer value with cases
                ℹ️ Creates 1 pin at creation; add more via set_node_property with action="add_pin"
            "ExecutionSequence" - Sequential execution with multiple outputs
                ℹ️ Creates 1 pin at creation; add/remove via set_node_property (add_pin/remove_pin)

        DATA:
            "VariableGet" - Read a variable value (⚠️ variable must exist in Blueprint)
            "VariableSet" - Set a variable value (⚠️ variable must exist and be assignable)
            "MakeArray" - Create array from individual inputs
                ℹ️ Creates 1 pin at creation; add/remove via set_node_property with action="set_num_elements"

        CASTING:
            "DynamicCast" - Cast object to specific class (⚠️ handle "Cast Failed" output)
            "ClassDynamicCast" - Cast class reference to derived class (⚠️ handle failure cases)
            "CastByteToEnum" - Convert byte value to enum (⚠️ byte must be valid enum range)

        UTILITY:
            "Print" - Debug output to screen/log (configurable duration and color)
            "CallFunction" - Call any blueprint/engine function (⚠️ function must exist)
            "Select" - Choose between two inputs based on boolean condition
            "SpawnActor" - Spawn actor from class (⚠️ class must derive from Actor)

        SPECIALIZED:
            "Timeline" - Animation timeline playback with curve tracks
                ⚠️ REQUIRES MANUAL IMPLEMENTATION: Animation curves must be added in editor
            "GetDataTableRow" - Query row from data table (⚠️ DataTable must exist)
            "AddComponentByClass" - Dynamically add component to actor
            "Self" - Reference to current actor/object
            "Knot" - Invisible reroute node (wire organization only)

        EVENT:
            "Event" - Blueprint event (specify event_type: BeginPlay, Tick, etc.)
                ℹ️ Tick events run every frame - be mindful of performance impact

    pos_x: float = 0,         # X position in graph (default: 0)
    pos_y: float = 0,         # Y position in graph (default: 0)
    message: str = "",        # For Print nodes, the text to print
    event_type: str = "",     # For Event nodes, the event name (BeginPlay, Tick, Destroyed, etc.)
    variable_name: str = "",  # For Variable nodes, the variable name
    target_function: str = "", # For CallFunction nodes, the function to call
    target_blueprint: str = "", # For CallFunction nodes, optional path to target Blueprint
    function_name: str = ""   # Optional name of function graph to add node to (if None, uses EventGraph)
)
```

**Returns**:
```json
{
  "success": true,
  "node_id": "K2Node_CallFunction_1",
  "node_type": "Print",
  "pos_x": 200,
  "pos_y": 0
}
```

**Important Notes**:
- Most nodes can have pins modified after creation via set_node_property
- Dynamic pin management: Switch/SwitchEnum/ExecutionSequence/MakeArray support pin operations
- Timeline is the ONLY node requiring manual implementation (curves must be added in editor)

**Examples**:
```bash
> "Add a BeginPlay event node to BP_MyActor"
→ add_node("BP_MyActor", "Event", event_type="BeginPlay", pos_x=0, pos_y=0)

> "Add a Print node that says 'Hello World' to BP_MyActor"
→ add_node("BP_MyActor", "Print", message="Hello World", pos_x=300, pos_y=0)

> "Add a Branch node (if/then/else) to BP_MyActor"
→ add_node("BP_MyActor", "Branch", pos_x=400, pos_y=0)

> "Add a Get Variable node for 'Speed' to BP_MyActor"
→ add_node("BP_MyActor", "VariableGet", variable_name="Speed", pos_x=200, pos_y=100)

> "Add a Spawn Actor node to BP_MyActor"
→ add_node("BP_MyActor", "SpawnActor", pos_x=600, pos_y=0)
```

---

### `connect_nodes`

Connect two nodes together to create execution or data flow.

**Parameters**:
```python
connect_nodes(
    blueprint_name: str,      # Name of the Blueprint
    source_node_id: str,      # ID of the source node (from add_node return)
    source_pin_name: str,     # Output pin name (e.g., "then", "execute")
    target_node_id: str,      # ID of the target node
    target_pin_name: str      # Input pin name (e.g., "execute")
)
```

**Returns**:
```json
{
  "success": true,
  "connection": {
    "source_node": "K2Node_Event_0",
    "source_pin": "then",
    "target_node": "K2Node_CallFunction_1",
    "target_pin": "execute",
    "connection_type": "exec"
  }
}
```

**Common Pin Names**:
- **Execution pins**: `execute`, `then`
- **Data pins**: Vary by node type (inspect nodes in Blueprint editor)

**Example**:
```bash
> "Connect the BeginPlay event to the Print node in BP_MyActor"
→ connect_nodes("BP_MyActor", "K2Node_Event_0", "then", "K2Node_CallFunction_1", "execute")
```

---

### `disconnect_nodes`

Disconnect two previously connected nodes by breaking the link between their pins.

**Parameters**:
```python
disconnect_nodes(
    blueprint_name: str,      # Name of the Blueprint
    source_node_id: str,      # ID of the source node
    source_pin_name: str,     # Output pin name (e.g., "then", "execute")
    target_node_id: str,      # ID of the target node
    target_pin_name: str      # Input pin name (e.g., "execute")
)
```

**Returns**:
```json
{
  "success": true,
  "result": {
    "message": "Connection removed successfully"
  }
}
```

**Example**:
```bash
> "Disconnect the BeginPlay event from the Print node in BP_MyActor"
→ disconnect_nodes("BP_MyActor", "K2Node_Event_0", "then", "K2Node_CallFunction_1", "execute")
```

**Error Handling**:
If the connection doesn't exist:
```json
{
  "success": false,
  "error": "Connection does not exist between specified pins"
}
```

---

### `create_variable`

Create a new variable in a Blueprint.

**Parameters**:
```python
create_variable(
    blueprint_name: str,      # Name of the Blueprint
    variable_name: str,       # Name of the variable
    variable_type: str,       # Type: "bool", "int", "float", "string", "vector", "rotator"
    default_value: Any = None, # Optional default value
    is_public: bool = False,  # Make variable public/editable
    tooltip: str = "",        # Optional tooltip
    category: str = "Default" # Variable category
)
```

**Returns**:
```json
{
  "success": true,
  "variable_name": "MyFloat",
  "variable_type": "float",
  "default_value": 3.14,
  "is_public": true
}
```

**Supported Types**:
- `bool`: True/False
- `int`: Integer numbers
- `float`: Decimal numbers
- `string`: Text
- `vector`: 3D position (X, Y, Z)
- `rotator`: 3D rotation (Pitch, Yaw, Roll)

**Example**:
```bash
> "Create a public float variable called Speed with default value 500 in BP_MyActor"
→ create_variable("BP_MyActor", "Speed", "float", default_value=500.0, is_public=true)
```

---

## Complete Workflow Example

### Creating a Simple "Hello World" Blueprint

**Step 1: Create or identify your Blueprint**
```bash
> "Create a new Blueprint called BP_HelloWorld"
→ create_blueprint("BP_HelloWorld", "Actor")
```

**Step 2: Add nodes**
```bash
> "Add a BeginPlay event to BP_HelloWorld at position 0,0"
→ add_node("BP_HelloWorld", "Event", event_type="BeginPlay", pos_x=0, pos_y=0)
# Returns: {"node_id": "K2Node_Event_0"}

> "Add a Print node with message 'Hello from Blueprint!' at position 400,0"
→ add_node("BP_HelloWorld", "Print", message="Hello from Blueprint!", pos_x=400, pos_y=0)
# Returns: {"node_id": "K2Node_CallFunction_1"}
```

**Step 3: Connect the nodes**
```bash
> "Connect the BeginPlay event to the Print node"
→ connect_nodes("BP_HelloWorld", "K2Node_Event_0", "then", "K2Node_CallFunction_1", "execute")
```

**Step 4: Compile the Blueprint**
```bash
> "Compile BP_HelloWorld"
→ compile_blueprint("BP_HelloWorld")
```

**Result**: A fully functional Blueprint that prints "Hello from Blueprint!" when it begins play!

---

## Advanced Example: Counter with Variable

Create a Blueprint that counts and displays a number.

```bash
# Step 1: Create variable
> "Create an integer variable called Counter in BP_Counter"
→ create_variable("BP_Counter", "Counter", "int", default_value=0, is_public=true)

# Step 2: Add BeginPlay event
> "Add BeginPlay event to BP_Counter"
→ add_node("BP_Counter", "Event", event_type="BeginPlay", pos_x=0, pos_y=0)

# Step 3: Add Variable Get node
> "Add a node to get the Counter variable in BP_Counter"
→ add_node("BP_Counter", "VariableGet", variable_name="Counter", pos_x=300, pos_y=0)

# Step 4: Add Print node
> "Add a Print node to display the counter value"
→ add_node("BP_Counter", "Print", message="Counter: ", pos_x=600, pos_y=0)

# Step 5: Connect execution flow
> "Connect BeginPlay to the Print node"
→ connect_nodes("BP_Counter", "K2Node_Event_0", "then", "K2Node_CallFunction_2", "execute")
```

---

## Tips & Best Practices

### 1. Plan Your Layout
Space nodes properly for readability:
- Use consistent X spacing (e.g., 300-400 units between nodes)
- Keep related nodes on similar Y coordinates
- Organize complex graphs into logical groups

### 2. Node Naming
Node IDs are returned when you create nodes. **Save these IDs** to reference nodes later for connections:

```python
# Good practice: Store node IDs
event_node = add_node("BP_MyActor", "Event", event_type="BeginPlay")
print_node = add_node("BP_MyActor", "Print", message="Hello")

# Use IDs for connections
connect_nodes("BP_MyActor",
              event_node["node_id"], "then",
              print_node["node_id"], "execute")
```

### 3. Always Compile After Changes
After adding nodes and connections, compile the Blueprint:
```bash
compile_blueprint("BP_MyActor")
```

### 4. Common Event Types
- `BeginPlay`: Runs once when actor starts
- `Tick`: Runs every frame
- `ActorBeginOverlap`: When actor overlaps another
- `EndPlay`: When actor is destroyed

### 5. Variable Visibility
- **Private** (`is_public=false`): Only visible inside Blueprint
- **Public** (`is_public=true`): Editable in Details panel, visible to other Blueprints

---

## Troubleshooting

### Problem: "Blueprint not found"
**Solution**: Ensure the Blueprint exists. Create it first with `create_blueprint()`.

### Problem: "Node not found" when connecting
**Solution**:
- Double-check the node IDs returned from `add_node()`
- Nodes might have been created with different IDs than expected
- Use the exact `node_id` from the return value

### Problem: Connections don't appear in Blueprint editor
**Solution**:
- Make sure to **compile** the Blueprint after making changes
- Close and reopen the Blueprint editor
- Check that pin names match exactly (case-sensitive)

### Problem: "Variable already exists"
**Solution**:
- Variables must have unique names within a Blueprint
- Use `create_variable()` only once per variable name
- To modify an existing variable, delete and recreate it

### Problem: Changes not visible in editor
**Solution**:
- Run `compile_blueprint()` after making changes
- Refresh the Blueprint editor (close and reopen)
- Check Unreal Engine Output Log for errors

---

## Common Pin Names Reference

### Execution Pins
- `execute` - Standard execution input
- `then` - Standard execution output
- `completed` - Async operation completion

### Common Function Pins
- Print node: `execute` (input), `then` (output), `InString` (text input)
- Branch node: `Condition` (bool input), `True` (output), `False` (output)
- Delay node: `execute` (input), `Duration` (float input), `Completed` (output)

**Pro Tip**: To discover pin names, create the node manually in Blueprint editor and hover over pins to see their internal names.

---

## Developer Notes

### Architecture
Blueprint Graph tools are implemented in native C++ for maximum performance:

**C++ Components**:
- `NodeManager.cpp` - Handles node creation
- `BPConnector.cpp` - Manages node connections
- `BPVariables.cpp` - Variable creation and management

**Python Wrapper**:
- `node_manager.py` - MCP tool wrapper for add_node
- `connector_manager.py` - MCP tool wrapper for connect_nodes and disconnect_nodes
- `variable_manager.py` - MCP tool wrapper for create_variable

### Extending the System

To add support for new node types, modify:
1. `NodeManager.cpp` - Add node creation logic
2. `node_manager.py` - Update tool parameters
3. Test with a Blueprint in Unreal Editor

### Performance Considerations
- Node creation: ~10-50ms per node
- Connection: ~5-20ms per connection
- Variable creation: ~20-50ms
- Blueprint compilation: ~100-500ms (varies by complexity)

Batch operations when possible to reduce round-trip time.

---

## Further Learning

### Resources
- [Unreal Engine Blueprint Documentation](https://docs.unrealengine.com/en-US/ProgrammingAndScripting/Blueprints/)
- [Blueprint Visual Scripting](https://docs.unrealengine.com/en-US/ProgrammingAndScripting/Blueprints/UserGuide/)
- Join our [Discord](https://discord.gg/3KNkke3rnH) for community support

### Video Tutorials
Check out [YouTube @flopperam](https://youtube.com/@flopperam) for video guides on Blueprint Graph programming with AI.

---

**Ready to build amazing Blueprint logic through AI?** Start with simple examples and work your way up to complex game mechanics!
