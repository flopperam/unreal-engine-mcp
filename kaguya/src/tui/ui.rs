use ratatui::{
    layout::{Alignment, Constraint, Direction, Layout, Position, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span, Text},
    widgets::{Block, Borders, Cell, Clear, Gauge, Paragraph, Row, Table, Wrap},
    Frame,
};

use crate::services::LogLevel;
use crate::tui::app::{App, Dialog, InputMode, LogEntry, ServiceRow};

// ── Refined dark palette ──────────────────────────────────────────
const BG: Color = Color::Rgb(13, 17, 23);        // Deep dark background
const BG_SURFACE: Color = Color::Rgb(22, 27, 34); // Card / elevated surface
const BG_HOVER: Color = Color::Rgb(30, 37, 46);   // Selected row
const BORDER: Color = Color::Rgb(48, 54, 61);     // Subtle borders
#[allow(dead_code)]
const BORDER_ACCENT: Color = Color::Rgb(110, 84, 148); // Purple accent border
const FG: Color = Color::Rgb(230, 237, 243);       // Primary text
const FG_DIM: Color = Color::Rgb(125, 133, 144);   // Secondary / muted text
const ACCENT: Color = Color::Rgb(137, 87, 229);    // Purple accent
const ACCENT2: Color = Color::Rgb(88, 166, 255);   // Blue accent
const GREEN: Color = Color::Rgb(63, 185, 80);      // Healthy / success
const YELLOW: Color = Color::Rgb(210, 153, 34);     // Warning / booting
const RED: Color = Color::Rgb(248, 81, 73);         // Error / dead
#[allow(dead_code)]
const PINK: Color = Color::Rgb(219, 97, 162);       // Decorative pink
const CYAN: Color = Color::Rgb(57, 211, 229);       // Debug / info accent
const WHITE: Color = Color::Rgb(255, 255, 255);

pub fn draw(frame: &mut Frame, app: &mut App) {
    let area = frame.area();
    frame.render_widget(Block::default().style(Style::default().bg(BG)), area);

    let main_layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(4),                                // Header (compact)
            Constraint::Length(1),                               // Separator
            Constraint::Min(6),                                  // Service table
            Constraint::Length(if app.booting { 2 } else { 0 }), // Progress
            Constraint::Percentage(30),                          // Log pane (dynamic)
            Constraint::Length(1),                               // Hint bar
            Constraint::Length(3),                               // Command bar
        ])
        .split(area);

    draw_header(frame, app, main_layout[0]);
    draw_separator(frame, main_layout[1]);
    draw_service_table(frame, app, main_layout[2]);
    if app.booting {
        draw_bamboo_gauge(frame, app, main_layout[3]);
    }
    draw_log_pane(frame, app, main_layout[4]);
    draw_hint_bar(frame, app, main_layout[5]);
    draw_command_bar(frame, app, main_layout[6]);

    // Overlays (on top)
    if app.show_suggestions && !app.command_suggestions.is_empty() {
        draw_suggestions(frame, app, main_layout[6]);
    }
    if app.show_help {
        draw_help_popup(frame, app, area);
    }
    if let Some(dialog) = &app.dialog {
        draw_dialog(frame, app, area, dialog);
    }
}

