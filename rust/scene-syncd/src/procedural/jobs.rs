//! Background procedural-generation job registry.
//!
//! Long-running generators (large WFC grids, deep L-System derivations) can
//! exceed the MCP bridge's request timeout. This module exposes a simple
//! tokio-based job queue with status polling so Python clients can submit a
//! job, poll for completion, and fetch the result without holding a single
//! HTTP connection open for the entire computation.

use std::collections::HashMap;
use std::sync::Arc;
use std::time::{Duration, Instant};

use serde::Serialize;
use serde_json::{json, Value};
use tokio::sync::{Mutex, Notify, Semaphore};
use ulid::Ulid;

use crate::procedural::generator::{GenerateContext, GenerationLimits, Generator};
use crate::procedural::lsystem::{LSystemGenerator, LSystemParams};
use crate::procedural::wfc::{WfcGenerator, WfcParams};

/// Default per-process concurrency for procedural jobs.
pub const DEFAULT_MAX_CONCURRENCY: usize = 4;

/// Time after completion before a job's result is purged from the registry.
pub const RESULT_TTL: Duration = Duration::from_secs(30 * 60);

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum JobStatus {
    Queued,
    Running,
    Completed,
    Failed,
    Cancelled,
}

#[derive(Debug, Clone, Serialize)]
pub enum JobGenerator {
    Wfc,
    Lsystem,
}

impl JobGenerator {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_ascii_lowercase().as_str() {
            "wfc" => Some(Self::Wfc),
            "lsystem" | "l_system" | "l-system" => Some(Self::Lsystem),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct JobRecord {
    pub job_id: String,
    pub generator: JobGenerator,
    pub status: JobStatus,
    pub progress: f32,
    pub message: Option<String>,
    pub created_at_ms: u64,
    pub started_at_ms: Option<u64>,
    pub completed_at_ms: Option<u64>,
    pub elapsed_ms: Option<u64>,
    pub seed: Option<u64>,
    pub limits: Option<JobLimits>,
    pub error: Option<String>,
    pub result: Option<Value>,
}

#[derive(Debug, Clone, Serialize)]
pub struct JobLimits {
    pub max_iterations: u32,
    pub max_execution_ms: u64,
}

impl From<&GenerationLimits> for JobLimits {
    fn from(l: &GenerationLimits) -> Self {
        Self {
            max_iterations: l.max_iterations,
            max_execution_ms: l.max_execution_ms,
        }
    }
}

#[derive(Debug)]
struct JobEntry {
    record: JobRecord,
    cancel_notify: Arc<Notify>,
    cancelled: bool,
}

/// Concurrency-limited registry of in-flight + recently completed jobs.
#[derive(Debug, Clone)]
pub struct JobRegistry {
    inner: Arc<Mutex<HashMap<String, JobEntry>>>,
    semaphore: Arc<Semaphore>,
}

impl JobRegistry {
    pub fn new(max_concurrency: usize) -> Self {
        Self {
            inner: Arc::new(Mutex::new(HashMap::new())),
            semaphore: Arc::new(Semaphore::new(max_concurrency.max(1))),
        }
    }

    pub fn semaphore(&self) -> Arc<Semaphore> {
        self.semaphore.clone()
    }

