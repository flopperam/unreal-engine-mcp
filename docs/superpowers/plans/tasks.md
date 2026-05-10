## 1. Project / Editor 基本操作

Unreal Editor自体にはProject Settings、Plugin管理、Editor Preference、World Settingsなどがありますが、現MCPはほぼ「レベル内のActorやAsset操作」に寄っています。UE公式ドキュメントでも、プロジェクト設定・制作パイプライン・エディタ自動化は独立した領域として扱われています。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-7-documentation?utm_source=chatgpt.com "Unreal Engine 5.7 Documentation"))

```md
## Project / Editor Control
- [x] プロジェクト設定の読み取り
- [x] プロジェクト設定の変更
- [x] Default Map設定
- [x] Game Default Map設定
- [x] Editor Startup Map設定
- [x] Project Description / Version / Company情報編集
- [x] Plugin有効化 / 無効化
- [x] Plugin一覧取得
- [x] Engine Scalability設定
- [x] Rendering設定変更
- [x] Physics設定変更
- [x] Input設定変更
- [x] Collision設定変更
- [x] AI System設定変更
- [x] Navigation System設定変更
- [x] Packaging設定変更
- [x] Maps & Modes設定変更
- [x] World Settings取得
- [x] World Settings変更
- [x] Editor Utility Widget作成
- [x] Editor Utility Blueprint作成
- [x] Editor Python Script実行
- [x] Editor Commandlet実行
- [x] Undo / Redo制御
- [x] Dirty Asset一覧取得
- [x] Save All
- [x] 特定Asset保存
- [x] Editorログ取得
- [x] PIE開始
- [x] PIE停止
- [x] Standalone Game起動
- [x] Simulate開始
- [x] Viewport操作
- [x] カメラ位置取得
- [x] カメラ位置設定
```

---

## 2. Level / World / Map管理

今のMCPはActorをレベルに置くのは強いです。ただし、**レベルそのものを作る・保存する・Sublevelに分ける・World Partitionで管理する**部分が弱いです。World Partitionは大規模ワールドをグリッドセルで分割・ストリーミングするUEの中核機能です。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/world-partition-builder-commandlet-reference "World Partition Builder Commandlet Reference | Unreal Engine 5.7 Documentation | Epic Developer Community"))

```md
## Level / Map Management
- [x] 新規Level作成
- [x] Level保存
- [x] Level読み込み
- [x] Level複製
- [x] Levelリネーム
- [x] Level削除
- [x] Persistent Level管理
- [x] Sublevel追加
- [x] Sublevel削除
- [x] Sublevel表示 / 非表示
- [x] Sublevelロード / アンロード
- [x] Level Streaming Volume作成
- [x] Level Streaming設定
- [x] World Partition有効化
- [x] World Partition Grid設定
- [x] World Partition Cell情報取得
- [x] World Partition Cellロード
- [x] World Partition Cellアンロード
- [x] Data Layer作成
- [x] Data LayerにActor追加
- [x] Data LayerからActor削除
- [x] Data Layer有効 / 無効切替
- [x] HLOD Layer作成
- [x] HLOD生成
- [x] HLOD再ビルド
- [x] One File Per Actor設定
- [x] Level Bounds管理
- [x] World Origin Rebasing設定
```

---

## 3. Content Browser / Asset管理

ここがかなり大きな穴です。  
今は既存Assetを参照して使うことはできますが、**Content Browserを本格操作する機能がない**。広くUEを使うなら、ここは最優先級です。素材を取り込めないエディタ自動化は、冷蔵庫のない料理人です。気合いだけで飢えます。

```md
## Content Browser / Asset Management
- [x] フォルダ作成
- [x] フォルダ削除
- [x] Asset一覧取得
- [x] Asset検索
- [x] Assetパス解決
- [x] Asset移動
- [x] Assetコピー
- [x] Asset複製
- [x] Assetリネーム
- [x] Asset削除
- [x] Asset保存
- [x] Assetロード
- [x] Assetアンロード
- [x] Assetメタデータ取得
- [x] Assetメタデータ編集
- [x] Assetタグ付け
- [x] Redirector検出
- [x] Redirector Fixup
- [x] 未使用Asset検出
- [x] 参照関係取得
- [x] 依存Asset一覧取得
- [x] Asset Reference Viewer相当
- [x] Asset Audit相当
- [x] Primary Asset Label作成
- [x] Asset Manager設定
- [x] Asset Registry検索
- [x] Bulk Rename
- [x] Bulk Move
- [x] Bulk Delete
```

---

## 4. Asset Import / Export

広くUEを使うなら、これは絶対に必要です。  
現状は「既にUE内にあるアセットを使う」前提が強いです。外部素材の取り込みが弱い。