// ── Header ────────────────────────────────────────────────────────
fn draw_header(frame: &mut Frame, app: &App, area: Rect) {
    frame.render_widget(Block::default().style(Style::default().bg(BG)), area);

    let cols = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(55), Constraint::Percentage(45)])
        .split(area);

    // Left: Logo + subtitle
    let moon = if app.use_emoji { app.moon_emoji() } else { "*" };
    let title_line = Line::from(vec![
        Span::styled(
            format!(" {} ", moon),
            Style::default().fg(ACCENT).add_modifier(Modifier::BOLD),
        ),
        Span::styled(
            "KAGUYA",
            Style::default().fg(WHITE).add_modifier(Modifier::BOLD),
        ),
        Span::styled("  ░  ", Style::default().fg(BORDER)),
        Span::styled(
            "Orchestrator",
            Style::default().fg(FG_DIM),
        ),
    ]);
    let sub_line = Line::from(vec![
        Span::styled("    ", Style::default()),
        Span::styled(
            "Quietly, surely, arranging things.",
            Style::default().fg(FG_DIM).add_modifier(Modifier::ITALIC),
        ),
    ]);
    let left_text = Text::from(vec![Line::from(""), title_line, sub_line]);
    frame.render_widget(Paragraph::new(left_text), cols[0]);

    // Right: Status badges
    let heal_color = if app.auto_heal { GREEN } else { RED };
    let heal_text = if app.auto_heal { "ON" } else { "OFF" };
    let uptime = app.uptime_str();
    let info_lines = vec![
        Line::from(""),
        Line::from(vec![
            Span::styled("preset ", Style::default().fg(FG_DIM)),
            Span::styled(&app.preset, Style::default().fg(ACCENT2).add_modifier(Modifier::BOLD)),
            Span::styled("  │  ", Style::default().fg(BORDER)),
            Span::styled("heal ", Style::default().fg(FG_DIM)),
            Span::styled(heal_text, Style::default().fg(heal_color).add_modifier(Modifier::BOLD)),
        ]),
        Line::from(vec![
            Span::styled("uptime ", Style::default().fg(FG_DIM)),
            Span::styled(&uptime, Style::default().fg(FG)),
            Span::styled("  │  ", Style::default().fg(BORDER)),
            Span::styled(format!("v{}", app.version), Style::default().fg(FG_DIM)),
        ]),
    ];
    let right_para = Paragraph::new(Text::from(info_lines)).alignment(Alignment::Right);
    frame.render_widget(right_para, cols[1]);
}

fn draw_separator(frame: &mut Frame, area: Rect) {
    let sep = "─".repeat(area.width as usize);
    let para = Paragraph::new(sep).style(Style::default().fg(BORDER));
    frame.render_widget(para, area);
}

// ── Service Table ─────────────────────────────────────────────────
fn draw_service_table(frame: &mut Frame, app: &mut App, area: Rect) {
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(BORDER))
        .style(Style::default().bg(BG));
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let header = Row::new(vec![
        Cell::from("  SERVICE").style(Style::default().fg(FG_DIM).add_modifier(Modifier::BOLD)),
        Cell::from("STATUS").style(Style::default().fg(FG_DIM).add_modifier(Modifier::BOLD)),
        Cell::from("PID").style(Style::default().fg(FG_DIM).add_modifier(Modifier::BOLD)),
        Cell::from("PORT").style(Style::default().fg(FG_DIM).add_modifier(Modifier::BOLD)),
        Cell::from("HEALTH").style(Style::default().fg(FG_DIM).add_modifier(Modifier::BOLD)),
    ])
    .height(1)
    .bottom_margin(1);

    let (rows, selected_idx) = {
        let order = ["surrealdb", "scene-syncd", "unreal", "mcp-server"];
        let mut service_rows: Vec<&ServiceRow> = app.services.values().collect();
        service_rows.sort_by_key(|r| order.iter().position(|&o| r.name == o).unwrap_or(99));

        let rows: Vec<Row> = service_rows
            .iter()
            .enumerate()
            .map(|(i, r)| {
                let is_selected = app.selected_service.as_ref() == Some(&r.name);
                service_row_to_row(r, app, is_selected, i)
            })
            .collect();

        let selected_idx = app
            .selected_service
            .as_ref()
            .and_then(|name| service_rows.iter().position(|r| r.name == *name));
        (rows, selected_idx)
    };

    app.table_state.select(selected_idx);

    let table = Table::new(
        rows,
        [
            Constraint::Min(18),
            Constraint::Length(12),
            Constraint::Length(10),
            Constraint::Length(10),
            Constraint::Min(14),
        ],
    )
    .header(header)
    .row_highlight_style(Style::default().bg(BG_HOVER))
    .style(Style::default().bg(BG).fg(FG));

    frame.render_stateful_widget(table, inner, &mut app.table_state);
}

