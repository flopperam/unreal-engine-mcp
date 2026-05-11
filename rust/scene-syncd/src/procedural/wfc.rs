use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};

use crate::procedural::generator::{
    GenerateContext, Generator, ProceduralError, ProceduralEstimate, ProceduralOutput,
    ProceduralStats, TileCell,
};

// ── Data Types ──────────────────────────────────────────────────────────

/// A single tile definition in a WFC tileset.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WfcTile {
    pub id: String,
    pub weight: f32,
}

/// Cardinal direction for adjacency constraints.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum WfcDirection {
    #[serde(rename = "north")]
    North,
    #[serde(rename = "south")]
    South,
    #[serde(rename = "east")]
    East,
    #[serde(rename = "west")]
    West,
}

/// A directed adjacency constraint: `left` tile may have `right` tile in `direction`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WfcConstraint {
    pub left: String,
    pub right: String,
    pub direction: WfcDirection,
}

/// Tileset with explicit constraints.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WfcTileset {
    pub tiles: Vec<WfcTile>,
    pub constraints: Vec<WfcConstraint>,
}

/// Parameters for WFC grid generation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WfcParams {
    pub width: u32,
    pub height: u32,
    pub tileset: WfcTileset,
    #[serde(default)]
    pub seed: Option<u64>,
    #[serde(default)]
    pub periodic: bool,
}

impl Default for WfcParams {
    fn default() -> Self {
        Self {
            width: 4,
            height: 4,
            tileset: WfcTileset {
                tiles: vec![
                    WfcTile {
                        id: "grass".to_string(),
                        weight: 1.0,
                    },
                ],
                constraints: vec![],
            },
            seed: None,
            periodic: false,
        }
    }
}

/// Output of a successful WFC solve.
#[derive(Debug, Clone, Serialize)]
pub struct TileGrid {
    pub width: u32,
    pub height: u32,
    pub tiles: Vec<TileCell>,
}

// ── Generator Implementation ────────────────────────────────────────────

/// Wave Function Collapse generator conforming to the unified `Generator` trait.
#[derive(Debug, Clone, Default)]
pub struct WfcGenerator;

impl Generator for WfcGenerator {
    type Params = WfcParams;
    type Output = TileGrid;

    fn name(&self) -> &'static str {
        "wfc"
    }

    fn validate(&self, params: &Self::Params) -> Result<(), ProceduralError> {
        if params.width == 0 || params.height == 0 {
            return Err(ProceduralError::Validation(
                "Grid dimensions must be positive".to_string(),
            ));
        }
        if params.tileset.tiles.is_empty() {
            return Err(ProceduralError::Validation(
                "Tileset cannot be empty".to_string(),
            ));
        }
        let max_cells = (params.width as u64).saturating_mul(params.height as u64);
        if max_cells > 10_000 {
            return Err(ProceduralError::Validation(
                "Grid too large: max 10_000 cells".to_string(),
            ));
        }
        Ok(())
    }

    fn estimate(&self, params: &Self::Params) -> Result<ProceduralEstimate, ProceduralError> {
        let cell_count = (params.width * params.height) as usize;
        Ok(ProceduralEstimate {
            estimated_actor_count: cell_count,
            estimated_execution_ms: cell_count as u64 * 5,
            ..Default::default()
        })
    }

    fn generate(
        &self,
        params: &Self::Params,
        ctx: &GenerateContext,
    ) -> Result<ProceduralOutput<Self::Output>, ProceduralError> {
        let start = std::time::Instant::now();
        let seed = params.seed.unwrap_or(ctx.seed);
        let max_attempts = ctx.limits.max_iterations as usize;

        let grid = solve_wfc(params, seed, max_attempts)?;

        let elapsed = start.elapsed().as_millis() as u64;
        let cell_count = (params.width * params.height) as usize;

        Ok(ProceduralOutput {
            data: grid,
            stats: ProceduralStats {
                execution_ms: elapsed,
                seed_used: seed,
                actor_count: Some(cell_count),
                ..Default::default()
            },
            warnings: Vec::new(),
        })
    }
}

// ── Internal Solver ─────────────────────────────────────────────────────

