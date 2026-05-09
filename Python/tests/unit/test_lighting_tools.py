"""L1 Unit tests for lighting and atmosphere MCP tools.

Uses FakeUnrealConnection to verify:
- Each tool sends the correct command string and parameters
- Input validation rejects bad arguments
- Spec serialization works correctly
"""

import pytest
from unittest.mock import patch
from server.specs.component_spec import LightConfigSpec
import server.lighting_tools as lighting_tools
import unreal_mcp_server_advanced as srv


class TestLightConfigSpec:
    def test_light_config_spec_defaults(self):
        spec = LightConfigSpec()
        assert spec.light_type == "point"
        assert spec.intensity == 5000.0
        assert spec.color == [1.0, 1.0, 1.0]
        assert spec.temperature == 6500.0
        assert spec.use_temperature is False
        assert spec.mobility == "Stationary"
        assert spec.cast_shadows is True
        assert spec.shadow_bias == 0.0
        assert spec.contact_shadow_length == 0.0
        assert spec.volumetric_scattering_intensity == 1.0
        assert spec.attenuation_radius == 1000.0
        assert spec.inner_cone_angle == 0.0
        assert spec.outer_cone_angle == 44.0
        assert spec.source_radius == 0.0
        assert spec.soft_source_radius == 0.0
        assert spec.light_channel == 0
        assert spec.rect_source_width == 64.0
        assert spec.rect_source_height == 64.0

    def test_light_config_spec_custom(self):
        spec = LightConfigSpec(
            light_type="spot",
            intensity=10000.0,
            color=[1.0, 0.5, 0.0],
            temperature=3200.0,
            use_temperature=True,
            mobility="Movable",
            cast_shadows=False,
            attenuation_radius=2000.0,
            inner_cone_angle=10.0,
            outer_cone_angle=30.0,
        )
        assert spec.light_type == "spot"
        assert spec.intensity == 10000.0
        assert spec.color == [1.0, 0.5, 0.0]
        assert spec.temperature == 3200.0
        assert spec.use_temperature is True
        assert spec.mobility == "Movable"
        assert spec.cast_shadows is False
        assert spec.attenuation_radius == 2000.0
        assert spec.inner_cone_angle == 10.0
        assert spec.outer_cone_angle == 30.0


