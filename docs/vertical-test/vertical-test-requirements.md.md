# 概要

tasks.mdにおける機能1~8＋10,11の機能実装完了による垂直テストの実施

## AI駆動 Playable Vertical Slice 生成テスト

## 1. テスト目的

本テストの目的は、MCP経由で実装されたUnreal Editor操作機能が、個別機能として動くだけでなく、**1つの制作ワークフローとして連携し、プレイ可能なミニゲームシーンを自動生成できるか**を検証することである。

今回の垂直テストでは、以下を重点確認する。

AIまたはMCPが、プロジェクト設定、プラグイン設定、アセット管理、外部アセット取り込み、レベル作成、World Partition/Data Layer、Static Mesh編集、Blueprint生成、Gameplay Framework設定、Enhanced Input、UMG UI、Material/Rendering設定、PIE起動、保存、再読み込みまでを一貫して実行できること。

つまり「UEエディタをAIが触れる」ではなく、**AIがUE制作パイプラインを縦に貫通できるか**を見る。

---

# 2. テストシナリオ概要

## シナリオ名

**VT-AAA-001：AI生成ミニゲーム “Extraction Room” 垂直制作テスト**

## 生成するゲーム内容

プレイヤーが小規模なSF施設内を移動し、3つのエネルギーコアを回収して、出口ゲートまで到達する一人称または三人称のミニゲームを生成する。

## 必須要素

プレイヤーキャラクター、移動入力、カメラ、収集アイテム、出口ゲート、簡易敵または障害物、HUD、スコア表示、残りアイテム数表示、クリア表示、ライト、マテリアル、衝突、LOD/Nanite、保存、再読み込み、PIE実行まで含める。

---

# 3. 成功条件の総合定義

この垂直テストは、以下を満たした場合に合格とする。

MCPがUEエディタを操作し、空に近いプロジェクトからプレイ可能な1レベルを生成できること。

PIE開始後、プレイヤーが移動でき、アイテムを回収でき、HUDが更新され、全アイテム回収後に出口へ到達するとクリア状態になること。

生成されたAsset、Blueprint、Level、Input Action、Input Mapping Context、Widget Blueprint、Material、Static Mesh設定、GameMode設定が保存され、エディタ再起動後も破損せず再利用できること。

最終状態で、重大なEditor Error、Blueprint Compile Error、Missing Reference、Redirector未処理、未保存Assetが残っていないこと。

---

# 4. 対象機能範囲

## 4.1 Project / Editor Control

このテストでは、Project Settings、Plugin、Maps & Modes、Rendering、Input、Collision、Navigation、Editorログ、PIE、保存処理を対象にする。

必須確認項目は以下。

プロジェクト設定を読み取れること。  
プロジェクト設定を変更できること。  
Default Map、Game Default Map、Editor Startup Mapを設定できること。  
必要Pluginを有効化できること。  
Plugin一覧を取得できること。  
Rendering設定を変更できること。  
Physics設定を変更できること。  
Input設定を変更できること。  
Collision設定を変更できること。  
Navigation System設定を変更できること。  
Maps & Modes設定を変更できること。  
World Settingsを取得・変更できること。  
Editorログを取得できること。  
PIE開始・停止ができること。  
Save Allができること。  
Dirty Asset一覧を取得できること。

## 合格条件

テスト終了時、Dirty Assetが0件であること。  
PIE開始時に重大エラーが発生しないこと。  
EditorログからError以上の問題を抽出し、重大度分類レポートを出せること。  
Default MapとGameMode設定が意図通り反映されていること。

---

# 5. 垂直テスト要求

## VT-001：プロジェクト初期構成テスト

### 目的

MCPがUEプロジェクトの基本設定を読み取り、制作に必要な設定を自動構成できるかを検証する。

### 操作要件

MCPは以下を実行する。

Project Description、Version、Company情報をテスト用に更新する。  
Default Map、Game Default Map、Editor Startup Mapを新規作成予定のテストレベルに設定する。  
Enhanced Input、Common UI、Modeling Tools、Navigation関連Pluginの状態を確認する。  
必要Pluginが無効な場合は有効化し、再起動が必要かを検出する。  
Rendering設定でLumen、Virtual Shadow Maps、Nanite利用前提の設定を確認する。  
Collision Presetを追加または確認する。  
Navigation System設定を確認する。

### 期待結果

