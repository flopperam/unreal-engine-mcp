## 1. Project / Editor 基本操作

Unreal Editor自体にはProject Settings、Plugin管理、Editor Preference、World Settingsなどがありますが、現MCPはほぼ「レベル内のActorやAsset操作」に寄っています。UE公式ドキュメントでも、プロジェクト設定・制作パイプライン・エディタ自動化は独立した領域として扱われています。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-7-documentation?utm_source=chatgpt.com "Unreal Engine 5.7 Documentation"))

```md
## Project / Editor Control
- [ ] プロジェクト設定の読み取り
- [ ] プロジェクト設定の変更
- [ ] Default Map設定
- [ ] Game Default Map設定
- [ ] Editor Startup Map設定
- [ ] Project Description / Version / Company情報編集
- [ ] Plugin有効化 / 無効化
- [ ] Plugin一覧取得
- [ ] Engine Scalability設定
- [ ] Rendering設定変更
- [ ] Physics設定変更
- [ ] Input設定変更
- [ ] Collision設定変更
- [ ] AI System設定変更
- [ ] Navigation System設定変更
- [ ] Packaging設定変更
- [ ] Maps & Modes設定変更
- [ ] World Settings取得
- [ ] World Settings変更
- [ ] Editor Utility Widget作成
- [ ] Editor Utility Blueprint作成
- [ ] Editor Python Script実行
- [ ] Editor Commandlet実行
- [ ] Undo / Redo制御
- [ ] Dirty Asset一覧取得
- [ ] Save All
- [ ] 特定Asset保存
- [ ] Editorログ取得
- [ ] PIE開始
- [ ] PIE停止
- [ ] Standalone Game起動
- [ ] Simulate開始
- [ ] Viewport操作
- [ ] カメラ位置取得
- [ ] カメラ位置設定
```

---

## 2. Level / World / Map管理

今のMCPはActorをレベルに置くのは強いです。ただし、**レベルそのものを作る・保存する・Sublevelに分ける・World Partitionで管理する**部分が弱いです。World Partitionは大規模ワールドをグリッドセルで分割・ストリーミングするUEの中核機能です。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/world-partition-builder-commandlet-reference "World Partition Builder Commandlet Reference | Unreal Engine 5.7 Documentation | Epic Developer Community"))

```md
## Level / Map Management
- [ ] 新規Level作成
- [ ] Level保存
- [ ] Level読み込み
- [ ] Level複製
- [ ] Levelリネーム
- [ ] Level削除
- [ ] Persistent Level管理
- [ ] Sublevel追加
- [ ] Sublevel削除
- [ ] Sublevel表示 / 非表示
- [ ] Sublevelロード / アンロード
- [ ] Level Streaming Volume作成
- [ ] Level Streaming設定
- [ ] World Partition有効化
- [ ] World Partition Grid設定
- [ ] World Partition Cell情報取得
- [ ] World Partition Cellロード
- [ ] World Partition Cellアンロード
- [ ] Data Layer作成
- [ ] Data LayerにActor追加
- [ ] Data LayerからActor削除
- [ ] Data Layer有効 / 無効切替
- [ ] HLOD Layer作成
- [ ] HLOD生成
- [ ] HLOD再ビルド
- [ ] One File Per Actor設定
- [ ] Level Bounds管理
- [ ] World Origin Rebasing設定
```

---

## 3. Content Browser / Asset管理

ここがかなり大きな穴です。  
今は既存Assetを参照して使うことはできますが、**Content Browserを本格操作する機能がない**。広くUEを使うなら、ここは最優先級です。素材を取り込めないエディタ自動化は、冷蔵庫のない料理人です。気合いだけで飢えます。

```md
## Content Browser / Asset Management
- [ ] フォルダ作成
- [ ] フォルダ削除
- [ ] Asset一覧取得
- [ ] Asset検索
- [ ] Assetパス解決
- [ ] Asset移動
- [ ] Assetコピー
- [ ] Asset複製
- [ ] Assetリネーム
- [ ] Asset削除
- [ ] Asset保存
- [ ] Assetロード
- [ ] Assetアンロード
- [ ] Assetメタデータ取得
- [ ] Assetメタデータ編集
- [ ] Assetタグ付け
- [ ] Redirector検出
- [ ] Redirector Fixup
- [ ] 未使用Asset検出
- [ ] 参照関係取得
- [ ] 依存Asset一覧取得
- [ ] Asset Reference Viewer相当
- [ ] Asset Audit相当
- [ ] Primary Asset Label作成
- [ ] Asset Manager設定
- [ ] Asset Registry検索
- [ ] Bulk Rename
- [ ] Bulk Move
- [ ] Bulk Delete
```

