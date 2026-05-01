use crate::config::Config;
use crate::error::AppError;
use crate::sync::UnrealActorObservation;
use serde_json::json;
use std::sync::Arc;
use std::time::Duration;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tokio::sync::Mutex;
use tokio::time::{sleep, timeout};

const MAX_RESPONSE_SIZE: usize = 16 * 1024 * 1024; // 16 MiB
const CONNECT_TIMEOUT: Duration = Duration::from_secs(10);
const READ_TIMEOUT: Duration = Duration::from_secs(30);

#[derive(Debug, Clone)]
pub struct UnrealClient {
    host: String,
    port: u16,
    stream: Arc<Mutex<Option<TcpStream>>>,
}

impl UnrealClient {
    pub fn new(config: &Config) -> Self {
        Self {
            host: config.unreal_host.clone(),
            port: config.unreal_port,
            stream: Arc::new(Mutex::new(None)),
        }
    }

    pub async fn get_actors_in_level(&self) -> Result<Vec<UnrealActorObservation>, AppError> {
        let response = self.send_command("get_actors_in_level", json!({})).await?;

        let actors = response
            .get("actors")
            .and_then(|v| v.as_array())
            .ok_or_else(|| {
                AppError::UnrealBridge("missing actors array in response".to_string())
            })?;

        let mut result = Vec::new();
        for actor_val in actors {
            let actor: UnrealActorObservation = serde_json::from_value(actor_val.clone())
                .map_err(|e| AppError::UnrealBridge(format!("failed to parse actor: {e}")))?;
            result.push(actor);
        }

        Ok(result)
    }

    pub async fn spawn_actor(
        &self,
        params: serde_json::Value,
    ) -> Result<serde_json::Value, AppError> {
        self.send_command("spawn_actor", params).await
    }

    pub async fn spawn_blueprint_actor(
        &self,
        blueprint_path: &str,
        actor_name: &str,
        location: [f64; 3],
        rotation: [f64; 3],
        scale: [f64; 3],
    ) -> Result<serde_json::Value, AppError> {
        self.send_command(
            "spawn_blueprint_actor",
            json!({
                "blueprint_name": blueprint_path,
                "actor_name": actor_name,
                "location": location,
                "rotation": rotation,
                "scale": scale,
            }),
        )
        .await
    }

    pub async fn find_actor_by_mcp_id(&self, mcp_id: &str) -> Result<serde_json::Value, AppError> {
        self.send_command("find_actor_by_mcp_id", json!({ "mcp_id": mcp_id }))
            .await
    }

    pub async fn set_actor_transform_by_mcp_id(
        &self,
        mcp_id: &str,
        location: [f64; 3],
        rotation: [f64; 3],
        scale: [f64; 3],
    ) -> Result<serde_json::Value, AppError> {
        self.send_command(
            "set_actor_transform_by_mcp_id",
            json!({
                "mcp_id": mcp_id,
                "location": location,
                "rotation": rotation,
                "scale": scale,
            }),
        )
        .await
    }

    pub async fn delete_actor_by_mcp_id(
        &self,
        mcp_id: &str,
    ) -> Result<serde_json::Value, AppError> {
        self.send_command("delete_actor_by_mcp_id", json!({ "mcp_id": mcp_id }))
            .await
    }

    pub async fn clone_actor(
        &self,
        source_actor_name: &str,
        new_actor_name: &str,
        location: [f64; 3],
        rotation: [f64; 3],
        scale: [f64; 3],
        mcp_id: &str,
        tags: &[String],
    ) -> Result<serde_json::Value, AppError> {
        self.send_command(
            "clone_actor",
            json!({
                "source_actor_name": source_actor_name,
                "new_actor_name": new_actor_name,
                "location": { "x": location[0], "y": location[1], "z": location[2] },
                "rotation": { "pitch": rotation[0], "yaw": rotation[1], "roll": rotation[2] },
                "scale": { "x": scale[0], "y": scale[1], "z": scale[2] },
                "mcp_id": mcp_id,
                "tags": tags,
            }),
        )
        .await
    }