プロジェクト設定がMCPから取得・変更できる。  
Maps & Modesに生成GameModeが設定される。  
必要Pluginの状態がレポート化される。  
再起動が必要な変更と即時反映可能な変更が区別される。  
設定変更後、Editorログに重大エラーが出ない。

### 失敗条件

Project Settingsの変更が保存されない。  
Default Mapが意図したLevelを参照しない。  
Plugin有効化後に状態を再確認できない。  
設定変更によってPIEが起動不能になる。

---

## VT-002：Content Browser / Asset管理テスト

### 目的

生成プロジェクトのAsset構造をMCPが管理できるかを検証する。

### 操作要件

以下のフォルダ構造を自動生成する。

`/Game/AI_VerticalTest/Maps`  
`/Game/AI_VerticalTest/Blueprints`  
`/Game/AI_VerticalTest/Blueprints/Gameplay`  
`/Game/AI_VerticalTest/Blueprints/UI`  
`/Game/AI_VerticalTest/Materials`  
`/Game/AI_VerticalTest/Meshes`  
`/Game/AI_VerticalTest/Textures`  
`/Game/AI_VerticalTest/Input`  
`/Game/AI_VerticalTest/Data`  
`/Game/AI_VerticalTest/DeveloperTrash`

さらに以下を実行する。

Asset一覧取得。  
Asset検索。  
Assetパス解決。  
Asset移動。  
Asset複製。  
Assetリネーム。  
Assetメタデータ付与。  
Assetタグ付け。  
依存Asset一覧取得。  
参照関係取得。  
Redirector検出。  
Redirector Fixup。  
未使用Asset検出。

### 期待結果

全Assetが指定フォルダ配下に整理される。  
一時AssetはDeveloperTrashに移動される。  
Redirectorが残らない。  
Missing Referenceが残らない。  
最終レポートに、Asset数、依存関係、未使用Asset、Redirector処理結果が出力される。

### 失敗条件

Asset移動後に参照が壊れる。  
BlueprintやMaterialが古いAssetパスを参照する。  
Redirector Fixup後も参照エラーが残る。  
未使用Asset検出ができない。

---

## VT-003：Asset Import / Export パイプラインテスト

### 目的

外部素材をUEプロジェクトへ取り込み、ゲーム内で利用可能な状態に変換できるかを検証する。

### 操作要件

テスト用に以下のアセットを取り込む。

FBXまたはOBJのStatic Mesh。  
PNG/JPGテクスチャ。  
Normal Map。  
Packed ORM Texture。  
WAV音声。  
任意でGLBまたはUSD。

Import時に以下を設定する。

Scale。  
Axis変換。  
Collision生成設定。  
LOD付きImportまたはLOD生成。  
Nanite有効化。  
Material自動生成。  
Texture圧縮設定。  
Normal Map判定。  
Reimport。  
Asset Export。

### 期待結果

取り込んだMeshがLevelに配置可能である。  
TextureがMaterialに接続可能である。  
Normal MapとORMが適切なTexture設定になっている。  
Nanite設定が有効化される。  
Reimport後も参照が壊れない。  
ExportしたMeshまたはLevelが指定パスに出力される。

### 失敗条件

ImportしたAssetがContent Browserに表示されない。  
ScaleやAxisが意図と異なる。  
Material参照が壊れる。  
ReimportでBlueprintやLevel上の参照が失われる。  
Texture設定が誤っていて見た目が破綻する。

---

## VT-004：Level / Map / World Partition テスト

### 目的

MCPが新規Levelを作成し、保存し、World PartitionやData Layerを使って管理できるかを検証する。

### 操作要件

新規Level `LV_ExtractionRoom_VT` を作成する。  
Persistent Levelとして保存する。  
World Partitionを有効化する。  
World Partition Grid設定を行う。  
Data Layerを作成する。  
以下のData Layerを作る。

`DL_Gameplay`  
`DL_Environment`  
`DL_Lighting`  
`DL_Debug`  
`DL_ImportedAssets`

ActorをData Layerに振り分ける。  
Level Boundsを設定する。  
World Settingsを変更する。  
Navigation設定を有効にする。  
HLOD Layerを作成する。  
HLOD生成または再ビルドを実行する。  
One File Per Actor設定を確認する。

### 期待結果