---

## 4. Asset Import / Export

広くUEを使うなら、これは絶対に必要です。  
現状は「既にUE内にあるアセットを使う」前提が強いです。外部素材の取り込みが弱い。

```md
## Asset Import / Export
- [ ] FBX Static Mesh Import
- [ ] FBX Skeletal Mesh Import
- [ ] GLTF / GLB Import
- [ ] OBJ Import
- [ ] USD Import
- [ ] Texture Import PNG
- [ ] Texture Import JPG
- [ ] Texture Import EXR
- [ ] Texture Import HDR
- [ ] Normal Map Import設定
- [ ] ORM / Packed Texture設定
- [ ] WAV Import
- [ ] MP3 / OGG Import
- [ ] Animation FBX Import
- [ ] Alembic Import
- [ ] Datasmith Import
- [ ] Reimport
- [ ] Import設定Preset
- [ ] Import時のScale / Axis / Collision設定
- [ ] LOD付きStatic Mesh Import
- [ ] Nanite有効化Import
- [ ] Material自動生成Import
- [ ] Texture圧縮設定
- [ ] Asset Export
- [ ] Level Export
- [ ] Mesh Export
- [ ] Screenshot Export
```

---

## 5. Static Mesh / Mesh Editing

現状はStaticMeshActorを置けます。  
でも、Static Mesh Assetそのものを編集する機能はほぼありません。

```md
## Static Mesh / Mesh Editing
- [~] Static Mesh Actor配置
- [ ] Static Mesh Asset詳細取得
- [ ] Static Mesh Collision生成
- [ ] Collision Complexity設定
- [ ] Simple Collision追加
- [ ] UCX Collision Import制御
- [ ] LOD生成
- [ ] LOD設定変更
- [ ] Nanite有効 / 無効
- [ ] Nanite Fallback設定
- [ ] Lightmap UV生成
- [ ] Lightmap Resolution設定
- [ ] Mesh Bounds編集
- [ ] Socket追加
- [ ] Socket削除
- [ ] Socket Transform変更
- [ ] Pivot変更
- [ ] Mesh Merge
- [ ] Mesh Simplify
- [ ] Mesh Bake
- [ ] Modeling Mode機能呼び出し
- [ ] Poly Edit
- [ ] Boolean
- [ ] Remesh
- [ ] Voxel Remesh
- [ ] UV Unwrap
- [ ] UV Layout
- [ ] Vertex Color Paint
```

---

## 6. Blueprint 基本機能の不足

Blueprint Graph操作はかなり強いです。  
ただし、Blueprint Editor全体から見ると、まだ未対応が多いです。

```md
## Blueprint - Missing / Partial
- [~] Blueprint作成
- [~] Component追加
- [~] Graph Node追加
- [~] Node接続
- [ ] Blueprint Interface作成
- [ ] Blueprint Interface実装
- [ ] Blueprint Macro Library作成
- [ ] Blueprint Function Library作成
- [ ] Enum作成
- [ ] Struct作成
- [ ] User Defined Struct編集
- [ ] User Defined Enum編集
- [ ] Blueprint継承関係変更
- [ ] Parent Class変更
- [ ] Blueprint Class Settings編集
- [ ] Blueprint Class Defaults編集
- [ ] Blueprint Component Defaults編集
- [ ] Construction Script詳細編集
- [ ] Event Dispatcher作成
- [ ] Event Dispatcher Binding
- [ ] Timeline Curve編集
- [ ] Latent Node制御
- [ ] Macro作成
- [ ] Collapsed Graph作成
- [ ] コメントノード作成
- [ ] Reroute Node整理
- [ ] Graph自動整列
- [ ] Blueprint Diff
- [ ] Blueprint Debug情報取得
- [ ] Breakpoint設定
- [ ] Watch変数設定
- [ ] Blueprint Profiler連携
```

---

## 7. Gameplay Framework

