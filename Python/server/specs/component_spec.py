from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional


@dataclass
class MeshComponentSpec:
    mesh_path: str
    material_path: Optional[str] = None
    lod_level: int = 0


@dataclass
class CollisionSpec:
    profile: str = "BlockAllDynamic"
    shape: str = "simple_box"  # simple_box | complex_as_simple | custom_convex


@dataclass
class NavSpec:
    behavior: str = "walkable"  # walkable | blocked | jump_link


@dataclass
class AISpec:
    faction: str = "neutral"
    behavior_tree: Optional[str] = None
    patrol_points: List[List[float]] = field(default_factory=list)
    perception_radius: float = 1000.0


@dataclass
class LightConfigSpec:
    """Full specification for light actor configuration used by MCP tools and scene sync."""
    light_type: str = "point"  # point | spot | directional | rect
    intensity: float = 5000.0
    color: List[float] = field(default_factory=lambda: [1.0, 1.0, 1.0])
    temperature: float = 6500.0
    use_temperature: bool = False
    mobility: str = "Stationary"  # Static | Stationary | Movable
    cast_shadows: bool = True
    shadow_bias: float = 0.0
    contact_shadow_length: float = 0.0
    volumetric_scattering_intensity: float = 1.0
    attenuation_radius: float = 1000.0
    inner_cone_angle: float = 0.0
    outer_cone_angle: float = 44.0
    source_radius: float = 0.0
    soft_source_radius: float = 0.0
    ies_profile_path: Optional[str] = None
    light_channel: int = 0
    rect_source_width: float = 64.0
    rect_source_height: float = 64.0
    rect_barn_door_angle: float = 88.0
    rect_barn_door_length: float = 0.0
