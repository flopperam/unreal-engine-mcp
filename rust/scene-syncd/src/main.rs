use axum::http::header;
use axum::http::Method;
use axum::routing::{get, post};
use axum::Router;
use scene_syncd::api::routes::AppState;
use scene_syncd::db::connect::connect_surreal;
use scene_syncd::db::SurrealSceneRepository;
use scene_syncd::unreal::client::UnrealClient;
use scene_syncd::Config;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::Mutex;
use tower_http::cors::CorsLayer;
use tower_http::trace::TraceLayer;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let config = Config::from_env();

    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new(&config.log_level)),
        )
        .json()
        .init();

    tracing::info!("scene-syncd starting on {}", config.bind_addr());

    let db = connect_surreal(&config).await?;
    let repo = SurrealSceneRepository::new(db.clone());

    tracing::info!("Applying schema migrations...");
    repo.ensure_schema().await?;
    tracing::info!("Schema migrations applied");

    tracing::info!("Ensuring default scene:main...");
    repo.ensure_default_scene().await?;
    tracing::info!("Default scene:main ready");

    let bind_addr = config.bind_addr();

    let state = AppState {
        db,
        config: config.clone(),
        scene_locks: Arc::new(Mutex::new(HashMap::new())),
        unreal_client: UnrealClient::new(&config),
        procedural_jobs: scene_syncd::procedural::jobs::JobRegistry::new(
            scene_syncd::procedural::jobs::DEFAULT_MAX_CONCURRENCY,
        ),
    };

    let app = Router::new()
        .route("/health", get(scene_syncd::api::routes::health))
        .route(
            "/scenes/create",
            post(scene_syncd::api::routes::create_scene),
        )
        .route("/scenes/list", post(scene_syncd::api::routes::list_scenes))
        .route(
            "/objects/upsert",
            post(scene_syncd::api::routes::upsert_object),
        )
        .route(
            "/objects/bulk-upsert",
            post(scene_syncd::api::routes::bulk_upsert_objects),
        )
        .route(
            "/objects/list",
            post(scene_syncd::api::routes::list_objects),
        )
        .route(
            "/objects/delete",
            post(scene_syncd::api::routes::delete_object),
        )
        .route(
            "/groups/create",
            post(scene_syncd::api::routes::create_group),
        )
        .route("/groups/list", post(scene_syncd::api::routes::list_groups))
        .route(
            "/generator-runs/create",
            post(scene_syncd::api::routes::create_generator_run),
        )
        .route(
            "/generator-runs/{run_id}",
            get(scene_syncd::api::routes::get_generator_run),
        )
        .route(
            "/snapshots/create",
            post(scene_syncd::api::routes::create_snapshot),
        )
        .route(
            "/snapshots/list",
            post(scene_syncd::api::routes::list_snapshots),
        )
        .route(
            "/snapshots/restore",
            post(scene_syncd::api::routes::restore_snapshot),
        )
        .route(
            "/sync/plan",
            post(scene_syncd::api::routes::plan_sync_route),
        )
        .route(
            "/sync/apply",
            post(scene_syncd::api::routes::apply_sync_route),
        )
        // P3: Semantic routes
        .route(
            "/entities/bulk-upsert",
            post(scene_syncd::api::routes::bulk_upsert_entities),
        )
        .route(
            "/entities/list",
            post(scene_syncd::api::routes::list_entities),
        )
        .route(
            "/relations/bulk-upsert",
            post(scene_syncd::api::routes::bulk_upsert_relations),
        )
        .route(
            "/relations/list",
            post(scene_syncd::api::routes::list_relations),
        )
        .route(
            "/assets/upsert",
            post(scene_syncd::api::routes::upsert_asset),
        )
        .route("/assets/list", post(scene_syncd::api::routes::list_assets))
        // P6: Component, Blueprint, Realization routes
        .route(
            "/components/upsert",
            post(scene_syncd::api::routes::upsert_component),
        )
        .route(
            "/components/list",
            post(scene_syncd::api::routes::list_components),
        )
        .route(
            "/components/delete",
            post(scene_syncd::api::routes::delete_component),
        )
        .route(
            "/blueprints/upsert",
            post(scene_syncd::api::routes::upsert_blueprint),
        )
        .route(
            "/blueprints/list",
            post(scene_syncd::api::routes::list_blueprints),
        )
        .route(
            "/blueprints/delete",
            post(scene_syncd::api::routes::delete_blueprint),
        )
        .route(
            "/realizations/upsert",
            post(scene_syncd::api::routes::upsert_realization),
        )
        .route(
            "/realizations/list",
            post(scene_syncd::api::routes::list_realizations),
        )
        .route(
            "/realizations/update-status",
            post(scene_syncd::api::routes::update_realization_status),
        )
        .route(
            "/layouts/{scene_id}/denormalize",
            post(scene_syncd::api::routes::denormalize_layout_route),
        )
        .route(
            "/layouts/{scene_id}/nodes/{entity_id}/transform",
            post(scene_syncd::api::routes::update_layout_node_transform),
        )
        .route(
            "/layouts/{scene_id}/approve",
            post(scene_syncd::api::routes::approve_layout),
        )
        .route(
            "/layouts/{scene_id}/preview",
            post(scene_syncd::api::routes::preview_layout_route),
        )
        .route(
            "/layouts/{scene_id}/compile/preview",
            post(scene_syncd::api::routes::compile_preview_route),
        )
        .route(
            "/layouts/{scene_id}/validate",
            post(scene_syncd::api::routes::validate_route),
        )
        .route(
            "/layouts/{scene_id}/compile/plan",
            post(scene_syncd::api::routes::compile_plan_route),
        )
        .route(
            "/layouts/{scene_id}/compile/apply",
            post(scene_syncd::api::routes::compile_apply_route),
        )
        .route(
            "/realizations/{scene_id}/realize",
            post(scene_syncd::api::routes::realize_layout_route),
        )
        // P10: PIE / Log Validation routes
        .route("/unreal/pie/run", post(scene_syncd::api::routes::pie_run))
        .route(
            "/unreal/logs/parse",
            post(scene_syncd::api::routes::parse_logs),
        )
        .route("/unreal/fix-plan", post(scene_syncd::api::routes::fix_plan))
        .route(
            "/procedural/create-mesh",
            post(scene_syncd::api::routes::create_procedural_mesh_route),
        )
        .route(
            "/procedural/sdf-mesh",
            post(scene_syncd::api::routes::sdf_mesh_route),
        )
        .route(
            "/procedural/superformula-mesh",
            post(scene_syncd::api::routes::superformula_mesh_route),
        )
        .route(
            "/procedural/lsystem-spline",
            post(scene_syncd::api::routes::lsystem_spline_route),
        )
        .route(
            "/procedural/wfc-grid",
            post(scene_syncd::api::routes::wfc_grid_route),
        )
        // Procedural background jobs (large WFC / L-System runs)
        .route(
            "/procedural/jobs/submit",
            post(scene_syncd::api::routes::procedural_job_submit_route),
        )
        .route(
            "/procedural/jobs/{job_id}",
            get(scene_syncd::api::routes::procedural_job_status_route),
        )
        .route(
            "/procedural/jobs/{job_id}/cancel",
            post(scene_syncd::api::routes::procedural_job_cancel_route),
        )
        .route(
            "/procedural/jobs",
            get(scene_syncd::api::routes::procedural_job_list_route),
        )
        .layer(axum::extract::DefaultBodyLimit::max(512 * 1024 * 1024))
        .layer(TraceLayer::new_for_http())
        .layer(
            CorsLayer::new()
                .allow_origin([
                    "http://localhost:3000".parse().unwrap(),
                    "http://127.0.0.1:3000".parse().unwrap(),
                ])
                .allow_methods([Method::GET, Method::POST])
                .allow_headers([header::CONTENT_TYPE]),
        )
        .with_state(state);

    let listener = tokio::net::TcpListener::bind(&bind_addr).await?;
    tracing::info!("scene-syncd listening on {}", bind_addr);

    axum::serve(listener, app).await?;

    Ok(())
}