fn service_row_to_row<'a>(row: &'a ServiceRow, app: &App, is_selected: bool, _idx: usize) -> Row<'a> {
    let status_color = Color::Rgb(row.status_color.0, row.status_color.1, row.status_color.2);
    let health_color = Color::Rgb(row.health_color.0, row.health_color.1, row.health_color.2);

    let status_indicator = if app.use_emoji {
        match row.status_text.as_str() {
            s if s == app.i18n.t("status.run") => "● ",
            s if s == app.i18n.t("status.boot") => "◐ ",
            s if s == app.i18n.t("status.dead") => "✖ ",
            _ => "○ ",
        }
    } else {
        match row.status_text.as_str() {
            s if s == app.i18n.t("status.run") => "[OK] ",
            s if s == app.i18n.t("status.boot") => "[..] ",
            s if s == app.i18n.t("status.dead") => "[NG] ",
            _ => "[--] ",
        }
    };

    let selector = if is_selected { "▎ " } else { "  " };
    let selector_color = if is_selected { ACCENT } else { BG };

    Row::new(vec![
        Cell::from(Line::from(vec![
            Span::styled(selector, Style::default().fg(selector_color)),
            Span::styled(&row.icon, Style::default()),
            Span::styled(" ", Style::default()),
            Span::styled(
                capitalize(&row.name),
                Style::default().fg(FG).add_modifier(if is_selected { Modifier::BOLD } else { Modifier::empty() }),
            ),
        ])),
        Cell::from(Line::from(vec![
            Span::styled(status_indicator, Style::default().fg(status_color)),
            Span::styled(&row.status_text, Style::default().fg(status_color)),
        ])),
        Cell::from(Span::styled(&row.pid, Style::default().fg(FG_DIM))),
        Cell::from(Span::styled(&row.port, Style::default().fg(FG_DIM))),
        Cell::from(Span::styled(&row.health, Style::default().fg(health_color))),
    ])
}

// ── Bamboo Gauge ──────────────────────────────────────────────────
fn draw_bamboo_gauge(frame: &mut Frame, app: &App, area: Rect) {
    let booting_count = app.services.values().filter(|r| r.is_booting).count();
    let total_count = app.services.len();
    let progress = if total_count > 0 {
        ((total_count - booting_count) as f64 / total_count as f64 * 100.0) as u16
    } else {
        0
    };

    let label = format!("  Starting services... {}%", progress);
    let gauge = Gauge::default()
        .block(Block::default().borders(Borders::NONE).style(Style::default().bg(BG)))
        .gauge_style(Style::default().fg(ACCENT).add_modifier(Modifier::BOLD))
        .percent(progress)
        .label(label);
    frame.render_widget(gauge, area);
}

