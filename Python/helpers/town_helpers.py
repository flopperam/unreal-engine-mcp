from typing import Dict, Any, List
import math
import random


def create_street_grid(
    unreal,
    set_actor_transform,
    blocks: int,
    block_size: float,
    street_width: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    streets: List[Dict[str, Any]] = []

    # Horizontal streets
    for i in range(blocks + 1):
        street_y = location[1] + (i - blocks / 2) * block_size
        for j in range(blocks):
            street_x = location[0] + (j - blocks / 2 + 0.5) * block_size
            actor_name = f"{name_prefix}_Street_H_{i}_{j}"
            result = unreal.send_command(
                "spawn_actor",
                {
                    "name": actor_name,
                    "type": "StaticMeshActor",
                    "location": [street_x, street_y, location[2] - 5],
                },
            )
            if result and result.get("status") == "success":
                set_actor_transform(
                    actor_name, scale=[block_size / 100.0 * 0.7, street_width / 100.0, 0.1]
                )
                streets.append(result.get("result"))

    # Vertical streets
    for i in range(blocks + 1):
        street_x = location[0] + (i - blocks / 2) * block_size
        for j in range(blocks):
            street_y = location[1] + (j - blocks / 2 + 0.5) * block_size
            actor_name = f"{name_prefix}_Street_V_{i}_{j}"
            result = unreal.send_command(
                "spawn_actor",
                {
                    "name": actor_name,
                    "type": "StaticMeshActor",
                    "location": [street_x, street_y, location[2] - 5],
                },
            )
            if result and result.get("status") == "success":
                set_actor_transform(
                    actor_name, scale=[street_width / 100.0, block_size / 100.0 * 0.7, 0.1]
                )
                streets.append(result.get("result"))

    return {"success": True, "actors": streets}


def create_street_lights(
    unreal,
    set_actor_transform,
    blocks: int,
    block_size: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    lights: List[Dict[str, Any]] = []
    for i in range(blocks + 1):
        for j in range(blocks + 1):
            if random.random() > 0.7:
                continue
            light_x = location[0] + (i - blocks / 2) * block_size
            light_y = location[1] + (j - blocks / 2) * block_size

            pole_name = f"{name_prefix}_LightPole_{i}_{j}"
            pole_result = unreal.send_command(
                "spawn_actor",
                {"name": pole_name, "type": "StaticMeshActor", "location": [light_x, light_y, location[2] + 200]},
            )
            if pole_result and pole_result.get("status") == "success":
                set_actor_transform(pole_name, scale=[0.2, 0.2, 4.0])
                lights.append(pole_result.get("result"))

            light_name = f"{name_prefix}_Light_{i}_{j}"
            light_result = unreal.send_command(
                "spawn_actor",
                {"name": light_name, "type": "StaticMeshActor", "location": [light_x, light_y, location[2] + 380]},
            )
            if light_result and light_result.get("status") == "success":
                set_actor_transform(light_name, scale=[0.3, 0.3, 0.3])
                lights.append(light_result.get("result"))

    return {"success": True, "actors": lights}


def create_traffic_lights(
    unreal,
    blocks: int,
    block_size: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    traffic_lights: List[Dict[str, Any]] = []
    for i in range(1, blocks, 2):
        for j in range(1, blocks, 2):
            intersection_x = location[0] + (i - blocks / 2) * block_size
            intersection_y = location[1] + (j - blocks / 2) * block_size
            for corner in range(4):
                angle = corner * math.pi / 2
                offset = 150
                pole_x = intersection_x + offset * math.cos(angle)
                pole_y = intersection_y + offset * math.sin(angle)
                pole_name = f"{name_prefix}_TrafficPole_{i}_{j}_{corner}"
                pole_result = unreal.send_command(
                    "spawn_actor",
                    {
                        "name": pole_name,
                        "type": "StaticMeshActor",
                        "location": [pole_x, pole_y, location[2] + 150],
                        "scale": [0.15, 0.15, 3.0],
                        "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
                    },
                )
                if pole_result and pole_result.get("status") == "success":
                    traffic_lights.append(pole_result.get("result"))
                light_name = f"{name_prefix}_TrafficLight_{i}_{j}_{corner}"
                light_result = unreal.send_command(
                    "spawn_actor",
                    {
                        "name": light_name,
                        "type": "StaticMeshActor",
                        "location": [pole_x, pole_y, location[2] + 280],
                        "scale": [0.3, 0.2, 0.8],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                    },
                )
                if light_result and light_result.get("status") == "success":
                    traffic_lights.append(light_result.get("result"))
    return {"success": True, "actors": traffic_lights}


def create_street_signage(
    unreal,
    blocks: int,
    block_size: float,
    location: List[float],
    name_prefix: str,
    town_size: str,
) -> Dict[str, Any]:
    signage: List[Dict[str, Any]] = []
    street_names = ["Main St", "1st Ave", "2nd Ave", "Park Blvd", "Commerce Dr", "Tech Way"]
    for i in range(0, blocks + 1, 2):
        for j in range(0, blocks + 1, 2):
            if random.random() > 0.5:
                continue
            sign_x = location[0] + (i - blocks / 2) * block_size + 100
            sign_y = location[1] + (j - blocks / 2) * block_size + 100
            pole_name = f"{name_prefix}_SignPole_{i}_{j}"
            pole_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": pole_name,
                    "type": "StaticMeshActor",
                    "location": [sign_x, sign_y, location[2] + 100],
                    "scale": [0.1, 0.1, 2.0],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
                },
            )
            if pole_result and pole_result.get("status") == "success":
                signage.append(pole_result.get("result"))
            sign_name = f"{name_prefix}_StreetSign_{i}_{j}"
            sign_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": sign_name,
                    "type": "StaticMeshActor",
                    "location": [sign_x, sign_y, location[2] + 180],
                    "scale": [1.5, 0.05, 0.3],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if sign_result and sign_result.get("status") == "success":
                signage.append(sign_result.get("result"))

    if town_size in ["large", "metropolis"]:
        num_billboards = random.randint(3, 8)
        for b in range(num_billboards):
            billboard_x = location[0] + random.uniform(-blocks * block_size / 3, blocks * block_size / 3)
            billboard_y = location[1] + random.uniform(-blocks * block_size / 3, blocks * block_size / 3)
            billboard_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_Billboard_{b}",
                    "type": "StaticMeshActor",
                    "location": [billboard_x, billboard_y, location[2] + 400],
                    "scale": [3.0, 0.1, 2.0],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if billboard_result and billboard_result.get("status") == "success":
                signage.append(billboard_result.get("result"))
            support_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_BillboardSupport_{b}_L",
                    "type": "StaticMeshActor",
                    "location": [billboard_x - 100, billboard_y, location[2] + 200],
                    "scale": [0.2, 0.2, 4.0],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
                },
            )
            if support_result and support_result.get("status") == "success":
                signage.append(support_result.get("result"))
            support_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_BillboardSupport_{b}_R",
                    "type": "StaticMeshActor",
                    "location": [billboard_x + 100, billboard_y, location[2] + 200],
                    "scale": [0.2, 0.2, 4.0],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
                },
            )
            if support_result and support_result.get("status") == "success":
                signage.append(support_result.get("result"))

    return {"success": True, "actors": signage}


