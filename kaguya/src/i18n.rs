use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct I18n {
    lang: String,
    translations: HashMap<String, HashMap<String, String>>,
}

impl I18n {
    pub fn new(lang: &str) -> Self {
        let mut translations: HashMap<String, HashMap<String, String>> = HashMap::new();

        let mut en = HashMap::new();
        en.insert("help.title".to_string(), "Command List".to_string());
        en.insert(
            "help.up".to_string(),
            "/up [preset]   Start stack in dependency order".to_string(),
        );
        en.insert(
            "help.down".to_string(),
            "/down          Safely stop all processes".to_string(),
        );
        en.insert(
            "help.status".to_string(),
            "/status        Force refresh and redraw".to_string(),
        );
        en.insert(
            "help.logs".to_string(),
            "/logs <svc>    Filter logs by service".to_string(),
        );
        en.insert(
            "help.cleanup".to_string(),
            "/cleanup       Free zombie processes / ports".to_string(),
        );
        en.insert(
            "help.preset".to_string(),
            "/preset <mode> Switch preset (minimal/full/headless)".to_string(),
        );
        en.insert(
            "help.health".to_string(),
            "/health        Run health checks".to_string(),
        );
        en.insert(
            "help.config".to_string(),
            "/config        Show current configuration".to_string(),
        );
        en.insert("help.quit".to_string(), "/quit, /q      Exit TUI".to_string());
        en.insert(
            "help.keyboard_i".to_string(),
            "i            INSERT mode (command input)".to_string(),
        );
        en.insert(
            "help.keyboard_esc".to_string(),
            "Esc          Cancel dialog / clear input".to_string(),
        );
        en.insert(
            "help.keyboard_enter".to_string(),
            "Enter        Execute command".to_string(),
        );
        en.insert(
            "help.keyboard_ctrl_l".to_string(),
            "Ctrl+l       Toggle log auto-scroll".to_string(),
        );
        en.insert(
            "help.keyboard_arrows".to_string(),
            "←/→          Select dialog choice".to_string(),
        );
        en.insert("help.logs_clear".to_string(), "/logs          Clear log filter".to_string());
        en.insert(
            "help.q".to_string(),
            "q            Quit TUI (Normal mode only)".to_string(),
        );
        en.insert(
            "help.slash".to_string(),
            "/            Start command input (pre-fills /)".to_string(),
        );
        en.insert(
            "help.scroll".to_string(),
            "↑/↓/PgUp/PgDn  Scroll logs (when auto-scroll is OFF)".to_string(),
        );
        en.insert(
            "help.ctrl_c".to_string(),
            "Ctrl+c       Hard quit (stops all services)".to_string(),
        );
        en.insert("help.keyboard_title".to_string(), "Keyboard:".to_string());
        en.insert(
            "dialog.hint".to_string(),
            "←/→ select • Enter confirm • Esc cancel".to_string(),
        );
        en.insert("dialog.kill_it".to_string(), "Kill it".to_string());
        en.insert("dialog.abort".to_string(), "Abort".to_string());
        en.insert("dialog.ok".to_string(), "OK".to_string());
        en.insert("status.run".to_string(), "Run".to_string());
        en.insert("status.boot".to_string(), "Boot".to_string());
        en.insert("status.dead".to_string(), "Dead".to_string());
        en.insert("status.stopped".to_string(), "Stop".to_string());
        en.insert("status.unknown".to_string(), "?".to_string());
        en.insert("status.blocked".to_string(), "Blocked".to_string());
        en.insert(
            "hint.normal_mode".to_string(),
            "/ command | i insert | q quit | ↑↓ scroll | ? help".to_string(),
        );
        en.insert(
            "hint.insert_mode".to_string(),
            "Esc cancel | Enter execute".to_string(),
        );
        en.insert(
            "log.auto_scroll_on".to_string(),
            "Auto-scroll: ON ".to_string(),
        );
        en.insert(
            "log.auto_scroll_off".to_string(),
            "Auto-scroll: OFF".to_string(),
        );
        translations.insert("en".to_string(), en);

        let mut ja = HashMap::new();
        ja.insert("help.title".to_string(), "コマンド一覧".to_string());
        ja.insert(
            "help.up".to_string(),
            "/up [preset]   スタック全体を依存順に起動".to_string(),
        );
        ja.insert(
            "help.down".to_string(),
            "/down          全プロセスを安全に終了".to_string(),
        );
        ja.insert(
            "help.status".to_string(),
            "/status        状態を強制再確認して再描画".to_string(),
        );
        ja.insert(
            "help.logs".to_string(),
            "/logs <svc>    特定サービスのログをフィルタ表示".to_string(),
        );
        ja.insert(
            "help.cleanup".to_string(),
            "/cleanup       ゾンビプロセス・ポートを解放".to_string(),
        );
        ja.insert(
            "help.preset".to_string(),
            "/preset <mode> 起動モード切替 (minimal/full/headless)".to_string(),
        );
        ja.insert(
            "help.health".to_string(),
            "/health        全サービスのヘルスチェック実行".to_string(),
        );
        ja.insert(
            "help.config".to_string(),
            "/config        現在の構成を表示".to_string(),
        );
        ja.insert("help.quit".to_string(), "/quit, /q      TUI を終了".to_string());
        ja.insert(
            "help.keyboard_i".to_string(),
            "i            INSERTモード (コマンド入力)".to_string(),
        );
        ja.insert(
            "help.keyboard_esc".to_string(),
            "Esc          ダイアログ解除 / 入力クリア".to_string(),
        );
        ja.insert(
            "help.keyboard_enter".to_string(),
            "Enter        コマンド実行".to_string(),
        );
        ja.insert(
            "help.keyboard_ctrl_l".to_string(),
            "Ctrl+l       ログAuto-scroll 切替".to_string(),
        );
        ja.insert(
            "help.keyboard_arrows".to_string(),
            "←/→          ダイアログ選択".to_string(),
        );
        ja.insert("help.logs_clear".to_string(), "/logs          ログフィルタ解除".to_string());
        ja.insert(
            "help.q".to_string(),
            "q            TUI を終了 (NORMALモードのみ)".to_string(),
        );
        ja.insert(
            "help.slash".to_string(),
            "/            コマンド入力開始 (/ を事前入力)".to_string(),
        );
        ja.insert(
            "help.scroll".to_string(),
            "↑/↓/PgUp/PgDn  ログスクロール (auto-scroll OFF時)".to_string(),
        );
        ja.insert(
            "help.ctrl_c".to_string(),
            "Ctrl+c       強制終了 (全サービス停止)".to_string(),
        );
        ja.insert("help.keyboard_title".to_string(), "キーボード:".to_string());
        ja.insert(
            "dialog.hint".to_string(),
            "←/→ 選択 • Enter 確定 • Esc キャンセル".to_string(),
        );
        ja.insert("dialog.kill_it".to_string(), "Kill it".to_string());
        ja.insert("dialog.abort".to_string(), "Abort".to_string());
        ja.insert("dialog.ok".to_string(), "OK".to_string());
        ja.insert("status.run".to_string(), "Run".to_string());
        ja.insert("status.boot".to_string(), "Boot".to_string());
        ja.insert("status.dead".to_string(), "Dead".to_string());
        ja.insert("status.stopped".to_string(), "Stop".to_string());
        ja.insert("status.unknown".to_string(), "?".to_string());
        ja.insert("status.blocked".to_string(), "Blocked".to_string());
        ja.insert(
            "hint.normal_mode".to_string(),
            "/ command | i insert | q quit | ↑↓ scroll | ? help".to_string(),
        );
        ja.insert(
            "hint.insert_mode".to_string(),
            "Esc cancel | Enter execute".to_string(),
        );
        ja.insert(
            "log.auto_scroll_on".to_string(),
            "Auto-scroll: ON ".to_string(),
        );
        ja.insert(
            "log.auto_scroll_off".to_string(),
            "Auto-scroll: OFF".to_string(),
        );
        translations.insert("ja".to_string(), ja);

        Self {
            lang: lang.to_string(),
            translations,
        }
    }

    pub fn t(&self, key: &str) -> String {
        self.translations
            .get(&self.lang)
            .and_then(|map| map.get(key))
            .cloned()
            .unwrap_or_else(|| {
                self.translations
                    .get("en")
                    .and_then(|map| map.get(key))
                    .cloned()
                    .unwrap_or_else(|| key.to_string())
            })
    }

    pub fn set_lang(&mut self, lang: &str) {
        self.lang = lang.to_string();
    }
}
