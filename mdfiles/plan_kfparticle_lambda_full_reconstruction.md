# Λ解析をSTAR標準のKFParticle再構成経路へ移行する計画

作成日: 2026-09-07  
状態: 2026-09-07に実装・検証を開始。§1–8は実装前の調査記録（ソース行番号・未実施の記述も当時のもの）。現在の実装は§9と実装記録を参照。

## 1. 結論と今回の目的

`analysis/anaLambda_KFParticle.C` から、他のSTARユーザーと同じ
`StKFParticleInterface → KFParticleTopoReconstructor → KFParticleFinder`
の再構成経路を利用してΛ／anti-Λを選別できるようにする。
現在の「Makerが陽子とπを全組合せし、scalar `KFParticle::Construct()` でfitする」実装から移行する。

ROOT 6の導入・STAR releaseの変更は前提にしない。
現行のSL24y／ROOT 5.34/38／GCC 4.8.5／Singularityで実装・検証する方針とする。
ただし、フル再構成経路のビルド・実イベント動作はまだ確認しておらず、成功済みとは記述しない。

本書は、以下の既存文書を踏まえ、scalar版に限定されていた実装範囲を改訂する。
既存解析の保護、`make all` 対応、KF依存の分離という従来の要件は維持する。

- [9月4日の作業ログ](../analysisnote/20260904/summary20260904.md)
- [Λへの導入計画](plan_kfparticle_lambda.md)
- [当初の統合計画](plan_kfparticle_pro_integration.md)

## 2. 現状確認と以前の説明の訂正

### 2.1 実装されている範囲

| 項目 | 9月4日の実装 | 今回の変更方針 |
| --- | --- | --- |
| 再構成の主体 | Makerの陽子×πループ | 標準Finderによる候補探索 |
| KF本体 | scalar中心の4つの`.cxx` | SIMD、Finder、Topo、PV再構成を含む依存一式 |
| PicoDstとの接続 | track covariance変換と2-track fit helper | STAR InterfaceのPico入力経路を移植したイベント単位adapter |
| PID | Maker内のTPC nSigma判定 | TPC／TOFと複数PID仮説を扱うInterface経路 |
| PVの扱い | 各ペアのfit後にPVを設定 | 入力PV・primary track分類・PV indexをTopoへ渡す |
| Λの取得 | `FitLambda()` の結果 | `GetParticles()` のPDG `+3122`／`-3122` |
| 動作検証 | ビルド、ロード、Init／Finish | 上記に加え、実イベント・候補・質量分布・参照経路との比較 |

現行実装の確認箇所:

- `Makefile:134` 付近: `KFParticle`、`KFPTrack`、`KFPVertex`、`KFParticleDatabase` のみをコンパイル。
- `StMaker/kfparticle/KfParticleHelper.cxx:96` 以降: `FitLambda()` と `lambda.Construct(daughters, 2)`。
- `StMaker/StLambdaKFParticleMaker/StLambdaKFParticleMaker.cxx:225` 以降: track選択と明示的なペアループ。
- `StRoot/KFParticle/PROVENANCE.md`: Finder／Topo／SIMDコンテナを除外したことを記録。

### 2.2 ROOT 6について

以前の「Finder／TopoReconstructorはROOT 6依存が強いので外した」という説明には、
今回確認したソースに基づく十分な根拠がないため訂正する。
当初の統合計画はFinder／SIMD／Topoの導入を予定し、scalar版はビルドが難しい場合の代替案だった。
現状はその代替案まで実装した状態であり、残っている主な作業はSTAR Interfaceの移植、
依存関係の整理、ビルド条件とABIの整合、物理的な動作検証である。