// ── Log Pane ──────────────────────────────────────────────────────
fn draw_log_pane(frame: &mut Frame, app: &App, area: Rect) {
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(BORDER))
        .style(Style::default().bg(BG));
    let inner = block.inner(area);
    frame.render_widget(block, area);

    if inner.height < 2 { return; }

    // Header line
    let header_area = Rect { x: inner.x, y: inner.y, width: inner.width, height: 1 };
    let header_cols = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(header_area);

    let filter_label = app.log_filter.as_ref().map(|f| format!(" ▸ {}", f)).unwrap_or_default();
    let log_title = format!(" LOGS{}", filter_label);
    frame.render_widget(
        Paragraph::new(log_title).style(Style::default().fg(ACCENT).add_modifier(Modifier::BOLD)),
        header_cols[0],
    );

    let scroll_icon = if app.auto_scroll { "▼" } else { "⏸" };
    let scroll_color = if app.auto_scroll { GREEN } else { FG_DIM };
    let scroll_label = format!("{} auto-scroll ", scroll_icon);
    frame.render_widget(
        Paragraph::new(scroll_label)
            .style(Style::default().fg(scroll_color))
            .alignment(Alignment::Right),
        header_cols[1],
    );

    // Log content
    let log_area = Rect {
        x: inner.x,
        y: inner.y + 1,
        width: inner.width.saturating_sub(1),
        height: inner.height.saturating_sub(1),
    };

    let logs = app.filtered_logs();
    let start = if app.auto_scroll {
        logs.len().saturating_sub(log_area.height as usize)
    } else {
        app.log_scroll_offset.min(logs.len().saturating_sub(log_area.height as usize))
    };
    let visible: Vec<&LogEntry> = logs.into_iter().skip(start).take(log_area.height as usize).collect();

    let mut lines = Vec::new();
    for entry in &visible {
        let (level_color, level_label) = match entry.level {
            LogLevel::Error => (RED, "ERR"),
            LogLevel::Warn => (YELLOW, "WRN"),
            LogLevel::Debug => (CYAN, "DBG"),
            LogLevel::Info => (FG_DIM, "INF"),
        };
        let line = Line::from(vec![
            Span::styled(format!(" {} ", entry.timestamp), Style::default().fg(FG_DIM)),
            Span::styled(
                format!("{:3}", level_label),
                Style::default().fg(BG).bg(level_color).add_modifier(Modifier::BOLD),
            ),
            Span::styled(" ", Style::default()),
            Span::styled(
                format!("{:12}", entry.service_name),
                Style::default().fg(ACCENT2),
            ),
            Span::styled(&entry.message, Style::default().fg(FG)),
        ]);
        lines.push(line);
    }

    frame.render_widget(
        Paragraph::new(Text::from(lines)).wrap(Wrap { trim: true }),
        log_area,
    );

    // Scrollbar indicator (right edge)
    if inner.height > 2 {
        let total_logs = app.filtered_logs().len();
        let view_h = log_area.height as usize;
        if total_logs > view_h {
            let bar_h = inner.height.saturating_sub(1) as usize;
            let thumb_pos = if total_logs > 0 {
                (start * bar_h) / total_logs
            } else { 0 };
            let sb_area = Rect {
                x: inner.x + inner.width.saturating_sub(1),
                y: inner.y + 1,
                width: 1,
                height: inner.height.saturating_sub(1),
            };
            for row in 0..sb_area.height {
                let ch = if row as usize == thumb_pos { "█" } else { "░" };
                let color = if row as usize == thumb_pos { ACCENT } else { BORDER };
                frame.render_widget(
                    Paragraph::new(ch).style(Style::default().fg(color)),
                    Rect { x: sb_area.x, y: sb_area.y + row, width: 1, height: 1 },
                );
            }
        }
    }
}

// ── Command Bar ───────────────────────────────────────────────────
fn draw_command_bar(frame: &mut Frame, app: &App, area: Rect) {
    let border_color = match app.input_mode {
        InputMode::Insert => ACCENT,
        InputMode::Normal => BORDER,
    };
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(border_color))
        .style(Style::default().bg(BG_SURFACE));
    let inner = block.inner(area);
    frame.render_widget(block, area);

    let layout = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Length(6), Constraint::Min(10)])
        .split(inner);

    // Mode badge
    let (mode_text, mode_fg, mode_bg) = match app.input_mode {
        InputMode::Normal => (" NOR ", FG_DIM, BG),
        InputMode::Insert => (" INS ", BG, GREEN),
    };
    frame.render_widget(
        Paragraph::new(mode_text)
            .style(Style::default().fg(mode_fg).bg(mode_bg).add_modifier(Modifier::BOLD)),
        layout[0],
    );

    // Input
    let prompt = if app.input_mode == InputMode::Insert { "❯ " } else { "  " };
    let input_text = format!("{}{}", prompt, app.input);
    frame.render_widget(
        Paragraph::new(input_text).style(Style::default().fg(FG)),
        layout[1],
    );

    if app.input_mode == InputMode::Insert {
        let x = layout[1].x + prompt.len() as u16 + app.cursor_pos as u16;
        let y = layout[1].y;
        frame.set_cursor_position(Position { x, y });
    }
}