    pub async fn apply_scene_delta(
        &self,
        transaction_id: &str,
        creates: Vec<serde_json::Value>,
        updates: Vec<serde_json::Value>,
        deletes: Vec<serde_json::Value>,
    ) -> Result<serde_json::Value, AppError> {
        self.send_command(
            "apply_scene_delta",
            json!({
                "transaction_id": transaction_id,
                "creates": creates,
                "updates": updates,
                "deletes": deletes,
            }),
        )
        .await
    }

    pub async fn set_mesh_material_color(
        &self,
        actor_name: &str,
        r: f64,
        g: f64,
        b: f64,
    ) -> Result<serde_json::Value, AppError> {
        self.send_command(
            "set_mesh_material_color",
            json!({
                "actor_name": actor_name,
                "r": r,
                "g": g,
                "b": b,
            }),
        )
        .await
    }

    pub async fn create_draft_proxy(
        &self,
        proxy_name: &str,
        mesh_path: &str,
        material_path: Option<&str>,
        instances: Vec<serde_json::Value>,
    ) -> Result<serde_json::Value, AppError> {
        let mut params = serde_json::Map::new();
        params.insert("proxy_name".to_string(), json!(proxy_name));
        params.insert("mesh_path".to_string(), json!(mesh_path));
        if let Some(mat) = material_path {
            params.insert("material_path".to_string(), json!(mat));
        }
        params.insert("instances".to_string(), json!(instances));
        self.send_command("create_draft_proxy", serde_json::Value::Object(params))
            .await
    }

    pub async fn update_draft_proxy(
        &self,
        proxy_name: &str,
        material_path: Option<&str>,
        instances: Vec<serde_json::Value>,
    ) -> Result<serde_json::Value, AppError> {
        let mut params = serde_json::Map::new();
        params.insert("proxy_name".to_string(), json!(proxy_name));
        if let Some(mat) = material_path {
            params.insert("material_path".to_string(), json!(mat));
        }
        params.insert("instances".to_string(), json!(instances));
        self.send_command("update_draft_proxy", serde_json::Value::Object(params))
            .await
    }

    pub async fn delete_draft_proxy(
        &self,
        proxy_name: &str,
    ) -> Result<serde_json::Value, AppError> {
        self.send_command("delete_draft_proxy", json!({ "proxy_name": proxy_name }))
            .await
    }

    /// Spawn an ISM/HISM instance set in Unreal (Phase 4).
    pub async fn spawn_instance_set(
        &self,
        set_id: &str,
        mesh_path: &str,
        material_path: Option<&str>,
        transforms: Vec<serde_json::Value>,
    ) -> Result<serde_json::Value, AppError> {
        let mut params = serde_json::Map::new();
        params.insert("set_id".to_string(), json!(set_id));
        params.insert("mesh_path".to_string(), json!(mesh_path));
        if let Some(mat) = material_path {
            params.insert("material_path".to_string(), json!(mat));
        }
        params.insert("transforms".to_string(), json!(transforms));
        self.send_command("spawn_instance_set", serde_json::Value::Object(params))
            .await
    }

    /// Update an existing instance set (Phase 4).
    pub async fn update_instance_set(
        &self,
        set_id: &str,
        transforms: Vec<serde_json::Value>,
    ) -> Result<serde_json::Value, AppError> {
        self.send_command(
            "update_instance_set",
            json!({
                "set_id": set_id,
                "transforms": transforms,
            }),
        )
        .await
    }

    /// Delete an instance set by set_id.
    pub async fn delete_instance_set(&self, set_id: &str) -> Result<serde_json::Value, AppError> {
        self.send_command("delete_instance_set", json!({"set_id": set_id}))
            .await
    }

    /// Get state of an existing instance set.
    pub async fn get_instance_set_state(&self, set_id: &str) -> Result<serde_json::Value, AppError> {
        self.send_command("get_instance_set_state", json!({"set_id": set_id}))
            .await
    }

    /// List all instance sets currently in the level.
    pub async fn list_instance_sets(&self) -> Result<serde_json::Value, AppError> {
        self.send_command("list_instance_sets", json!({})).await
    }