def create_town_vehicles(
    unreal,
    set_actor_transform,
    blocks: int,
    block_size: float,
    location: List[float],
    name_prefix: str,
    vehicle_count: int,
) -> Dict[str, Any]:
    vehicles: List[Dict[str, Any]] = []
    for i in range(vehicle_count):
        street_x = location[0] + random.uniform(-blocks * block_size / 2, blocks * block_size / 2)
        street_y = location[1] + random.uniform(-blocks * block_size / 2, blocks * block_size / 2)
        car_name = f"{name_prefix}_Car_{i}"
        car_result = unreal.send_command(
            "spawn_actor",
            {"name": car_name, "type": "StaticMeshActor", "location": [street_x, street_y, location[2] + 50]},
        )
        if car_result and car_result.get("status") == "success":
            set_actor_transform(car_name, scale=[4.0, 2.0, 1.5])
            vehicles.append(car_result.get("result"))
    return {"success": True, "actors": vehicles}


def create_town_decorations(
    unreal,
    set_actor_transform,
    blocks: int,
    block_size: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    decorations: List[Dict[str, Any]] = []
    num_parks = max(1, blocks // 3)
    for park_id in range(num_parks):
        park_x = location[0] + random.uniform(-blocks * block_size / 3, blocks * block_size / 3)
        park_y = location[1] + random.uniform(-blocks * block_size / 3, blocks * block_size / 3)
        trees_per_park = random.randint(3, 8)
        for tree_id in range(trees_per_park):
            tree_x = park_x + random.uniform(-200, 200)
            tree_y = park_y + random.uniform(-200, 200)
            trunk_name = f"{name_prefix}_TreeTrunk_{park_id}_{tree_id}"
            trunk_result = unreal.send_command(
                "spawn_actor",
                {"name": trunk_name, "type": "StaticMeshActor", "location": [tree_x, tree_y, location[2] + 150]},
            )
            if trunk_result and trunk_result.get("status") == "success":
                set_actor_transform(trunk_name, scale=[0.5, 0.5, 3.0])
                decorations.append(trunk_result.get("result"))
            leaves_name = f"{name_prefix}_TreeLeaves_{park_id}_{tree_id}"
            leaves_result = unreal.send_command(
                "spawn_actor",
                {"name": leaves_name, "type": "StaticMeshActor", "location": [tree_x, tree_y, location[2] + 350]},
            )
            if leaves_result and leaves_result.get("status") == "success":
                set_actor_transform(leaves_name, scale=[2.0, 2.0, 2.0])
                decorations.append(leaves_result.get("result"))
    return {"success": True, "actors": decorations}


def create_sidewalks_crosswalks(
    unreal,
    blocks: int,
    block_size: float,
    street_width: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    sidewalks: List[Dict[str, Any]] = []
    sidewalk_width = 150.0

    # Horizontal sidewalks
    for i in range(blocks):
        for j in range(blocks + 1):
            sidewalk_y = location[1] + (j - blocks / 2) * block_size
            sidewalk_x = location[0] + (i - blocks / 2 + 0.5) * block_size
            north_sidewalk_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_SidewalkH_North_{i}_{j}",
                    "type": "StaticMeshActor",
                    "location": [sidewalk_x, sidewalk_y - street_width / 2 + sidewalk_width / 2, location[2]],
                    "scale": [block_size / 100.0 * 0.7, sidewalk_width / 100.0, 0.05],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if north_sidewalk_result and north_sidewalk_result.get("status") == "success":
                sidewalks.append(north_sidewalk_result.get("result"))
            south_sidewalk_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_SidewalkH_South_{i}_{j}",
                    "type": "StaticMeshActor",
                    "location": [sidewalk_x, sidewalk_y + street_width / 2 - sidewalk_width / 2, location[2]],
                    "scale": [block_size / 100.0 * 0.7, sidewalk_width / 100.0, 0.05],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if south_sidewalk_result and south_sidewalk_result.get("status") == "success":
                sidewalks.append(south_sidewalk_result.get("result"))

    # Vertical sidewalks
    for i in range(blocks + 1):
        for j in range(blocks):
            sidewalk_x = location[0] + (i - blocks / 2) * block_size
            sidewalk_y = location[1] + (j - blocks / 2 + 0.5) * block_size
            east_sidewalk_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_SidewalkV_East_{i}_{j}",
                    "type": "StaticMeshActor",
                    "location": [sidewalk_x - street_width / 2 + sidewalk_width / 2, sidewalk_y, location[2]],
                    "scale": [sidewalk_width / 100.0, block_size / 100.0 * 0.7, 0.05],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if east_sidewalk_result and east_sidewalk_result.get("status") == "success":
                sidewalks.append(east_sidewalk_result.get("result"))
            west_sidewalk_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_SidewalkV_West_{i}_{j}",
                    "type": "StaticMeshActor",
                    "location": [sidewalk_x + street_width / 2 - sidewalk_width / 2, sidewalk_y, location[2]],
                    "scale": [sidewalk_width / 100.0, block_size / 100.0 * 0.7, 0.05],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if west_sidewalk_result and west_sidewalk_result.get("status") == "success":
                sidewalks.append(west_sidewalk_result.get("result"))

    # Crosswalks
    crosswalk_width = 200.0
    for i in range(blocks + 1):
        for j in range(blocks + 1):
            intersection_x = location[0] + (i - blocks / 2) * block_size
            intersection_y = location[1] + (j - blocks / 2) * block_size
            for stripe in range(5):
                stripe_offset = (stripe - 2) * 40
                ns_res = unreal.send_command(
                    "spawn_actor",
                    {
                        "name": f"{name_prefix}_CrosswalkNS_{i}_{j}_{stripe}",
                        "type": "StaticMeshActor",
                        "location": [intersection_x + stripe_offset, intersection_y, location[2] + 1],
                        "scale": [0.3, crosswalk_width / 100.0, 0.02],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                    },
                )
                if ns_res and ns_res.get("status") == "success":
                    sidewalks.append(ns_res.get("result"))
                ew_res = unreal.send_command(
                    "spawn_actor",
                    {
                        "name": f"{name_prefix}_CrosswalkEW_{i}_{j}_{stripe}",
                        "type": "StaticMeshActor",
                        "location": [intersection_x, intersection_y + stripe_offset, location[2] + 1],
                        "scale": [crosswalk_width / 100.0, 0.3, 0.02],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                    },
                )
                if ew_res and ew_res.get("status") == "success":
                    sidewalks.append(ew_res.get("result"))

    return {"success": True, "actors": sidewalks}


def create_street_utilities(
    unreal,
    blocks: int,
    block_size: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    utilities: List[Dict[str, Any]] = []

    num_meters = blocks * 4
    for m in range(num_meters):
        meter_x = location[0] + random.uniform(-blocks * block_size / 3, blocks * block_size / 3)
        meter_y = location[1] + random.uniform(-blocks * block_size / 3, blocks * block_size / 3)
        sidewalk_offset = random.choice([-180, 180])
        if random.random() > 0.5:
            meter_x += sidewalk_offset
        else:
            meter_y += sidewalk_offset
        meter_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_ParkingMeter_{m}",
                "type": "StaticMeshActor",
                "location": [meter_x, meter_y, location[2] + 50],
                "scale": [0.15, 0.15, 1.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
            },
        )
        if meter_result and meter_result.get("status") == "success":
            utilities.append(meter_result.get("result"))
        head_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_MeterHead_{m}",
                "type": "StaticMeshActor",
                "location": [meter_x, meter_y, location[2] + 100],
                "scale": [0.25, 0.15, 0.3],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube",
            },
        )
        if head_result and head_result.get("status") == "success":
            utilities.append(head_result.get("result"))

    num_hydrants = blocks + 2
    for h in range(num_hydrants):
        hydrant_x = location[0] + random.uniform(-blocks * block_size / 2, blocks * block_size / 2)
        hydrant_y = location[1] + random.uniform(-blocks * block_size / 2, blocks * block_size / 2)
        hydrant_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_Hydrant_{h}",
                "type": "StaticMeshActor",
                "location": [hydrant_x, hydrant_y, location[2] + 40],
                "scale": [0.3, 0.3, 0.8],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
            },
        )
        if hydrant_result and hydrant_result.get("status") == "success":
            utilities.append(hydrant_result.get("result"))
        cap_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_HydrantCap_{h}",
                "type": "StaticMeshActor",
                "location": [hydrant_x, hydrant_y, location[2] + 75],
                "scale": [0.35, 0.35, 0.1],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
            },
        )
        if cap_result and cap_result.get("status") == "success":
            utilities.append(cap_result.get("result"))

    return {"success": True, "actors": utilities}