// ── Command Suggestions Overlay ───────────────────────────────────
fn draw_suggestions(frame: &mut Frame, app: &App, cmd_bar_area: Rect) {
    let suggestions = &app.command_suggestions;
    let count = suggestions.len().min(11); // max visible
    let height = count as u16 + 2; // +2 for borders

    let popup_area = Rect {
        x: cmd_bar_area.x + 1,
        y: cmd_bar_area.y.saturating_sub(height),
        width: cmd_bar_area.width.min(50).saturating_sub(2),
        height,
    };

    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(ACCENT))
        .title(" Commands ")
        .title_style(Style::default().fg(ACCENT).add_modifier(Modifier::BOLD))
        .style(Style::default().bg(BG_SURFACE));
    let inner = block.inner(popup_area);
    frame.render_widget(Clear, popup_area);
    frame.render_widget(block, popup_area);

    let mut lines = Vec::new();
    for (i, (cmd, desc)) in suggestions.iter().take(count).enumerate() {
        let is_selected = app.suggestion_index == Some(i);
        let (fg, bg) = if is_selected { (BG, ACCENT) } else { (FG, BG_SURFACE) };
        lines.push(Line::from(vec![
            Span::styled(
                format!(" {:<12}", cmd),
                Style::default().fg(fg).bg(bg).add_modifier(Modifier::BOLD),
            ),
            Span::styled(
                format!(" {}", desc),
                Style::default().fg(if is_selected { BG } else { FG_DIM }).bg(bg),
            ),
        ]));
    }

    frame.render_widget(Paragraph::new(Text::from(lines)), inner);
}

// ── Hint Bar ──────────────────────────────────────────────────────
fn draw_hint_bar(frame: &mut Frame, app: &App, area: Rect) {
    if app.show_help || app.dialog.is_some() { return; }

    let hints: Vec<(&str, &str)> = match app.input_mode {
        InputMode::Normal => vec![
            ("/", "cmd"), ("i", "insert"), ("j/k", "select"), ("s", "start/stop"),
            ("r", "restart"), ("l", "filter"), ("?", "help"), ("q", "quit"),
        ],
        InputMode::Insert => vec![
            ("Tab", "complete"), ("↑↓", "navigate"), ("Enter", "exec"), ("Esc", "cancel"),
        ],
    };

    let mut spans = vec![Span::styled(" ", Style::default())];
    for (i, (key, action)) in hints.iter().enumerate() {
        if i > 0 {
            spans.push(Span::styled("  ", Style::default().fg(BORDER)));
        }
        spans.push(Span::styled(
            format!(" {} ", key),
            Style::default().fg(BG).bg(FG_DIM).add_modifier(Modifier::BOLD),
        ));
        spans.push(Span::styled(format!(" {}", action), Style::default().fg(FG_DIM)));
    }

    frame.render_widget(
        Paragraph::new(Line::from(spans)).style(Style::default().bg(BG)),
        area,
    );
}

// ── Help Popup ────────────────────────────────────────────────────
fn draw_help_popup(frame: &mut Frame, app: &App, area: Rect) {
    let popup_area = centered_rect(55, 65, area);
    let block = Block::default()
        .title(format!(" {} {} ", if app.use_emoji { "🌙" } else { "" }, app.i18n.t("help.title")))
        .title_style(Style::default().fg(ACCENT).add_modifier(Modifier::BOLD))
        .borders(Borders::ALL)
        .border_style(Style::default().fg(ACCENT))
        .style(Style::default().bg(BG_SURFACE).fg(FG));

    let mut help_lines = vec![
        Line::from(""),
        Line::from(Span::styled("  COMMANDS", Style::default().fg(ACCENT).add_modifier(Modifier::BOLD))),
        Line::from(""),
    ];
    for key in ["help.up", "help.down", "help.status", "help.logs", "help.logs_clear",
                "help.cleanup", "help.preset", "help.health", "help.config", "help.quit"] {
        help_lines.push(Line::from(format!("  {}", app.i18n.t(key))));
    }
    help_lines.push(Line::from(""));
    help_lines.push(Line::from(Span::styled("  KEYBOARD", Style::default().fg(ACCENT).add_modifier(Modifier::BOLD))));
    help_lines.push(Line::from(""));
    for key in ["help.keyboard_i", "help.q", "help.slash", "help.scroll",
                "help.keyboard_esc", "help.keyboard_enter", "help.keyboard_ctrl_l",
                "help.keyboard_arrows", "help.ctrl_c"] {
        help_lines.push(Line::from(format!("  {}", app.i18n.t(key))));
    }
    help_lines.push(Line::from(""));
    help_lines.push(Line::from(Span::styled(
        "  Press Esc / Enter / q to close",
        Style::default().fg(FG_DIM).add_modifier(Modifier::ITALIC),
    )));

    let para = Paragraph::new(Text::from(help_lines))
        .block(block)
        .wrap(Wrap { trim: true });
    frame.render_widget(Clear, popup_area);
    frame.render_widget(para, popup_area);
}