Levelが作成・保存・再読み込みできる。  
Data LayerごとにActorを表示/非表示できる。  
World Partition Cell情報を取得できる。  
HLOD生成処理が失敗しない。  
Level再読み込み後もActor配置、Data Layer所属、World Settingsが維持される。

### 失敗条件

Level保存後に再読み込みできない。  
Data Layer所属が消える。  
World PartitionのCell情報が取得できない。  
HLOD生成でEditorがクラッシュする。  
Persistent LevelとSublevel/Data Layerの管理が混線する。

---

## VT-005：Static Mesh / Mesh Editing テスト

### 目的

配置したStatic Mesh Actorだけでなく、Static Mesh Assetそのものの編集・最適化・衝突設定ができるかを検証する。

### 操作要件

以下のMesh Assetを作成またはImportする。

床。  
壁。  
柱。  
出口ゲート。  
収集アイテム台座。  
障害物。  
装飾メッシュ。

それぞれに対して以下を設定する。

Collision生成。  
Collision Complexity設定。  
Simple Collision追加。  
LOD生成。  
LOD設定変更。  
Nanite有効/無効。  
Lightmap UV生成。  
Lightmap Resolution設定。  
Socket追加。  
Pivot変更。  
Bounds確認。  
Mesh Merge。  
Mesh Simplify。  
Vertex Color Paintまたはテスト用Vertex Color付与。

### 期待結果

プレイヤーが床・壁・障害物と正しく衝突する。  
収集アイテムにはOverlap Collisionが設定される。  
出口ゲートにはTrigger VolumeまたはOverlap判定がある。  
Nanite対象と非対象を区別できる。  
LOD設定がAsset上で確認できる。  
Socketを利用してライトやVFXの装着位置を制御できる。  
Mesh Merge後もMaterial Slotが破綻しない。

### 失敗条件

床をすり抜ける。  
壁の衝突がない。  
Overlapが発火しない。  
LOD生成後にMeshが消える。  
Nanite設定変更が保存されない。  
Lightmap UV生成でエラーが出る。

---

## VT-006：Materials / Rendering テスト

### 目的

Material Graph作成だけでなく、Material Instance、Rendering設定、Lumen、Post Processまで含めた見た目の制御ができるかを検証する。

### 操作要件

以下のMaterialを作成する。

`M_Floor_Tech`  
`M_Wall_Panel`  
`M_Core_Emissive`  
`M_Gate_Locked`  
`M_Gate_Unlocked`  
`M_Debug_Color`  
`MI_Floor_Tech_01`  
`MI_Core_Red`  
`MI_Core_Blue`  
`MI_Core_Green`

Material Graphに以下を含める。

Base Color。  
Roughness。  
Metallic。  
Normal Map。  
Emissive Color。  
Scalar Parameter。  
Vector Parameter。  
Texture Parameter。  
Static Switch Parameter。

さらに以下を実行する。

Material Instance Constant作成。  
Scalar/Vector/Texture Parameter編集。  
Material Parameter Collection作成。  
RuntimeまたはEditor上で色変更。  
Post Process Volume作成。  
Bloom、Exposure、Color Grading設定。  
Lumen有効/無効確認。  
Virtual Shadow Maps設定確認。  
Shader Compile状態取得。

### 期待結果

アイテムはEmissiveで発光する。  
未回収時と回収後でMaterialが変化する。  
出口ゲートはロック時とアンロック時で見た目が変わる。  
Material InstanceのParameter変更が保存される。  
Shader Compileが完了し、未コンパイル状態が残らない。  
PIE中にDynamic Materialの色変更が反映される。

### 失敗条件

Material Graphが壊れてCompile Errorになる。  
Texture ParameterがMissingになる。  
Material Instanceが親Materialを失う。  
Shader Compileが完了しない。  
PIE中のMaterial変更が反映されない。

---

## VT-007：Blueprint 基本機能テスト

### 目的

Blueprint作成、Component追加、Graph Node追加、接続、変数、イベント、関数、Interface、Dispatcherなどを組み合わせてGameplayを成立させられるかを検証する。

### 操作要件

以下のBlueprintを作成する。

`BP_VT_GameMode`  
`BP_VT_GameState`  
`BP_VT_PlayerController`  
`BP_VT_Character`  
`BP_VT_CorePickup`  
`BP_VT_ExitGate`  
`BP_VT_Hazard`  
`BP_VT_HUDController`  
`BPI_VT_Interactable`  
`BFL_VT_GameplayHelpers`  
`E_VT_GameState`  
`S_VT_RunResult`