def create_central_plaza(
    unreal,
    location: List[float],
    block_size: float,
    name_prefix: str,
) -> Dict[str, Any]:
    plaza: List[Dict[str, Any]] = []
    plaza_size = block_size * 0.8

    plaza_floor_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_PlazaFloor",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 2],
            "scale": [plaza_size / 100.0, plaza_size / 100.0, 0.05],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if plaza_floor_result and plaza_floor_result.get("status") == "success":
        plaza.append(plaza_floor_result.get("result"))

    fountain_base_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_FountainBase",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 10],
            "scale": [3.0, 3.0, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
        },
    )
    if fountain_base_result and fountain_base_result.get("status") == "success":
        plaza.append(fountain_base_result.get("result"))

    fountain_center_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_FountainCenter",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 50],
            "scale": [0.5, 0.5, 0.8],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
        },
    )
    if fountain_center_result and fountain_center_result.get("status") == "success":
        plaza.append(fountain_center_result.get("result"))

    fountain_top_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_FountainTop",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 80],
            "scale": [1.5, 1.5, 0.1],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
        },
    )
    if fountain_top_result and fountain_top_result.get("status") == "success":
        plaza.append(fountain_top_result.get("result"))

    monument_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Monument",
            "type": "StaticMeshActor",
            "location": [location[0] + plaza_size / 3, location[1], location[2] + 100],
            "scale": [1.0, 1.0, 2.0],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
        },
    )
    if monument_result and monument_result.get("status") == "success":
        plaza.append(monument_result.get("result"))

    monument_base_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_MonumentBase",
            "type": "StaticMeshActor",
            "location": [location[0] + plaza_size / 3, location[1], location[2] + 30],
            "scale": [2.0, 2.0, 0.6],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if monument_base_result and monument_base_result.get("status") == "success":
        plaza.append(monument_base_result.get("result"))

    num_benches = 8
    for i in range(num_benches):
        angle = (2 * math.pi * i) / num_benches
        bench_x = location[0] + plaza_size / 3 * math.cos(angle)
        bench_y = location[1] + plaza_size / 3 * math.sin(angle)
        bench_rotation = [0, 0, angle * 180 / math.pi]
        bench_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_PlazaBench_{i}",
                "type": "StaticMeshActor",
                "location": [bench_x, bench_y, location[2] + 30],
                "rotation": bench_rotation,
                "scale": [1.5, 0.5, 0.6],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube",
            },
        )
        if bench_result and bench_result.get("status") == "success":
            plaza.append(bench_result.get("result"))

    num_lights = 12
    for i in range(num_lights):
        angle = (2 * math.pi * i) / num_lights
        light_x = location[0] + plaza_size / 2 * math.cos(angle)
        light_y = location[1] + plaza_size / 2 * math.sin(angle)
        post_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_PlazaLightPost_{i}",
                "type": "StaticMeshActor",
                "location": [light_x, light_y, location[2] + 100],
                "scale": [0.15, 0.15, 2.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
            },
        )
        if post_result and post_result.get("status") == "success":
            plaza.append(post_result.get("result"))
        light_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_PlazaLight_{i}",
                "type": "StaticMeshActor",
                "location": [light_x, light_y, location[2] + 180],
                "scale": [0.4, 0.4, 0.3],
                "static_mesh": "/Engine/BasicShapes/Sphere.Sphere",
            },
        )
        if light_result and light_result.get("status") == "success":
            plaza.append(light_result.get("result"))

    return {"success": True, "actors": plaza}