ここは未実装寄りです。  
UEのGameplay Frameworkは、GameMode、GameState、PlayerController、PlayerState、Pawn、Cameraなどを含むゲーム構築の根幹です。公式でもこれらはコアシステムとして説明されています。([Epic Games Developers](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine?utm_source=chatgpt.com "Gameplay Framework in Unreal engine"))

```md
## Gameplay Framework
- [ ] GameMode Blueprint作成
- [ ] GameMode C++ Class作成
- [ ] Default GameMode設定
- [ ] GameState作成
- [ ] PlayerState作成
- [ ] PlayerController作成
- [ ] AIController作成
- [ ] Pawn作成
- [ ] Character作成
- [ ] Default Pawn設定
- [ ] HUD Class設定
- [ ] Spectator Pawn設定
- [ ] Player Start配置
- [ ] Spawn Rule設定
- [ ] Possess設定
- [ ] Camera Manager設定
- [ ] Camera Component設定
- [ ] Spring Arm設定
- [ ] SaveGame Class作成
- [ ] GameInstance作成
- [ ] GameInstance Subsystem作成
- [ ] World Subsystem作成
- [ ] Local Player Subsystem作成
- [ ] Gameplay Tags設定
- [ ] Gameplay Tags追加
- [ ] Gameplay Tag Query作成
```

---

## 8. Enhanced Input

完全に足りていません。  
Enhanced InputはInput ActionやInput Mapping Contextを使う現在の標準入力システムです。公式ドキュメントでもPluginとして独立説明されています。([Epic Games Developers](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine?utm_source=chatgpt.com "Enhanced Input in Unreal Engine"))

```md
## Enhanced Input
- [ ] Input Action作成
- [ ] Input Mapping Context作成
- [ ] Key Mapping追加
- [ ] Key Mapping削除
- [ ] Trigger設定
- [ ] Modifier設定
- [ ] Dead Zone設定
- [ ] Swizzle Axis設定
- [ ] Negate設定
- [ ] Hold / Tap / Pressed / Released設定
- [ ] Gamepad Mapping
- [ ] Mouse Mapping
- [ ] Keyboard Mapping
- [ ] Runtime Mapping Context追加
- [ ] Runtime Mapping Context削除
- [ ] PlayerControllerへのBinding生成
- [ ] Character BlueprintへのBinding生成
- [ ] Input Debug情報取得
- [ ] Rebind UI連携
- [ ] Local Multiplayer用Input設定
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
- [ ] Widget Blueprint作成
- [ ] User Widget作成
- [ ] Canvas Panel追加
- [ ] Vertical Box追加
- [ ] Horizontal Box追加
- [ ] Overlay追加
- [ ] Border追加
- [ ] Button追加
- [ ] Text Block追加
- [ ] Image追加
- [ ] Progress Bar追加
- [ ] Slider追加
- [ ] Check Box追加
- [ ] Combo Box追加
- [ ] Scroll Box追加
- [ ] Uniform Grid追加
- [ ] Widget Anchor設定
- [ ] Widget Position設定
- [ ] Widget Size設定
- [ ] Widget Alignment設定
- [ ] Font設定
- [ ] Color設定
- [ ] Brush設定
- [ ] Style設定
- [ ] Button OnClicked Binding
- [ ] Widget Animation作成
- [ ] HUDとしてViewport追加
- [ ] Remove From Parent
- [ ] UI変数Binding
- [ ] Health Bar Binding
- [ ] Score Text Binding
- [ ] Main Menu生成
- [ ] Pause Menu生成
- [ ] Settings Menu生成
- [ ] Dialogue UI生成
- [ ] Inventory UI生成
- [ ] Common UI Plugin対応
- [ ] Input Mode Game/UI設定
- [ ] Mouse Cursor表示制御
```

---

## 11. Materials / Rendering