struct AdjacencyMap {
    east: HashMap<String, HashSet<String>>,
    west: HashMap<String, HashSet<String>>,
    north: HashMap<String, HashSet<String>>,
    south: HashMap<String, HashSet<String>>,
}

fn build_adjacency(tileset: &WfcTileset) -> AdjacencyMap {
    let mut map = AdjacencyMap {
        east: HashMap::new(),
        west: HashMap::new(),
        north: HashMap::new(),
        south: HashMap::new(),
    };

    for constraint in &tileset.constraints {
        match constraint.direction {
            WfcDirection::East => {
                map.east
                    .entry(constraint.left.clone())
                    .or_default()
                    .insert(constraint.right.clone());
                map.west
                    .entry(constraint.right.clone())
                    .or_default()
                    .insert(constraint.left.clone());
            }
            WfcDirection::West => {
                map.west
                    .entry(constraint.left.clone())
                    .or_default()
                    .insert(constraint.right.clone());
                map.east
                    .entry(constraint.right.clone())
                    .or_default()
                    .insert(constraint.left.clone());
            }
            WfcDirection::North => {
                map.north
                    .entry(constraint.left.clone())
                    .or_default()
                    .insert(constraint.right.clone());
                map.south
                    .entry(constraint.right.clone())
                    .or_default()
                    .insert(constraint.left.clone());
            }
            WfcDirection::South => {
                map.south
                    .entry(constraint.left.clone())
                    .or_default()
                    .insert(constraint.right.clone());
                map.north
                    .entry(constraint.right.clone())
                    .or_default()
                    .insert(constraint.left.clone());
            }
        }
    }

    map
}

#[derive(Debug, Clone)]
struct Cell {
    possible: Vec<String>,
    collapsed: Option<String>,
}

#[derive(Debug, Clone)]
struct Grid {
    width: u32,
    height: u32,
    cells: Vec<Cell>,
}

impl Grid {
    fn new(width: u32, height: u32, all_tiles: Vec<String>) -> Self {
        let cell_count = (width * height) as usize;
        Self {
            width,
            height,
            cells: (0..cell_count)
                .map(|_| Cell {
                    possible: all_tiles.clone(),
                    collapsed: None,
                })
                .collect(),
        }
    }

    fn get(&self, x: u32, y: u32) -> &Cell {
        &self.cells[(y * self.width + x) as usize]
    }

    fn get_mut(&mut self, x: u32, y: u32) -> &mut Cell {
        &mut self.cells[(y * self.width + x) as usize]
    }
}

/// Simple LCG random number generator seeded from the request.
struct SeededRng {
    state: u64,
}

impl SeededRng {
    fn new(seed: u64) -> Self {
        Self { state: seed }
    }

    fn next_u64(&mut self) -> u64 {
        self.state = self.state.wrapping_mul(6364136223846793005).wrapping_add(1);
        self.state
    }

    fn shuffle(&mut self, slice: &mut [String]) {
        for i in (1..slice.len()).rev() {
            let j = (self.next_u64() as usize) % (i + 1);
            slice.swap(i, j);
        }
    }
}

fn solve_wfc(
    params: &WfcParams,
    seed: u64,
    max_attempts: usize,
) -> Result<TileGrid, ProceduralError> {
    let adjacency = build_adjacency(&params.tileset);
    let all_tiles: Vec<String> = params.tileset.tiles.iter().map(|t| t.id.clone()).collect();
    let mut rng = SeededRng::new(seed);

    let mut grid = Grid::new(params.width, params.height, all_tiles.clone());
    let mut attempts = 0usize;

    if !backtrack(
        &mut grid,
        &adjacency,
        &mut rng,
        &mut attempts,
        max_attempts,
        params.periodic,
    ) {
        return Err(ProceduralError::Contradiction {
            details: format!("WFC failed after {} attempts (max {})", attempts, max_attempts),
        });
    }

    let mut tiles = Vec::new();
    for y in 0..params.height {
        for x in 0..params.width {
            let cell = grid.get(x, y);
            if let Some(tile_id) = &cell.collapsed {
                tiles.push(TileCell {
                    x,
                    y,
                    tile_id: tile_id.clone(),
                    rotation_degrees: 0.0,
                });
            }
        }
    }

    Ok(TileGrid {
        width: params.width,
        height: params.height,
        tiles,
    })
}