    /// Start Play-In-Editor session for smoke/verification testing.
    pub async fn start_pie(&self) -> Result<serde_json::Value, AppError> {
        self.send_command("start_pie", json!({})).await
    }

    /// Stop Play-In-Editor session.
    pub async fn stop_pie(&self) -> Result<serde_json::Value, AppError> {
        self.send_command("stop_pie", json!({})).await
    }

    async fn send_command(
        &self,
        command: &str,
        params: serde_json::Value,
    ) -> Result<serde_json::Value, AppError> {
        let mut last_error = None;
        for attempt in 0..6 {
            match self.send_command_once(command, params.clone()).await {
                Ok(response) => return Ok(response),
                Err(e) => {
                    last_error = Some(e);
                    if attempt < 5 {
                        sleep(Duration::from_millis(500 * (attempt + 1) as u64)).await;
                    }
                }
            }
        }
        Err(last_error.unwrap_or_else(|| {
            AppError::UnrealBridge(format!("command {command} failed without error detail"))
        }))
    }

    async fn send_command_once(
        &self,
        command: &str,
        params: serde_json::Value,
    ) -> Result<serde_json::Value, AppError> {
        let mut payload_obj = serde_json::Map::new();
        payload_obj.insert("command".to_string(), json!(command));
        payload_obj.insert("params".to_string(), params);

        if let Ok(token) = std::env::var("UNREAL_MCP_AUTH_TOKEN") {
            if !token.is_empty() {
                payload_obj.insert("auth_token".to_string(), json!(token));
            }
        }

        let payload = serde_json::Value::Object(payload_obj);

        let mut payload_bytes = serde_json::to_vec(&payload)
            .map_err(|e| AppError::UnrealBridge(format!("json encode error: {e}")))?;
        payload_bytes.push(b'\n');

        let mut guard = self.stream.lock().await;

        let mut stream = match guard.take() {
            Some(s) => {
                // Health check: try a non-blocking peek to detect stale connections.
                // If the read returns Ok(0), the peer has closed. If it returns an
                // error (e.g. ECONNRESET), the connection is dead. Either way we
                // reconnect proactively to avoid an expensive retry chain.
                let mut peek = [0u8; 0];
                match s.try_read(&mut peek) {
                    Ok(_) => s, // connection alive
                    Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => s,
                    _ => {
                        // Connection dead — reconnect below
                        drop(s);
                        let addr = format!("{}:{}", self.host, self.port);
                        timeout(CONNECT_TIMEOUT, TcpStream::connect(&addr))
                            .await
                            .map_err(|_| {
                                AppError::UnrealBridge(format!("connect timeout to {addr}"))
                            })?
                            .map_err(|e| {
                                AppError::UnrealBridge(format!("connect error to {addr}: {e}"))
                            })?
                    }
                }
            }
            None => {
                let addr = format!("{}:{}", self.host, self.port);
                let s = timeout(CONNECT_TIMEOUT, TcpStream::connect(&addr))
                    .await
                    .map_err(|_| AppError::UnrealBridge(format!("connect timeout to {addr}")))?
                    .map_err(|e| AppError::UnrealBridge(format!("connect error to {addr}: {e}")))?;
                s
            }
        };

        let write_result = stream.write_all(&payload_bytes).await;
        if let Err(e) = write_result {
            let _ = stream.shutdown().await;
            return Err(AppError::UnrealBridge(format!("write payload error: {e}")));
        }

        let resp_buf = match timeout(
            READ_TIMEOUT,
            read_line_limited(&mut stream, MAX_RESPONSE_SIZE),
        )
        .await
        {
            Ok(Ok(buf)) => buf,
            Ok(Err(e)) => {
                let _ = stream.shutdown().await;
                return Err(e);
            }
            Err(_) => {
                let _ = stream.shutdown().await;
                return Err(AppError::UnrealBridge("read timeout".to_string()));
            }
        };

        let response: serde_json::Value = match serde_json::from_slice(&resp_buf) {
            Ok(v) => v,
            Err(e) => {
                let _ = stream.shutdown().await;
                return Err(AppError::UnrealBridge(format!("json decode error: {e}")));
            }
        };

        // Keep connection alive for reuse (server handles graceful close).
        *guard = Some(stream);
        drop(guard);

        Ok(normalize_response(response))
    }
}