    /// Submit a new job. Returns the assigned job_id and spawns a background
    /// task that runs the generator under the provided limits.
    pub async fn submit(
        &self,
        generator: JobGenerator,
        params_value: Value,
        seed: Option<u64>,
        limits: GenerationLimits,
    ) -> Result<String, String> {
        let job_id = Ulid::new().to_string();
        let now = now_ms();
        let limits_summary = JobLimits::from(&limits);
        let cancel_notify = Arc::new(Notify::new());

        {
            let mut g = self.inner.lock().await;
            g.insert(
                job_id.clone(),
                JobEntry {
                    record: JobRecord {
                        job_id: job_id.clone(),
                        generator: generator.clone(),
                        status: JobStatus::Queued,
                        progress: 0.0,
                        message: Some("queued".to_string()),
                        created_at_ms: now,
                        started_at_ms: None,
                        completed_at_ms: None,
                        elapsed_ms: None,
                        seed,
                        limits: Some(limits_summary),
                        error: None,
                        result: None,
                    },
                    cancel_notify: cancel_notify.clone(),
                    cancelled: false,
                },
            );
        }

        let registry = self.clone();
        let semaphore = self.semaphore.clone();
        let job_id_clone = job_id.clone();
        let cancel_clone = cancel_notify.clone();

        tokio::spawn(async move {
            // Wait for a concurrency slot, but bail out on cancellation.
            let permit = tokio::select! {
                permit = semaphore.acquire_owned() => permit,
                _ = cancel_clone.notified() => {
                    registry.mark_cancelled(&job_id_clone, "cancelled before start").await;
                    return;
                }
            };

            let permit = match permit {
                Ok(p) => p,
                Err(_) => {
                    registry.mark_failed(&job_id_clone, "semaphore closed").await;
                    return;
                }
            };

            registry.mark_running(&job_id_clone).await;

            let started_at = Instant::now();
            let ctx = GenerateContext::new(seed, Some(limits.clone()));

            // Spawn a watchdog that reads ctx.progress every 200ms and updates JobRecord.progress.
            let progress_arc = ctx.progress.clone();
            let watchdog_registry = registry.clone();
            let watchdog_job_id = job_id_clone.clone();
            let watchdog_cancel = cancel_clone.clone();
            let watchdog_handle = tokio::spawn(async move {
                loop {
                    tokio::select! {
                        _ = tokio::time::sleep(Duration::from_millis(200)) => {
                            if let Some(frac) = progress_arc.read() {
                                let still_running = watchdog_registry
                                    .update_progress(&watchdog_job_id, 0.05 + frac * 0.9)
                                    .await;
                                if !still_running { break; }
                            }
                        }
                        _ = watchdog_cancel.notified() => break,
                    }
                }
            });

            let outcome: Result<Value, String> = match generator {
                JobGenerator::Wfc => match serde_json::from_value::<WfcParams>(params_value) {
                    Ok(params) => run_with_cancellation(cancel_clone.clone(), move || {
                        let g = WfcGenerator;
                        match g.generate(&params, &ctx) {
                            Ok(out) => Ok(json!({
                                "data": out.data,
                                "stats": out.stats,
                                "warnings": out.warnings,
                            })),
                            Err(e) => Err(format!("{e}")),
                        }
                    })
                    .await,
                    Err(e) => Err(format!("invalid WFC params: {e}")),
                },
                JobGenerator::Lsystem => {
                    match serde_json::from_value::<LSystemParams>(params_value) {
                        Ok(params) => run_with_cancellation(cancel_clone.clone(), move || {
                            let g = LSystemGenerator;
                            match g.generate(&params, &ctx) {
                                Ok(out) => Ok(json!({
                                    "data": out.data,
                                    "stats": out.stats,
                                    "warnings": out.warnings,
                                })),
                                Err(e) => Err(format!("{e}")),
                            }
                        })
                        .await,
                        Err(e) => Err(format!("invalid L-System params: {e}")),
                    }
                }
            };

            let elapsed_ms = started_at.elapsed().as_millis() as u64;

            match outcome {
                Ok(result) => registry.mark_completed(&job_id_clone, result, elapsed_ms).await,
                Err(err) if err == "__cancelled__" => {
                    registry.mark_cancelled(&job_id_clone, "cancelled mid-run").await
                }
                Err(err) => registry.mark_failed(&job_id_clone, &err).await,
            }

            // Stop the watchdog - either it already exited or we abort it.
            watchdog_handle.abort();
            let _ = watchdog_handle.await;

            drop(permit);
        });

        Ok(job_id)
    }

