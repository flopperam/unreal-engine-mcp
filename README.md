## Update 01/24/2026

---

This repository contains the **MCP (Model Context Protocol) server** for Unreal Engine, a powerful tool for controlling Unreal through AI clients like Claude, Cursor, and other MCP Hosts.

However, we've now built something significantly more advanced: **The Flop Agent**, a fully autonomous AI agent specifically designed for Unreal Engine.

## v0.6 Now Released - [https://flopperam.com/](https://flopperam.com/)

### CHAT RIGHT INSIDE UNREAL
The agent now runs in an embedded browser directly in the Unreal Editor. No more switching windows. Open the FlopAI panel and chat with the agent while you are looking at your scene.

<img width="1706" height="1309" alt="Screenshot 2026-01-14 220636" src="https://github.com/user-attachments/assets/6e3e7439-3055-467a-9fe0-63c85dd7a0ad" />

### UNREAL VERSION SUPPORT
v0.6 now supports Unreal Engine 5.5 and 5.6, in addition to 5.7.

### BLUEPRINT ANALYSIS TOOL
We added a new Blueprint Analysis tool that lets the agent deeply inspect your Blueprints, including:
- Variables, functions, event dispatchers, interfaces
- Component hierarchies with key properties
- Graph nodes and connections for execution and data flow
- Selected nodes in the active editor
- Full graph analysis

Ask it to explain how a Blueprint works and it can actually read the graph structure and talk through what is happening.

**Important note:** Blueprint Analysis is focused on reading and understanding right now. It cannot perform complex Blueprint modifications yet. That is coming in the next release in early February.

### PREMIUM MODEL SUPPORT
We added support for top-tier reasoning models like Claude Opus 4.5. These are much better at complex multi-step builds that used to be hit or miss. Think full environments from a single prompt.

The agent will execute dozens of tools in sequence, creating materials, placing actors, and verifying its work as it goes.

### 3D GENERATION QUALITY TIERS
Text and image to 3D now has three quality levels:
- **Good**: Fast iteration, lower detail
- **High Quality**: Balanced quality
- **Very High Quality**: Maximum detail, longer generation

