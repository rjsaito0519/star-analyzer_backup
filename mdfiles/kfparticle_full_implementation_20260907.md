# KFParticle full Λ再構成: 実装・検証記録

日付: 2026-09-07（再開セッション、EDT）  
状態: full Finder／Topo経路およびモード別vertex選別を実装。ROOT 5でclean build、人工テスト、AuAu13p5先頭1ファイルの実イベント・出力QA・限定Helix回帰比較を確認。全データ・校正・独立参照比較まで完了したものではない。

関連: [承認済み計画](plan_kfparticle_lambda_full_reconstruction.md)、[同日の日報](../analysisnote/20260907/summary20260907.md)。
前半セッションの実装詳細・失敗調査は日報§2–7に記録済み。本書では再開後に確認した最終状態を区別する。

## 1. 再構成の構成とROOT環境

`anaLambda_KFParticle.C → StLambdaKFParticleMaker → StPicoKFParticleInterface → KFParticleTopoReconstructor → KFParticleFinder`。

macroはStChainの構築・イベントループ・mainconfパス引渡しだけを行う。
track covariance、PID仮説、PV分類、Finderによる±3122探索、raw massとPV拘束コピーの量はコンパイル済みKFライブラリ内で処理する。
scalar手動pair fitへのfallbackはない。既存StLambda／StLambdaMakerのAPI・配置・出力は変更しない。

- STAR SL24y、sl73_x8664_gcc485、ROOT 5.34/38、GCC 4.8.5、STAR batch相当Singularity。
- ROOT 6／別STAR releaseは導入していない。
- .DEV2の39ファイル／11実装を同じsnapshotから取り込み、`star_analyzer_kfp`へ隔離した。
  snapshot ID: `033c38b7128ea7cb351a2785a3a65ff0ec2e19e391ca3680d8ffd4d17c27ceb5`。
- `make all` は旧coreとフルKFを構築する。今回追加したyaml-cpp includeもKF helper flagsのみ。
- 再開前のCINTのconst初期化問題は、runnerの宣言とProcessLine代入を分離した修正を維持。
- raw parent massは親質量拘束なし。ただし採用TopoのPV χ²/NDF < 3等の内部選別後である。
  旧xwu2版の < 5との差を、同じ効率・同じ候補集合と扱わない。

## 2. analysis.modeとvertex cut

実装を再調査した結果:

- 既存 `setup_config_from_analysisinfo.py` は、analysis-infoの `analysis.mode` から**設定生成時にVzのみ**を切り替える。
  `fxtmult` は198–202 cm、`refmult` は−100–100 cm。
- 既存 `ConfigManager` はanalysis-infoからanaNameを読むが、実行時のmode別vertex変更はしない。
- `EventCutConfig` のXY中心は指定がなければ(0,0)。
- 旧 `StLambdaMaker::PassEventCuts` はmaxNTrのみで、Vz／Vrを実際には適用していなかった。
  前回の旧Helix実行成功だけでvertex cutの正しさは判断できない。**旧解析のこの挙動は変更していない。**

新しいKF専用 `KfEventSelection` がmainconfの `analysis:` と `event:` を読み、
`analysis.mode` に対応する `event.vertexByMode.<mode>` を実行時に選択する。

| mode | Vz [cm] | XY中心 [cm] | 半径 [cm] |
| --- | --- | --- | --- |
| refmult（collider） | [-100,100] | (0,0) | 2 |
| fxtmult（fixed target） | [198,202] | (-0.4,-2.0) | 2 |

これらの数値はC++に固定せず、KF専用event YAML内に置いた。
FXTのXY中心は前回確認したxwu2 `IsGoodPV` の参照値であり、Run20や全FXTデータのbeam calibrationを測定した値ではない。

設定例（mainconfが参照するevent YAML）:

```yaml
vertexByMode:
  refmult:
    vzRange: [-100.0, 100.0]
    center: [0.0, 0.0]
    radius: 2.0
  fxtmult:
    vzRange: [198.0, 202.0]
    center: [-0.4, -2.0]
    radius: 2.0
```

優先順位・安全性:

- KFではprofileのVz／XY／半径がflatなminVz／maxVz／vtxCenterX/Y／maxVrより優先。
- RefMult、valid VPD、maxNTrは既存event設定を引き続き適用。
- 共有EventCutConfig singletonを変更せず、KF内へコピーする。
- unknown／missing mode、必要profile欠落、重複、不正型・次元・非有限値・範囲違反は初期化エラー。
- enabledなcentrality.modeがanalysis.modeと不一致なら拒否。誤設定後に以前のprofileを使い続けない。
- 既存parserは階層を平坦化するため、profile内ではflatキーと異なるvzRange／center／radiusを用いた。
  KF helperはyaml-cppで階層を明示的に読む。