def create_urban_furniture(
    unreal,
    blocks: int,
    block_size: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    furniture: List[Dict[str, Any]] = []
    num_furniture_items = blocks * blocks // 2
    for f in range(num_furniture_items):
        street_x = location[0] + random.uniform(-blocks * block_size / 2, blocks * block_size / 2)
        street_y = location[1] + random.uniform(-blocks * block_size / 2, blocks * block_size / 2)
        sidewalk_offset = random.choice([-200, 200])
        if random.random() > 0.5:
            furniture_x = street_x + sidewalk_offset
            furniture_y = street_y
        else:
            furniture_x = street_x
            furniture_y = street_y + sidewalk_offset
        furniture_type = random.choice(["bench", "trash", "bus_stop"])
        if furniture_type == "bench":
            bench_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_Bench_{f}",
                    "type": "StaticMeshActor",
                    "location": [furniture_x, furniture_y, location[2] + 30],
                    "scale": [1.5, 0.5, 0.6],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if bench_result and bench_result.get("status") == "success":
                furniture.append(bench_result.get("result"))
            for support_offset in [-50, 50]:
                support_result = unreal.send_command(
                    "spawn_actor",
                    {
                        "name": f"{name_prefix}_BenchSupport_{f}_{support_offset}",
                        "type": "StaticMeshActor",
                        "location": [furniture_x + support_offset, furniture_y, location[2] + 15],
                        "scale": [0.1, 0.5, 0.3],
                        "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                    },
                )
                if support_result and support_result.get("status") == "success":
                    furniture.append(support_result.get("result"))
        elif furniture_type == "trash":
            trash_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_TrashCan_{f}",
                    "type": "StaticMeshActor",
                    "location": [furniture_x, furniture_y, location[2] + 40],
                    "scale": [0.4, 0.4, 0.8],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
                },
            )
            if trash_result and trash_result.get("status") == "success":
                furniture.append(trash_result.get("result"))
        else:
            shelter_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_BusStop_{f}",
                    "type": "StaticMeshActor",
                    "location": [furniture_x, furniture_y, location[2] + 120],
                    "scale": [2.0, 1.0, 0.1],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if shelter_result and shelter_result.get("status") == "success":
                furniture.append(shelter_result.get("result"))
            for post_x in [-80, 80]:
                post_result = unreal.send_command(
                    "spawn_actor",
                    {
                        "name": f"{name_prefix}_BusStopPost_{f}_{post_x}",
                        "type": "StaticMeshActor",
                        "location": [furniture_x + post_x, furniture_y, location[2] + 60],
                        "scale": [0.1, 0.1, 1.2],
                        "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
                    },
                )
                if post_result and post_result.get("status") == "success":
                    furniture.append(post_result.get("result"))
            bench_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_BusStopBench_{f}",
                    "type": "StaticMeshActor",
                    "location": [furniture_x, furniture_y + 30, location[2] + 25],
                    "scale": [1.8, 0.4, 0.5],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if bench_result and bench_result.get("status") == "success":
                furniture.append(bench_result.get("result"))
    return {"success": True, "actors": furniture}


