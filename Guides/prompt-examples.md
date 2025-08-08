# 💡 Prompt Examples

Real-world examples showing how natural language requests translate into MCP tool calls for amazing results.

## 🏗️ Architectural Projects

### Modern House with Garden
**User Prompt:** *"Create a modern house with a garden area featuring some decorative spheres"*

**Tool Sequence:**
```bash
1. construct_house(house_style="modern", width=1200, depth=800, location=[0, 0, 0])
2. create_pyramid(base_size=2, block_size=50, location=[800, 400, 0])  # Garden feature
3. spawn_physics_actor(name="GardenSphere1", mesh_path="/Engine/BasicShapes/Sphere.Sphere", location=[600, 200, 50])
4. spawn_physics_actor(name="GardenSphere2", mesh_path="/Engine/BasicShapes/Sphere.Sphere", location=[700, 300, 50])
```

### Medieval Castle Complex
**User Prompt:** *"Build a medieval castle with defensive walls and towers"*

**Tool Sequence:**
```bash
1. construct_house(house_style="mansion", width=1000, depth=1000, location=[0, 0, 0])  # Main keep
2. create_tower(height=12, base_size=3, tower_style="cylindrical", location=[-600, -600, 0])  # Corner tower
3. create_tower(height=12, base_size=3, tower_style="cylindrical", location=[600, -600, 0])   # Corner tower  
4. create_wall(length=12, height=4, orientation="x", location=[-600, -800, 0])  # North wall
5. create_wall(length=12, height=4, orientation="y", location=[-800, -600, 0])  # West wall
6. create_arch(radius=200, segments=8, location=[0, -800, 0])  # Grand entrance
```

## 🎮 Game Level Design

### Physics Playground
**User Prompt:** *"Create a fun physics area where balls can knock down a pyramid"*

**Tool Sequence:**
```bash
1. create_pyramid(base_size=4, block_size=100, location=[0, 0, 0])
2. create_bouncy_ball(name="Ball1", location=[-500, 0, 300])
3. create_bouncy_ball(name="Ball2", location=[-400, 100, 400]) 
4. create_bouncy_ball(name="Ball3", location=[-600, -100, 350])
5. create_wall(length=3, height=1, orientation="x", location=[-700, 0, 0])  # Launch ramp
```

### Adventure Maze
**User Prompt:** *"Design a challenging maze with multiple exits and obstacles"*

**Tool Sequence:**
```bash
1. create_maze(rows=15, cols=15, wall_height=3, cell_size=200, location=[0, 0, 0])
2. delete_actor("MazeWall_7_14")   # Create second exit
3. delete_actor("MazeWall_14_7")   # Create third exit  
4. create_obstacle_course(checkpoints=3, spacing=300, location=[1500, 0, 0])  # Bonus area

```

## 🏙️ Urban Planning

### Downtown District
**User Prompt:** *"Create a bustling downtown area with skyscrapers and city infrastructure"*

**Tool Sequence:**
```bash
1. create_town(town_size="large", architectural_style="downtown", building_density=0.9, location=[0, 0, 0])
2. create_tower(height=20, base_size=4, tower_style="square", location=[500, 500, 0])  # Landmark tower
3. create_tower(height=15, base_size=3, tower_style="square", location=[-400, 600, 0]) # Additional skyscraper

```

### Suburban Neighborhood  
**User Prompt:** *"Build a quiet residential area with several different house styles"*

**Tool Sequence:**
```bash
1. construct_house(house_style="modern", location=[0, 0, 0])
2. construct_house(house_style="cottage", location=[800, 0, 0]) 
3. construct_house(house_style="mansion", location=[1600, 0, 0])
4. construct_house(house_style="modern", location=[0, 800, 0])
5. create_arch(radius=150, segments=6, location=[800, 400, 0])  # Neighborhood entrance
```

## 🎪 Multi-AI Collaboration

### East vs West City Building Challenge
**User Prompt:** *"Set up two AI agents to build competing cities on opposite sides"*

**LLM Agent 1 (West Team) Instructions:**
> *"🏗️ CITY BUILDER CHALLENGE! You're building in the WEST zone. Create the most spectacular city to win using:"*
> 
> `create_town(town_size="metropolis", architectural_style="futuristic", building_density=0.9, location=[-3000, 0, 0])`
>
> *"Add landmarks with create_tower() and create_arch() to make your city unique!"*

**LLM Agent 2 (East Team) Instructions:**  
> *"🏙️ CITY BUILDER SHOWDOWN! You're building in the EAST zone. Create an amazing city that beats the competition using:"*
>
> `create_town(town_size="metropolis", architectural_style="downtown", building_density=0.8, location=[3000, 0, 0])`
>
> *"Use construct_house() for custom districts and create_obstacle_course() for parks!"*

### Collaborative World Building
**User Prompt:** *"Have multiple AIs work together to build different districts of the same city"*

**District Assignments:**
```bash
# AI 1: Residential District
construct_house(house_style="cottage", location=[-1000, -1000, 0])
construct_house(house_style="modern", location=[-500, -1000, 0])
construct_house(house_style="cottage", location=[-1000, -500, 0])

# AI 2: Commercial District  
create_town(town_size="small", architectural_style="downtown", location=[0, 0, 0])
create_tower(height=15, base_size=5, tower_style="square", location=[200, 200, 0])

# AI 3: Entertainment District
create_maze(rows=10, cols=10, location=[1000, 0, 0])
create_obstacle_course(checkpoints=5, spacing=400, location=[1000, 1000, 0])

```

## 🎨 Creative Showcases