    pub async fn status(&self, job_id: &str) -> Option<JobRecord> {
        let g = self.inner.lock().await;
        g.get(job_id).map(|e| e.record.clone())
    }

    pub async fn cancel(&self, job_id: &str) -> Result<JobRecord, String> {
        let mut g = self.inner.lock().await;
        let entry = g
            .get_mut(job_id)
            .ok_or_else(|| format!("job '{job_id}' not found"))?;
        entry.cancelled = true;
        entry.cancel_notify.notify_waiters();
        match entry.record.status {
            JobStatus::Queued | JobStatus::Running => {
                entry.record.status = JobStatus::Cancelled;
                entry.record.message = Some("cancellation requested".to_string());
            }
            _ => {}
        }
        Ok(entry.record.clone())
    }

    pub async fn evict_old(&self) {
        let cutoff_ms = now_ms().saturating_sub(RESULT_TTL.as_millis() as u64);
        let mut g = self.inner.lock().await;
        g.retain(|_, e| {
            let completed = e.record.completed_at_ms.unwrap_or(u64::MAX);
            completed >= cutoff_ms
        });
    }

    pub async fn list(&self) -> Vec<JobRecord> {
        let g = self.inner.lock().await;
        g.values().map(|e| e.record.clone()).collect()
    }

    async fn mark_running(&self, job_id: &str) {
        let mut g = self.inner.lock().await;
        if let Some(entry) = g.get_mut(job_id) {
            entry.record.status = JobStatus::Running;
            entry.record.started_at_ms = Some(now_ms());
            entry.record.progress = 0.05;
            entry.record.message = Some("running".to_string());
        }
    }

    /// Update an in-flight job's progress fraction. Returns false when the
    /// job is gone or already finished so the watchdog task can exit.
    pub async fn update_progress(&self, job_id: &str, fraction: f32) -> bool {
        let mut g = self.inner.lock().await;
        let Some(entry) = g.get_mut(job_id) else {
            return false;
        };
        match entry.record.status {
            JobStatus::Running | JobStatus::Queued => {
                let bounded = fraction.clamp(0.05, 0.99);
                if bounded > entry.record.progress {
                    entry.record.progress = bounded;
                }
                true
            }
            _ => false,
        }
    }

    async fn mark_completed(&self, job_id: &str, result: Value, elapsed_ms: u64) {
        let mut g = self.inner.lock().await;
        if let Some(entry) = g.get_mut(job_id) {
            if entry.cancelled {
                entry.record.status = JobStatus::Cancelled;
                entry.record.message = Some("cancelled".to_string());
            } else {
                entry.record.status = JobStatus::Completed;
                entry.record.message = Some("completed".to_string());
                entry.record.result = Some(result);
            }
            entry.record.progress = 1.0;
            entry.record.completed_at_ms = Some(now_ms());
            entry.record.elapsed_ms = Some(elapsed_ms);
        }
    }

    async fn mark_failed(&self, job_id: &str, error: &str) {
        let mut g = self.inner.lock().await;
        if let Some(entry) = g.get_mut(job_id) {
            entry.record.status = JobStatus::Failed;
            entry.record.error = Some(error.to_string());
            entry.record.message = Some("failed".to_string());
            entry.record.completed_at_ms = Some(now_ms());
        }
    }

    async fn mark_cancelled(&self, job_id: &str, message: &str) {
        let mut g = self.inner.lock().await;
        if let Some(entry) = g.get_mut(job_id) {
            entry.record.status = JobStatus::Cancelled;
            entry.record.message = Some(message.to_string());
            entry.record.completed_at_ms = Some(now_ms());
        }
    }
}

/// Runs a synchronous closure in a tokio blocking task while listening for a
/// cancellation notification. On cancel, returns a sentinel `Err` containing
/// the literal string `"__cancelled__"`.
async fn run_with_cancellation<F>(cancel: Arc<Notify>, f: F) -> Result<Value, String>
where
    F: FnOnce() -> Result<Value, String> + Send + 'static,
{
    let handle = tokio::task::spawn_blocking(f);
    tokio::select! {
        res = handle => match res {
            Ok(v) => v,
            Err(e) => Err(format!("task panicked: {e}")),
        },
        _ = cancel.notified() => Err("__cancelled__".to_string()),
    }
}

fn now_ms() -> u64 {
    use std::time::{SystemTime, UNIX_EPOCH};
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as u64)
        .unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::procedural::wfc::{WfcTile, WfcTileset};

