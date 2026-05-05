# Agent Instructions for Unreal MCP Project

このプロジェクトは **Unreal Engine 5.7** をターゲットにしており、最新のAPI仕様とMCPブリッジ特有のアーキテクチャに厳格に従う必要があります。

## 1. Unreal Engine 5.7 API 準拠ルール

LLMの学習データに含まれる古いAPI（UE4やUE5.0〜5.3）は、5.7では廃止または非推奨になっていることが多いです。以下のルールを遵守してください。

- **実装前の事前調査を必須化:** 今後実装するすべての機能（プロジェクト設定、レベル管理、ビューポート操作等）について、実装を開始する前に必ず `google_web_search` 等を用いて **Unreal Engine 5.7** の最新API仕様、ヘッダー定義、非推奨情報を調査してください。
- **学習データの盲信禁止:** LLMの学習データにある古いAPI（UE4 / UE5初期）は、5.7では高確率でビルドエラーやクラッシュの原因になります。「動くはず」という思い込みを捨て、公式ドキュメントやGitHubのソースコードを正体としてください。
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

### D. 設定の反映と永続化の原則
- **UIとの同期:** `GConfig` で設定を書き換えただけでは、現在開いている「Project Settings」ウィンドウには反映されません。ユーザーに見える形で更新が必要な場合は、`ISettingsModule` を介して設定オブジェクトの `PostEditChangeProperty` を叩くか、`NotifyPostChange` を呼ぶことを検討してください。
- **即時反映:** レンダリング設定やスケーラビリティなどの「実行時に即座に変わるべき設定」は、`IConsoleManager` を介して `ExecuteConsoleCommand` で CVar (Console Variables) を操作するのが最も確実です。

### C. ヘッダーの選択
- **アセット保存:** `EditorLoadingAndSavingUtils.h` が見つからない場合は `FileHelpers.h` を使用してください。`FEditorLoadingAndSavingUtils` 名前空間の関数が最新です。

## 2. スレッド安全性（GameThread）の厳守

MCPブリッジは別スレッドでリクエストを受信しますが、**UEのエディタ操作やアセット操作は必ず GameThread で実行しなければなりません。**

- **ルール:** `EpicUnrealMCPBridge.cpp` の `ExecuteCommand` は既に `AsyncTask` を使って GameThread にディスパッチしていますが、コマンドハンドラ内で重い処理や、エンジンを不安定にする操作を行う際は特に注意してください。
- **PIEの停止:** `GEditor->EndPlayMap()` は即時実行のためクラッシュを誘発しやすいです。必ず `GEditor->RequestEndPlayMap()` を使用して、エンジンの次フレームで安全にクリーンアップさせてください。
- **非同期処理の待機:** アセットのインポートやレベルのビルドなど、エンジン側で非同期タスクが走る操作の後は、`FAssetCompilingManager::Get().FinishAllCompilation()` や `FlushRenderingCommands()` を呼び、状態が確定してからレスポンスを返してください。これを怠ると、AIが「完了した」と判断して次の操作を行った際に、未完了のアセットを参照してクラッシュする原因になります。

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

## 6. アセット操作とエクスポートのベストプラクティス

アセットのインポート・エクスポートや変換を行う際は、以下の「UE5.7流」の作法を遵守してください。

### A. タスクベースAPIの優先 (Task-based API)
- **非推奨:** `TObjectIterator<UExporter>` での検索や、`UExporter::ExportToFile` の直接呼び出し。
- **推奨:** `UAssetExportTask` / `UAssetImportTask` を作成し、`UExporter::RunAssetExportTask` を使用する。
  - **理由:** インポート/エクスポートの設定（Options）を統一的に扱え、エンジンが最適なエクスポーターを自動で解決してくれるため、CDO（クラスデフォルトオブジェクト）がメモリにない状態でも確実に動作します。

### B. エクスポーターの動的解決
- `UExporter::FindExporter(Asset, *Format)` を使用して、アセットとフォーマットに最適なエクスポーターを解決してください。手動でのクラスパス指定は避けるべきです。

### C. クラスパス指定の罠
- どうしても `StaticLoadClass` 等でクラスを指定する必要がある場合、`/Script/UnrealEd.UStaticMeshExporterFBX` のようにクラス名の頭に `U` を付けると失敗することがあります（UE5.7では `/Script/UnrealEd.StaticMeshExporterFBX` が正解の場合が多い）。

## 7. C++変更時の反映プロセス（重要）

プラグインのC++コード（`.cpp`, `.h`）や `Build.cs` を変更した際、エディタを起動したままだと変更が正しく反映されない、あるいは古いDLLがロードされ続けることがあります。

- **反映の手順:**
  1. Unreal Editor を完全に終了する。
  2. `scripts/launch-dev-stack.py` 等でプロセスが残っていないか確認（必要ならタスクマネージャーで殺す）。
  3. `Build.bat` を使用して手動ビルドを実行する。
     ```powershell
     & "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" FlopperamUnrealMCPEditor Win64 Development "-Project=..." -WaitMutex
     ```
  4. ビルド成功を確認してからエディタを再起動する。
- **注意:** エラーメッセージが「修正前のもの」と同じである場合は、ほぼ確実に「古いバイナリ」が動いています。

## 8. エディタ・レベル操作の安定性ガイドライン

### A. ワールド Partition とストリーミング
- **Cellのロード状態:** World Partition 制御時は、操作対象の Actor がロードされたセルに含まれているか常に確認してください。セルがアンロードされている状態で Actor を操作しようとすると、不正アクセスが発生します。
- **変更のマーク:** レベルやアセットをプログラムから変更した後は、必ず `Modify()` を呼び、パッケージを `SetDirtyFlag(true)` にして保存対象としてマークしてください。

### B. ビューポートとカメラ操作
- **エディタ専用:** Viewport 操作（カメラ移動など）は `GCurrentLevelEditingViewportClient` が有効な場合のみ実行してください。PIE中か編集時かで取得先が変わるため、`GEditor->GetActiveViewport()` 等の戻り値チェックは必須です。

### C. 標準的なレスポンス形式
- MCP経由でAIに情報を返す際は、以下のJSON構造を標準としてください。
  - `success`: bool (成功・失敗)
  - `error`: string (失敗時の詳細メッセージ、原因を具体的に)
  - `data`: object (戻り値がある場合。リスト取得なら `items` 配列など)
- **ヒントの提供:** 失敗した際、単に「Failed」と返すのではなく「○○の設定がオフになっている可能性があります」といった「AIが次に取るべき行動」のヒントをエラーメッセージに含めると、AIの自己修復能力が向上します。