- AuAu19もKF専用eventコピーを新規作成し、旧Helix設定の共有参照から分離。
- モードだけを変えても別データ用のPID、centrality校正、histogram軸まで切り替わるわけではない。
- 汎用設定生成scriptは変更していない。KFへのmode適用にこのscriptを再実行しないこと。
  generic macro生成は独自KF macroを上書きする可能性がある。

## 3. ビルドと人工テスト

通常clean allとテストを別コマンドで確認し、**いずれもexit 0**。

```bash
./script/singularity_make.sh config/mainconf/main_auau13p5_anaLambda_KFParticle.yaml all -j4

./script/singularity_make.sh config/mainconf/main_auau13p5_anaLambda_KFParticle.yaml --no-clean test-kfparticle-full-chain test-kfparticle-pico-adapter KF_TEST_CUTS=config/cuts/kf/kf_auau13p5_anaLambda_KFParticle.yaml -j4
```

- [clean allログ](../analysisnote/20260907/kfparticle_resume_logs/clean_all.log)
- [人工テストログ](../analysisnote/20260907/kfparticle_resume_logs/synthetic_tests.log)

full-chain: Λ／anti-Λ、磁場±5、ID／Sort、複数PID、rawと拘束コピーの分離、reset。
Pico adapter: 6 parameter／21 covariance、TOF index 0／欠測／不正参照、dEdxPull数値対応、strict KF schema。
今回追加: 両mode、profile優先順位、共有設定の非変更、境界、RefMult／VPD／track数、
相対／絶対参照、mode不一致・不正YAML・失敗後の拒否。

再開前のclean複合実行exit 2は、日報記載のテストassertion問題であり、本節の成功ログとは別。
engineの数式やカットを変えてテストを通したわけではない。

## 4. AuAu13p5実イベントと出力QA

mainconf: `config/mainconf/main_auau13p5_anaLambda_KFParticle.yaml`  
元入力リスト: `config/picoDstList/auau13p5GeV.list`（42ファイル、未変更）  
実入力: その先頭1ファイルのlocal copy  
`/tmp/star-kf-validation-20260907.Ho7eLz/auau13p5_first.picoDst.root`（69,953,736 bytes、1479イベント）

ROOT 5側にNetx pluginがないため、前半セッションでhost xrdcpから取得した同じPicoDstを再利用。
今回remote入力リスト全体は処理していない。/tmpの存続は保証しない。

| 実行 | 先頭1000イベント | 先頭ファイル全1479イベント |
| --- | ---: | ---: |
| local jobid | validation13p5mode | validation13p5mode1479 |
| 読取数 | 1000 | 1479 |
| event cut通過（centrality前） | 996 | 1475 |
| Vr棄却 | 1 | 1 |
| VPD差棄却 | 3 | 3 |
| centrality invalid | 39 | 53 |
| KF処理 | 957 | 1422 |
| Topo Λ候補 | 15011 | 22223 |
| candidate検査棄却 | 206 | 282 |
| 有効raw候補 | 14805 | 21941 |
| 最終Λ | 3189 | 4701 |
| 最終anti-Λ | 463 | 716 |
| 解析／出力QA終了コード | 0 / 0 | 0 / 0 |

bad run、非有限PV、Vz、RefMult、track数、pileup、centrality-bin、KF errorの各棄却は両実行とも0。
1000イベントで前回のKF処理12→957へ改善した主因は、原点中心のVr棄却982→1である。
これは入力統計の増加ではなく、同じ先頭1000イベントでの比較。

出力:

- [local_mode_1000.root](../rootfile/auau13p5_anaLambda_KFParticle_validation_20260907/local_mode_1000.root)
- [local_mode_1479.root](../rootfile/auau13p5_anaLambda_KFParticle_validation_20260907/local_mode_1479.root)
- [1000実行ログ](../analysisnote/20260907/kfparticle_resume_logs/local_mode_1000.log)／[出力QA](../analysisnote/20260907/kfparticle_resume_logs/output_qa_mode_1000.log)
- [1479実行ログ](../analysisnote/20260907/kfparticle_resume_logs/local_mode_1479.log)／[出力QA](../analysisnote/20260907/kfparticle_resume_logs/output_qa_mode_1479.log)

farm configlog: **なし**（farm未投入、jobidはlocal label）。
ROOT内にKFParticleEffectiveConfiguration、KFEventSelectionConfiguration、backend fingerprint、KFRunStatus=completedを保存。
実効mode=fxtmult、中心(-0.4,-2)、半径2、Vz[198,202]をROOT metadataでも確認した。

QAはイベント棄却数の総和、stage／tree／符号別histの件数、ID、有限値・正の誤差、vertex分布と実効cutの整合性を検査。
1479イベントのevent-cut後平均PVは(-0.223472,-2.28041,199.968) cm、有効半径の平均0.384745 cm。
raw Λ massの最多binは1.115–1.116 GeV/c²（107件）。
これは記述的な分布確認であり、背景fit・ピーク有意度・効率・物理yieldの確定ではない。