    #[tokio::test]
    async fn submit_completes_for_small_grid() {
        let registry = JobRegistry::new(2);
        let params = WfcParams {
            width: 2,
            height: 2,
            tileset: WfcTileset {
                tiles: vec![WfcTile { id: "g".into(), weight: 1.0 }],
                constraints: vec![
                    crate::procedural::wfc::WfcConstraint {
                        left: "g".into(), right: "g".into(),
                        direction: crate::procedural::wfc::WfcDirection::East,
                    },
                    crate::procedural::wfc::WfcConstraint {
                        left: "g".into(), right: "g".into(),
                        direction: crate::procedural::wfc::WfcDirection::South,
                    },
                ],
            },
            seed: Some(1),
            periodic: false,
        };
        let job_id = registry
            .submit(
                JobGenerator::Wfc,
                serde_json::to_value(&params).unwrap(),
                Some(1),
                GenerationLimits::default(),
            )
            .await
            .unwrap();

        // Poll for completion.
        for _ in 0..50 {
            let s = registry.status(&job_id).await.unwrap();
            if matches!(s.status, JobStatus::Completed | JobStatus::Failed) {
                assert!(matches!(s.status, JobStatus::Completed), "{:?}", s);
                let data = s.result.unwrap();
                assert_eq!(data["data"]["width"], 2);
                return;
            }
            tokio::time::sleep(Duration::from_millis(20)).await;
        }
        panic!("job did not complete");
    }

    #[tokio::test]
    async fn unknown_generator_returns_none() {
        assert!(JobGenerator::from_str("does-not-exist").is_none());
        assert!(JobGenerator::from_str("WFC").is_some());
        assert!(JobGenerator::from_str("L-System").is_some());
    }

    #[tokio::test]
    async fn progress_advances_during_long_lsystem_run() {
        // Use a deeper L-System derivation so the watchdog observes intermediate progress.
        let registry = JobRegistry::new(2);
        let params = serde_json::json!({
            "axiom": "F",
            "rules": [["F", "F[+F][-F]"]],
            "iterations": 5,
            "angle_degrees": 25.0,
            "step_length": 5.0,
            "width": 1.0,
            "origin": [0.0, 0.0, 0.0],
            "heading": [1.0, 0.0, 0.0],
            "up": [0.0, 0.0, 1.0],
            "dimension_mode": "ThreeD",
        });
        let job_id = registry
            .submit(
                JobGenerator::Lsystem,
                params,
                Some(7),
                GenerationLimits::default(),
            )
            .await
            .unwrap();

        // Wait a tick for the job to start, then poll a few times.
        tokio::time::sleep(Duration::from_millis(100)).await;
        let mut max_seen = 0.0_f32;
        for _ in 0..30 {
            let s = registry.status(&job_id).await.unwrap();
            if s.progress > max_seen { max_seen = s.progress; }
            if matches!(s.status, JobStatus::Completed | JobStatus::Failed | JobStatus::Cancelled) {
                break;
            }
            tokio::time::sleep(Duration::from_millis(40)).await;
        }
        let final_status = registry.status(&job_id).await.unwrap();
        assert!(matches!(final_status.status, JobStatus::Completed), "{:?}", final_status);
        assert!(final_status.progress >= 0.99, "final progress = {}", final_status.progress);
        assert!(max_seen >= 0.05, "max observed progress = {} (should be >= initial 0.05)", max_seen);
    }
}