async fn read_line_limited(stream: &mut TcpStream, max_size: usize) -> Result<Vec<u8>, AppError> {
    let mut resp_buf = Vec::new();
    let mut chunk = [0u8; 4096];
    loop {
        let bytes_read = stream
            .read(&mut chunk)
            .await
            .map_err(|e| AppError::UnrealBridge(format!("read error: {e}")))?;
        if bytes_read == 0 {
            if resp_buf.is_empty() {
                return Err(AppError::UnrealBridge(
                    "connection closed before response".to_string(),
                ));
            }
            break;
        }

        if resp_buf.len() + bytes_read > max_size {
            return Err(AppError::UnrealBridge(format!(
                "response exceeded maximum size of {max_size} bytes"
            )));
        }

        resp_buf.extend_from_slice(&chunk[..bytes_read]);
        if let Some(pos) = resp_buf.iter().position(|b| *b == b'\n') {
            resp_buf.truncate(pos);
            break;
        }
    }
    Ok(resp_buf)
}

fn normalize_response(response: serde_json::Value) -> serde_json::Value {
    if response.get("status").and_then(|v| v.as_str()) == Some("error") {
        return json!({
            "success": false,
            "error": response
                .get("error")
                .or_else(|| response.get("message"))
                .and_then(|v| v.as_str())
                .unwrap_or("unknown Unreal error"),
        });
    }

    if response.get("status").and_then(|v| v.as_str()) == Some("success") {
        if let Some(result) = response.get("result").and_then(|v| v.as_object()) {
            // Preserve nested actor envelope (find_actor_by_mcp_id uses this)
            if result.contains_key("actor") || result.contains_key("success") {
                let mut normalized = serde_json::Map::new();
                normalized.insert("success".to_string(), serde_json::Value::Bool(true));
                for (key, value) in result {
                    normalized.insert(key.clone(), value.clone());
                }
                return serde_json::Value::Object(normalized);
            }
            let mut normalized = serde_json::Map::new();
            normalized.insert("success".to_string(), serde_json::Value::Bool(true));
            for (key, value) in result {
                if key != "success" {
                    normalized.insert(key.clone(), value.clone());
                }
            }
            return serde_json::Value::Object(normalized);
        }
        return json!({ "success": true });
    }

    response
}

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::net::TcpListener;

    async fn mock_bridge_server(port: u16) -> tokio::task::JoinHandle<()> {
        let listener = TcpListener::bind(format!("127.0.0.1:{port}"))
            .await
            .expect("mock bridge bind");
        tokio::spawn(async move {
            loop {
                let (mut socket, _) = listener.accept().await.unwrap();
                tokio::spawn(async move {
                    loop {
                        let mut buf = vec![0u8; 4096];
                        let n = match socket.read(&mut buf).await {
                            Ok(0) => break,
                            Ok(n) => n,
                            Err(_) => break,
                        };
                        let req: serde_json::Value =
                            serde_json::from_slice(&buf[..n]).unwrap_or(json!({}));
                        let command = req.get("command").and_then(|v| v.as_str());
                        let resp = match command {
                            Some("ping") => {
                                json!({"status": "success", "result": {"message": "pong"}})
                            }
                            Some("get_actors_in_level") => json!({
                                "status": "success",
                                "result": {"actors": [
                                    {
                                        "name": "cube_01",
                                        "class": "/Script/Engine.StaticMeshActor",
                                        "location": [0.0, 0.0, 0.0],
                                        "rotation": [0.0, 0.0, 0.0],
                                        "scale": [1.0, 1.0, 1.0],
                                        "tags": ["managed_by_mcp", "mcp_id:cube_01"]
                                    }
                                ]}}
                            ),
                            Some("spawn_actor") => json!({
                                "status": "success",
                                "result": {"actor_name": "cube_01", "success": true}
                            }),
                            Some("find_actor_by_mcp_id") => json!({
                                "status": "success",
                                "result": {"actor": {"name": "cube_01", "class": "StaticMeshActor"}, "success": true}
                            }),
                            Some("delete_actor_by_mcp_id") => json!({
                                "status": "success",
                                "result": {"deleted_actor": {"name": "cube_01"}, "success": true}
                            }),
                            Some("set_actor_transform_by_mcp_id") => json!({
                                "status": "success",
                                "result": {"success": true}
                            }),
                            Some("spawn_blueprint_actor") => json!({
                                "status": "success",
                                "result": {"actor_name": "bp_actor_01", "success": true}
                            }),
                            Some("spawn_instance_set") => json!({
                                "status": "success",
                                "result": {"success": true, "set_id": "crenellation_test", "instance_count": 300, "actor_name": "InstanceSet_crenellation_test", "use_hism": true}
                            }),
                            Some("update_instance_set") => json!({
                                "status": "success",
                                "result": {"success": true, "set_id": "crenellation_test", "instance_count": 300}
                            }),
                            Some("delete_instance_set") => json!({
                                "status": "success",
                                "result": {"success": true, "deleted": true, "set_id": "crenellation_test"}
                            }),
                            Some("get_instance_set_state") => json!({
                                "status": "success",
                                "result": {"success": true, "set_id": "crenellation_test", "instance_count": 300, "actor_name": "InstanceSet_crenellation_test", "use_hism": true, "mesh_path": "/Engine/BasicShapes/Cube.Cube"}
                            }),
                            Some("list_instance_sets") => json!({
                                "status": "success",
                                "result": {"success": true, "count": 1, "sets": [
                                    {"set_id": "crenellation_test", "instance_count": 300, "actor_name": "InstanceSet_crenellation_test", "use_hism": true, "mesh_path": "/Engine/BasicShapes/Cube.Cube"}
                                ]}
                            }),
                            _ => json!({"status": "error", "error": "unknown command"}),
                        };
                        let mut resp_bytes = serde_json::to_vec(&resp).unwrap();
                        resp_bytes.push(b'\n');
                        if socket.write_all(&resp_bytes).await.is_err() {
                            break;
                        }
                        if socket.flush().await.is_err() {
                            break;
                        }
                    }
                });
            }
        })
    }

    #[tokio::test]
    async fn mock_bridge_ping_roundtrip() {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let port = listener.local_addr().unwrap().port();
        drop(listener); // release so server can bind

        let _server = mock_bridge_server(port).await;
        tokio::time::sleep(Duration::from_millis(50)).await;

        let client = UnrealClient {
            host: "127.0.0.1".to_string(),
            port,
            stream: Arc::new(Mutex::new(None)),
        };
        let resp = client.send_command("ping", json!({})).await.unwrap();
        assert_eq!(resp.get("success"), Some(&serde_json::Value::Bool(true)));
    }

    #[tokio::test]
    async fn mock_bridge_get_actors_parses_correctly() {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let port = listener.local_addr().unwrap().port();
        drop(listener);

        let _server = mock_bridge_server(port).await;
        tokio::time::sleep(Duration::from_millis(50)).await;

        let client = UnrealClient {
            host: "127.0.0.1".to_string(),
            port,
            stream: Arc::new(Mutex::new(None)),
        };
        let actors = client.get_actors_in_level().await.unwrap();
        assert_eq!(actors.len(), 1);
        assert_eq!(actors[0].name, "cube_01");
    }

    #[tokio::test]
    async fn mock_bridge_spawn_and_find() {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let port = listener.local_addr().unwrap().port();
        drop(listener);

        let _server = mock_bridge_server(port).await;
        tokio::time::sleep(Duration::from_millis(50)).await;

        let client = UnrealClient {
            host: "127.0.0.1".to_string(),
            port,
            stream: Arc::new(Mutex::new(None)),
        };
        let spawn_resp = client
            .spawn_actor(json!({"type": "StaticMeshActor"}))
            .await
            .unwrap();
        assert_eq!(
            spawn_resp.get("success"),
            Some(&serde_json::Value::Bool(true))
        );

        let find_resp = client.find_actor_by_mcp_id("cube_01").await.unwrap();
        assert_eq!(
            find_resp.get("success"),
            Some(&serde_json::Value::Bool(true))
        );
    }

    #[tokio::test]
    async fn mock_bridge_spawn_blueprint_actor() {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let port = listener.local_addr().unwrap().port();
        drop(listener);

        let _server = mock_bridge_server(port).await;
        tokio::time::sleep(Duration::from_millis(50)).await;

        let client = UnrealClient {
            host: "127.0.0.1".to_string(),
            port,
            stream: Arc::new(Mutex::new(None)),
        };
        let resp = client
            .spawn_blueprint_actor(
                "/Game/Blueprints/TestActor.TestActor",
                "bp_actor_01",
                [0.0, 0.0, 0.0],
                [0.0, 0.0, 0.0],
                [1.0, 1.0, 1.0],
            )
            .await
            .unwrap();
        assert_eq!(resp.get("success"), Some(&serde_json::Value::Bool(true)));
    }

    #[tokio::test]
    async fn mock_bridge_spawn_instance_set() {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let port = listener.local_addr().unwrap().port();
        drop(listener);

        let _server = mock_bridge_server(port).await;
        tokio::time::sleep(Duration::from_millis(50)).await;

        let client = UnrealClient {
            host: "127.0.0.1".to_string(),
            port,
            stream: Arc::new(Mutex::new(None)),
        };
        let transforms: Vec<serde_json::Value> = (0..3)
            .map(|i| {
                json!({
                    "location": [i as f64 * 10.0, 0.0, 100.0],
                    "rotation": [0.0, 0.0, 0.0],
                    "scale": [1.0, 1.0, 1.0],
                })
            })
            .collect();
        let resp = client
            .spawn_instance_set(
                "crenellation_test",
                "/Engine/BasicShapes/Cube.Cube",
                None,
                transforms,
            )
            .await
            .unwrap();
        assert_eq!(resp.get("success"), Some(&serde_json::Value::Bool(true)));
        assert_eq!(
            resp.get("instance_count"),
            Some(&serde_json::Value::Number(300.into()))
        );
    }

    #[tokio::test]
    async fn mock_bridge_update_delete_get_list_instance_set() {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let port = listener.local_addr().unwrap().port();
        drop(listener);

        let _server = mock_bridge_server(port).await;
        tokio::time::sleep(Duration::from_millis(50)).await;

        let client = UnrealClient {
            host: "127.0.0.1".to_string(),
            port,
            stream: Arc::new(Mutex::new(None)),
        };

        // update
        let transforms: Vec<serde_json::Value> = (0..3)
            .map(|i| {
                json!({
                    "location": [i as f64 * 10.0, 0.0, 100.0],
                    "rotation": [0.0, 0.0, 0.0],
                    "scale": [1.0, 1.0, 1.0],
                })
            })
            .collect();
        let resp = client
            .update_instance_set("crenellation_test", transforms)
            .await
            .unwrap();
        assert_eq!(resp.get("success"), Some(&serde_json::Value::Bool(true)));

        // get state
        let resp = client.get_instance_set_state("crenellation_test").await.unwrap();
        assert_eq!(resp.get("success"), Some(&serde_json::Value::Bool(true)));
        assert_eq!(
            resp.get("instance_count"),
            Some(&serde_json::Value::Number(300.into()))
        );

        // list
        let resp = client.list_instance_sets().await.unwrap();
        assert_eq!(resp.get("success"), Some(&serde_json::Value::Bool(true)));
        let sets = resp.get("sets").and_then(|v| v.as_array());
        assert!(sets.is_some());
        assert_eq!(sets.unwrap().len(), 1);

        // delete
        let resp = client.delete_instance_set("crenellation_test").await.unwrap();
        assert_eq!(resp.get("success"), Some(&serde_json::Value::Bool(true)));
    }
}