```md
## Asset Import / Export
- [x] FBX Static Mesh Import
- [x] FBX Skeletal Mesh Import
- [x] GLTF / GLB Import
- [x] OBJ Import
- [x] USD Import
- [x] Texture Import PNG
- [x] Texture Import JPG
- [x] Texture Import EXR
- [x] Texture Import HDR
- [x] Normal Map Import設定
- [x] ORM / Packed Texture設定
- [x] WAV Import
- [x] MP3 / OGG Import
- [ ] Animation FBX Import
- [x] Alembic Import
- [x] Datasmith Import
- [x] Reimport
- [x] Import設定Preset
- [x] Import時のScale / Axis / Collision設定
- [x] LOD付きStatic Mesh Import
- [x] Nanite有効化Import
- [x] Material自動生成Import
- [x] Texture圧縮設定
- [x] Asset Export
- [x] Level Export
- [x] Mesh Export
- [x] Screenshot Export
```

---

## 5. Static Mesh / Mesh Editing

現状はStaticMeshActorを置けます。  
でも、Static Mesh Assetそのものを編集する機能はほぼありません。

```md
## Static Mesh / Mesh Editing
- [x] Static Mesh Actor配置
- [x] Static Mesh Asset詳細取得
- [x] Static Mesh Collision生成
- [x] Collision Complexity設定
- [x] Simple Collision追加
- [x] UCX Collision Import制御
- [x] LOD生成
- [x] LOD設定変更
- [x] Nanite有効 / 無効
- [x] Nanite Fallback設定
- [x] Lightmap UV生成
- [x] Lightmap Resolution設定
- [x] Mesh Bounds編集
- [x] Socket追加
- [x] Socket削除
- [x] Socket Transform変更
- [x] Pivot変更
- [x] Mesh Merge
- [x] Mesh Simplify
- [~] Mesh Bake
- [x] Modeling Mode機能呼び出し
- [x] Poly Edit
- [~] Boolean
- [x] Remesh
- [~] Voxel Remesh
- [x] UV Unwrap
- [x] UV Layout
- [x] Vertex Color Paint
```

---

## 6. Blueprint 基本機能の不足

Blueprint Graph操作はかなり強いです。  
ただし、Blueprint Editor全体から見ると、まだ未対応が多いです。

```md
## Blueprint - Missing / Partial
- [x] Blueprint作成
- [x] Component追加
- [x] Graph Node追加
- [x] Node接続
- [x] Blueprint Interface作成
- [x] Blueprint Interface実装
- [x] Blueprint Macro Library作成
- [x] Blueprint Function Library作成
- [x] Enum作成
- [x] Struct作成
- [x] User Defined Struct編集
- [x] User Defined Enum編集
- [x] Blueprint継承関係変更
- [x] Parent Class変更
- [x] Blueprint Class Settings編集
- [x] Blueprint Class Defaults編集
- [x] Blueprint Component Defaults編集
- [x] Construction Script詳細編集
- [x] Event Dispatcher作成
- [x] Event Dispatcher Binding
- [x] Timeline Curve編集
- [ ] Latent Node制御
- [x] Macro作成
- [x] Collapsed Graph作成
- [x] コメントノード作成
- [x] Reroute Node整理
- [x] Graph自動整列
- [x] Blueprint Diff
- [x] Blueprint Debug情報取得
- [x] Breakpoint設定
- [x] Watch変数設定
- [ ] Blueprint Profiler連携
```

---

## 7. Gameplay Framework

ここは未実装寄りです。  
UEのGameplay Frameworkは、GameMode、GameState、PlayerController、PlayerState、Pawn、Cameraなどを含むゲーム構築の根幹です。公式でもこれらはコアシステムとして説明されています。([Epic Games Developers](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine?utm_source=chatgpt.com "Gameplay Framework in Unreal engine"))

```md
## Gameplay Framework
- [x] GameMode Blueprint作成
- [x] GameMode C++ Class作成
- [x] Default GameMode設定
- [x] GameState作成
- [x] PlayerState作成
- [x] PlayerController作成
- [x] AIController作成
- [x] Pawn作成
- [x] Character作成
- [x] Default Pawn設定
- [x] HUD Class設定
- [x] Spectator Pawn設定
- [x] Player Start配置
- [x] Spawn Rule設定
- [x] Possess設定
- [x] Camera Manager設定
- [x] Camera Component設定
- [x] Spring Arm設定
- [x] SaveGame Class作成
- [x] GameInstance作成
- [x] GameInstance Subsystem作成
- [x] World Subsystem作成
- [x] Local Player Subsystem作成
- [x] Gameplay Tags設定
- [x] Gameplay Tags追加
- [x] Gameplay Tag Query作成
```