Material Graphは強いですが、Rendering全体はまだ薄いです。  
Lumenは動的GIと反射、Post Processは露出・ブルーム・カラーグレーディングなどに関わります。([Epic Games Developers](https://dev.epicgames.com/documentation/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine?utm_source=chatgpt.com "Lumen Global Illumination and Reflections in Unreal Engine"))

```md
## Materials / Rendering
- [~] Material作成
- [~] Material Graph構築
- [~] Material適用
- [~] Dynamic Material Color変更
- [ ] Material Instance Constant作成
- [ ] Material Instance Dynamic詳細制御
- [ ] Scalar Parameter編集
- [ ] Vector Parameter編集
- [ ] Texture Parameter編集
- [ ] Static Switch Parameter編集
- [ ] Material Parameter Collection作成
- [ ] Material Parameter Collection編集
- [ ] Substrate Material作成
- [ ] Layered Material作成
- [ ] Decal Material作成
- [ ] Landscape Material作成
- [ ] Runtime Virtual Texture設定
- [ ] Light Function Material設定
- [ ] Post Process Material設定
- [ ] Global Illumination設定
- [ ] Lumen有効 / 無効
- [ ] Lumen Scene Detail設定
- [ ] Lumen Reflection Quality設定
- [ ] Hardware Ray Tracing設定
- [ ] Path Tracing設定
- [ ] Virtual Shadow Maps設定
- [ ] Shadow Quality設定
- [ ] Anti-Aliasing設定
- [ ] TSR設定
- [ ] DLSS / FSR / XeSS設定
- [ ] Nanite Visualization切替
- [ ] Shader Compile状態取得
```

---

## 12. Lighting / Atmosphere

Light ActorはSpawnできます。でも、それは「照明機能を実装した」とは言えません。  
電球を床に置いて「建築電気設備を実装した」と言い張るやつです。やめましょう。

```md
## Lighting / Atmosphere
- [~] Directional Light配置
- [~] Point Light配置
- [~] Spot Light配置
- [~] Rect Light配置
- [ ] Light Intensity設定
- [ ] Light Color設定
- [ ] Light Temperature設定
- [ ] Mobility設定
- [ ] Shadow有効 / 無効
- [ ] Shadow Bias設定
- [ ] Contact Shadow設定
- [ ] Volumetric Scattering設定
- [ ] IES Profile設定
- [ ] Light Channel設定
- [ ] Sky Light作成
- [ ] Sky Light Cubemap設定
- [ ] Sky Atmosphere作成
- [ ] Atmospheric Fog / Height Fog設定
- [ ] Exponential Height Fog作成
- [ ] Volumetric Fog設定
- [ ] Directional LightをSunに設定
- [ ] Sun Position Calculator連携
- [ ] HDRI Backdrop作成
- [ ] Reflection Capture配置
- [ ] Sphere Reflection Capture設定
- [ ] Box Reflection Capture設定
- [ ] Lightmass Importance Volume作成
- [ ] Baked Lighting Build
- [ ] Lighting Scenario管理
- [ ] MegaLights設定
```

---

## 13. Post Process / Camera Look

未実装です。見た目の完成度に直結します。

```md
## Post Process / Camera Look
- [ ] Post Process Volume作成
- [ ] Infinite Extent設定
- [ ] Exposure設定
- [ ] Auto Exposure設定
- [ ] Bloom設定
- [ ] Lens Flare設定
- [ ] Chromatic Aberration設定
- [ ] Vignette設定
- [ ] Film Grain設定
- [ ] Color Grading設定
- [ ] LUT設定
- [ ] White Balance設定
- [ ] Depth of Field設定
- [ ] Motion Blur設定
- [ ] Ambient Occlusion設定
- [ ] Global Illumination Override
- [ ] Reflections Override
- [ ] Camera Actor作成
- [ ] Cine Camera Actor作成
- [ ] Focal Length設定
- [ ] Aperture設定
- [ ] Focus Distance設定
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
- [~] NavMeshBoundsVolume作成
- [~] NavMesh Rebuild要求
- [~] Patrol Route Spline作成
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
- [ ] Sound Cue作成
- [ ] Sound Cue Graph編集
- [ ] Audio Component追加
- [ ] Sound Attenuation作成
- [ ] Attenuation Radius設定
- [ ] Spatialization設定
- [ ] Reverb設定
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
- [~] Simulate Physics設定
- [~] Mass / Damping一部設定
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
- [ ] DataTable作成
- [ ] CSVからDataTable作成
- [ ] JSONからDataTable作成
- [ ] DataTable Row追加
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
- [ ] SaveGame Blueprint作成
- [ ] Save Slot作成
- [ ] Save Game To Slot Node生成
- [ ] Load Game From Slot Node生成
- [ ] Save Data Struct作成
- [ ] Player Progress保存
- [ ] Inventory保存
- [ ] World State保存
- [ ] Settings保存
- [ ] Checkpoint System生成
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
