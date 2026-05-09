from dataclasses import dataclass, field
from typing import Any, Dict, List, Union

from server.specs.component_spec import (
    AISpec,
    CollisionSpec,
    LightConfigSpec,
    MeshComponentSpec,
    NavSpec,
)

ComponentSpec = Union[MeshComponentSpec, CollisionSpec, NavSpec, AISpec, LightConfigSpec]


@dataclass
class EntitySpec:
    """Semantic world object: a castle, room, road, or enemy.

    Entities are abstract design concepts that may be realized into
    one or more ActorSpecs depending on the RealizationPolicy.
    """

    entity_id: str
    kind: str
    name: str
    properties: Dict[str, Any] = field(default_factory=dict)
    tags: List[str] = field(default_factory=list)
    mcp_ids: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    components: List[ComponentSpec] = field(default_factory=list)

    def to_layout_node(self) -> Dict[str, Any]:
        """Convert to Rust scene_entity bulk-upsert format."""
        result: Dict[str, Any] = {
            "entity_id": self.entity_id,
            "kind": self.kind,
            "name": self.name,
            "properties": self.properties,
            "tags": self.tags,
        }
        if self.mcp_ids:
            result["mcp_ids"] = self.mcp_ids
        if self.metadata:
            result["metadata"] = self.metadata
        return result
