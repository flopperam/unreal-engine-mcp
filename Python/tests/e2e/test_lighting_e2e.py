"""L3 E2E tests for Lighting / Atmosphere MCP tools.

Requires live Unreal Editor + MCP server.
Tests the full stack: Python tool -> TCP -> C++ handler -> UE5 actor modification.
"""

import pytest

pytestmark = [pytest.mark.e2e]


@pytest.mark.requires_unreal
def test_spawn_and_configure_point_light(unreal):
    """Spawn a PointLight and configure all basic properties."""
    # Spawn
    spawn_result = unreal.send_command("spawn_actor", {
        "type": "PointLight",
        "name": "E2E_PointLight",
        "location": [0, 0, 200]
    })
    assert spawn_result["success"] is True

    # Set intensity
    result = unreal.send_command("set_light_intensity", {"actor_name": "E2E_PointLight", "intensity": 10000.0})
    assert result["success"] is True
    assert result["intensity"] == 10000.0

    # Set color
    result = unreal.send_command("set_light_color", {"actor_name": "E2E_PointLight", "color": [1.0, 0.5, 0.0]})
    assert result["success"] is True

    # Set temperature
    result = unreal.send_command("set_light_temperature", {"actor_name": "E2E_PointLight", "temperature": 3200.0, "enabled": True})
    assert result["success"] is True
    assert result["use_temperature"] is True

    # Set mobility
    result = unreal.send_command("set_light_mobility", {"actor_name": "E2E_PointLight", "mobility": "Movable"})
    assert result["success"] is True
    assert result["mobility"] == "Movable"

    # Shadow on/off
    result = unreal.send_command("set_light_shadow_enabled", {"actor_name": "E2E_PointLight", "enabled": False})
    assert result["success"] is True
    assert result["cast_shadows"] is False

    # Attenuation radius
    result = unreal.send_command("set_light_attenuation_radius", {"actor_name": "E2E_PointLight", "radius": 3000.0})
    assert result["success"] is True
    assert result["attenuation_radius"] == 3000.0

    # Source radius
    result = unreal.send_command("set_light_source_radius", {"actor_name": "E2E_PointLight", "radius": 10.0, "soft_radius": 5.0})
    assert result["success"] is True

    # Cleanup
    unreal.send_command("delete_actor", {"name": "E2E_PointLight"})


@pytest.mark.requires_unreal
def test_spawn_and_configure_spot_light(unreal):
    """Spawn a SpotLight and set cone angles."""
    spawn_result = unreal.send_command("spawn_actor", {
        "type": "SpotLight",
        "name": "E2E_SpotLight",
        "location": [0, 0, 300]
    })
    assert spawn_result["success"] is True

    result = unreal.send_command("set_light_cone_angles", {"actor_name": "E2E_SpotLight", "inner": 15.0, "outer": 35.0})
    assert result["success"] is True
    assert result["inner_cone_angle"] == 15.0
    assert result["outer_cone_angle"] == 35.0

    result = unreal.send_command("set_light_volumetric_scattering", {"actor_name": "E2E_SpotLight", "enabled": True, "intensity": 1.5})
    assert result["success"] is True

    unreal.send_command("delete_actor", {"name": "E2E_SpotLight"})


@pytest.mark.requires_unreal
def test_spawn_and_configure_rect_light(unreal):
    """Spawn a RectLight and set rect-specific properties."""
    spawn_result = unreal.send_command("spawn_actor", {
        "type": "RectLight",
        "name": "E2E_RectLight",
        "location": [0, 0, 250]
    })
    assert spawn_result["success"] is True

    result = unreal.send_command("set_rect_light_properties", {
        "actor_name": "E2E_RectLight",
        "source_width": 128.0,
        "source_height": 64.0,
        "barn_door_angle": 45.0,
        "barn_door_length": 10.0
    })
    assert result["success"] is True
    assert result["source_width"] == 128.0
    assert result["source_height"] == 64.0

    unreal.send_command("delete_actor", {"name": "E2E_RectLight"})


