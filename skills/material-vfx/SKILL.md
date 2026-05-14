---
name: material-vfx
description: >
  Material authoring and VFX workflow for the Flopperam Unreal Engine MCP. Covers 
  material creation, expression graphs, Niagara particle systems, Chaos destruction, 
  and curve assets. Use when creating materials, material instances, Niagara systems, 
  destruction effects, or procedural audio.
---

# Material & VFX Workflow

## Materials

### Inspect Before Edit
Always run `material_inspect` before editing an existing material to understand:
- Current expression graph structure
- Existing connections and parameters
- Texture references and shader stats
- Parent chain (for Material Instances)

### Creating Materials
```
material_edit(operation="create_material", material_path="/Game/Materials/M_MyMaterial")
```

### Material Workflow
1. `search_assets` with `class_names=["Texture2D"]` — find textures
2. `material_edit` with `create_material` — create base material
3. `material_edit` — create parameters (scalar, vector, texture) and expressions
4. `material_edit` — wire expressions to material properties (BaseColor, Roughness, Metallic, Normal, Emissive, etc.)
5. `material_edit` with `recompile_material` — compile
6. `material_inspect` — verify graph, connections, parameter setup

### Material Instances
Create instances that inherit from a parent material and expose parameter overrides without recompiling the parent shader.

### Material Functions
Reusable expression subgraphs that can be shared across materials.

### Material Parameter Collections
Global parameter sets accessible from any material — useful for time-of-day, weather, global tinting.

### Pin Name Resolution
`material_edit` auto-retries single-input pin name resolution — passing empty `to_input_name` for single-input nodes (Sine, Cosine, Abs, Normalize) is the canonical form.

## Niagara VFX

### System Architecture
- **NiagaraSystem** — top-level container with emitters
- **NiagaraEmitter** — particle behavior (spawn, update, render)
- **NiagaraScript** — reusable module graphs

### Workflow
1. `niagara_inspect` — read-only system inspection (emitters, modules, parameters, renderers)
2. `niagara_edit` — create/modify systems, emitters, user parameters, renderers
3. `niagara_script_edit` — author reusable NiagaraScript module graphs (custom particle behavior)

### Spawning Niagara in Blueprints
Use `function_call` with `SpawnSystemAtLocation` from `/Script/Niagara.NiagaraFunctionLibrary`:
- Pin defaults: `SystemTemplate` (asset path), `Location` (vector), `Rotation` (rotator), `Scale` (vector)

## Chaos Destruction

`chaos_edit` handles Geometry Collections:
- Fracture geometry into pieces
- Clustering (how pieces group)
- Damage thresholds (when things break)
- Physics properties for debris

## Curve Assets

`ramp_authoring` creates:
- `UCurveFloat` — scalar curves (falloff, opacity over time)
- `UCurveVector` — vector curves (position paths, wind)
- `UCurveLinearColor` — color gradients (material ramps, VFX color over life)

**Author the curve FIRST, then reference its path from the consumer** (material, Niagara module, widget animation, etc.).

## Procedural Audio

`metasound_edit` — MetaSound graph authoring:
- Oscillators (sine, saw, square, noise)
- Filters (low-pass, high-pass, band-pass)
- Envelopes (ADSR)
- Mixers and gain
- Sample players

`sound_asset_edit` — traditional audio:
- SoundCue graphs (attenuation, random, modulator, mixer nodes)
- SoundClass hierarchy
- SoundAttenuation curves and spatialization settings

## PCG (Procedural Content Generation)

`pcg_graph_edit` — PCG graph authoring:
- Point generators and samplers
- Filters and density operators
- Mesh spawners with typed pin wiring
- Landscape-aware placement