---

## 8. Enhanced Input

完全に足りていません。  
Enhanced InputはInput ActionやInput Mapping Contextを使う現在の標準入力システムです。公式ドキュメントでもPluginとして独立説明されています。([Epic Games Developers](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine?utm_source=chatgpt.com "Enhanced Input in Unreal Engine"))

```md
## Enhanced Input
- [x] Input Action作成
- [x] Input Mapping Context作成
- [x] Key Mapping追加
- [x] Key Mapping削除
- [x] Trigger設定
- [x] Modifier設定
- [x] Dead Zone設定
- [x] Swizzle Axis設定
- [x] Negate設定
- [x] Hold / Tap / Pressed / Released設定
- [x] Gamepad Mapping
- [x] Mouse Mapping
- [x] Keyboard Mapping
- [x] Runtime Mapping Context追加
- [x] Runtime Mapping Context削除
- [x] PlayerControllerへのBinding生成
- [x] Character BlueprintへのBinding生成
- [x] Input Debug情報取得
- [x] Rebind UI連携
- [x] Local Multiplayer用Input設定
```

---

## 9. Networking / Multiplayer

C++側にネットワーク関連クラスはありますが、MCPツールとして広く制御できているとは言いにくいです。UEのNetworking/MultiplayerはReplication、RPC、Actor relevancy、Irisなどを含む大領域です。([Epic Games Developers](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-and-multiplayer-in-unreal-engine?application_version=5.7 "Networking and Multiplayer in Unreal Engine | Unreal Engine 5.7 Documentation | Epic Developer Community"))

```md
## Networking / Multiplayer
- [~] 一部C++ Network Componentあり
- [~] Blueprint変数Replication設定の一部あり
- [ ] Actor Replicates設定
- [ ] Component Replicates設定
- [ ] Replicate Movement設定
- [ ] Net Dormancy設定
- [ ] Net Cull Distance設定
- [ ] Owner Only See / Only Owner関連設定
- [ ] RPC Server Function作成
- [ ] RPC Client Function作成
- [ ] RPC Multicast Function作成
- [ ] Reliable / Unreliable設定
- [ ] RepNotify生成
- [ ] Replicated変数一覧取得
- [ ] Network Prediction設定
- [ ] Dedicated Server設定
- [ ] Listen Server起動
- [ ] Client起動
- [ ] Multi-PIE設定
- [ ] Online Subsystem設定
- [ ] Session作成
- [ ] Session検索
- [ ] Session参加
- [ ] Iris Replication設定
- [ ] Replication Graph設定
- [ ] Bandwidth Profiling
- [ ] Network Profiler連携
```

---

## 10. UI / UMG / Common UI

現状ほぼ未実装です。  
UMGはWidget Blueprint、Canvas、Button、Text、Progress Barなどを使ってUIを作る仕組みです。UE公式のQuick StartでもWidget Blueprint作成が基本手順になっています。([Epic Games Developers](https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-quick-start-guide?application_version=4.27&utm_source=chatgpt.com "UMG UI Designer Quick Start Guide | Unreal Engine 4.27 ..."))

```md
## UI / UMG
- [x] Widget Blueprint作成
- [x] User Widget作成
- [x] Canvas Panel追加
- [x] Vertical Box追加
- [x] Horizontal Box追加
- [x] Overlay追加
- [x] Border追加
- [x] Button追加
- [x] Text Block追加
- [x] Image追加
- [x] Progress Bar追加
- [x] Slider追加
- [x] Check Box追加
- [x] Combo Box追加
- [x] Scroll Box追加
- [x] Uniform Grid追加
- [x] Widget Anchor設定
- [x] Widget Position設定
- [x] Widget Size設定
- [x] Widget Alignment設定
- [x] Font設定
- [x] Color設定
- [x] Brush設定
- [x] Style設定
- [x] Button OnClicked Binding
- [x] Widget Animation作成
- [x] HUDとしてViewport追加
- [x] Remove From Parent
- [x] UI変数Binding
- [x] Health Bar Binding
- [x] Score Text Binding
- [x] Main Menu生成
- [x] Pause Menu生成
- [x] Settings Menu生成
- [x] Dialogue UI生成
- [x] Inventory UI生成
- [x] Common UI Plugin対応
- [x] Input Mode Game/UI設定
- [x] Mouse Cursor表示制御
```

---

## 11. Materials / Rendering

Material Graphは強いですが、Rendering全体はまだ薄いです。  
Lumenは動的GIと反射、Post Processは露出・ブルーム・カラーグレーディングなどに関わります。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine?utm_source=chatgpt.com "Lumen Global Illumination and Reflections in Unreal Engine"))