// ── Dialog ────────────────────────────────────────────────────────
fn draw_dialog(frame: &mut Frame, app: &App, area: Rect, dialog: &Dialog) {
    // Dynamic sizing
    let msg_lines = dialog.message.lines().count() as u16;
    let height = (msg_lines + 6).min(area.height - 4).max(8);
    let width_pct = 45u16;
    let height_pct = ((height as f32 / area.height as f32) * 100.0) as u16;
    let popup_area = centered_rect(width_pct.max(40), height_pct.max(20).min(60), area);

    let block = Block::default()
        .title(format!(" {} ", dialog.title))
        .title_style(Style::default().fg(YELLOW).add_modifier(Modifier::BOLD))
        .borders(Borders::ALL)
        .border_style(Style::default().fg(YELLOW))
        .style(Style::default().bg(BG_SURFACE).fg(FG));

    let inner = block.inner(popup_area);
    frame.render_widget(Clear, popup_area);
    frame.render_widget(block, popup_area);

    let msg_area = Rect { x: inner.x + 1, y: inner.y + 1, width: inner.width.saturating_sub(2), height: inner.height.saturating_sub(3) };
    frame.render_widget(
        Paragraph::new(dialog.message.clone()).wrap(Wrap { trim: true }).style(Style::default().fg(FG)),
        msg_area,
    );

    // Option buttons
    let mut options = Vec::new();
    for (i, opt) in dialog.options.iter().enumerate() {
        if i > 0 { options.push(Span::styled("   ", Style::default())); }
        if i == dialog.selected {
            options.push(Span::styled(
                format!(" {} ", opt),
                Style::default().fg(BG).bg(GREEN).add_modifier(Modifier::BOLD),
            ));
        } else {
            options.push(Span::styled(
                format!(" {} ", opt),
                Style::default().fg(FG_DIM).bg(BG),
            ));
        }
    }
    let opts_area = Rect { x: inner.x, y: inner.y + inner.height.saturating_sub(2), width: inner.width, height: 1 };
    frame.render_widget(
        Paragraph::new(Line::from(options)).alignment(Alignment::Center),
        opts_area,
    );

    let hint_area = Rect { x: inner.x, y: inner.y + inner.height.saturating_sub(1), width: inner.width, height: 1 };
    frame.render_widget(
        Paragraph::new(app.i18n.t("dialog.hint"))
            .style(Style::default().fg(FG_DIM))
            .alignment(Alignment::Center),
        hint_area,
    );
}

// ── Utilities ─────────────────────────────────────────────────────
fn centered_rect(percent_x: u16, percent_y: u16, r: Rect) -> Rect {
    let popup_layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Percentage((100 - percent_y) / 2),
            Constraint::Percentage(percent_y),
            Constraint::Percentage((100 - percent_y) / 2),
        ])
        .split(r);

    Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Percentage((100 - percent_x) / 2),
            Constraint::Percentage(percent_x),
            Constraint::Percentage((100 - percent_x) / 2),
        ])
        .split(popup_layout[1])[1]
}

fn capitalize(s: &str) -> String {
    let mut c = s.chars();
    match c.next() {
        None => String::new(),
        Some(f) => f.to_uppercase().collect::<String>() + &c.as_str().to_lowercase(),
    }
}
