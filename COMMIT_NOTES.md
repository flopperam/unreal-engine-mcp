# Commit Notes - Blueprint Graph Tools

**Author**: Zoscran
**Date**: 2025-10-18
**Branch**: BlueprintGraph
**Type**: Feature Addition

---

## 📋 Summary

This commit adds **Blueprint Graph manipulation tools** to the Unreal Engine MCP server, enabling AI-driven programmatic creation and connection of Blueprint nodes.

**New Capabilities**:
- Create Blueprint nodes (Events, Print, Variables)
- Connect nodes to build execution flow
- Create and manage Blueprint variables
- Programmatic Blueprint logic construction via AI commands

---

## 🎯 What Was Added

### 1. New MCP Tools (3 tools)

| Tool | Description | Implementation |
|------|-------------|----------------|
| `add_node` | Add nodes to Blueprint graphs | C++: `NodeManager.cpp/h`, Python: `node_manager.py` |
| `connect_nodes` | Connect Blueprint nodes | C++: `BPConnector.cpp/h`, Python: `connector_manager.py` |
| `create_variable` | Create Blueprint variables | C++: `BPVariables.cpp/h`, Python: `variable_manager.py` |

### 2. C++ Implementation (Native Plugin)

**New Files**:
```
FlopperamUnrealMCP/Plugins/UnrealMCP/Source/UnrealMCP/
├── Private/Commands/BlueprintGraph/
│   ├── NodeManager.cpp         (Node creation logic)
│   ├── BPConnector.cpp         (Node connection logic)
│   └── BPVariables.cpp         (Variable creation logic)
└── Public/Commands/BlueprintGraph/
    ├── NodeManager.h
    ├── BPConnector.h
    └── BPVariables.h
```

**Modified Files**:
- `UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCPBlueprintGraphCommands.cpp`
  - Added handlers for the 3 new commands
- `UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCPBlueprintGraphCommands.h`
  - Added method declarations
- `FlopperamUnrealMCP/Plugins/UnrealMCP/Source/UnrealMCP/Private/EpicUnrealMCPBridge.cpp`
  - Registered new commands in command router

### 3. Python MCP Server Integration

**New Files**:
```
Python/helpers/blueprint_graph/
├── __init__.py
├── node_manager.py          (MCP tool wrapper for add_node)
├── connector_manager.py     (MCP tool wrapper for connect_nodes)
└── variable_manager.py      (MCP tool wrapper for create_variable)
```

**Modified Files**:
- `Python/unreal_mcp_server_advanced.py`
  - Imported Blueprint Graph helpers
  - Registered 3 new MCP tools: `add_node`, `connect_nodes`, `create_variable`

### 4. Documentation

**New Files**:
- `Guides/blueprint-graph-guide.md` - Complete user guide with examples, troubleshooting, and developer notes

**Modified Files**:
- `README.md` - Added "Blueprint Graph" category to tools table and link to guide

---

## 🔧 Technical Architecture

```
AI Client (Claude/Cursor)
    ↓ MCP Protocol
Python Server (unreal_mcp_server_advanced.py)
    ↓ Helper: node_manager.py / connector_manager.py / variable_manager.py
    ↓ TCP Socket (port 55557)
C++ Plugin (UnrealMCP)
    ↓ EpicUnrealMCPBlueprintGraphCommands
    ↓ NodeManager / BPConnector / BPVariables
Unreal Engine Blueprint System
```

### Design Decisions

**Why Native C++?**
- Direct access to Unreal's Blueprint API (`UBlueprint`, `UEdGraph`, `UEdGraphNode`)
- Better performance (~10-50ms per operation vs potential Python overhead)
- Type safety and compile-time validation
- Consistency with existing plugin architecture

**Why Python Wrappers?**
- Clean separation: C++ handles Unreal logic, Python handles MCP protocol
- Easy to extend with new tools
- Maintains compatibility with FastMCP server architecture

**Why TCP Socket?**
- Already established in the project
- Persistent connection reduces latency
- Proven reliability for complex operations

---

## ✅ Testing Status

**Manual Testing Completed**:
- ✅ `add_node`: Successfully creates Print, Event, and Variable nodes
- ✅ `connect_nodes`: Successfully connects execution and data pins
- ✅ `create_variable`: Successfully creates variables with types (bool, int, float, string, vector, rotator)
- ✅ Blueprint compilation: Modified Blueprints compile successfully
- ✅ Integration: All tools work correctly with existing MCP infrastructure

**Test Blueprint**: `BP_Test_F22` (included in project)

**Known Limitations**:
- Multiple Event nodes of the same type can be created (no duplicate check)
- Node validation is minimal (Unreal handles most validation)
- Some node types not yet supported (only Print, Event, VariableGet/Set currently)

---

## 📝 Merge Considerations

### Compatibility
- ✅ **Backward Compatible**: Existing tools and features unchanged
- ✅ **No Breaking Changes**: Only additions, no modifications to existing APIs
- ✅ **Independent**: Blueprint Graph tools don't interfere with other systems

### Integration Points

**Potential Conflicts**:
1. **Blueprint Graph helpers**: Ensure no naming conflicts in Python helpers
2. **Command registration**: Check EpicUnrealMCPBridge.cpp command list

**Recommended Merge Strategy**:
1. Review C++ implementation in BlueprintGraph folder
2. Test Python MCP tool integration
3. Verify Blueprint compilation works correctly

### Documentation Integration

All documentation follows existing project style:
- `Guides/blueprint-graph-guide.md` matches format of other guides
- README.md updates are minimal and non-invasive
- No changes to DEBUGGING.md or other core docs

---

## 🎓 For Reviewers

### Code Review Checklist

**Architecture**:
- [ ] C++ follows plugin coding standards
- [ ] Python wrappers follow MCP best practices
- [ ] Error handling is comprehensive
- [ ] Memory management is safe (no leaks)

**Functionality**:
- [ ] Test all 3 tools in Unreal Editor
- [ ] Verify Blueprint compilation after modifications
- [ ] Check node visibility in Blueprint editor
- [ ] Validate variable creation and types

**Documentation**:
- [ ] User guide is clear and accurate
- [ ] Code comments are sufficient
- [ ] README updates are appropriate

### Questions for Discussion

1. **Naming Convention**: Should we rename internal "Blueprint Graph" to something more specific?
2. **Error Messages**: Are current error messages user-friendly enough?
3. **Future Extensions**: Priority for adding support for more node types?
4. **Testing**: Should we add automated tests for Blueprint operations?

---

## 📞 Contact

**Developer**: Zoscran
**For Questions**: Available on Discord or via GitHub issues

---

## 🔄 Next Steps (Post-Merge)

Potential future enhancements:
- Add support for more node types (Branch, ForLoop, Delay, etc.)
- Implement Blueprint graph inspection tools
- Add Blueprint graph validation tools
- Create visual Blueprint graph export/import
- Add Blueprint debugging tools

---

**Ready for Review and Merge** ✅