```md
## Materials / Rendering
- [x] Material作成
- [x] Material Graph構築
- [x] Material適用
- [x] Dynamic Material Color変更
- [x] Material Instance Constant作成
- [x] Material Instance Dynamic詳細制御
- [x] Scalar Parameter編集
- [x] Vector Parameter編集
- [x] Texture Parameter編集
- [x] Static Switch Parameter編集
- [x] Material Parameter Collection作成
- [x] Material Parameter Collection編集
- [ ] Substrate Material作成
- [ ] Layered Material作成
- [ ] Decal Material作成
- [ ] Landscape Material作成
- [ ] Runtime Virtual Texture設定
- [ ] Light Function Material設定
- [ ] Post Process Material設定
- [x] Global Illumination設定
- [x] Lumen有効 / 無効
- [x] Lumen Scene Detail設定
- [x] Lumen Reflection Quality設定
- [x] Hardware Ray Tracing設定
- [x] Path Tracing設定
- [x] Virtual Shadow Maps設定
- [x] Shadow Quality設定
- [x] Anti-Aliasing設定
- [x] TSR設定
- [x] DLSS / FSR / XeSS設定
- [x] Nanite Visualization切替
- [x] Shader Compile状態取得
```

---

## 12. Lighting / Atmosphere

Light ActorはSpawnできます。でも、それは「照明機能を実装した」とは言えません。  
電球を床に置いて「建築電気設備を実装した」と言い張るやつです。やめましょう。

```md
## Lighting / Atmosphere
- [x] Directional Light配置
- [x] Point Light配置
- [x] Spot Light配置
- [x] Rect Light配置
- [x] Light Intensity設定
- [x] Light Color設定
- [x] Light Temperature設定
- [x] Mobility設定
- [x] Shadow有効 / 無効
- [x] Shadow Bias設定
- [x] Contact Shadow設定
- [x] Volumetric Scattering設定
- [x] IES Profile設定
- [x] Light Channel設定
- [x] Sky Light作成
- [x] Sky Light Cubemap設定
- [x] Sky Atmosphere作成
- [x] Atmospheric Fog / Height Fog設定
- [x] Exponential Height Fog作成
- [x] Volumetric Fog設定
- [x] Directional LightをSunに設定
- [x] Sun Position Calculator連携
- [x] HDRI Backdrop作成
- [x] Reflection Capture配置
- [x] Sphere Reflection Capture設定
- [x] Box Reflection Capture設定
- [x] Lightmass Importance Volume作成
- [x] Baked Lighting Build
- [x] Lighting Scenario管理
- [x] MegaLights設定
```

---

## 13. Post Process / Camera Look

未実装です。見た目の完成度に直結します。

```md
## Post Process / Camera Look
- [x] Post Process Volume作成
- [x] Infinite Extent設定
- [x] Exposure設定
- [x] Auto Exposure設定
- [x] Bloom設定
- [x] Lens Flare設定
- [x] Chromatic Aberration設定
- [x] Vignette設定
- [x] Film Grain設定
- [x] Color Grading設定
- [x] LUT設定
- [x] White Balance設定
- [x] Depth of Field設定
- [x] Motion Blur設定
- [x] Ambient Occlusion設定
- [ ] Global Illumination Override
- [ ] Reflections Override
- [x] Camera Actor作成
- [x] Cine Camera Actor作成
- [x] Focal Length設定
- [x] Aperture設定
- [x] Focus Distance設定
- [ ] Camera Shake設定
- [ ] Camera Rig Rail作成
- [ ] Camera Rig Crane作成
```

---

## 14. Landscape / Terrain

完全に弱いです。  
Landscapeは屋外地形を作るUEの標準機能です。現MCPは建物や街を置けますが、地形そのものはほぼ未対応です。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-6-release-notes?lang=ja&utm_source=chatgpt.com "Unreal Engine 5.7 リリース ノート"))

```md
## Landscape / Terrain
- [ ] Landscape作成
- [ ] Landscapeサイズ設定
- [ ] Section / Component設定
- [ ] Heightmap Import
- [ ] Heightmap Export
- [ ] Landscape Sculpt
- [ ] Landscape Smooth
- [ ] Landscape Flatten
- [ ] Landscape Ramp
- [ ] Landscape Erosion
- [ ] Landscape Noise
- [ ] Landscape Paint Layer作成
- [ ] Landscape Layer Blend設定
- [ ] Landscape Material適用
- [ ] Landscape Grass Output設定
- [ ] Landscape Collision設定
- [ ] Landscape Hole作成
- [ ] Landscape Spline作成
- [ ] Road Spline作成
- [ ] River Terrain Carve
- [ ] Runtime Virtual Texture連携
- [ ] Nanite Landscape設定
- [ ] World Partition Landscape管理
```

