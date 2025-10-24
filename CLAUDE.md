# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is an advanced MCP (Model Context Protocol) server for Unreal Engine 5.5+ that allows AI clients to control Unreal Engine through natural language commands. The system consists of a Python MCP server that communicates with a C++ Unreal Engine plugin via TCP sockets.

## Architecture

The codebase follows a three-tier architecture:

1. **AI Client Layer**: Cursor/Claude Desktop/Windsurf that sends MCP commands
2. **Python MCP Server**: `Python/unreal_mcp_server_advanced.py` - handles MCP protocol and tool orchestration
3. **C++ Plugin**: `UnrealMCP/` - native Unreal Engine plugin that executes commands via TCP socket communication

### Key Components

- **Python Server**: `Python/unreal_mcp_server_advanced.py` - Main MCP server with 21+ advanced tools
- **Helper Modules**: `Python/helpers/` - Specialized creation logic for different object types
- **C++ Plugin**: `UnrealMCP/Source/UnrealMCP/` - Native plugin with TCP bridge
- **Bridge Communication**: C++ classes handle socket communication between Python and Unreal

## Essential Development Commands

### Running the MCP Server
```bash
cd Python
uv run unreal_mcp_server_advanced.py
```

### Testing Connection
```bash
# Check if MCP dependencies are installed
python -c "import mcp; print('MCP installed successfully')"

# Verify Python dependencies
cd Python && python -c "import fastmcp, uvicorn, fastapi, pydantic, requests; print('All dependencies OK')"
```

### Building the Unreal Plugin
- Open `FlopperamUnrealMCP/FlopperamUnrealMCP.uproject` in Unreal Engine 5.5+
- Plugin will auto-compile on first load
- For manual compilation: `Build → Build Solution` in Unreal Editor

## Core Tool Categories

The Python server provides 21+ specialized tools organized into categories:

### World Building Tools
- `create_town`: Complete urban environments with infrastructure
- `construct_house`/`construct_mansion`: Detailed architectural structures  
- `create_tower`: Various tower types (guard, bell, wizard, etc.)
- `create_arch`/`create_staircase`: Architectural elements

### Epic Structures  
- `create_castle_fortress`: Massive medieval fortifications with keeps, baileys, towers
- `create_suspension_bridge`: Large-scale bridge engineering
- `create_aqueduct`: Roman-style water transport systems

### Level Design
- `create_maze`: Solvable mazes using recursive backtracking algorithm
- `create_pyramid`: Step pyramids and geometric structures
- `create_wall`: Defensive fortifications

### Technical Systems
- Blueprint creation and compilation tools
- Physics simulation and material systems
- Actor management and transformation tools

## Development Guidelines

### TCP Communication Protocol
- Server runs on `127.0.0.1:55557` 
- JSON-based command protocol between Python server and C++ plugin
- Automatic reconnection handling in Python client
- 30-second socket timeout with keep-alive options

### Error Handling Patterns
- Python server logs to `unreal_mcp_advanced.log`
- Use `safe_spawn_actor` and `safe_delete_actor` helpers for actor lifecycle
- Blueprint names must be unique - use descriptive prefixes
- Always compile blueprints after modifications

### Physics Object Creation Workflow
When creating physics-enabled objects, follow this proven pattern:
1. Use `spawn_physics_blueprint_actor` for initial creation
2. Set material colors with `set_mesh_material_color` 
3. Compile blueprint with `compile_blueprint`
4. Fix scale issues with `set_actor_transform` if needed

### Naming Conventions
- Blueprints: `{Purpose}_{UniqueId}_BP` (e.g., `RedCube_001_BP`)
- Actors: `{Type}_{Location}_{Number}` (e.g., `Building_Downtown_01`)
- Components: Use descriptive names matching their function

## Common Issues and Solutions

### MCP Setup Problems
- **Missing MCP module**: Install with `python -m pip install mcp`
- **FastMCP version issues**: Comment out description field in server file if needed
- **Configuration errors**: Use absolute paths in MCP client config files

### Unreal Engine Integration
- Plugin requires Unreal Engine 5.5+ and Windows/Mac/Linux platforms
- TCP server starts automatically when plugin loads
- Check Unreal output log for connection status

### Blueprint and Actor Issues  
- Scale problems: Always use actual spawned actor name from return values
- Color not appearing: Verify component names match exactly
- Blueprint conflicts: Use more unique naming schemes

## File Structure Context

### Python Helpers Organization
Each helper module handles a specific creation domain:
- `building_creation.py`: Generic building structures
- `castle_creation.py`: Medieval fortress systems
- `house_construction.py`: Residential architecture  
- `bridge_aqueduct_creation.py`: Infrastructure engineering
- `actor_utilities.py`: Low-level actor operations
- `actor_name_manager.py`: Safe lifecycle management

### C++ Module Structure
- `Public/`: Header files with API declarations
- `Private/`: Implementation files
- `Commands/`: Specialized command handlers for different tool categories
- `UnrealMCP.Build.cs`: Module dependency configuration

## Testing and Validation

### Connection Testing
1. Launch Unreal Engine with plugin enabled
2. Start Python MCP server
3. Verify TCP connection in Unreal output log
4. Test basic commands through AI client

### Tool Validation
- Use `get_actors_in_level` to verify spawned objects
- Check `get_available_materials` for material system functionality
- Test physics with `spawn_physics_blueprint_actor` and movement commands

The system is designed for advanced 3D world creation through AI, with emphasis on architectural accuracy, physics realism, and scalable urban environments.