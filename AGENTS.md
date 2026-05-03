# Agent Instructions for Unreal MCP Project

このプロジェクトは **Unreal Engine 5.7** をターゲットにしており、最新のAPI仕様とMCPブリッジ特有のアーキテクチャに厳格に従う必要があります。

## 1. Unreal Engine 5.7 API 準拠ルール

LLMの学習データに含まれる古いAPI（UE4やUE5.0〜5.3）は、5.7では廃止または非推奨になっていることが多いです。以下のルールを遵守してください。

- **不明な点は必ず検索:** 少しでもAPIの挙動や存在に疑問がある場合は、必ず `google_web_search` 等を用いて最新のドキュメント、フォーラム、またはGitHub上のUE5.7ソースコードを調査してください。学習データのみに頼った実装は、ほぼ確実にコンパイルエラーを引き起こします。
- **設定保存の最新化**
- **非推奨:** `UpdateDefaultConfigFile()`
- **推奨:** `TryUpdateDefaultConfigFile()`
  - 理由: 5.7では古いメソッドはコンパイル警告またはエラーになります。

### B. プロパティへの直接アクセスの回避
- **現象:** `UGameMapsSettings` の `GameDefaultMap` など、以前はパブリックだったメンバが `private` になっている場合があります。
- **対策:** プロパティに直接アクセスできない場合は、`GConfig->SetString()` を使用して `GEngineIni` に直接書き込み、`GConfig->Flush()` を呼ぶ手法をとってください。
  ```cpp
  GConfig->SetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), *MapPath, GEngineIni);
  GConfig->Flush(false, GEngineIni);
  ```

### C. ヘッダーの選択
- **アセット保存:** `EditorLoadingAndSavingUtils.h` が見つからない場合は `FileHelpers.h` を使用してください。`FEditorLoadingAndSavingUtils` 名前空間の関数が最新です。

## 2. スレッド安全性（GameThread）の厳守

MCPブリッジは別スレッドでリクエストを受信しますが、**UEのエディタ操作やアセット操作は必ず GameThread で実行しなければなりません。**

- **ルール:** `EpicUnrealMCPBridge.cpp` の `ExecuteCommand` は既に `AsyncTask` を使って GameThread にディスパッチしていますが、コマンドハンドラ内で重い処理や、エンジンを不安定にする操作を行う際は特に注意してください。
- **PIEの停止:** `GEditor->EndPlayMap()` は即時実行のためクラッシュを誘発しやすいです。必ず `GEditor->RequestEndPlayMap()` を使用して、エンジンの次フレームで安全にクリーンアップさせてください。

## 3. モジュール依存関係の管理

新しいツールを実装する際は、`Plugins/UnrealMCP/Source/UnrealMCP/UnrealMCP.Build.cs` に必要なモジュールが追加されているか確認してください。

- **Project Settings 操作:** `EngineSettings` モジュールが必須です。
- **Blueprint 操作:** `UnrealEd`, `BlueprintGraph`, `KismetCompiler` が必要です。

## 4. コマンド実装の整合性

新機能を追加する際は、以下の3箇所をセットで更新してください。

1.  **C++ ハンドラ:** `EpicUnrealMCPProjectEditorCommands.cpp` 等にロジックを追加。
2.  **C++ ルーター:** `EpicUnrealMCPBridge.cpp` の `RouteCommand` にコマンド名を登録。
3.  **Python ツール:** `Python/server/` 内の対応するツール定義に `action` を追加。

## 5. テスト駆動の推奨

変更を加えた後は、必ず `Python/tests/integration/test_project_editor_tools.py` のような統合テストを実行（または作成）してください。
- UEエディタを `scripts/launch-dev-stack.py --unreal` で起動。
- `python Python/tests/integration/your_test.py` で検証。
- **「コンパイルが通る」ことと「UEがクラッシュしない」ことは別物**であることを常に意識してください。