### DOCUMENTATION
Full docs are live at [flopperam.com/docs](https://flopperam.com/docs) covering installation, agent modes, 3D generation, Blueprint analysis, and examples.

---

# The Most Advanced MCP Server for Unreal Engine

**Control Unreal Engine 5.5+ through AI with natural language** — This MCP server enables AI clients to build incredible 3D worlds and architectural masterpieces. Create entire towns, medieval castles, modern mansions, challenging mazes, and complex structures with AI-powered commands.

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-orange.svg)](https://www.unrealengine.com/)
[![YouTube](https://img.shields.io/badge/YouTube-@flopperam-red.svg?logo=youtube)](https://youtube.com/@flopperam)
[![Discord](https://img.shields.io/badge/Discord-Join%20Server-5865F2.svg?logo=discord&logoColor=white)](https://discord.gg/3KNkke3rnH)
[![Twitter](https://img.shields.io/badge/X-@Flopperam-1DA1F2.svg?logo=x&logoColor=white)](https://twitter.com/Flopperam)
[![TikTok](https://img.shields.io/badge/TikTok-@flopperam-000000.svg?logo=tiktok&logoColor=white)](https://tiktok.com/@flopperam)

> **Active Development**: This MCP server is under active development with regular updates and improvements. Join our [Discord](https://discord.gg/3KNkke3rnH) to stay updated on the latest releases!

## See It In Action
Watch our comprehensive tutorial for complete setup and usage:
- **[Complete MCP Tutorial & Installation Guide](https://youtu.be/ct5dNJC-Hx4)** - Full walkthrough of installation, setup, and advanced usage

Check out these examples of the MCP server in action on our channel:
- **[GPT-5 vs Claude](https://youtube.com/shorts/xgoJ4d3d4-4)** - Watch Claude and GPT-5 go head-to-head building simultaneously - Claude creates a massive fortress while GPT-5 builds a sprawling city
- **[Advanced Metropolis Generation](https://youtube.com/shorts/6WkxCQXbCWk)** - Watch AI generate a full-blown metropolis with towers, streets, parks, and over 4,000 objects from a single prompt
- **[Advanced Maze & Mansion Generation](https://youtube.com/shorts/ArExYGpIZwI)** - Watch Claude generate a playable maze and complete mansion complex with wings, towers, and arches

## Featured Capabilities

### Complete Blueprint Visual Scripting

**Program Blueprints entirely through AI** with comprehensive node creation, graph management, and variable systems.

```bash
# Create complex Blueprint logic with control flow, variables, and functions
> "Create a Blueprint with a health system that tracks damage and triggers a death event"
→ create_blueprint() + create_variable() + add_node() + connect_nodes()

# Support for 23+ node types across 6 categories:
# Control Flow: Branch, Comparison, Switch (Byte/Enum/Integer), ExecutionSequence
# Data: VariableGet, VariableSet, MakeArray
# Casting: DynamicCast, ClassDynamicCast, CastByteToEnum
# Utility: Print, CallFunction, Select, SpawnActor
# Specialized: Timeline, GetDataTableRow, AddComponentByClass, Self, Knot
# Animation: PlayAnimation, StopAnimation, Timeline nodes
```

**Advanced Blueprint Features:**
- **Function Management**: Create custom functions with inputs/outputs, rename, and delete
- **Variable System**: Full property control (public/private, replication, tooltips, ranges, units)
- **Node Properties**: Dynamic pin management, type modification, semantic editing
- **Graph Analysis**: Read complete Blueprint content, analyze execution flow, inspect variables
- **Connection System**: Wire nodes together with automatic type validation

### World Building & Architecture  
```bash
# Create massive futuristic cities with skyscrapers, flying cars, and advanced infrastructure
> "Build a massive futuristic city with towering skyscrapers"
→ create_town(town_size="massive", architectural_style="futuristic", building_density=0.95)

# Build complex multi-room houses with windows, doors, and roofs
> "Create a Victorian mansion complex with east and west wing houses."
→ construct_house(house_style="mansion", width=1500, height=900)
```

### Intelligent Mazes
```bash
# Generate solvable mazes with guaranteed paths using recursive backtracking
> "Make a 15x15 maze with high walls"
→ create_maze(rows=15, cols=15, wall_height=4, cell_size=250)
```

---

## Complete Tool Arsenal

| **Category** | **Tools** | **Description** |
|--------------|-----------|-----------------|
| **Blueprint Visual Scripting** | `add_node`, `connect_nodes`, `delete_node`, `set_node_property`, `create_variable`, `set_blueprint_variable_properties`, `create_function`, `add_function_input`, `add_function_output`, `delete_function`, `rename_function` | Complete Blueprint programming with 23+ node types, variables with full property control, custom functions, and dynamic graph management |
| **Blueprint Analysis** | `read_blueprint_content`, `analyze_blueprint_graph`, `get_blueprint_variable_details`, `get_blueprint_function_details` | Deep inspection of Blueprint structure, event graphs, execution flow, variables, and functions |
| **World Building** | `create_town`, `construct_house`, `construct_mansion`, `create_tower`, `create_arch`, `create_staircase` | Build complex architectural structures and entire settlements |
| **Epic Structures** | `create_castle_fortress`, `create_suspension_bridge`, `create_aqueduct` | Massive engineering marvels and medieval fortresses |
| **Level Design** | `create_maze`, `create_pyramid`, `create_wall` | Design challenging game levels and puzzles |
| **Physics & Materials** | `spawn_physics_blueprint_actor`, `set_physics_properties`, `get_available_materials`, `apply_material_to_actor`, `apply_material_to_blueprint`, `set_mesh_material_color` | Create realistic physics simulations and material systems |
| **Blueprint System** | `create_blueprint`, `compile_blueprint`, `add_component_to_blueprint`, `set_static_mesh_properties` | Visual scripting and custom actor creation |
| **Actor Management** | `get_actors_in_level`, `find_actors_by_name`, `delete_actor`, `set_actor_transform`, `get_actor_material_info` | Precise control over scene objects and inspection |

---

## Lightning-Fast Setup

### Prerequisites
- **Unreal Engine 5.5+** 
- **Python 3.12+**
- **MCP Client** (Claude Desktop, Cursor, or Windsurf)

### 1. Setup Options

**Option A: Use the Pre-Built Project (Recommended for Quick Start)**
```bash
# Clone the repository
git clone https://github.com/flopperam/unreal-engine-mcp.git
cd unreal-engine-mcp

# Open the pre-configured project
# Double-click FlopperamUnrealMCP/FlopperamUnrealMCP.uproject
# or open it through Unreal Engine launcher

# The plugin is already installed and enabled!
```

**Option B: Add Plugin to Your Existing Project**
```bash
# Copy the plugin to your project
cp -r UnrealMCP/ YourProject/Plugins/

# Enable in Unreal Editor
Edit → Plugins → Search "UnrealMCP" → Enable → Restart Editor
```

**Option C: Install for All Projects**
```bash
# Copy to Engine plugins folder (available to all projects)
cp -r UnrealMCP/ "C:/Program Files/Epic Games/UE_5.5/Engine/Plugins/"

# Enable in any project through the Plugin Browser
Edit → Plugins → Search "UnrealMCP" → Enable
```

#### Extra steps for Mac
If you're on macOS and Unreal Engine fails to open the project due to compilation errors, you'll need to manually compile the C++ plugin first. To do so, follow these steps:

##### Step 1: Check Your Xcode Version

```bash
xcodebuild -version
xcrun --show-sdk-version
```

Note your Xcode version number (e.g., `26.0.1`, `16.0`, `15.2`, etc.). If your version is newer than 16.0, you'll need to patch the Unreal Engine SDK configuration.

##### Step 2: Patch Unreal Engine SDK Configuration

Edit the file at your Unreal Engine installation (replace `UE_5.X` with your version):

```bash
# Path to edit:
/Users/Shared/Epic Games/UE_5.X/Engine/Config/Apple/Apple_SDK.json
```

Update the following values:

**Change 1:** Update `MaxVersion` to support your Xcode version
```json
{
  "MaxVersion": "YOUR_XCODE_VERSION.9.0",  // e.g., "26.9.0" if you have Xcode 26.x
}
```
Replace `YOUR_XCODE_VERSION` with your major Xcode version from Step 1.

**Change 2:** Add LLVM version mapping for your Xcode version (add to the `AppleVersionToLLVMVersions` array)
```json
{
  "AppleVersionToLLVMVersions": [
    "14.0.0-14.0.0",
    "14.0.3-15.0.0",
    "15.0.0-16.0.0",
    "16.0.0-17.0.6",
    "16.3.0-19.1.4",
    "YOUR_XCODE_VERSION.0.0-19.1.4"  // e.g., "26.0.0-19.1.4" for Xcode 26.x
  ]
}
```
Replace `YOUR_XCODE_VERSION` with your major Xcode version from Step 1.

##### Step 3: Compile the Plugin

Run the Unreal Build Tool to compile the project:

```bash
"/Users/Shared/Epic Games/UE_5.X/Engine/Build/BatchFiles/Mac/Build.sh" \
  UnrealEditor Mac Development \
  -Project="/path/to/unreal-engine-mcp/FlopperamUnrealMCP/FlopperamUnrealMCP.uproject" \
  -WaitMutex
```

Replace:
- `UE_5.X` with your Unreal Engine version (e.g., `UE_5.5`)
- `/path/to/unreal-engine-mcp/` with the actual path to your cloned repository

##### Step 4: Open the Project

Once compilation succeeds, you can open `FlopperamUnrealMCP.uproject` in Unreal Engine.

### 2. Launch the MCP Server

```bash
cd Python
uv run unreal_mcp_server_advanced.py
```

### 3. Configure Your AI Client

Add this to your MCP configuration:

**Cursor**: `.cursor/mcp.json`
**Claude Desktop**: `~/.config/claude-desktop/mcp.json`
**Windsurf**: `~/.config/windsurf/mcp.json`

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "/path/to/unreal-engine-mcp/Python",
        "run",
        "unreal_mcp_server_advanced.py"
      ]
    }
  }
}
```

Note that on Mac, and sometimes on Windows, you may have to replace the "uv" string passed as the value to "command" in the above `mcp.json` file with the exact absolute path to the uv executable. To get that path, run one of these commands:
- Mac: `which uv`
- Windows: `where uv`

> **Having issues with setup?** Check our [Debugging & Troubleshooting Guide](DEBUGGING.md) for solutions to common problems like MCP installation errors and configuration issues.
>
> **Want to program Blueprints with AI?** Check our [Blueprint Graph Programming Guide](Guides/blueprint-graph-guide.md) to learn how to create nodes, connections, and variables programmatically.

### 4. Start Building!

```bash
> "Create a medieval castle with towers and walls"
> "Generate a town square with fountain and buildings"
> "Make a challenging maze for players to solve"
```

---

## Architecture

```mermaid
graph TB
    A[AI Client<br/>Cursor/Claude/Windsurf] -->|MCP Protocol| B[Python Server<br/>unreal_mcp_server_advanced.py]
    B -->|TCP Socket| C[C++ Plugin<br/>UnrealMCP]
    C -->|Native API| D[Unreal Engine 5.5+<br/>Editor & Runtime]
    
    B --> E[Advanced Tools]
    E --> F[World Building]
    E --> G[Physics Simulation]  
    E --> H[Blueprint System]
    
    C --> I[Actor Management]
    C --> J[Component System]
    C --> K[Material System]
```

**Performance**: Native C++ plugin ensures minimal latency for real-time control
**Reliability**: Robust TCP communication with automatic reconnection
**Flexibility**: Full access to Unreal's actor, component, and Blueprint systems

---

## Community & Support

**Join our community and get help building amazing worlds!**

### Connect With Us
- **YouTube**: [youtube.com/@flopperam](https://youtube.com/@flopperam) - Tutorials, showcases, and development updates
- **Discord**: [discord.gg/8yr1RBv](https://discord.gg/3KNkke3rnH) - Get help, share creations, and discuss the plugin
- **Twitter/X**: [twitter.com/Flopperam](https://twitter.com/Flopperam) - Latest news and quick updates  
- **TikTok**: [tiktok.com/@flopperam](https://tiktok.com/@flopperam) - Quick tips and amazing builds

### Get Help & Share
- **Setup Issues?** Check our [Debugging & Troubleshooting Guide](DEBUGGING.md) first
- **Questions?** Ask in our Discord server for real-time support
- **Bug reports?** Open an issue on GitHub with reproduction steps
- **Feature ideas?** Join the discussion in our community channels

---

## License
MIT License - Build amazing things freely.