---

## 15. Foliage / Vegetation

未実装です。  
UE 5.7ではPCGやNanite Foliageなど、広大な環境制作まわりが強化されています。PCGはProduction-readyになったとEpicが発表しています。([Unreal Engine](https://www.unrealengine.com/news/unreal-engine-5-7-is-now-available?utm_source=chatgpt.com "Unreal Engine 5.7 is now available"))

```md
## Foliage / Vegetation
- [ ] Foliage Type作成
- [ ] Static Mesh Foliage登録
- [ ] Actor Foliage登録
- [ ] Foliage Paint
- [ ] Foliage Erase
- [ ] Foliage Density設定
- [ ] Foliage Scale Range設定
- [ ] Foliage Random Yaw設定
- [ ] Foliage Align to Normal設定
- [ ] Foliage Cull Distance設定
- [ ] Foliage LOD設定
- [ ] Procedural Foliage Spawner作成
- [ ] Procedural Foliage Volume作成
- [ ] Seed設定
- [ ] Biome別Foliage生成
- [ ] Grass Type作成
- [ ] Landscape Grass連携
- [ ] Nanite Foliage設定
- [ ] Wind設定
- [ ] Pivot Painter連携
```

---

## 16. PCG Framework

独自Procedural生成はあります。  
でも、UE公式PCG Graphそのものを操作する機能はありません。PCG FrameworkはUE内でプロシージャルコンテンツやツールを作るための機能です。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/procedural-content-generation-overview?utm_source=chatgpt.com "Procedural Content Generation Overview | Unreal Engine ..."))

```md
## PCG Framework
- [~] 独自Procedural生成あり
- [ ] PCG Graph作成
- [ ] PCG Component追加
- [ ] PCG Volume作成
- [ ] PCG Node追加
- [ ] PCG Node接続
- [ ] PCG Graph Parameter設定
- [ ] PCG Spline Sampler設定
- [ ] PCG Surface Sampler設定
- [ ] PCG Static Mesh Spawner設定
- [ ] PCG Rule設定
- [ ] PCG Biome Graph作成
- [ ] PCG Point Data操作
- [ ] PCG Attribute操作
- [ ] PCG Graph実行
- [ ] PCG Graph再生成
- [ ] PCG Runtime Generation設定
- [ ] PCG Editor Mode操作
- [ ] PCG Tool作成
- [ ] PCG Debug表示
```

---

## 17. Water System

未実装です。

```md
## Water System
- [ ] Water Plugin有効化
- [ ] Water Body Ocean作成
- [ ] Water Body Lake作成
- [ ] Water Body River作成
- [ ] Water Body Custom作成
- [ ] River Spline設定
- [ ] Water Material設定
- [ ] Wave設定
- [ ] Flow設定
- [ ] Buoyancy設定
- [ ] Water Mesh Actor設定
- [ ] Underwater Post Process設定
- [ ] Shoreline設定
- [ ] Landscape Carving設定
- [ ] Boat / Floating Actor連携
```

---

## 18. AI / Navigation

NavMeshVolumeと巡回Splineはあります。  
でも、AI Editor機能としてはまだ入り口です。Behavior TreeはNPC AI用の主要Assetとして公式に説明されています。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/behavior-trees-in-unreal-engine?utm_source=chatgpt.com "Behavior Trees in Unreal Engine"))

```md
## AI / Navigation
- [x] NavMeshBoundsVolume作成
- [x] NavMesh Rebuild要求
- [x] Patrol Route Spline作成
- [~] AI Behaviorタグ設定
- [~] CognitiveAIController一部あり
- [ ] Behavior Tree Asset作成
- [ ] Behavior Tree Node追加
- [ ] Behavior Tree Node接続
- [ ] Task作成
- [ ] Service作成
- [ ] Decorator作成
- [ ] Blackboard Asset作成
- [ ] Blackboard Key追加
- [ ] Blackboard Key削除
- [ ] Blackboard型設定
- [ ] AIControllerにBehavior Tree設定
- [ ] Run Behavior Tree Node生成
- [ ] AI Perception Component追加
- [ ] Sight Sense設定
- [ ] Hearing Sense設定
- [ ] Damage Sense設定
- [ ] Team Sense設定
- [ ] EQS Query作成
- [ ] EQS Generator設定
- [ ] EQS Test設定
- [ ] EQS Debug
- [ ] Nav Modifier Volume作成
- [ ] Nav Link Proxy作成
- [ ] Smart Nav Link設定
- [ ] Nav Area作成
- [ ] Agent Radius / Height設定
- [ ] Recast NavMesh詳細設定
- [ ] Crowd Following設定
- [ ] MassEntity連携
- [ ] StateTree作成
- [ ] StateTree State追加
- [ ] StateTree Task追加
```