class TestLightingToolPayloads:
    def test_set_light_intensity_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_intensity(actor_name="PointLight_0", intensity=8000.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_intensity"
        assert last["params"]["actor_name"] == "PointLight_0"
        assert last["params"]["intensity"] == 8000.0

    def test_set_light_color_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_color(actor_name="SpotLight_1", color=[1.0, 0.8, 0.6])
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_color"
        assert last["params"]["color"] == [1.0, 0.8, 0.6]

    def test_set_light_temperature_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_temperature(actor_name="PointLight_0", temperature=3200.0, enabled=True)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_temperature"
        assert last["params"]["temperature"] == 3200.0
        assert last["params"]["enabled"] is True

    def test_set_light_mobility_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_mobility(actor_name="DirLight_0", mobility="Static")
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_mobility"
        assert last["params"]["mobility"] == "Static"

    def test_set_light_shadow_enabled_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_shadow_enabled(actor_name="PointLight_0", enabled=False)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_shadow_enabled"
        assert last["params"]["enabled"] is False

    def test_set_light_shadow_bias_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_shadow_bias(actor_name="PointLight_0", bias=0.5)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_shadow_bias"
        assert last["params"]["bias"] == 0.5

    def test_set_light_contact_shadows_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_contact_shadows(actor_name="SpotLight_1", enabled=True, length=0.1)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_contact_shadows"
        assert last["params"]["enabled"] is True
        assert last["params"]["length"] == 0.1

    def test_set_light_volumetric_scattering_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_volumetric_scattering(actor_name="PointLight_0", enabled=True, intensity=2.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_volumetric_scattering"
        assert last["params"]["intensity"] == 2.0

    def test_set_light_attenuation_radius_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_attenuation_radius(actor_name="PointLight_0", radius=5000.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_attenuation_radius"
        assert last["params"]["radius"] == 5000.0

    def test_set_light_cone_angles_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_cone_angles(actor_name="SpotLight_1", inner=10.0, outer=45.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_cone_angles"
        assert last["params"]["inner"] == 10.0
        assert last["params"]["outer"] == 45.0

    def test_set_light_source_radius_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_source_radius(actor_name="PointLight_0", radius=5.0, soft_radius=2.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_source_radius"
        assert last["params"]["radius"] == 5.0
        assert last["params"]["soft_radius"] == 2.0

    def test_set_light_ies_profile_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_ies_profile(actor_name="SpotLight_1", ies_path="/Game/IES/Profile.ies")
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_ies_profile"
        assert last["params"]["ies_path"] == "/Game/IES/Profile.ies"

    def test_set_light_channel_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_light_channel(actor_name="PointLight_0", channel=1)
        last = fake_conn.history[-1]
        assert last["command"] == "set_light_channel"
        assert last["params"]["channel"] == 1

    def test_set_rect_light_properties_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_rect_light_properties(actor_name="RectLight_0", source_width=128.0, source_height=64.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_rect_light_properties"
        assert last["params"]["source_width"] == 128.0
        assert last["params"]["source_height"] == 64.0

    def test_set_sky_light_properties_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_sky_light_properties(actor_name="SkyLight_0", cubemap_path="/Game/HDRI/Sky", intensity=2.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_sky_light_properties"
        assert last["params"]["cubemap_path"] == "/Game/HDRI/Sky"
        assert last["params"]["intensity"] == 2.0

    def test_set_sky_atmosphere_properties_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_sky_atmosphere_properties(actor_name="SkyAtmosphere_0", ground_radius=6360.0, atmosphere_height=100.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_sky_atmosphere_properties"
        assert last["params"]["ground_radius"] == 6360.0
        assert last["params"]["atmosphere_height"] == 100.0

    def test_set_height_fog_properties_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_height_fog_properties(actor_name="Fog_0", fog_density=0.05, fog_height_falloff=0.2)
        last = fake_conn.history[-1]
        assert last["command"] == "set_height_fog_properties"
        assert last["params"]["fog_density"] == 0.05
        assert last["params"]["fog_height_falloff"] == 0.2

    def test_set_volumetric_fog_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_volumetric_fog(actor_name="Fog_0", enabled=True)
        last = fake_conn.history[-1]
        assert last["command"] == "set_volumetric_fog"
        assert last["params"]["enabled"] is True

    def test_set_directional_light_as_sun_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_directional_light_as_sun(actor_name="DirLight_0", is_sun=True)
        last = fake_conn.history[-1]
        assert last["command"] == "set_directional_light_as_sun"
        assert last["params"]["is_sun"] is True

    def test_set_sun_position_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_sun_position(actor_name="DirLight_0", azimuth=90.0, zenith=30.0)
        last = fake_conn.history[-1]
        assert last["command"] == "set_sun_position"
        assert last["params"]["azimuth"] == 90.0
        assert last["params"]["zenith"] == 30.0

    def test_create_hdri_backdrop_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.create_hdri_backdrop(actor_name="HDRI_0", hdri_path="/Game/HDRI/Studio", intensity=1.5)
        last = fake_conn.history[-1]
        assert last["command"] == "create_hdri_backdrop"
        assert last["params"]["actor_name"] == "HDRI_0"
        assert last["params"]["hdri_path"] == "/Game/HDRI/Studio"
        assert last["params"]["intensity"] == 1.5

    def test_create_reflection_capture_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.create_reflection_capture(actor_name="RC_0", type="Sphere", location=[0, 0, 100], radius=500.0)
        last = fake_conn.history[-1]
        assert last["command"] == "create_reflection_capture"
        assert last["params"]["type"] == "Sphere"
        assert last["params"]["radius"] == 500.0

    def test_create_lightmass_importance_volume_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.create_lightmass_importance_volume(location=[0, 0, 0], extent=[2000, 2000, 500])
        last = fake_conn.history[-1]
        assert last["command"] == "create_lightmass_importance_volume"
        assert last["params"]["location"] == [0, 0, 0]
        assert last["params"]["extent"] == [2000, 2000, 500]

    def test_build_lighting_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.build_lighting(quality="Production")
        last = fake_conn.history[-1]
        assert last["command"] == "build_lighting"
        assert last["params"]["quality"] == "Production"

    def test_set_lighting_scenario_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_lighting_scenario(scenario_name="Night", activate=True)
        last = fake_conn.history[-1]
        assert last["command"] == "set_lighting_scenario"
        assert last["params"]["scenario_name"] == "Night"
        assert last["params"]["activate"] is True

    def test_set_megaliights_payload(self, fake_conn):
        with patch.object(lighting_tools, "get_unreal_connection", return_value=fake_conn):
            srv.set_megaliights(enabled=True, quality=2)
        last = fake_conn.history[-1]
        assert last["command"] == "set_megaliights"
        assert last["params"]["enabled"] is True
        assert last["params"]["quality"] == 2


class TestLightingToolValidation:
    def test_set_light_mobility_invalid(self):
        result = srv.set_light_mobility(actor_name="Light", mobility="Flying")
        assert result.get("success") is False or result.get("error") is not None

    def test_set_light_channel_invalid(self):
        result = srv.set_light_channel(actor_name="Light", channel=5)
        assert result.get("success") is False or result.get("error") is not None

    def test_build_lighting_invalid_quality(self):
        result = srv.build_lighting(quality="Ultra")
        assert result.get("success") is False or result.get("error") is not None