@pytest.mark.requires_unreal
def test_spawn_and_configure_sky_light(unreal):
    """Spawn a SkyLight and set properties."""
    spawn_result = unreal.send_command("spawn_actor", {
        "type": "SkyLight",
        "name": "E2E_SkyLight",
        "location": [0, 0, 0]
    })
    assert spawn_result["success"] is True

    result = unreal.send_command("set_sky_light_properties", {
        "actor_name": "E2E_SkyLight",
        "intensity": 2.0
    })
    assert result["success"] is True
    assert result["intensity"] == 2.0

    unreal.send_command("delete_actor", {"name": "E2E_SkyLight"})


@pytest.mark.requires_unreal
def test_spawn_and_configure_sky_atmosphere(unreal):
    """Spawn a SkyAtmosphere and set properties."""
    spawn_result = unreal.send_command("spawn_actor", {
        "type": "SkyAtmosphere",
        "name": "E2E_SkyAtmosphere",
        "location": [0, 0, 0]
    })
    assert spawn_result["success"] is True

    result = unreal.send_command("set_sky_atmosphere_properties", {
        "actor_name": "E2E_SkyAtmosphere",
        "ground_radius": 6360.0,
        "atmosphere_height": 100.0
    })
    assert result["success"] is True

    unreal.send_command("delete_actor", {"name": "E2E_SkyAtmosphere"})


@pytest.mark.requires_unreal
def test_spawn_and_configure_height_fog(unreal):
    """Spawn an ExponentialHeightFog and configure it."""
    spawn_result = unreal.send_command("spawn_actor", {
        "type": "ExponentialHeightFog",
        "name": "E2E_Fog",
        "location": [0, 0, 0]
    })
    assert spawn_result["success"] is True

    result = unreal.send_command("set_height_fog_properties", {
        "actor_name": "E2E_Fog",
        "fog_density": 0.02,
        "fog_height_falloff": 0.1,
        "fog_max_opacity": 0.8
    })
    assert result["success"] is True

    result = unreal.send_command("set_volumetric_fog", {"actor_name": "E2E_Fog", "enabled": True})
    assert result["success"] is True
    assert result["volumetric_fog_enabled"] is True

    unreal.send_command("delete_actor", {"name": "E2E_Fog"})


@pytest.mark.requires_unreal
def test_directional_light_as_sun(unreal):
    """Spawn a DirectionalLight, tag it as sun, and set sun position."""
    spawn_result = unreal.send_command("spawn_actor", {
        "type": "DirectionalLight",
        "name": "E2E_Sun",
        "location": [0, 0, 500],
        "rotation": [-45, 0, 0]
    })
    assert spawn_result["success"] is True

    result = unreal.send_command("set_directional_light_as_sun", {"actor_name": "E2E_Sun", "is_sun": True})
    assert result["success"] is True
    assert result["is_sun"] is True

    result = unreal.send_command("set_sun_position", {"actor_name": "E2E_Sun", "azimuth": 90.0, "zenith": 30.0})
    assert result["success"] is True

    unreal.send_command("delete_actor", {"name": "E2E_Sun"})


@pytest.mark.requires_unreal
def test_create_reflection_capture(unreal):
    """Create a sphere reflection capture."""
    result = unreal.send_command("create_reflection_capture", {
        "actor_name": "E2E_RC",
        "type": "Sphere",
        "location": [0, 0, 100],
        "radius": 800.0,
        "brightness": 1.5
    })
    assert result["success"] is True
    assert result["actor_name"] == "E2E_RC"
    assert result["type"] == "Sphere"

    unreal.send_command("delete_actor", {"name": "E2E_RC"})


@pytest.mark.requires_unreal
def test_create_lightmass_importance_volume(unreal):
    """Create a Lightmass Importance Volume."""
    result = unreal.send_command("create_lightmass_importance_volume", {
        "location": [0, 0, 0],
        "extent": [2000, 2000, 500]
    })
    assert result["success"] is True
    assert result["class"] == "LightmassImportanceVolume"

    # Cleanup
    unreal.send_command("delete_actor", {"name": result["actor_name"]})


@pytest.mark.requires_unreal
def test_build_lighting_preview(unreal):
    """Trigger a preview lighting build."""
    result = unreal.send_command("build_lighting", {"quality": "Preview"})
    assert result["success"] is True
    assert result["quality"] == "Preview"
    assert "message" in result