Blueprintには以下を含める。

Component追加。  
変数作成。  
関数作成。  
Event Dispatcher作成。  
Interface実装。  
Construction Script編集。  
Event BeginPlay。  
Overlap Event。  
Input Event。  
Widget生成。  
Material切替。  
Gate開放処理。  
Clear判定。  
コメントノード。  
Reroute Node整理。  
Graph自動整列。  
Blueprint Compile。  
Blueprint Diff取得。  
Breakpoint設定またはDebug情報取得。

### 期待結果

全BlueprintがCompile Errorなしで保存される。  
PickupがOverlap時に回収される。  
GameStateが回収数を保持する。  
ExitGateが全回収後に開く。  
HUDがGameState変更に追従する。  
Interface経由でInteract処理を呼べる。  
Event DispatcherでUI更新が通知される。

### 失敗条件

Blueprint Compile Errorが1件でも残る。  
Node接続が不完全。  
変数型が不一致。  
Interface実装が呼ばれない。  
Dispatcher Bindingが機能しない。  
PIE中にBlueprint Runtime Errorが出る。

---

## VT-008：Gameplay Framework テスト

### 目的

GameMode、GameState、PlayerController、Character、HUD、GameInstanceなど、UEのGameplay FrameworkをMCPが構成できるかを検証する。

### 操作要件

以下を作成・設定する。

GameMode Blueprint。  
GameState Blueprint。  
PlayerState Blueprint。  
PlayerController Blueprint。  
Character Blueprint。  
Default Pawn設定。  
HUD Class設定。  
Player Start配置。  
Spawn Rule設定。  
Possess設定。  
Camera Component設定。  
Spring Arm設定。  
SaveGame Class作成。  
GameInstance作成。  
Gameplay Tags設定。  
Gameplay Tags追加。  
Gameplay Tag Query作成。

### 期待結果

PIE開始時、指定CharacterがSpawnする。  
PlayerControllerがCharacterをPossessする。  
Cameraが正しく追従する。  
GameModeが生成Levelに設定される。  
GameStateがスコアと回収数を管理する。  
SaveGameにクリア時間や回収数を保存できる。  
Gameplay TagsでPickup、Gate、Hazardを分類できる。

### 失敗条件

Default Pawnが意図せずNoneになる。  
PlayerControllerがPossessしない。  
Cameraが原点固定になる。  
GameModeがProject Settingsに反映されない。  
SaveGameが保存・読み込みできない。

---

## VT-009：Enhanced Input テスト

### 目的

現在標準のEnhanced Inputを使って、移動・視点・ジャンプ・インタラクト・ポーズ操作を構成できるかを検証する。

### 操作要件

以下を作成する。

`IA_Move`  
`IA_Look`  
`IA_Jump`  
`IA_Interact`  
`IA_Pause`  
`IMC_DefaultPlayer`

Input Mapping Contextに以下を登録する。

WASD移動。  
左スティック移動。  
Mouse Look。  
右スティックLook。  
Space Jump。  
E Interact。  
Escape Pause。

以下も設定する。

Dead Zone。  
Swizzle Axis。  
Negate。  
Pressed/Released。  
HoldまたはTap Trigger。  
Runtime Mapping Context追加。  
PlayerControllerまたはCharacterへのBinding生成。  
Input Debug情報取得。

### 期待結果

キーボードとゲームパッドの両方で移動できる。  
Mouse Lookが動作する。  
Interactキーで近接対象に反応できる。  
PauseキーでUIが表示される。  
Input Mapping ContextがBeginPlay時に追加される。  
Input Debug情報でAction発火が確認できる。

### 失敗条件

Input Actionが発火しない。  
Mapping Contextが追加されない。  
Axisが反転または入れ替わる。  
Game/UI Input Mode切替後に操作不能になる。  
Pause後にゲームへ復帰できない。

---

## VT-010：UI / UMG / Common UI テスト

### 目的

Widget Blueprintを生成し、Gameplay状態と連動するHUD、Pause Menu、Clear画面を作成できるかを検証する。

### 操作要件

以下のWidget Blueprintを作成する。

`WBP_VT_HUD`  
`WBP_VT_PauseMenu`  
`WBP_VT_ClearScreen`  
`WBP_VT_SettingsMenu`

HUDには以下を配置する。