### Architectural Portfolio
**User Prompt:** *"Create a showcase of different architectural styles side by side"*

**Tool Sequence:**
```bash
# Ancient/Classical Section
1. create_arch(radius=300, segments=10, location=[-1000, 0, 0])
2. create_tower(height=8, base_size=4, tower_style="cylindrical", location=[-800, 0, 0])

# Medieval Section  
3. construct_house(house_style="cottage", location=[-200, 0, 0])
4. create_wall(length=5, height=3, orientation="x", location=[-300, 200, 0])

# Modern Section
5. construct_house(house_style="modern", location=[400, 0, 0])  
6. create_tower(height=12, base_size=3, tower_style="square", location=[600, 0, 0])

# Futuristic Section
7. create_town(town_size="small", architectural_style="futuristic", location=[1200, 0, 0])
```

### Fantasy Village
**User Prompt:** *"Build a magical fantasy village with mystical elements"*

**Tool Sequence:**
```bash
1. construct_house(house_style="cottage", location=[0, 0, 0])       # Wizard's cottage
2. construct_house(house_style="cottage", location=[400, 300, 0])   # Villager house 1  
3. construct_house(house_style="cottage", location=[-300, 400, 0])  # Villager house 2
4. create_tower(height=15, base_size=2, tower_style="tapered", location=[200, -200, 0])  # Wizard tower
5. create_arch(radius=200, segments=8, location=[0, 500, 0])        # Mystical portal
6. create_maze(rows=6, cols=6, wall_height=2, location=[-600, 0, 0]) # Enchanted hedge maze

```

## 🚀 Advanced Techniques

### Procedural Content Pipeline
**User Prompt:** *"Generate a complete game level with multiple challenge areas"*

**Phase 1: Base Layout**
```bash
1. create_maze(rows=12, cols=12, wall_height=3, location=[0, 0, 0])          # Central maze
2. create_town(town_size="small", location=[2000, 0, 0])                      # Safe zone
3. create_obstacle_course(checkpoints=8, spacing=300, location=[-2000, 0, 0]) # Challenge area
```

**Phase 2: Interactive Elements**
```bash
4. create_pyramid(base_size=3, location=[0, 2000, 0])                        # Climbable structure
5. create_bouncy_ball(name="PuzzleBall", location=[100, 100, 200])           # Physics puzzle
6. spawn_physics_actor(name="MovableCrate", location=[500, 500, 50])         # Interactive object
```

**Phase 3: Characters and Atmosphere**
```bash
  
9. construct_house(house_style="cottage", location=[1800, 200, 0])           # Quest hub building
```

### Dynamic Weather/Time Simulation
**User Prompt:** *"Create different seasonal versions of the same location"*

**Spring Version:**
```bash
construct_house(house_style="cottage", location=[0, 0, 0])
# Add green spheres as "foliage"
spawn_physics_actor(name="Tree1", mesh_path="/Engine/BasicShapes/Sphere.Sphere", location=[300, 300, 100])
spawn_physics_actor(name="Tree2", mesh_path="/Engine/BasicShapes/Sphere.Sphere", location=[-200, 400, 100])
```

**Winter Version:**
```bash
construct_house(house_style="cottage", location=[0, 0, 0])  
# Add white cubes as "snow blocks"
spawn_actor(name="Snow1", type="StaticMeshActor", location=[100, 100, 25])
spawn_actor(name="Snow2", type="StaticMeshActor", location=[200, 150, 25])
```

## 💰 Commercial Applications

### Real Estate Visualization
**User Prompt:** *"Create a property development showcase with different housing options"*

**Tool Sequence:**
```bash
# Luxury Section
1. construct_house(house_style="mansion", width=1500, depth=1200, location=[0, 0, 0])
2. create_arch(radius=250, segments=8, location=[0, 800, 0])  # Grand entrance

# Family Section  
3. construct_house(house_style="modern", location=[1000, 0, 0])
4. construct_house(house_style="modern", location=[1000, 600, 0])

# Starter Homes Section
5. construct_house(house_style="cottage", location=[2000, 0, 0])
6. construct_house(house_style="cottage", location=[2000, 400, 0])
7. construct_house(house_style="cottage", location=[2400, 200, 0])
```

### Theme Park Design
**User Prompt:** *"Design a theme park with different attraction zones"*

**Tool Sequence:**
```bash
# Adventure Zone
1. create_maze(rows=8, cols=8, wall_height=2, location=[-1000, 0, 0])
2. create_obstacle_course(checkpoints=6, spacing=250, location=[-1000, 1000, 0])

# Fantasy Zone  
3. construct_house(house_style="cottage", location=[0, 0, 0])        # Fairy tale house
4. create_tower(height=12, tower_style="tapered", location=[200, 0, 0])  # Princess tower

# Thrill Zone
5. create_pyramid(base_size=5, location=[1000, 0, 0])               # Climbing pyramid  
6. create_bouncy_ball(name="GiantBall", location=[1200, 0, 300])    # Interactive element


```

---

## 🎯 Key Success Patterns

### 1. **Start Simple, Build Complex**
Begin with basic structures, then add details and interactive elements.

### 2. **Use Consistent Spacing**  
Maintain 500-1000cm separation between major structures for clarity.

### 3. **Layer Your Approach**
- **Structure**: Basic building forms
- **Detail**: Architectural elements  
- **Life**: Characters and interactive objects
- **Context**: Surrounding environment

### 4. **Name Systematically**
Use clear, descriptive names that indicate purpose and location.

### 5. **Think in 3D**
Remember to use Z-coordinates for elevated elements like balconies, towers, and flying objects.

---