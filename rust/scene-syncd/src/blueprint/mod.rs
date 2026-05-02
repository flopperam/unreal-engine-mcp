use serde::{Deserialize, Serialize};

/// Blueprint Graph IR: a structured representation of a Blueprint's
/// nodes, edges, variables, and functions extracted from Unreal.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct BlueprintGraphIr {
    /// The asset path of the blueprint (e.g., "/Game/Blueprints/BP_MyActor").
    pub asset_path: String,
    /// Blueprint name.
    pub name: String,
    /// Parent class the blueprint inherits from.
    pub parent_class: Option<String>,
    /// Variables defined in the blueprint.
    #[serde(default)]
    pub variables: Vec<BlueprintVariable>,
    /// Functions defined in the blueprint.
    #[serde(default)]
    pub functions: Vec<BlueprintFunction>,
    /// Nodes in the event graph and function graphs.
    #[serde(default)]
    pub nodes: Vec<BlueprintNode>,
    /// Edges connecting nodes (pins wired together).
    #[serde(default)]
    pub edges: Vec<BlueprintEdge>,
    /// Metadata from the blueprint.
    #[serde(default)]
    pub metadata: serde_json::Value,
}

/// A variable defined in a Blueprint.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct BlueprintVariable {
    pub name: String,
    pub var_type: String,
    #[serde(default)]
    pub default_value: Option<serde_json::Value>,
    #[serde(default)]
    pub is_exposed: bool,
    #[serde(default)]
    pub is_instance_editable: bool,
    #[serde(default)]
    pub category: String,
}

/// A function defined in a Blueprint.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct BlueprintFunction {
    pub name: String,
    #[serde(default)]
    pub return_type: Option<String>,
    #[serde(default)]
    pub inputs: Vec<FunctionParameter>,
    #[serde(default)]
    pub outputs: Vec<FunctionParameter>,
    #[serde(default)]
    pub is_pure: bool,
    #[serde(default)]
    pub access_level: String,
}

/// A parameter (input or output) of a blueprint function.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct FunctionParameter {
    pub name: String,
    pub param_type: String,
    #[serde(default)]
    pub default_value: Option<serde_json::Value>,
}

/// A node in the Blueprint graph.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct BlueprintNode {
    /// Unique node ID within the graph.
    pub node_id: String,
    /// Node type (e.g., "K2Node_Event", "K2Node_CallFunction", "K2Node_VariableGet").
    pub node_type: String,
    /// Human-readable title of the node.
    #[serde(default)]
    pub title: String,
    /// The function or property name this node references, if applicable.
    #[serde(default)]
    pub referenced_name: Option<String>,
    /// The graph this node belongs to.
    #[serde(default)]
    pub graph_name: String,
    /// Node position in the graph editor (x, y).
    #[serde(default)]
    pub position: Option<(f64, f64)>,
    /// Node-specific metadata.
    #[serde(default)]
    pub metadata: serde_json::Value,
}

/// An edge (wire) connecting two pins in the Blueprint graph.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct BlueprintEdge {
    /// Source node ID.
    pub from_node: String,
    /// Source pin name.
    pub from_pin: String,
    /// Target node ID.
    pub to_node: String,
    /// Target pin name.
    pub to_pin: String,
    /// Edge type (execution, data, etc.).
    #[serde(default)]
    pub edge_type: String,
}

impl BlueprintGraphIr {
    /// Parse a BlueprintGraphIr from the JSON response of `read_blueprint_content`.
    pub fn from_json(json: &serde_json::Value) -> Result<Self, String> {
        serde_json::from_value(json.clone())
            .map_err(|e| format!("Failed to parse BlueprintGraphIr: {e}"))
    }

    /// Find all function calls in the graph (nodes that call another function).
    pub fn function_calls(&self) -> Vec<&BlueprintNode> {
        self.nodes
            .iter()
            .filter(|n| n.node_type == "K2Node_CallFunction")
            .collect()
    }

    /// Find all events in the graph.
    pub fn events(&self) -> Vec<&BlueprintNode> {
        self.nodes
            .iter()
            .filter(|n| n.node_type == "K2Node_Event" || n.node_type == "K2Node_CustomEvent")
            .collect()
    }

    /// Find all variable access nodes (get/set).
    pub fn variable_access(&self) -> Vec<&BlueprintNode> {
        self.nodes
            .iter()
            .filter(|n| n.node_type == "K2Node_VariableGet" || n.node_type == "K2Node_VariableSet")
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    fn sample_node(node_id: &str, node_type: &str, title: &str) -> BlueprintNode {
        BlueprintNode {
            node_id: node_id.to_string(),
            node_type: node_type.to_string(),
            title: title.to_string(),
            referenced_name: None,
            graph_name: "EventGraph".to_string(),
            position: None,
            metadata: json!({}),
        }
    }

    #[test]
    fn parse_blueprint_from_json() {
        let json = json!({
            "asset_path": "/Game/BP_Test",
            "name": "BP_Test",
            "parent_class": "Actor",
            "variables": [],
            "functions": [],
            "nodes": [],
            "edges": [],
            "metadata": {}
        });
        let ir = BlueprintGraphIr::from_json(&json).unwrap();
        assert_eq!(ir.asset_path, "/Game/BP_Test");
        assert_eq!(ir.parent_class, Some("Actor".to_string()));
    }

    #[test]
    fn find_function_calls() {
        let ir = BlueprintGraphIr {
            asset_path: "/Game/BP_Test".to_string(),
            name: "BP_Test".to_string(),
            parent_class: None,
            variables: vec![],
            functions: vec![],
            nodes: vec![
                sample_node("1", "K2Node_CallFunction", "Print String"),
                sample_node("2", "K2Node_Event", "Event BeginPlay"),
                sample_node("3", "K2Node_CallFunction", "Set Timer"),
            ],
            edges: vec![],
            metadata: json!({}),
        };
        let calls = ir.function_calls();
        assert_eq!(calls.len(), 2);
    }

    #[test]
    fn find_events() {
        let ir = BlueprintGraphIr {
            asset_path: "/Game/BP_Test".to_string(),
            name: "BP_Test".to_string(),
            parent_class: None,
            variables: vec![],
            functions: vec![],
            nodes: vec![
                sample_node("1", "K2Node_Event", "Event BeginPlay"),
                sample_node("2", "K2Node_CustomEvent", "MyEvent"),
                sample_node("3", "K2Node_CallFunction", "DoSomething"),
            ],
            edges: vec![],
            metadata: json!({}),
        };
        let events = ir.events();
        assert_eq!(events.len(), 2);
    }
}