fn backtrack(
    grid: &mut Grid,
    adjacency: &AdjacencyMap,
    rng: &mut SeededRng,
    attempts: &mut usize,
    max_attempts: usize,
    periodic: bool,
) -> bool {
    // Find uncollapsed cell with minimum entropy.
    let mut best: Option<(u32, u32, usize)> = None;
    for y in 0..grid.height {
        for x in 0..grid.width {
            if grid.get(x, y).collapsed.is_none() {
                let entropy = grid.get(x, y).possible.len();
                match best {
                    None => best = Some((x, y, entropy)),
                    Some((_, _, e)) if entropy < e => best = Some((x, y, entropy)),
                    _ => {}
                }
            }
        }
    }

    let (x, y) = match best {
        Some((x, y, _)) => (x, y),
        None => return true, // all collapsed
    };

    let possible = grid.get(x, y).possible.clone();
    if possible.is_empty() {
        return false;
    }

    let mut shuffled = possible;
    rng.shuffle(&mut shuffled);

    for tile_id in &shuffled {
        if *attempts >= max_attempts {
            return false;
        }
        *attempts += 1;

        let mut new_grid = grid.clone();
        {
            let cell = new_grid.get_mut(x, y);
            cell.collapsed = Some(tile_id.clone());
            cell.possible = vec![tile_id.clone()];
        }

        if propagate(&mut new_grid, x, y, adjacency, periodic) {
            if backtrack(
                &mut new_grid, adjacency, rng, attempts, max_attempts, periodic,
            ) {
                *grid = new_grid;
                return true;
            }
        }
    }

    false
}

