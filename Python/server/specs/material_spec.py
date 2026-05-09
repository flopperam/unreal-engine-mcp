"""Material and rendering spec dataclasses for MCP commands."""

from dataclasses import dataclass, field
from typing import Any, Dict, List, Literal, Optional


@dataclass
class MaterialParameterSpec:
    """Specification for a single material parameter update."""
    name: str
    type: Literal["scalar", "vector", "texture", "static_switch"]
    value: Any


@dataclass
class BatchUpdateParametersSpec:
    """Specification for batch material parameter updates."""
    instance_path: str
    parameters: List[MaterialParameterSpec] = field(default_factory=list)


@dataclass
class MaterialInstanceSpec:
    """Specification for creating a material instance."""
    parent_material: str
    instance_name: str
    package_path: str = "/Game/Materials/"


@dataclass
class MaterialParameterCollectionSpec:
    """Specification for editing a material parameter collection."""
    collection_path: str
    add_scalars: List[str] = field(default_factory=list)
    add_vectors: List[str] = field(default_factory=list)
    remove_params: List[str] = field(default_factory=list)


@dataclass
class AdvancedMaterialSpec:
    """Specification for creating an advanced material with a specific domain."""
    name: str
    material_domain: Literal["Surface", "DeferredDecal", "LightFunction", "PostProcess", "VirtualTexture", "Landscape"]
    package_path: str = "/Game/Materials/"
