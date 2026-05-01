use serde::{Deserialize, Serialize};

/// Result of running a PIE (Play-In-Editor) smoke test.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct UnrealTestRun {
    /// Unique run identifier.
    pub run_id: String,
    /// Scene ID that was tested.
    pub scene_id: String,
    /// Test mode (smoke, full, performance).
    pub mode: TestMode,
    /// Test result.
    pub result: TestResult,
    /// Collected log events from the Unreal output log.
    #[serde(default)]
    pub logs: Vec<UnrealLogEvent>,
    /// Parsed diagnostics from the log.
    #[serde(default)]
    pub diagnostics: Vec<UnrealDiagnostic>,
}

/// PIE test mode.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum TestMode {
    /// Quick smoke test: start PIE, wait for load, stop.
    Smoke,
    /// Full test: run all scripted tests.
    Full,
    /// Performance benchmark.
    Performance,
}

/// Result of a test run.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum TestResult {
    /// Test passed successfully.
    Passed,
    /// Test failed with errors.
    Failed,
    /// Test timed out.
    Timeout,
    /// Could not connect to Unreal.
    ConnectionError,
}

/// An event from the Unreal output log.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct UnrealLogEvent {
    /// Timestamp of the log event.
    pub timestamp: String,
    /// Log category.
    pub category: String,
    /// Log verbosity level (Display, Warning, Error, etc.).
    pub verbosity: String,
    /// Log message.
    pub message: String,
}

/// A parsed diagnostic from Unreal logs.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct UnrealDiagnostic {
    /// Diagnostic severity.
    pub severity: String,
    /// Diagnostic code or category.
    pub code: String,
    /// Human-readable description.
    pub description: String,
    /// Suggested fix, if available.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub suggestion: Option<String>,
    /// Source object or blueprint, if applicable.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub source: Option<String>,
}

/// A plan to fix issues detected during PIE testing.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct FixPlan {
    /// Diagnostics that triggered this fix plan.
    pub diagnostics: Vec<UnrealDiagnostic>,
    /// Proposed fix operations.
    pub operations: Vec<FixOperation>,
    /// Confidence score 0.0–1.0.
    pub confidence: f32,
    /// Whether the fix requires explicit user approval.
    pub requires_user_approval: bool,
}

/// A single fix operation.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct FixOperation {
    /// Operation type (adjust_transform, fix_material, remove_collision, etc.).
    pub operation_type: String,
    /// Target object mcp_id.
    pub target_mcp_id: String,
    /// Parameters for the fix.
    pub params: serde_json::Value,
    /// Description of what this fix does.
    pub description: String,
}

impl UnrealDiagnostic {
    /// Parse a severity string from Unreal log output.
    pub fn severity_from_verbosity(verbosity: &str) -> String {
        match verbosity.to_lowercase().as_str() {
            "error" | "fatal" => "error".to_string(),
            "warning" => "warning".to_string(),
            _ => "info".to_string(),
        }
    }
}

/// Parse Unreal log output into structured log events.
/// Parse Unreal log output into structured log events.
/// Unreal log format: [YYYY.MM.DD-HH.MM.SS:NNN][Channel]Verbosity: Message
/// e.g. [2024.01.15-10.30.00:123][LogPhysics]Display: Physics simulation started
pub fn parse_unreal_logs(raw_output: &str) -> Vec<UnrealLogEvent> {
    raw_output
        .lines()
        .filter_map(|line| {
            let trimmed = line.trim();
            if trimmed.is_empty() {
                return None;
            }

            let (category, verbosity, message) = parse_unreal_line(trimmed);

            Some(UnrealLogEvent {
                timestamp: String::new(),
                category,
                verbosity,
                message,
            })
        })
        .collect()
}