---

## 19. Animation / Skeletal / Rigging

ほぼ未実装です。  
Animation Blueprint、IK Rig、Control Rig、Retargeterは、キャラクターを扱うなら避けられません。IK Rig/Retargeterは公式でも専用ドキュメントがあります。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?utm_source=chatgpt.com "IK Rig Animation Retargeting in Unreal Engine"))

```md
## Animation / Skeletal / Rigging
- [ ] Skeletal Mesh Import
- [ ] Skeleton Asset作成
- [ ] Physics Asset作成
- [ ] Animation Sequence Import
- [ ] Animation Blueprint作成
- [ ] AnimGraph Node追加
- [ ] State Machine作成
- [ ] State追加
- [ ] Transition Rule作成
- [ ] BlendSpace作成
- [ ] Aim Offset作成
- [ ] Animation Montage作成
- [ ] Notify追加
- [ ] Notify State追加
- [ ] Root Motion設定
- [ ] Retarget Manager設定
- [ ] IK Rig作成
- [ ] IK Goal追加
- [ ] IK Solver追加
- [ ] IK Retargeter作成
- [ ] Retarget Chain設定
- [ ] Control Rig作成
- [ ] Control追加
- [ ] Bone追加
- [ ] Constraint設定
- [ ] Sequencer Control Rig連携
- [ ] Pose Asset作成
- [ ] Facial Animation設定
- [ ] Morph Target設定
- [ ] MetaHuman連携
```

---

## 20. Niagara / VFX

未実装です。  
NiagaraはUE5の主要VFXシステムで、System、Emitter、Module、User Parameterなどを扱います。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/Niagara/UNiagaraSystem?utm_source=chatgpt.com "UNiagaraSystem | Unreal Engine 5.7 Documentation"))

```md
## Niagara / VFX
- [ ] Niagara System作成
- [ ] Niagara Emitter作成
- [ ] Emitter追加
- [ ] Module追加
- [ ] Module削除
- [ ] Spawn Rate設定
- [ ] Burst設定
- [ ] Lifetime設定
- [ ] Velocity設定
- [ ] Gravity設定
- [ ] Color設定
- [ ] Size設定
- [ ] Ribbon Renderer設定
- [ ] Sprite Renderer設定
- [ ] Mesh Renderer設定
- [ ] GPU Simulation設定
- [ ] Collision設定
- [ ] User Parameter追加
- [ ] User Parameter変更
- [ ] Niagara Component追加
- [ ] ActorにNiagara適用
- [ ] Niagara Parameter Binding
- [ ] Niagara Data Channel設定
- [ ] Niagara Effect Type設定
- [ ] Scalability設定
- [ ] Niagara Debug
- [ ] Niagara SIM Cache
```

---

## 21. Audio / MetaSounds

ほぼ未実装です。

```md
## Audio / MetaSounds
- [ ] Sound Wave Import
- [~] Sound Cue作成
- [ ] Sound Cue Graph編集
- [x] Audio Component追加
- [x] Sound Attenuation作成
- [x] Attenuation Radius設定
- [x] Spatialization設定
- [x] Reverb設定
- [ ] Sound Class作成
- [ ] Sound Mix作成
- [ ] Submix作成
- [ ] MetaSound Source作成
- [ ] MetaSound Patch作成
- [ ] MetaSound Graph Node追加
- [ ] MetaSound Parameter設定
- [ ] Ambient Sound配置
- [ ] Audio Volume作成
- [ ] Dialogue Wave作成
- [ ] Footstep Audio連携
- [ ] UI Sound設定
```

---

## 22. Physics / Chaos

Blueprint ComponentのPhysics設定は一部あります。  
でも、Chaos全体から見るとかなり未実装です。Chaos PhysicsはUEの軽量物理シミュレーション基盤で、Chaos Destructionはリアルタイム破壊表現のシステムです。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/physics-in-unreal-engine?utm_source=chatgpt.com "Physics in Unreal Engine | Unreal Engine 5.7 Documentation"))