Canvas Panel。  
Text Block。  
Progress Bar。  
Image。  
Vertical Box。  
Horizontal Box。  
Button。  
Border。

以下を実装する。

残りCore数表示。  
回収Progress表示。  
クリアタイム表示。  
Pause Menu表示/非表示。  
Resume Button。  
Restart Button。  
Quit Button。  
Button OnClicked Binding。  
Widget Animation。  
Viewport追加。  
Remove From Parent。  
UI変数Binding。  
Input Mode Game/UI設定。  
Mouse Cursor表示制御。

### 期待結果

PIE開始時にHUDが表示される。  
Core回収時に残り数とProgress Barが更新される。  
Pause時にPause Menuが表示され、マウスカーソルが表示される。  
ResumeでGame Inputに戻る。  
Clear時にClear Screenが表示される。  
UIが保存・再読み込み後も壊れない。

### 失敗条件

WidgetがViewportに追加されない。  
Bindingが更新されない。  
Button OnClickedが発火しない。  
Input Mode切替で操作不能になる。  
Widget参照がMissingになる。

---

## VT-011：Gameplay統合テスト

### 目的

Level、Blueprint、Input、UI、Material、Collisionが連動し、ゲームとして成立するかを検証する。

### テスト手順

PIEを開始する。  
プレイヤーがPlayer StartからSpawnする。  
WASDまたはGamepadで移動する。  
3つのCoreを回収する。  
HUDの残り数が3→2→1→0に変化する。  
Core回収時に音またはMaterial変化が発生する。  
全Core回収後、ExitGateのMaterialがLockedからUnlockedに変化する。  
ExitGateのCollisionまたは状態が変更される。  
プレイヤーがExitGateに到達する。  
Clear Screenが表示される。  
SaveGameに結果が保存される。  
PIEを停止する。

### 期待結果

最初から最後まで手動プレイでクリア可能。  
Runtime Errorが発生しない。  
HUDとGameStateの値が一致する。  
Gateの状態遷移が正しい。  
Clear後、入力状態とUI状態が破綻しない。

### 失敗条件

クリア不能。  
回収数がズレる。  
HUDが更新されない。  
Gateが開かない。  
Overlapが複数回発火してスコアが壊れる。  
PIE停止時にEditorが不安定になる。

---

## VT-012：保存・再起動・再現性テスト

### 目的

生成されたプロジェクト状態が保存され、Editor再起動後も再現可能かを検証する。

### 操作要件

Save Allを実行する。  
Dirty Asset一覧を取得する。  
Editorを閉じる。  
Editorを再起動する。  
Default Mapを開く。  
全BlueprintをCompileする。  
Asset Referenceを検査する。  
PIEを再実行する。  
同じゲームフローを再度クリアする。

### 期待結果

Dirty Assetが0件。  
再起動後にLevelが開ける。  
Missing Assetがない。  
Blueprint Compile Errorがない。  
PIEが再実行できる。  
前回と同じ手順でクリアできる。

### 失敗条件

再起動後にAsset参照が壊れる。  
Levelが開けない。  
Default Mapが未設定に戻る。  
Blueprintが再Compileで失敗する。  
SaveGameやUIが壊れる。

---

## VT-013：Undo / Redo / 差分復旧テスト

### 目的

MCP操作が失敗した場合や変更を戻したい場合に、Undo/Redoや差分復旧が使えるかを検証する。

### 操作要件

意図的に以下を実施する。

Actorを配置する。  
Materialを変更する。  
Blueprint変数を追加する。  
Widget位置を変更する。  
Undoを実行する。  
Redoを実行する。  
変更前後の差分を記録する。  
失敗した変更だけを再適用する。

### 期待結果

Undo後、Actor/Material/Blueprint/UIの状態が戻る。  
Redo後、同じ変更が復元される。  
差分ログに変更対象、変更前、変更後が記録される。  
MCPが「どこまで成功したか」を把握できる。

### 失敗条件

UndoでEditor状態が不整合になる。  
Redoで参照が壊れる。  
差分ログが取れない。  
失敗箇所を特定できない。

---

## VT-014：負荷・大量操作テスト

### 目的

MCPが多数のAsset、Actor、Blueprint、Materialを扱った場合に破綻しないかを検証する。

### 操作要件

以下を自動生成する。