fn propagate(
    grid: &mut Grid,
    origin_x: u32,
    origin_y: u32,
    adjacency: &AdjacencyMap,
    periodic: bool,
) -> bool {
    let mut queue: Vec<(u32, u32)> = vec![(origin_x, origin_y)];

    let width = grid.width;
    let height = grid.height;

    while let Some((x, y)) = queue.pop() {
        let tile_id = match &grid.get(x, y).collapsed {
            Some(id) => id.clone(),
            None => continue,
        };

        // helper closure
        let mut try_dir = |nx: u32, ny: u32, dir_map: &HashMap<String, HashSet<String>>| {
            let neighbor = grid.get_mut(nx, ny);
            if neighbor.collapsed.is_some() {
                return true;
            }
            let allowed = dir_map.get(&tile_id).cloned().unwrap_or_default();
            let old_len = neighbor.possible.len();
            neighbor.possible.retain(|t| allowed.contains(t));
            if neighbor.possible.is_empty() {
                return false;
            }
            if neighbor.possible.len() < old_len {
                queue.push((nx, ny));
            }
            true
        };

        // east
        if x + 1 < width {
            if !try_dir(x + 1, y, &adjacency.east) {
                return false;
            }
        } else if periodic && width > 0 {
            if !try_dir(0, y, &adjacency.east) {
                return false;
            }
        }

        // west
        if x > 0 {
            if !try_dir(x - 1, y, &adjacency.west) {
                return false;
            }
        } else if periodic && width > 0 {
            if !try_dir(width - 1, y, &adjacency.west) {
                return false;
            }
        }

        // south
        if y + 1 < height {
            if !try_dir(x, y + 1, &adjacency.south) {
                return false;
            }
        } else if periodic && height > 0 {
            if !try_dir(x, 0, &adjacency.south) {
                return false;
            }
        }

        // north
        if y > 0 {
            if !try_dir(x, y - 1, &adjacency.north) {
                return false;
            }
        } else if periodic && height > 0 {
            if !try_dir(x, height - 1, &adjacency.north) {
                return false;
            }
        }
    }

    true
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::procedural::generator::Generator;

    fn simple_tileset() -> WfcTileset {
        WfcTileset {
            tiles: vec![
                WfcTile {
                    id: "grass".to_string(),
                    weight: 1.0,
                },
                WfcTile {
                    id: "water".to_string(),
                    weight: 1.0,
                },
            ],
            constraints: vec![
                // self adjacency
                WfcConstraint {
                    left: "grass".to_string(),
                    right: "grass".to_string(),
                    direction: WfcDirection::East,
                },
                WfcConstraint {
                    left: "water".to_string(),
                    right: "water".to_string(),
                    direction: WfcDirection::East,
                },
                WfcConstraint {
                    left: "grass".to_string(),
                    right: "grass".to_string(),
                    direction: WfcDirection::South,
                },
                WfcConstraint {
                    left: "water".to_string(),
                    right: "water".to_string(),
                    direction: WfcDirection::South,
                },
            ],
        }
    }

    #[test]
    fn test_generator_trait() {
        let gen = WfcGenerator;
        assert_eq!(gen.name(), "wfc");

        let ctx = GenerateContext::new(None, None);
        let params = WfcParams {
            width: 2,
            height: 2,
            tileset: simple_tileset(),
            seed: Some(42),
            periodic: false,
        };
        let output = gen.generate(&params, &ctx).unwrap();
        assert_eq!(output.data.width, 2);
        assert_eq!(output.data.height, 2);
        assert_eq!(output.data.tiles.len(), 4);
    }

    #[test]
    fn test_validate_zero_width() {
        let gen = WfcGenerator;
        let params = WfcParams {
            width: 0,
            height: 2,
            ..Default::default()
        };
        assert!(gen.validate(&params).is_err());
    }

    #[test]
    fn test_validate_empty_tileset() {
        let gen = WfcGenerator;
        let params = WfcParams {
            width: 2,
            height: 2,
            tileset: WfcTileset {
                tiles: vec![],
                constraints: vec![],
            },
            ..Default::default()
        };
        assert!(gen.validate(&params).is_err());
    }

    #[test]
    fn test_single_tile_always_succeeds() {
        let gen = WfcGenerator;
        let ctx = GenerateContext::new(None, None);
        let params = WfcParams {
            width: 3,
            height: 3,
            tileset: WfcTileset {
                tiles: vec![WfcTile {
                    id: "grass".to_string(),
                    weight: 1.0,
                }],
                constraints: vec![
                    WfcConstraint {
                        left: "grass".to_string(),
                        right: "grass".to_string(),
                        direction: WfcDirection::East,
                    },
                    WfcConstraint {
                        left: "grass".to_string(),
                        right: "grass".to_string(),
                        direction: WfcDirection::South,
                    },
                ],
            },
            seed: Some(1),
            periodic: false,
        };
        let output = gen.generate(&params, &ctx).unwrap();
        assert_eq!(output.data.tiles.len(), 9);
        for tile in &output.data.tiles {
            assert_eq!(tile.tile_id, "grass");
        }
    }

    #[test]
    fn test_estimate() {
        let gen = WfcGenerator;
        let params = WfcParams {
            width: 4,
            height: 4,
            ..Default::default()
        };
        let est = gen.estimate(&params).unwrap();
        assert_eq!(est.estimated_actor_count, 16);
    }

    #[test]
    fn test_periodic_mode() {
        let gen = WfcGenerator;
        let ctx = GenerateContext::new(None, None);
        let params = WfcParams {
            width: 2,
            height: 2,
            tileset: simple_tileset(),
            seed: Some(99),
            periodic: true,
        };
        let output = gen.generate(&params, &ctx).unwrap();
        assert_eq!(output.data.tiles.len(), 4);
    }

    #[test]
    fn test_contradiction_returns_error() {
        let gen = WfcGenerator;
        let ctx = GenerateContext::new(None, None);
        // Impossible: any grid >1 cell with no adjacency constraints
        // will exhaust all branches because no tile can neighbour another.
        let params = WfcParams {
            width: 2,
            height: 1,
            tileset: WfcTileset {
                tiles: vec![
                    WfcTile {
                        id: "a".to_string(),
                        weight: 1.0,
                    },
                    WfcTile {
                        id: "b".to_string(),
                        weight: 1.0,
                    },
                ],
                constraints: vec![],
            },
            seed: Some(1),
            periodic: false,
        };
        let result = gen.generate(&params, &ctx);
        assert!(matches!(result, Err(ProceduralError::Contradiction { .. })));
    }
}