```md
## Physics / Chaos
- [x] Simulate Physics設定
- [x] Mass / Damping一部設定
- [ ] Collision Preset設定
- [ ] Collision Channel作成
- [ ] Object Channel作成
- [ ] Trace Channel作成
- [ ] Collision Response設定
- [ ] Physical Material作成
- [ ] Friction設定
- [ ] Restitution設定
- [ ] Physics Constraint作成
- [ ] Constraint Limit設定
- [ ] Constraint Motor設定
- [ ] Radial Force作成
- [ ] Physics Volume作成
- [ ] Destructible / Geometry Collection作成
- [ ] Geometry Collection Fracture
- [ ] Chaos Field作成
- [ ] Chaos Solver設定
- [ ] Chaos Cache作成
- [ ] Chaos Vehicle作成
- [ ] Wheel設定
- [ ] Suspension設定
- [ ] Engine Torque設定
- [ ] Cloth設定
- [ ] Chaos Cloth Asset作成
- [ ] Groom Physics設定
- [ ] Ragdoll設定
- [ ] Physics Asset Body編集
- [ ] Physics Asset Constraint編集
- [ ] Chaos Visual Debugger連携
```

---

## 23. Sequencer / Cinematics

未実装です。  
SequencerはLevel Sequenceでカメラ、Actor、Audio、Animationなどをタイムライン制御するEditor機能です。Movie Render Queueは高品質なレンダリング出力に使われます。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/rendering-high-quality-frames-with-movie-render-queue-in-unreal-engine?utm_source=chatgpt.com "Rendering High Quality Frames with Movie Render Queue"))

```md
## Sequencer / Cinematics
- [ ] Level Sequence作成
- [ ] Level Sequence Actor配置
- [ ] Actor Binding追加
- [ ] Camera Cut Track追加
- [ ] Transform Track追加
- [ ] Visibility Track追加
- [ ] Event Track追加
- [ ] Audio Track追加
- [ ] Animation Track追加
- [ ] Material Parameter Track追加
- [ ] Keyframe追加
- [ ] Keyframe削除
- [ ] Keyframe補間設定
- [ ] Playback Range設定
- [ ] Frame Rate設定
- [ ] Shot Track作成
- [ ] Subsequence追加
- [ ] Cine Camera作成
- [ ] Camera Rail連携
- [ ] Camera Crane連携
- [ ] Sequencer Render Preview
- [ ] Take Recorder連携
- [ ] Control Rig Track追加
```

---

## 24. Movie Render Queue / Render Output

未実装です。

```md
## Movie Render Queue
- [ ] Movie Render Queue Job作成
- [ ] SequenceをQueueに追加
- [ ] Output Directory設定
- [ ] Resolution設定
- [ ] Frame Range設定
- [ ] Anti-Aliasing設定
- [ ] EXR出力設定
- [ ] PNG出力設定
- [ ] JPG出力設定
- [ ] ProRes / Video出力設定
- [ ] Path Tracer設定
- [ ] Console Variables設定
- [ ] Render Pass追加
- [ ] Object ID / Mask Pass設定
- [ ] Burn In設定
- [ ] Warm Up設定
- [ ] Render開始
- [ ] Renderキャンセル
- [ ] Render進捗取得
- [ ] Render結果検証
- [ ] Movie Render Graph作成
```

---

## 25. Data Tables / Data Assets

Blueprint Graphで`GetDataTableRow`のようなノードはありますが、DataTable Assetそのものの作成・編集は弱いです。Data Tablesは関連データを表形式で管理するUEのデータ駆動Gameplay要素です。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/data-driven-gameplay-elements-in-unreal-engine?utm_source=chatgpt.com "Data Driven Gameplay Elements in Unreal Engine"))

```md
## Data Tables / Data Assets
- [~] Blueprint Graph上でDataTable参照Nodeは一部あり
- [x] DataTable作成
- [x] CSVからDataTable作成
- [ ] JSONからDataTable作成
- [x] DataTable Row追加
- [ ] DataTable Row削除
- [ ] DataTable Row更新
- [ ] DataTable Export CSV
- [ ] DataTable Export JSON
- [ ] Row Struct作成
- [ ] Row Struct編集
- [ ] Primary Data Asset作成
- [ ] Data Asset作成
- [ ] Data Asset Property編集
- [ ] Curve Table作成
- [ ] String Table作成
- [ ] Gameplay Tag Table Import
- [ ] Item DB生成
- [ ] Enemy DB生成
- [ ] Quest DB生成
- [ ] Dialogue DB生成
```

---

## 26. Gameplay Ability System

未実装です。  
広くUEゲーム制作を扱うなら、GASは大きな領域です。

```md
## Gameplay Ability System
- [ ] GAS Plugin有効化
- [ ] Ability System Component追加
- [ ] Attribute Set作成
- [ ] Gameplay Ability作成
- [ ] Gameplay Effect作成
- [ ] Gameplay Cue作成
- [ ] Ability Input Binding
- [ ] Ability Grant
- [ ] Ability Activation設定
- [ ] Cooldown設定
- [ ] Cost設定
- [ ] Attribute初期化
- [ ] Attribute変更Event
- [ ] Gameplay Tag連携
- [ ] Replication設定
- [ ] Prediction設定
```