/// Parse a single Unreal log line.
/// Format: [timestamp][Channel]Verbosity: Message
fn parse_unreal_line(line: &str) -> (String, String, String) {
    // Find all ']' positions in the line
    let bytes = line.as_bytes();
    let bracket_positions: Vec<usize> = bytes
        .iter()
        .enumerate()
        .filter(|(_, &b)| b == b']')
        .map(|(i, _)| i)
        .collect();

    // For each ']', check if the text after it matches "Verbosity: Message"
    // where Verbosity is a single word (no spaces, no brackets).
    for &bracket_pos in &bracket_positions {
        let after_bracket = &line[bracket_pos + 1..];
        // Find ": " after the verbosity word
        if let Some(colon_pos) = after_bracket.find(": ") {
            let verbosity = &after_bracket[..colon_pos];
            // Verbosity should be a single word like Display, Warning, Error, etc.
            if verbosity.is_empty() || verbosity.contains(']') || verbosity.contains('[') {
                continue;
            }
            let message = after_bracket[colon_pos + 2..].to_string();

            // Extract channel: text between this ']' and the preceding '['.
            let before_bracket = &line[..bracket_pos];
            if let Some(channel_start) = before_bracket.rfind('[') {
                let channel = &line[channel_start + 1..bracket_pos];
                return (channel.to_string(), verbosity.to_string(), message);
            }
        }
    }

    // Fallback: simple "Category: Message" parse
    fallback_parse(line)
}

fn fallback_parse(line: &str) -> (String, String, String) {
    if let Some(idx) = line.find(": ") {
        let prefix = &line[..idx];
        let msg = line[idx + 2..].to_string();
        (prefix.to_string(), "Display".to_string(), msg)
    } else if let Some(idx) = line.find(':') {
        let prefix = &line[..idx];
        let msg = line[idx + 1..].trim().to_string();
        (prefix.to_string(), "Display".to_string(), msg)
    } else {
        ("General".to_string(), "Display".to_string(), line.to_string())
    }
}

/// Extract diagnostics from Unreal log events.
pub fn extract_diagnostics(logs: &[UnrealLogEvent]) -> Vec<UnrealDiagnostic> {
    logs.iter()
        .filter(|log| matches!(log.verbosity.to_lowercase().as_str(), "error" | "warning" | "fatal"))
        .map(|log| UnrealDiagnostic {
            severity: UnrealDiagnostic::severity_from_verbosity(&log.verbosity),
            code: log.category.clone(),
            description: log.message.clone(),
            suggestion: None,
            source: None,
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_unreal_logs_extracts_events() {
        let raw = "[2024.01.15-10.30.00:123][LogPhysics]Display: Physics simulation started\n[2024.01.15-10.30.01:456][LogNet]Warning: Network latency high";
        let events = parse_unreal_logs(raw);
        assert_eq!(events.len(), 2);
        assert_eq!(events[0].category, "LogPhysics");
        assert_eq!(events[0].verbosity, "Display");
        assert!(events[0].message.contains("Physics"));
        assert_eq!(events[1].category, "LogNet");
        assert_eq!(events[1].verbosity, "Warning");
        assert!(events[1].message.contains("Network"));
    }

    #[test]
    fn extract_diagnostics_filters_errors_and_warnings() {
        let logs = vec![
            UnrealLogEvent {
                timestamp: String::new(),
                category: "LogPhysics".to_string(),
                verbosity: "Display".to_string(),
                message: "Physics started".to_string(),
            },
            UnrealLogEvent {
                timestamp: String::new(),
                category: "LogNet".to_string(),
                verbosity: "Warning".to_string(),
                message: "Network latency high".to_string(),
            },
            UnrealLogEvent {
                timestamp: String::new(),
                category: "LogRenderer".to_string(),
                verbosity: "Error".to_string(),
                message: "Shader compilation failed".to_string(),
            },
        ];
        let diags = extract_diagnostics(&logs);
        assert_eq!(diags.len(), 2);
        assert_eq!(diags[0].severity, "warning");
        assert_eq!(diags[1].severity, "error");
    }

    #[test]
    fn fix_plan_requires_approval_for_low_confidence() {
        let plan = FixPlan {
            diagnostics: vec![],
            operations: vec![FixOperation {
                operation_type: "adjust_transform".to_string(),
                target_mcp_id: "wall_1".to_string(),
                params: serde_json::json!({}),
                description: "Fix overlapping wall".to_string(),
            }],
            confidence: 0.5,
            requires_user_approval: true,
        };
        assert!(plan.requires_user_approval);
    }
}