再実行例（既存出力は上書き拒否されるので、新しい出力名を指定）:

```bash
./script/singularity_run_anaLambda_KFParticle.sh /tmp/star-kf-validation-20260907.Ho7eLz/auau13p5_first.picoDst.root rootfile/auau13p5_anaLambda_KFParticle_validation_20260907/NEW_RESULT.root validation13p5 -1 config/mainconf/main_auau13p5_anaLambda_KFParticle.yaml
```

## 5. 誤った成功判定を防ぐ否定テスト

- 旧出力にはmode metadataがないため、新QAはexit 6で拒否。
  [ログ](../analysisnote/20260907/kfparticle_resume_logs/output_qa_old_metadata_rejected.log)。
- legacy Event-only診断へvertexByMode付きYAMLを渡すと、入力解析前にexit 1で拒否。
  TEnvで階層を無視して原点cutと誤診断させない。
  [ログ](../analysisnote/20260907/kfparticle_resume_logs/event_legacy_nested_rejected.log)。

新しいevent cutの診断には、実行ROOTのKFEventSelectionConfiguration／hKfEventSelectionと新出力QAを利用する。

## 6. 既存Helix解析の保護

- 保護対象7ファイルのSHA256が前回基準と一致（旧StLambdaMaker、旧macro／runner／run script、共通Helix helper）。
- 再build後のlibStLambdaMaker.so／libStCommon.so／libStarAnaConfig.soにKFのDT_NEEDEDなし、ROOTは.so.5。
- 同じPicoDst先頭20イベントを旧mainconfで再実行。Finish20、centrality ok17／invalid3。
  mainconf: `config/mainconf/main_auau13p5_anaLambda.yaml`、jobid: `helixmodecontrol20`、farm configlogなし。
- 旧 `helix_control_20.root` と新 `helix_mode_control_20.root` を
  `tests/compare_helix_histograms.C` で厳密比較し、**46ヒストグラム、2,469,952セル、差分0、比較exit 0**。
  型・軸・ラベル・entries・bin内容・誤差・under/overflowを含む。
  [比較ログ](../analysisnote/20260907/kfparticle_resume_logs/helix_comparison.log)。
- 比較macroの初回はROOT 5にないGetNcellsを使って失敗したため、新規比較macroだけを公開GetBin APIへ修正して再実行した。
  [初回ログ](../analysisnote/20260907/kfparticle_resume_logs/helix_comparison_first_attempt.log)。
- これは今回再開前後の限定20イベントにおける非退行確認であり、全解析・全統計での一致保証ではない。

## 7. 再開セッションの変更ファイル

- `StMaker/kfparticle/KfEventSelection.{h,cxx}`（新規）
- `StMaker/StLambdaKFParticleMaker/StLambdaKFParticleMaker.{h,cxx}`
- `analysis/anaLambda_KFParticle.C`
- `Makefile`（KF helperのyaml-cpp include／test header依存のみ追加）
- `config/cuts/event/event_auau13p5_anaLambda_KFParticle.yaml`
- `config/cuts/event/event_auau19_anaLambda_KFParticle.yaml`（新規）
- `config/mainconf/main_auau19_anaLambda_KFParticle.yaml`
- `config/hist/hist_auau13p5_anaLambda_KFParticle.yaml`、`hist_auau19_anaLambda_KFParticle.yaml`
- `tests/kfparticle_event_selection.h`（新規）、`tests/kfparticle_pico_adapter.cxx`
- `tests/check_kfparticle_output.C`、`tests/inspect_kf_input_events.C`、`tests/compare_helix_histograms.C`（新規）
- `docs/REFERENCE.md`、計画§10、本書、同日日報と付属ログ

前半セッションから存在するConfigManager等の変更と、無関係なユーザー変更は区別する。
今回の再開で共有ConfigManager／EventCutConfig／centrality実装、旧Helix設定・ソースは変更していない。
config／tests／analysisnoteの一部はgitignore対象だがworkspaceには保存されている。commit／push／farm submitは未実施。

## 8. 次に残る検証

- リスト全42ファイル、複数runでの安定性と統計。
- Run20 AuAu13p5へのTPC／TOF校正妥当性。現状TOF profileはxwu2 reference unvalidated。
- 同一core・同じ入力／設定を用いた、独立した参照Interfaceとの候補ペア・fit量の数値比較。
- background／signal分離、質量ピーク有意度、PID／topology効率、物理yield。
- 現行upstreamと旧ユーザーcopyとの内部cut差を含めた系統比較。

今回の「モード別vertex対応と再検証」は終了したが、これらを未検証のまま本番物理解析の全面検証完了とは扱わない。