---

## 27. Save / Load / Persistence

Scene DBはありますが、UEゲームとしてのSaveGameは未対応です。

```md
## Save / Load
- [x] SaveGame Blueprint作成
- [x] Save Slot作成
- [x] Save Game To Slot Node生成
- [x] Load Game From Slot Node生成
- [x] Save Data Struct作成
- [x] Player Progress保存
- [x] Inventory保存
- [x] World State保存
- [x] Settings保存
- [x] Checkpoint System生成
- [ ] Auto Save設定
```

---

## 28. Packaging / Build / Deployment

未実装です。  
作ったものをパッケージ化できないと、「できた」と言いながらEditor内に幽閉されます。ソフトウェア版座敷牢です。UEにはPackagingやAutomation Toolがあります。([Unreal Engine](https://www.unrealengine.com/download?utm_source=chatgpt.com "Download Unreal Engine"))

```md
## Packaging / Build / Deployment
- [ ] Project Build
- [ ] C++ Compile
- [ ] Hot Reload / Live Coding制御
- [ ] Cook Content
- [ ] Package Project
- [ ] BuildCookRun実行
- [ ] Windows Package
- [ ] Linux Package
- [ ] Android Package
- [ ] iOS Package
- [ ] Dedicated Server Build
- [ ] Shipping / Development設定
- [ ] Pak / IoStore設定
- [ ] Chunk設定
- [ ] Localization Cook設定
- [ ] Crash Reporter設定
- [ ] Buildログ取得
- [ ] Build失敗解析
- [ ] Build成果物パス取得
- [ ] AutomationTool連携
```

---

## 29. Testing / Validation

Python/Rust側のテストはあります。  
でも、UE Editor内のAutomation TestやFunctional Testとの連携は弱いです。

```md
## Testing / Validation
- [~] Python Unit Test
- [~] Rust Test
- [ ] UE Automation Test作成
- [ ] Functional Test Actor作成
- [ ] Automation Test実行
- [ ] Automation Test結果取得
- [ ] Map Check実行
- [ ] Asset Validation実行
- [ ] Blueprint Compile All
- [ ] Broken Reference検出
- [ ] Missing Material検出
- [ ] Missing Mesh検出
- [ ] Collision Validation
- [ ] Navigation Validation
- [ ] Performance Budget Validation
- [ ] FPS測定
- [ ] Stat Unit取得
- [ ] Stat GPU取得
- [ ] Memory使用量取得
- [ ] Unreal Insights Trace開始
- [ ] Unreal Insights Trace停止
- [ ] Gameplay Screenshot Test
```

---

## 30. Localization

未実装です。

```md
## Localization
- [ ] Localization Dashboard操作
- [ ] Culture追加
- [ ] Text Gather
- [ ] PO Export
- [ ] PO Import
- [ ] String Table作成
- [ ] String Table編集
- [ ] Widget Text Localization
- [ ] Dialogue Localization
- [ ] Font Fallback設定
```

---

## 31. Mobile / XR / Platform

未実装です。

```md
## Platform / Mobile / XR
- [ ] Android設定
- [ ] iOS設定
- [ ] Mobile Rendering設定
- [ ] Touch Input設定
- [ ] Device Profile設定
- [ ] Scalability Profile作成
- [ ] XR Plugin有効化
- [ ] OpenXR設定
- [ ] VR Pawn作成
- [ ] Motion Controller設定
- [ ] HMD Camera設定
- [ ] AR Session設定
- [ ] AR Plane Detection設定
- [ ] Platform-specific Packaging
```

---

## 32. Collaboration / Source Control

未実装です。

```md
## Collaboration / Source Control
- [ ] Source Control状態取得
- [ ] Git連携
- [ ] Perforce連携
- [ ] Checkout
- [ ] Checkin
- [ ] Revert
- [ ] File Lock取得
- [ ] File Lock解除
- [ ] Changelist作成
- [ ] Asset Diff
- [ ] Blueprint Diff
- [ ] Merge支援
- [ ] Multi-User Editing起動
- [ ] Multi-User Session接続
```

---


## 2026-05-05 capability/CI audit note
- Added automated Python unit routing coverage for project/editor control actions in `Python/tests/unit/test_project_editor_tools_routing.py`.
- Verified existing implemented surface includes: project settings read/write, default maps, plugin enable/list, world settings get/set, save/log/undo/redo, PIE/simulate control, viewport camera control, level/sublevel/world-partition operations.
- Noted gaps still requiring new tool work: dedicated content-browser bulk operations and dedicated import/export toolchain.