Static Mesh Actor 300個以上。  
Material Instance 50個以上。  
Pickup Actor 30個以上。  
Decorative Actor 200個以上。  
Data Layer 5個以上。  
Blueprint 20個以上。  
Widget 5個以上。  
Input Action 10個以上。

さらに以下を実行する。

一括Asset検索。  
一括Rename。  
一括Move。  
依存関係取得。  
Reference Viewer相当のレポート生成。  
未使用Asset検出。  
Redirector Fixup。  
Save All。  
PIE起動。

### 期待結果

大量操作後もEditorがクラッシュしない。  
Asset移動後も参照が壊れない。  
PIE開始まで到達できる。  
Editorログに致命的エラーがない。  
処理時間がレポート化される。

### 合格目安

Asset操作成功率：99%以上。  
Blueprint Compile成功率：100%。  
PIE起動成功：必須。  
Missing Reference：0件。  
Redirector残存：0件。  
Dirty Asset：0件。

---

# 6. 高難易度チェックポイント

このテストで特に見るべきポイントは以下。

Assetの参照破壊が起きないか。  
Blueprint Graph生成が複雑化してもCompileできるか。  
Input、UI、GameState、Actorがイベント経由で正しく連動するか。  
Editor再起動後も状態が維持されるか。  
Import、Reimport、Move、Rename、Fixup後に破綻しないか。  
World Partition/Data Layer/HLODを使ってもLevel管理が壊れないか。  
Material InstanceとDynamic Materialの両方を扱えるか。  
PIE中とEditor中の状態差分を区別できるか。  
Undo/Redoや失敗復旧ができるか。  
最終的に「人間がプレイしてクリアできる」状態になるか。

ここが通れば、かなり強いです。  
単にUEのAPIを叩けるだけではなく、**制作フローそのものを制御できている**と言える。

---

# 7. 成果物要件

テスト完了時、MCPは以下を出力する。

## 7.1 生成物

`LV_ExtractionRoom_VT`  
`BP_VT_GameMode`  
`BP_VT_GameState`  
`BP_VT_PlayerController`  
`BP_VT_Character`  
`BP_VT_CorePickup`  
`BP_VT_ExitGate`  
`WBP_VT_HUD`  
`WBP_VT_PauseMenu`  
`WBP_VT_ClearScreen`  
`IA_Move`  
`IA_Look`  
`IA_Jump`  
`IA_Interact`  
`IMC_DefaultPlayer`  
各種Material / Material Instance  
Import済みMesh / Texture / Audio  
SaveGame Class  
Gameplay Tags  
Data Layer  
HLOD Layer

## 7.2 レポート

Project設定変更レポート。  
Plugin状態レポート。  
Asset一覧レポート。  
依存関係レポート。  
Redirector Fixup結果。  
Blueprint Compile結果。  
Editorログ解析結果。  
PIE実行結果。  
Input動作確認結果。  
UI動作確認結果。  
Collision/Overlap確認結果。  
保存・再起動テスト結果。  
失敗箇所一覧。  
再実行可能なMCPコマンド履歴。

---

# 8. 判定基準

## S判定

全機能が通り、PIEでクリア可能。  
再起動後も再現可能。  
Missing Reference、Blueprint Error、Editor Critical Errorが0。  
Asset整理、依存関係、Redirector Fixupまで完了。  
自動レポートが十分に詳細。

## A判定

ゲームとしてクリア可能。  
一部軽微なWarningはあるが、保存・再起動・PIEに問題なし。  
UI、Input、Gameplay、Materialの主要連携が動作する。

## B判定

Level生成とPIE起動は可能。  
ただしUI更新、SaveGame、HLOD、Data Layerなど一部に不備あり。  
手動修正すればゲームとして成立する。

## C判定

AssetやBlueprintは生成されるが、PIEで正常プレイできない。  
Input、Collision、GameMode設定などに重大な不整合がある。

## F判定

Levelが開けない。  
Blueprint Compile Errorが大量発生。  
PIE起動不能。  
Asset参照破壊。  
保存・再起動に耐えない。

---

# 9. 最重要の受け入れ条件

最終的な受け入れ条件はこれです。

**MCPに対して「小規模な収集型ゲームを作って」と指示したとき、UEプロジェクト設定から、Asset取り込み、Level構築、Blueprint Gameplay、Input、UI、Material、Lighting、保存、PIE実行、再起動後の再現まで、破綻なく完遂できること。**

これを満たしかつS判定を取れるまで実装を行ってください。