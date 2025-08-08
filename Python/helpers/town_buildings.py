from typing import Dict, Any, List, Callable
import random


def create_town_building(
    unreal,
    set_actor_transform: Callable[..., Dict[str, Any]],
    construct_house_fn: Callable[..., Dict[str, Any]],
    create_tower_fn: Callable[..., Dict[str, Any]],
    location: List[float],
    max_size: float,
    max_height: int,
    name_prefix: str,
    building_id: int,
    building_type: str,
) -> Dict[str, Any]:
    offset_x = random.uniform(-max_size / 4, max_size / 4)
    offset_y = random.uniform(-max_size / 4, max_size / 4)
    building_loc = [location[0] + offset_x, location[1] + offset_y, location[2]]

    if building_type == "house":
        styles = ["modern", "cottage"]
        width = random.randint(800, 1200)
        depth = random.randint(600, 1000)
        height = random.randint(300, 500)
        return construct_house_fn(
            width=width,
            depth=depth,
            height=height,
            location=building_loc,
            name_prefix=f"{name_prefix}_{building_id}",
            house_style=random.choice(styles),
        )

    if building_type == "mansion":
        return construct_house_fn(
            width=random.randint(1500, 2000),
            depth=random.randint(1200, 1600),
            height=random.randint(500, 700),
            location=building_loc,
            name_prefix=f"{name_prefix}_Mansion_{building_id}",
            house_style="mansion",
        )

    if building_type == "tower":
        tower_height = random.randint(max_height // 2, max_height)
        base_size = random.randint(3, 6)
        styles = ["cylindrical", "square", "tapered"]
        return create_tower_fn(
            height=tower_height,
            base_size=base_size,
            location=building_loc,
            name_prefix=f"{name_prefix}_Tower_{building_id}",
            tower_style=random.choice(styles),
        )

    if building_type == "skyscraper":
        return create_skyscraper(
            unreal,
            set_actor_transform,
            height=random.randint(max(20, max_height // 2), max_height),
            base_width=random.randint(600, 1000),
            base_depth=random.randint(600, 1000),
            location=building_loc,
            name_prefix=f"{name_prefix}_Skyscraper_{building_id}",
        )

    if building_type == "office_tower":
        return create_office_tower(
            unreal,
            set_actor_transform,
            floors=random.randint(10, max(15, max_height // 2)),
            width=random.randint(800, 1200),
            depth=random.randint(800, 1200),
            location=building_loc,
            name_prefix=f"{name_prefix}_Office_{building_id}",
        )

    if building_type == "apartment_complex":
        return create_apartment_complex(
            unreal,
            set_actor_transform,
            floors=random.randint(5, max(10, max_height // 3)),
            units_per_floor=random.randint(4, 8),
            location=building_loc,
            name_prefix=f"{name_prefix}_Apartments_{building_id}",
        )

    if building_type == "shopping_mall":
        return create_shopping_mall(
            unreal,
            width=random.randint(1500, 2500),
            depth=random.randint(1500, 2500),
            floors=random.randint(2, 4),
            location=building_loc,
            name_prefix=f"{name_prefix}_Mall_{building_id}",
        )

    if building_type == "parking_garage":
        return create_parking_garage(
            unreal,
            levels=random.randint(3, 6),
            width=random.randint(1000, 1500),
            depth=random.randint(800, 1200),
            location=building_loc,
            name_prefix=f"{name_prefix}_Parking_{building_id}",
        )

    if building_type == "hotel":
        return create_hotel(
            unreal,
            set_actor_transform,
            floors=random.randint(10, max(20, max_height // 2)),
            width=random.randint(1000, 1500),
            depth=random.randint(800, 1200),
            location=building_loc,
            name_prefix=f"{name_prefix}_Hotel_{building_id}",
        )

    if building_type == "restaurant":
        return create_restaurant(
            unreal,
            width=random.randint(600, 1000),
            depth=random.randint(500, 800),
            location=building_loc,
            name_prefix=f"{name_prefix}_Restaurant_{building_id}",
        )

    if building_type == "store":
        return create_store(
            unreal,
            width=random.randint(500, 800),
            depth=random.randint(400, 600),
            location=building_loc,
            name_prefix=f"{name_prefix}_Store_{building_id}",
        )

    if building_type == "apartment_building":
        return create_apartment_building(
            unreal,
            set_actor_transform,
            floors=random.randint(3, 5),
            width=random.randint(800, 1200),
            depth=random.randint(600, 1000),
            location=building_loc,
            name_prefix=f"{name_prefix}_AptBuilding_{building_id}",
        )

    # Fallback commercial
    return construct_house_fn(
        width=random.randint(1000, 1500),
        depth=random.randint(800, 1200),
        height=random.randint(400, 600),
        location=building_loc,
        name_prefix=f"{name_prefix}_Commercial_{building_id}",
        house_style="modern",
    )


def create_skyscraper(
    unreal,
    set_actor_transform: Callable[..., Dict[str, Any]],
    height: int,
    base_width: float,
    base_depth: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    floor_height = 150.0
    floor_thickness = 30.0
    
    # Create foundation - positioned at ground level
    foundation_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [(base_width + 200) / 100.0, (base_depth + 200) / 100.0, floor_thickness/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if foundation_result and foundation_result.get("status") == "success":
        actors.append(foundation_result.get("result"))


    
    sections = min(5, height // 5)
    current_width = base_width
    current_depth = base_depth
    current_height = location[2] + floor_thickness
    
    for section in range(sections):
        section_floors = height // sections
        if section == sections - 1:
            section_floors += height % sections
        taper_factor = 1 - (section * 0.1)
        current_width = base_width * max(0.6, taper_factor)
        current_depth = base_depth * max(0.6, taper_factor)
        section_height = section_floors * floor_height
        section_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_Section_{section}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1], current_height + section_height / 2],
                "scale": [current_width / 100.0, current_depth / 100.0, section_height / 100.0],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube",
            },
        )
        if section_result and section_result.get("status") == "success":
            actors.append(section_result.get("result"))
        if section < sections - 1:
            balcony_result = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_Balcony_{section}",
                    "type": "StaticMeshActor",
                    "location": [location[0], location[1], current_height + section_height - 25],
                    "scale": [(current_width + 100) / 100.0, (current_depth + 100) / 100.0, 0.5],
                    "static_mesh": "/Engine/BasicShapes/Cube.Cube",
                },
            )
            if balcony_result and balcony_result.get("status") == "success":
                actors.append(balcony_result.get("result"))
        current_height += section_height

    spire_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Spire",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], current_height + 300],
            "scale": [0.2, 0.2, 6.0],
            "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
        },
    )
    if spire_result and spire_result.get("status") == "success":
        actors.append(spire_result.get("result"))

    for i in range(3):
        equipment_x = location[0] + random.uniform(-current_width / 4, current_width / 4)
        equipment_y = location[1] + random.uniform(-current_depth / 4, current_depth / 4)
        equipment_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_RoofEquipment_{i}",
                "type": "StaticMeshActor",
                "location": [equipment_x, equipment_y, current_height + 50],
                "scale": [1.0, 1.0, 1.0],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube",
            },
        )
        if equipment_result and equipment_result.get("status") == "success":
            actors.append(equipment_result.get("result"))
    return {"success": True, "actors": actors}


def create_office_tower(
    unreal,
    set_actor_transform: Callable[..., Dict[str, Any]],
    floors: int,
    width: float,
    depth: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    floor_height = 140.0
    floor_thickness = 30.0
    
    # Create foundation - positioned at ground level
    foundation_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [(width + 100) / 100.0, (depth + 100) / 100.0, floor_thickness/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if foundation_result and foundation_result.get("status") == "success":
        actors.append(foundation_result.get("result"))


    
    lobby_height = floor_height * 1.5
    lobby_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Lobby",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness + lobby_height / 2],
            "scale": [width / 100.0, depth / 100.0, lobby_height / 100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if lobby_result and lobby_result.get("status") == "success":
        actors.append(lobby_result.get("result"))

    tower_height = (floors - 1) * floor_height
    tower_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Tower",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness + lobby_height + tower_height / 2],
            "scale": [width / 100.0, depth / 100.0, tower_height / 100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if tower_result and tower_result.get("status") == "success":
        actors.append(tower_result.get("result"))

    for floor in range(2, floors, 3):
        band_height = location[2] + lobby_height + (floor - 1) * floor_height
        band_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_WindowBand_{floor}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1], band_height],
                "scale": [(width + 20) / 100.0, (depth + 20) / 100.0, 0.2],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube",
            },
        )
        if band_result and band_result.get("status") == "success":
            actors.append(band_result.get("result"))

    rooftop_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Rooftop",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + lobby_height + tower_height + 30],
            "scale": [(width - 100) / 100.0, (depth - 100) / 100.0, 0.6],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if rooftop_result and rooftop_result.get("status") == "success":
        actors.append(rooftop_result.get("result"))

    return {"success": True, "actors": actors}


def create_apartment_complex(
    unreal,
    set_actor_transform: Callable[..., Dict[str, Any]],
    floors: int,
    units_per_floor: int,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    floor_height = 120.0
    floor_thickness = 30.0
    width = 200 * units_per_floor // 2
    depth = 800
    
    # Create foundation - positioned at ground level
    foundation_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [(width + 100) / 100.0, (depth + 100) / 100.0, floor_thickness/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if foundation_result and foundation_result.get("status") == "success":
        actors.append(foundation_result.get("result"))
    

    
    building_height = floors * floor_height
    building_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Building",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness + building_height / 2],
            "scale": [width / 100.0, depth / 100.0, building_height / 100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if building_result and building_result.get("status") == "success":
        actors.append(building_result.get("result"))
    for floor in range(1, floors):
        balcony_height = location[2] + floor * floor_height - 20
        front_balcony_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_FrontBalcony_{floor}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1] - depth / 2 - 50, balcony_height],
                "scale": [width / 100.0, 1.0, 0.2],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube",
            },
        )
        if front_balcony_result and front_balcony_result.get("status") == "success":
            actors.append(front_balcony_result.get("result"))
        back_balcony_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_BackBalcony_{floor}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1] + depth / 2 + 50, balcony_height],
                "scale": [width / 100.0, 1.0, 0.2],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube",
            },
        )
        if back_balcony_result and back_balcony_result.get("status") == "success":
            actors.append(back_balcony_result.get("result"))
    rooftop_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Rooftop",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + building_height + 15],
            "scale": [(width + 50) / 100.0, (depth + 50) / 100.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if rooftop_result and rooftop_result.get("status") == "success":
        actors.append(rooftop_result.get("result"))
    return {"success": True, "actors": actors}


def create_shopping_mall(
    unreal,
    width: float,
    depth: float,
    floors: int,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    floor_height = 200.0
    floor_thickness = 30.0
    
    # Create foundation - positioned at ground level
    foundation_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [(width + 200) / 100.0, (depth + 200) / 100.0, floor_thickness/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if foundation_result and foundation_result.get("status") == "success":
        actors.append(foundation_result.get("result"))
    

    
    mall_height = floors * floor_height
    main_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Main",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness + mall_height / 2],
            "scale": [width / 100.0, depth / 100.0, mall_height / 100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if main_result and main_result.get("status") == "success":
        actors.append(main_result.get("result"))
    canopy_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Canopy",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - depth / 2 - 50, location[2] + 50],
            "scale": [width / 100.0 * 0.6, 1.0, 0.3],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if canopy_result and canopy_result.get("status") == "success":
        actors.append(canopy_result.get("result"))
    return {"success": True, "actors": actors}


def create_parking_garage(
    unreal,
    levels: int,
    width: float,
    depth: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    level_height = 150.0
    floor_thickness = 30.0
    
    # Create foundation - positioned at ground level
    foundation_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [(width + 100) / 100.0, (depth + 100) / 100.0, floor_thickness/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if foundation_result and foundation_result.get("status") == "success":
        actors.append(foundation_result.get("result"))
    

    
    # Create each level floor
    for level in range(levels):
        level_result = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_Level_{level}",
                "type": "StaticMeshActor",
                "location": [location[0], location[1], location[2] + floor_thickness + level_height * level],
                "scale": [width / 100.0, depth / 100.0, floor_thickness/100.0],
                "static_mesh": "/Engine/BasicShapes/Cube.Cube",
            },
        )
        if level_result and level_result.get("status") == "success":
            actors.append(level_result.get("result"))

    # Corner pillars that span all levels
    pillar_radius = 0.4
    pillar_height = levels * level_height
    corner_offsets = [
        (-width / 2 + 60, -depth / 2 + 60),
        ( width / 2 - 60, -depth / 2 + 60),
        (-width / 2 + 60,  depth / 2 - 60),
        ( width / 2 - 60,  depth / 2 - 60),
    ]
    for idx, (dx, dy) in enumerate(corner_offsets):
        pillar_res = unreal.send_command(
            "spawn_actor",
            {
                "name": f"{name_prefix}_Pillar_Corner_{idx}",
                "type": "StaticMeshActor",
                "location": [location[0] + dx, location[1] + dy, location[2] + pillar_height / 2],
                "scale": [pillar_radius, pillar_radius, pillar_height / 100.0],
                "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
            },
        )
        if pillar_res and pillar_res.get("status") == "success":
            actors.append(pillar_res.get("result"))

    # Interior pillars grid
    interior_cols_x = max(1, int(width // 400) - 1)
    interior_cols_y = max(1, int(depth // 400) - 1)
    for ix in range(interior_cols_x):
        for iy in range(interior_cols_y):
            x = -width / 2 + (ix + 1) * (width / (interior_cols_x + 1))
            y = -depth / 2 + (iy + 1) * (depth / (interior_cols_y + 1))
            pillar_res = unreal.send_command(
                "spawn_actor",
                {
                    "name": f"{name_prefix}_Pillar_{ix}_{iy}",
                    "type": "StaticMeshActor",
                    "location": [location[0] + x, location[1] + y, location[2] + pillar_height / 2],
                    "scale": [pillar_radius * 0.8, pillar_radius * 0.8, pillar_height / 100.0],
                    "static_mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
                },
            )
            if pillar_res and pillar_res.get("status") == "success":
                actors.append(pillar_res.get("result"))
    
    # Add roof at the top level
    roof_thickness = 30.0
    roof_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Roof",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + levels * level_height + roof_thickness/2],
            "scale": [(width + 50) / 100.0, (depth + 50) / 100.0, roof_thickness/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if roof_result and roof_result.get("status") == "success":
        actors.append(roof_result.get("result"))
    
    # Add entrance ramp
    ramp_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Ramp",
            "type": "StaticMeshActor",
            "location": [location[0] + width / 3, location[1] - depth / 2 - 50, location[2] + level_height/2],
            "scale": [1.5, 4.0, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if ramp_result and ramp_result.get("status") == "success":
        actors.append(ramp_result.get("result"))
    return {"success": True, "actors": actors}


def create_hotel(
    unreal,
    set_actor_transform: Callable[..., Dict[str, Any]],
    floors: int,
    width: float,
    depth: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    floor_height = 150.0
    lobby_height = 220.0
    floor_thickness = 30.0
    
    # Create foundation - positioned at ground level
    foundation_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [(width + 100) / 100.0, (depth + 100) / 100.0, floor_thickness/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if foundation_result and foundation_result.get("status") == "success":
        actors.append(foundation_result.get("result"))
    

    
    lobby_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Lobby",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness + lobby_height / 2],
            "scale": [width / 100.0, depth / 100.0, lobby_height / 100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if lobby_result and lobby_result.get("status") == "success":
        actors.append(lobby_result.get("result"))
    tower_height = (floors - 1) * floor_height
    tower_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Tower",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness + lobby_height + tower_height / 2],
            "scale": [width / 100.0, depth / 100.0, tower_height / 100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if tower_result and tower_result.get("status") == "success":
        actors.append(tower_result.get("result"))
    canopy_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_EntranceCanopy",
            "type": "StaticMeshActor",
            "location": [location[0], location[1] - depth / 2 - 50, location[2] + 40],
            "scale": [width / 100.0 * 0.5, 1.0, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if canopy_result and canopy_result.get("status") == "success":
        actors.append(canopy_result.get("result"))
    return {"success": True, "actors": actors}


def create_restaurant(
    unreal,
    width: float,
    depth: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Main",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 100],
            "scale": [width / 100.0, depth / 100.0, 2.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if result and result.get("status") == "success":
        actors.append(result.get("result"))
    return {"success": True, "actors": actors}


def create_store(
    unreal,
    width: float,
    depth: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Main",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + 100],
            "scale": [width / 100.0, depth / 100.0, 2.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if result and result.get("status") == "success":
        actors.append(result.get("result"))
    return {"success": True, "actors": actors}


def create_apartment_building(
    unreal,
    set_actor_transform: Callable[..., Dict[str, Any]],
    floors: int,
    width: float,
    depth: float,
    location: List[float],
    name_prefix: str,
) -> Dict[str, Any]:
    actors: List[Dict[str, Any]] = []
    floor_height = 130.0
    floor_thickness = 30.0
    
    # Create foundation - positioned at ground level
    foundation_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Foundation",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness/2],
            "scale": [(width + 80) / 100.0, (depth + 80) / 100.0, floor_thickness/100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if foundation_result and foundation_result.get("status") == "success":
        actors.append(foundation_result.get("result"))
    

    
    building_height = floors * floor_height
    building_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Building",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + floor_thickness + building_height / 2],
            "scale": [width / 100.0, depth / 100.0, building_height / 100.0],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if building_result and building_result.get("status") == "success":
        actors.append(building_result.get("result"))
    rooftop_result = unreal.send_command(
        "spawn_actor",
        {
            "name": f"{name_prefix}_Rooftop",
            "type": "StaticMeshActor",
            "location": [location[0], location[1], location[2] + building_height + 10],
            "scale": [(width + 40) / 100.0, (depth + 40) / 100.0, 0.2],
            "static_mesh": "/Engine/BasicShapes/Cube.Cube",
        },
    )
    if rooftop_result and rooftop_result.get("status") == "success":
        actors.append(rooftop_result.get("result"))
    return {"success": True, "actors": actors}