- 調査した`.DEV2`のFinder／Topoの再構成経路に、ROOT 6を必須とするversion gateは見つからなかった。
- ROOT dictionaryの利用そのものはROOT 6必須ではない。ROOT 5にも`rootcint`、`ClassDef`、ACLiCがある。
  根拠: [ROOT 5.34 User's Guide: Adding a Class](https://root.cern.ch/root/html534/guides/users-guide/AddingaClass.html)。
- `KFParticleStandalone`は主に内部クラスのdictionary／streamingを切り替える定義であり、
  「scalarのみを使う」という意味ではない。Finder／Topoを通常のコンパイル済みC++として使う方針と両立する。
- `.DEV2`の通常のSIMD経路は同梱`KFPSimd`を使う。古いユーザーコピーにある`Vc`前提を、
  現在の`.DEV2`にもそのまま当てはめない。
- ROOT 5への対応可能性を示すソース調査と、実際のビルド成功は別である。後者は本計画の検証項目とする。

### 2.3 9月4日の検証の限界

ログ上、clean `make core`／`make all`、ACLiCコンパイル、ライブラリロード、
`TrackCovMatrix`の有効化、`Init()`／`Finish()`は成功している。
一方、入力リスト内の`/home/starlib/...`が利用できず、処理イベント数は0だった。
この結果からΛの質量ピーク、PID、covariance変換の物理的正しさ、既存解析との同等性は結論できない。

## 3. 参照実装とバージョンの選び方

### 3.1 調査対象

- KF本体のupstream: `/star/nfs4/AFS/star/packages/.DEV2/StRoot/KFParticle/`
- 最新STAR adapter: `/star/nfs4/AFS/star/packages/.DEV2/StRoot/StKFParticleAnalysisMaker/StKFParticleInterface.{h,cxx}`
- ユーザー例: `/star/u/gwang1/kfp/`
- ユーザー例: `/star/u/xwu2/test/ampt_test_xing/KFTree_lambda/`、`KFTree_ks0/`
- 当初の計画から参照した読み取り可能なΛ解析:
  `/star/u/oura/gpfs/papers/psn0878/AnaPico/Lambda/KFtree_lam/`

特に最後のΛ解析では、以下をソースで確認した。

- `StRoot/macro/analysispicodst_ldxi_pol.C:55–73`: Interface経由のカット設定と±3122の登録。
- `StRoot/StKFParticleAnalysisMaker/StKFParticleInterface.cxx:647–884`:
  PicoDst→PID仮説→`CleanPV()`→`InitParticles()`→`AddPV()`→`ReconstructParticles()`。
- 同Interfaceの`ReconstructParticles():74`付近:
  `SortTracks()`→Topoの`ReconstructParticles()`。
- `StRoot/StKFParticleAnalysisMaker/StKFParticleAnalysisMaker.cxx:625`以降:
  `GetParticles()`から`abs(PDG)==3122`を取り出し、別コピーでPV拘束量を評価。

追加で、指定されたユーザー領域についても以下を実際に読み取り確認した。

- xwu2の`KFTree_lambda/StRoot/macro/analysispicodst_ldxi_pol.C:58–75`では、
  上記論文Λと同じ主要Finderカット、soft TOF PID、±3122登録を使用。
  `StKFParticleInterface.cxx:701`以降はPico入力、`:875–887`はInit／AddPV／再構成、
  `:68–71`はSortTracks／Topo呼出し。PID入力は`dEdxPull()`であり、
  `GetPID():388`以降ではTPCとTOFの仮説を照合し、追加`Calibration()`呼出しは無効。
- xwu2の`KFTree_ks0`は同じ形式のmacroでPDG 310を登録しており、Λ専用の別アルゴリズムではない。
- gwang1のdata用Λ実装は
  `/star/u/gwang1/kfp/KFParticle_data_QA/KFParticle_Tree/Lambda/`にある。
  `StKFParticleInterface.cxx:784`以降のPico経路と`:1004–1015`のInit／PV／再構成を確認。
  `analysispicodst_ld_ana.C`はdataset 309、最大daughter距離15、2-body χ² cut 20、+3122登録。
  有効な`GetPID()`ではTOFを無効化し、π校正とpの専用dEdx帯を使うため、汎用PIDの既定値にはしない。
- gwang1のembedding用Λ実装は
  `/star/u/gwang1/kfp/KFParticle_embedding_QA/KFPraticle_embedding_Tree/Ld/`にあり、
  そのInterfaceでもSortTracks／Topoと、Pico／MuDst経路からの再構成呼出しを確認した。
  embeddingのtruth選別・出力全体を今回実行・検証したわけではない。

したがって、共通なのはInterface→Topo→Finderという再構成方式であり、
ユーザーごとのPID・校正・カット値まで一律ではない。
主たるPico入力・PID移植元はxwu2のΛ版とし、psn0878を補助比較元、
gwang1をdata固有改変とembedding拡張の参照元とする。

### 3.2 採用方針

1. KF数学・Finder・Topoは、現行vendor元と同じ`.DEV2`の**一つのスナップショット**に揃えて取り込む。
   追加ファイルだけでなく既存scalarファイルもchecksumを照合し、互換性のない版を混在させない。
2. PicoDst adapterは、主にxwu2のΛ版`StKFParticleInterface`にある
   `GetTrack`、`GetPID`／TOF PID、`AddTrackToParticleList`、PV入力、再構成呼出しを移植する。
   SL24y APIに合わせた変更点は元関数と対応付け、独自のペア探索へ置き換えない。
3. 最新`.DEV2` Interfaceを無修正でコピーする方針にはしない。
   同Interfaceは`StTrackCombPiD`、TFG用API、固定標的PV再fit等にも依存し、
   SL24yには`StTrackCombPiD`が見当たらない。ユーザー例の従来型Pico PID経路を移植対象にする。
4. 古い論文コピーの`KFParticleBase`／`BaseSIMD`／`Vc`と、現在の`.DEV2` coreを混ぜない。
   coreの世代差による数値差は別に評価する。「同じ方式」と「過去の解析とbit単位で同じ結果」は区別する。
5. コピー元、取得日、全ファイルのchecksum、ライセンス、局所patch、使用defineを`PROVENANCE.md`に記録する。
   他ユーザーの領域やSTARのインストール先は変更しない。

### 3.3 「フルに使う」の範囲

今回の必須範囲は、汎用KFParticle再構成エンジンを省略せず導入し、その正規のInterface経路でΛを選別すること。
SIMD、Finder、Topo、必要なPVクラス、複数PID仮説、PV分類、標準の候補コンテナを使用可能にする。
探索する崩壊を±3122に設定することは、Finderを省略することとは異なる。

`StKFParticleAnalysisMaker`全体に含まれるflow／event-plane、TMVA、MuDst／MC入出力、
embedding用Performance一式、全粒子種の出力は、今回のPicoDst Λ選別には要求しない。
これらをROOT 6のせいで除外するのではなく、今回の出力・入力の範囲とは分ける。
将来のΞ／Ω等は同じエンジンを拡張利用できる構造にし、今回その解析まで完成したとは扱わない。

## 4. 修正後の処理構成

```text
analysis/anaLambda_KFParticle.C  [StChain構築とイベントループ]
  └─ StLambdaKFParticleMaker    [event selection、結果選別・QA]
       └─ StPicoKFParticleInterface（仮称、KF専用adapter）
            ├─ Pico track + TrackCovMatrix → KFPTrack
            ├─ TPC/TOF PID → 複数の粒子仮説 + 元track対応表
            ├─ 入力PV + covariance + primary track list
            └─ KFParticleTopoReconstructor
                 ├─ Init / AddPV / SortTracks
                 └─ KFParticleFinderによる再構成
                      └─ GetParticles() → ±3122 → Λ用出力
```

macroへ直接KFアルゴリズムを記述せず、repoのStChain／ACLiC構成を維持する。
`anaLambda_KFParticle.C`はこの新経路を起動する入口とし、処理本体はコンパイル済みMaker／adapterへ置く。
既存scalar `FitLambda()`は本番の再構成経路から外す。残す場合も比較テスト用と明記し、
Finder失敗時に暗黙にscalarへ戻るfallbackは作らない。

### 4.1 イベント入力とcovariance

- `Event`、`Track`、`TrackCovMatrix`、利用するBTOF branchを明示して有効化する。
- `StPicoTrackCovMatrix::dcaGeometry()`はSL24yでは`__TFG__VERSION__`条件付き。
  現行の`params`／`sigmas`／`correlations`から`StDcaGeometry::GetXYZ()`への変換を再利用・照合する。
  プロジェクト全体へ`__TFG__VERSION__`を定義しない。
- track数とcovariance数、index対応、null、finite値、分散、変換後の異常を検査する。
  欠落時にゼロ誤差や任意の小さい誤差を作ってfitを続けない。
- `bField()`の単位・符号をSTAR Interfaceと揃える。イベントの最初、
  PID仮説粒子のPVへの偏差を計算する前に磁場を設定し、前イベントの磁場を使わない。
- PVはPicoDstの座標と`primaryVertexError()`から構築し、誤差の二乗を共分散へ渡す。
  off-diagonalを持たない入力で勝手に相関を推定しない。
- 最初の運用では参照Λ経路と同じ入力PVを使用し、固定標的用の独自PV再fitは自動で有効にしない。
  PV再fitを追加する場合は、入力データ条件・比較検証・設定を独立させる。
- イベント境界で粒子、PV、PID、ID対応表、QA用一時データをリセットし、寿命切れ参照を保持しない。

### 4.2 PIDとtrack選択

- 参照InterfaceのTPC／TOF判定と、その交差・複数仮説の作り方を移植する。
  πとpのいずれか一つへ強制分類せず、許容される仮説を各々Topoへ渡す。
- TOFあり／なしを区別する。soft TOF PIDは「TOFを使わない」ことではない。
  有効なTOFがあれば設定されたPID判定に使い、欠測時に全trackを無条件排除しない。
- `bTofPidTraitsIndex()==0`は有効なindexとして扱う。範囲・pointer・betaを検査する。
  参照コードの`>0`等をそのままコピーする場合の欠落は修正点として記録する。
- 論文Λの`GetPID()`には`Calibration()`による運動量依存nSigma補正、
  TOF判定には係数表がある。これらを全データ共通のSTAR定数とみなさない。
  校正profile、適用データ、元の係数を明示し、対象不明の校正を現在のAuAu19設定へ自動適用しない。
- 参照比較用profileと現行データ用profileを区別する。初期の参照PIDはxwu2と同じ
  `dEdxPull(..., fit=1, charge=1)`を用い、追加のdataset固有補正は明示設定時のみ適用する。
  PicoDstの保存済みnSigmaを利用する別profileも区別して定義し、両者を同じ量として置換しない。
  TOF係数の適用可否もQAで検証する。校正条件が一致していなければ
  「参照解析と同じPID効率」とは報告しない。
- `StPicoTrack::dEdxPull()`もSL24yでは`__TFG__VERSION__`条件付きである
  （`StPicoTrack.h:120`、`StPicoTrack.cxx:217`以降）。この計算をKF専用adapterへ移す。
  公開APIの`gMom()`、`dEdx()`、`dEdxError()`を用い、元と同じ単位変換・βγ・fit設定で
  SL24yの`StBichsel/StdEdxPull::Eval()`を呼ぶ方針とする。
  そのヘッダー・ソースと`StBichsel.so`の存在は確認済みであり、link／実データ値の一致を検証する。
  エラー値を有効なpullとして採用せず、参照入力に対応できない場合は黙ってTPC nSigmaへfallbackしない。
- 現行scalar版の陽子DCA≥0.7 cm、π DCA≥1 cmなどを、隠れた必須前段カットとして残さない。
  covarianceに基づくprimary／secondary分類とFinderカットを主体とする。
  追加の幾何学カットは用途・値をYAMLに明示し、参照再現設定では無効化できるようにする。
- `nHitsFit`、`nHitsDedx`、fit/max比、dEdxError、pT、η、HFT条件もdatasetごとに設定する。
  参照コードではBES-II向けにhit比カットがコメントアウトされており、現行の0.52を無条件に持ち込まない。

### 4.3 PV分類とFinder設定

Λ参照macroに実際にある値を、比較用profileの出発点とする。
これは全データに対する最適値の保証ではない。

| 設定先 | Λ参照macroの値 | 注意点 |
| --- | --- | --- |
| Interface `SetChiPrimaryCut` | 3 | Interfaceのprimary track分類用 |
| Finder `SetMaxDistanceBetweenParticlesCut` | 1.5 | 距離、cm |
| Finder `SetLCut` | 1.0 | PVからの距離に関する内部カット、cm |
| Finder `SetChiPrimaryCut2D` | 3 | 2-daughter探索用。上の分類設定と同一キーにしない |
| Finder `SetChi2Cut2D` | 10 | fit品質。採用版の比較式／NDFの扱いを確認して保存 |
| Finder `SetLdLCut2D` | 3 | 距離の有意度。現行helperのdecay-length significanceと混同しない |
| Interface `SetSoftTofPidMode` | 有効 | TOFの複数PID仮説を許容 |
| Interface `SetSoftKaonPIDMode` | 有効 | 参照設定として記録。Λ専用π/p仮説への実効影響も確認 |
| `AddDecayToReconstructionList` | +3122と-3122 | 採用Finderは符号付きで照合するため両方登録 |

`SetPrimaryProbCut()`によるTopoの閾値設定も別に存在する。確率、χ²、χ²/NDF、
`GetDeviationFromVertex()`の返り値を混同せず、各キーを実際のsetter／比較式に対応付ける。
API名の「Chi」だけを根拠に平方・平方根を追加しない。

採用`.DEV2`には`SetChi2TopoCut2D()`はない。
`SetSecondaryCuts(massSigma, chi2Topo, ldl)`は質量窓を伴う候補分類にも使われるため、
これを「全Λの最終topologyカット」として誤用しない。
必要な最終χ²topo/NDFは、次節のPV拘束コピーで計算して明示的に適用する。

setterで指定する値以外の内部条件も監査対象にする。採用Finderの2-body経路には、
有限かつ正のχ²、`lMin < 200 cm`、PV由来の幾何学条件等がある
（`KFParticleFinder.cxx:816`以降）。候補分類用のmass幅はdatabaseのpeak sigmaであり、
各候補のfitから得たmass errorとは異なる（同`:868`付近）。
初期実装ではこれらのupstream内部条件を維持し、provenance／比較仕様へ記録する。
YAMLで変更できる設定値と区別し、YAMLの外で効いているカットを隠さない。

### 4.4 Λ候補、daughter ID、拘束の扱い

1. `GetParticles()`からPDGが±3122、daughter数が2の候補を抽出する。
2. compositeの`DaughterIds()`は粒子配列のindex、track由来粒子のdaughter IDは入力track IDである。
   PicoDstの配列indexと`StPicoTrack::id()`も別物なので、明示的な対応表を作る。
3. SortTracks、複数PID仮説、非連続track IDを含めて対応を検証する。
   proton／pionの順序を固定indexと決めつけず、daughter PDG・電荷から判別する。
   同じ元trackを二重使用する候補を排除し、不正IDは計数・診断する。
4. 質量スペクトルには**親Λへの質量拘束をしていない候補**のmassとerrorを使用する。
   `GetSecondaryLambda()`等の質量拘束済みコンテナと混ぜない。
   daughterへの質量仮説と親へのPDG質量拘束は区別する。
5. 元候補を保持し、別コピーに`SetProductionVertex(PV)`を適用してχ²topo/NDF、
   decay length、error等を評価する。拘束前fit χ²/NDF、vertex-lineのL/σL、
   PV拘束後の量には別名を付け、返り値・NDF・有限性を検査する。
6. mass拘束候補やprimary／secondaryコンテナも必要な比較・分類に利用可能とするが、
   inclusive Λ、PV整合Λ、質量拘束Λを同じサンプルとして数えない。
7. 出力はKF専用データ構造で保持し、PDG符号、元track index／ID、fit量、
   適用cut段階、使用したcore／設定profileを追跡可能にする。既存`StLambda`は変更しない。

## 5. ビルドと既存解析の安全性

### 5.1 フルcoreのソース範囲

既存4実装に加え、少なくとも同じ版の以下と、そのinclude依存を取り込む。

- `KFVertex`: 一つの一次頂点（PV）の位置と誤差を表し、trackを追加・除去しながら
  カルマンフィルターで頂点をfitするクラス。今回もPicoDstのPVをKF側で扱うために使う。
- `KFPTrackVector`: 多数の入力trackの位置・運動量・共分散、電荷、PID仮説、ID、
  対応するPVなどを、SIMDで扱いやすい配列形式にまとめて保持するクラス。
- `KFPEmcCluster`: 電磁カロリメータ（EMC）のclusterの位置・エネルギー・共分散・IDを
  まとめて保持するクラス。今回のΛ→pπ解析ではEMCデータを入力しないが、
  coreのコンパイル依存として含める。
- `KFParticleSIMD`: 複数の粒子候補に対するKF fit、軌跡の輸送、距離・誤差の計算を、
  CPUのベクトル演算でまとめて行う版の`KFParticle`。複数スレッドでの実行とは別の高速化手法。
- `KFParticlePVReconstructor`: trackを頂点ごとにまとめ、`KFVertex`を使って一次頂点を
  探索・fitし、trackとの対応を管理するクラス。複数PVや外部で求めたPVの登録にも対応する。
  今回はPicoDstの既存PVを登録して使い、PVの再探索・再fitを自動では有効にしない。
- `KFParticleFinder`: 入力trackのPID仮説とPVを使い、娘粒子を組み合わせて崩壊点などをfitし、
  設定した条件でΛなどの短寿命粒子候補を選ぶクラス。再構成済みの粒子を使う多段崩壊にも対応する。
- `KFParticleTopoReconstructor`: イベント全体の再構成を取りまとめるクラス。
  PVの管理、trackのprimary／secondary分類・並べ替え、Finderの呼び出しを行い、
  再構成した粒子候補を解析側へ提供する。
- 必要なMath、InputData、Fieldヘッダーと`KFPSimd`／allocator一式

globでPerformanceや実行programまで無差別にコンパイルせず、再構成coreの依存閉包を明示する。
新規実装の数学部分を独自に書き直すのではなく、upstreamの処理を保ったまま組み込む。

### 5.2 ROOT定義とSIMD

- `KFParticleStandalone`で内部KF型のdictionaryを省略する方針を維持できる。
  Maker等、外部公開に必要なdictionaryは従来どおりROOT 5で生成する。
  内部型のROOT streamingが必要になった場合は、その型のROOT 5 dictionary生成を別途追加する。
- `__ROOT__`はROOTのmajor version指定ではなくSTAR用条件分岐である。
  `.DEV2/KFParticleFinder.cxx:1056`付近では、この有無でsecondary候補への
  χ²topo最小値<500の条件が変わる。また`KFParticle.h`内のデータメンバーも変わる。
  単にコンパイルを通すために外すと、STAR版と違う選別・ABIになる。
- 原則としてKF専用target内で`__ROOT__`、`HomogeneousField`、`KFParticleStandalone`を
  一貫して使用し、STAR用分岐を維持する。adapter／Makerの実装単位も同じ定義に揃える。
  `TRVector`／`TRSymMatrix`等、STAR用分岐の追加依存はSL24yの提供物と照合する。
- 調査した`KFPSimd`はSSE実装が選択される。`-msse4.1`等をKF専用targetに限定し、
  対象ノードのCPU条件も確認する。無条件な`-march=native`で未対応AVX経路へ切り替えない。
- KFヘッダーは可能な限りMaker公開ヘッダーから隠し、前方宣言／実装保持クラスを使って、
  CINT・ACLiCや既存MakerへSIMDとdefineが漏れないようにする。

### 5.3 今回見つかった同名クラス・シンボル衝突

SL24yに独立した新型`KFParticle`パッケージが見当たらなくても、
`StarRoot`には旧`KFParticle`／`KFParticleBase`が含まれている。
以下をヘッダーと既存バイナリの読み取りで確認した。

- `/cvmfs/star.sdcc.bnl.gov/packages/SL24y/StRoot/StarRoot/StarRootLinkDef.h`は旧型を公開する。
- 同SL24yの`.sl73_x8664_gcc485/lib/StarRoot.so`と現行`lib/libKFParticle.so`が、
  同じ`KFParticle::fgBz`シンボルをそれぞれ8 byte／4 byteで定義している。
  同名のvtable／RTTI／メソッドも存在し、クラスのABIが異なる。
- SL24yの`StRoot/macros/rootlogon.C:7`は`libStarRoot`をロードする。
  通常のSTAR起動で旧型が先に存在し得る。`libKFParticle.so`というファイル名だけの重複検査では不十分。

これは今回実測したクラッシュの報告ではなく、シンボル表から確認した衝突リスクである。
以前の0イベントのロード成功だけでは、この問題がないとは判断できない。

対策として、移植coreの型・関数をプロジェクト専用namespace（例: `star_analyzer_kfp`）へ隔離する。
include guard等の衝突も調査し、外部のROOT／STL／STARヘッダーまでnamespaceで囲まない。
これは機械的な識別名の隔離patchとして記録し、fitやFinderの数式・カットを変更しない。
adapterも独自クラス名にし、グローバルな`StKFParticleInterface`と競合させない。

ライブラリのロード順変更、ファイル名変更、`-Bsymbolic`だけを安全性の根拠にしない。
`nm`／`readelf`によるexport検査と、旧`StarRoot`をロード済みのプロセスでの動作検証を必須にする。
既存scalar版のobject／ACLiC cacheとはABIが変わるため、KF関連を整合した状態で再ビルドし、
古いROOTプロセスを使い回さず新しいプロセスで検証する。

### 5.4 `make all`と依存分離

- `all: core kfparticle-analysis`を維持し、通常の`make all`でフルKF版までビルドする。
  失敗時に黙ってKFを省略して成功扱いにしない。
- `make core`は引き続き新規KFライブラリ非依存で、既存解析のみをビルドできる。
- フルcoreは`libKFParticle.so`、adapterは`libStKfParticleCommon.so`、
  解析は`libStLambdaKFParticleMaker.so`へ分離する。coreのC++シンボルは前節のnamespaceで隔離する。
- `libStCommon.so`、既存Makerのinclude／link／defineに新規KF依存を加えない。
  `ConfigManager`には設定データのみを追加し、KFヘッダー／ライブラリを要求しない。
- PID adapterに必要な`StBichsel`等の追加リンク・ロードもKF専用経路に限定する。
- `script/singularity_make.sh`の通常経路を利用する。`.DEV2` runtimeや`cons`への切替は要求しない。
- Makefileのソース、ヘッダー、namespace patch、define変更が正しく再ビルドへ反映されるよう依存を管理する。

### 5.5 変更しないもの

- `StMaker/StLambdaMaker/`内の既存ファイル
- 既存`StLambda`のclass layout、API、出力形式
- `analysis/anaLambda.C`、`analysis/run_anaLambda.C`
- `script/run_anaLambda.sh`と既存解析のmainconf／cut値
- ユーザーの無関係な作業中ファイル、既存ROOT出力、他ユーザーのコード

KF設定がない従来のmainconfも従来どおり動作させる。
KF専用の出力先を使い、以前のHelix解析結果を上書きしない。

## 6. 変更予定ファイル

| ファイル／領域 | 修正内容 |
| --- | --- |
| `StRoot/KFParticle/` | 同一版のフルcore、必要依存、namespace隔離patch、provenance更新 |
| `StMaker/kfparticle/` | Pico Interface移植、PID、covariance、PV、ID対応、候補結果型 |
| `StMaker/StLambdaKFParticleMaker/` | 手動ペアループからイベント単位Finder呼出しへ変更、選別・QA |
| `analysis/anaLambda_KFParticle.C` | 新経路の起動、branch／入力検査、エラー・0イベントの明確化 |
| `analysis/run_anaLambda_KFParticle.C` | 必要ライブラリのロード、ABI／ACLiC整合、失敗時の明確な終了 |
| `include/cuts/KfParticleCutConfig.h`、`src/cuts/KfParticleCutConfig.cpp` | Finder／Interface／最終選別を分離した設定 |
| `config/cuts/kf/`、KF専用mainconf／hist設定 | データ別・参照比較profile、カット段階別QA |
| `Makefile` | フルcore sources、専用flags／依存、`all`／`core`分離 |
| KF専用実行script | 必要な場合のみ、入力異常を成功扱いにしない修正 |
| `mdfiles/`、`PROVENANCE.md`、作業ログ | 実装差分、実コマンド、検証結果・未検証事項を追記 |

設定は既存YAML parserで扱える形式にし、未対応の階層構造を読み取れると仮定しない。
KF設定にschema／backend識別を設け、旧scalar用カットを新Finder用として黙って再解釈しない。
KF解析では不明・無効なキーを診断し、最終的な実効カット、PID profile、全defineをログへ出す。
config配下はgitで無視される場合があるため、変更した実ファイルの一覧も記録する。

## 7. 実装順序と完了条件

### Phase A: 参照固定・依存・衝突の解決

1. coreのsnapshotとInterface移植元を固定し、関数対応表・ローカルpatch一覧を作る。
2. フルcoreと専用namespaceを導入し、SL24yでコンパイルする。
3. KF専用define／SIMD設定／追加STAR依存を整合させ、export衝突がないことを確認する。
4. ROOT 5で最小Topo／Finderテストを行い、±3122登録、PV設定、候補取得まで実行する。

### Phase B: Pico InterfaceとΛ解析の接続

1. covariance変換、PID、track仮説、PV分類、ID対応を移植する。
2. Makerの手動ペアループを置き換え、macroから本経路を呼び出す。
3. raw mass／PV拘束量／daughter情報／符号別QAを実装する。
4. 参照profileと現行データ用profileを用意し、追加カットと校正差分を可視化する。

### Phase C: ビルド・実データ・非退行検証

将来の実装時に実施するビルド例（本書作成時には実行しない）:

```bash
./script/singularity_make.sh config/mainconf/main_auau19_anaLambda.yaml core
./script/singularity_make.sh config/mainconf/main_auau19_anaLambda_KFParticle.yaml all
```

wrapperは既定でcleanを実行するため、実装開始時に既存生成物・他作業への影響を確認する。

必須の確認項目:

- clean `make core`、clean通常`make all`、KF専用target、増分・並列buildが成功する。
- 旧Helix経路に新規KF依存がなく、旧解析macroのコンパイル・ロード・実行が維持される。
- `StarRoot`を先にロードした実際のROOT 5環境で、新KF型のconstructor／fit／Finderを実行する。
  シンボル衝突、未解決参照、dictionary重複、ROOT version mismatchを検査する。
- 到達可能なPicoDstを実装時に改めて確保し、`TrackCovMatrix`が存在し、処理イベント数が正であることを確認する。
  過去の`/home/starlib/...`や計画中のbenchファイルが現在も存在するとは仮定しない。
- 入力なし・branchなし・0イベントを「Λ再構成の成功」としない。
  明示的なsmoke testと物理検証を分け、原因と処理件数を返す。
- 同じcore、磁場、PV、track入力、PID／cut設定で参照Interface経路と移植経路を比較する。
  accepted track／仮説／primary分類／Λ候補の元trackペアとfit量を照合する。
  旧論文coreとの差は、adapter差とは分けて評価する。
- covariance変換と`dEdxPull` adapterの要素順・符号・単位・数値、TOF index 0、TOF欠測、非連続ID、複数PID、
  track並べ替え、同一track再使用、イベント切替・磁場変更をテストする。
- Λとanti-Λそれぞれに、拘束前mass、mass error、fit χ²/NDF、χ²topo/NDF、
  L／σL、decay length、pT／rapidity、daughter PIDの分布を出す。
  十分な統計で質量構造を確認し、親質量拘束による人工的なピークでないことを検証する。
- raw tracks→covariance有効→PID仮説→Finder出力→±3122→最終選別の段階別件数を記録する。
  候補が0でも、どの段階で消えたか説明できるようにする。
- 同じ入力で既存Helix解析の変更前後のevent数・候補数・主要histogramを比較する。
  KFとHelixの結果が一致することではなく、既存Helix解析自体が変わっていないことを確認する。

完了とは、ROOT 5でフルFinder／Topo経路が動き、実イベントからΛを選別でき、
参照との差分と既存解析への非退行が検証された状態を指す。
コンパイル・0イベントのInit／Finishだけ、あるいはscalar版へのfallbackでは完了としない。

## 8. この計画作成時点の実施範囲

今回行ったのはログ・既存計画・現在の実装・参照ソース・既存バイナリの読み取り調査と、
本書の新規作成のみ。解析ソース、Makefile、設定、STAR環境は変更していない。
フルKF版のbuild、実イベント実行、既存出力の再生成はまだ行っていない。

## 9. 2026-09-07 実装時の追記

ユーザー承認後、scalar手動ペアfitを本番経路から外し、
`StPicoKFParticleInterface → KFParticleTopoReconstructor → KFParticleFinder`へ移行した。
同日の追加指定により、実イベント検証はAuAu19ではなく
`config/picoDstList/auau13p5GeV.list`を用い、13p5専用のmainconfと参照先10設定を新規作成した。
既存Helixソース・`StLambda`・既存13p5設定は変更しない。

- .DEV2同一snapshotの39ファイル／11実装を取り込み、`star_analyzer_kfp`へ隔離。
  ハッシュ、namespace変換、ROOT reflectionだけの互換patchはcoreのPROVENANCE／manifestに記録。
- 参照Pico PID／TOF・PV分類・ID対応を移植。旧Helix daughter DCAカットへの暗黙fallbackはない。
- current TopoのΛ内部カットはPV拘束コピーのχ²/NDF < 3。
  実装時の再調査では旧xwu2版は < 5だった。raw parent massは質量拘束なしだが、
  Topoの内部選別後のサンプルである。この差を隠して同じ効率と扱わない。
- ROOT 5でフルcore／adapter／Maker／ACLiCのコンパイル・ロードと、
  13p5実イベントによるΛ候補出力まで到達した。ROOT 6・STAR release変更は行っていない。
- この環境のROOT 5には`Netx`がなく、リストの`root://`を直接開けなかった。
  ホストの`xrdcp`で同じ先頭PicoDstをローカル取得して検証した。
- 初回dE/dxモデル初期化のクラッシュは、CINTの`const`宣言初期化中の再入で再現した。
  runnerの宣言と`ProcessLine`代入を分離して修正した。STAR table、校正、PID式は変更しない。
- 13p5のTOF係数は`xwu2_lambda_reference_unvalidated_auau13p5`として明示した。
  実行成功とRun20での校正・効率の妥当性は別である。
- 完了済みの検査、出力件数、未検証項目、再実行コマンドは
  [実装・検証記録](kfparticle_full_implementation_20260907.md)へ記載する。
  0イベントの初期化成功や少数候補を、質量ピーク・参照解析との効率一致の証明とはしない。

## 10. 2026-09-07 再開: analysis.modeとvertex cutの接続

最新のユーザー指示に従い、既存の `analysis.mode: refmult / fxtmult` をKF解析の実行時にも参照する。
調査した既存実装は `script/setup_config_from_analysisinfo.py` による**設定生成時のVz切替のみ**で、
XY中心の自動切替は存在しなかった。さらに旧 `StLambdaMaker::PassEventCuts` は `maxNTr` のみを適用していた。
これらの既存挙動は今回変更せず、前回問題になった新KF解析の原点中心VrをKF専用で修正した。

- `StMaker/kfparticle/KfEventSelection.{h,cxx}` がmainconf→analysis-infoのmodeを読み、
  event YAMLの `vertexByMode.<mode>` を選ぶ。macroはmainconfのパスをMakerへ渡すだけ。
- 各profileは `vzRange: [min,max]`、`center: [x,y]`、`radius`（cm）。
  colliderは[-100,100]、中心(0,0)、FXTは[198,202]、中心(-0.4,-2.0)、半径2をYAMLに記録した。
  FXT中心は前回確認したxwu2参照値であり、すべてのFXTデータに対する校正値とはしない。
- KFに限りprofileのvertex値が従来のflat vertex値より優先される。
  RefMult／valid VPD／maxNTrはevent YAMLの値を引き続き利用する。
  共有 `EventCutConfig` singletonは書き換えず、KF helper内の設定コピーで処理する。
- 不明／欠落mode、profile不足、重複・不正profile、有限値／範囲違反、
  有効centralityのmodeとの不一致は初期化エラー。失敗後に以前の設定へfallbackしない。
- AuAu13p5専用eventにprofileを追加し、AuAu19も専用eventコピーへ参照を分離した。
  既存Helix YAML、共有設定クラス、汎用設定生成scriptは変更していない。
- `KFEventSelectionConfiguration` と `hKfEventSelection`、vertex XY／radius／Z QAで実効値と排他的な棄却理由を保存。
  旧Event-only診断は階層profileに未対応のため明示的に拒否し、誤った原点中心の診断をさせない。
- 通常clean `make all` と両人工テストはそれぞれexit 0。13p5先頭1000イベントのKF処理は12→957へ改善し、
  同じ入力ファイルの全1479イベントも正常処理した。詳細・出力QA・非退行の結果は
  [実装・検証記録](kfparticle_full_implementation_20260907.md)を参照する。

本節は実装・限定サンプル検証の記録であり、元のPhase Cにある全42ファイル検証、
同一coreでの独立参照Interface数値比較、Run20 PID／TOF校正の妥当性まで完了したとはしない。
