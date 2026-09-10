# AuAu13p5 Λ解析：未使用カット設定の整理（2026-09-08）

## 結果

対象は現行のAuAu13p5旧Helix、標準KF、Imp5 KFの3設定。未使用条件を整理し、実際の選別条件を変えていないことを再実行で確認した。farm job投入はしていない。

| 整理前後の比較 | 入力イベント数 | 比較したhistogram数 / cell数 | 差分 |
|---|---:|---:|---:|
| 旧Helix | 20 | 46 / 2469952 | 0 |
| 標準KF | 1000 | 68 / 2536396 | 0 |
| Imp5 KF | 10000 | 68 / 2536396 | 0 |

クラス・次元・軸・ラベル・entries・全bin内容/誤差（underflow/overflow込み）を比較した。Tree/metadataのバイト一致試験ではない。新しいmetadata形式と候補選別は別途KF出力QAで検査し、こちらも成功した。

Imp5の10,000イベントは引き続きreconstructed=9744、selected Λ=3135。全mass binも一致するため、[直前の質量比較図](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle_Imp5/lambda_mass_compare_10000_20260908.png) の結果は変わらない。図は上書き・再生成していない。

## 1. mainconfを実際に使う設定だけに整理

| mainconf | 現在の参照キー |
|---|---|
| [main_auau13p5_anaLambda.yaml](../config/mainconf/main_auau13p5_anaLambda.yaml) | event / centrality / lambda / hist / analysis |
| [main_auau13p5_anaLambda_KFParticle.yaml](../config/mainconf/main_auau13p5_anaLambda_KFParticle.yaml) | event / centrality / kf / hist / analysis |
| [main_auau13p5_anaLambda_KFParticle_Imp5.yaml](../config/mainconf/main_auau13p5_anaLambda_KFParticle_Imp5.yaml) | event / centrality / kf / hist / analysis |

旧Helixのtrack/PID/topologyは `lambda:`、KFでは `kf:` が唯一の実効参照先。読み込むだけだったgeneric track / PID / V0 / mixingと、KF用の旧Lambda互換設定への参照を削除した。

これからPIDを変更する場合は以下を編集する:

- 旧Helix: [maker_auau13p5_anaLambda.yaml](../config/maker/maker_auau13p5_anaLambda.yaml)。
- 標準KF: [kf_auau13p5_anaLambda_KFParticle.yaml](../config/cuts/kf/kf_auau13p5_anaLambda_KFParticle.yaml)。
- Imp5 KF: [kf_auau13p5_anaLambda_KFParticle_Imp5.yaml](../config/cuts/kf/kf_auau13p5_anaLambda_KFParticle_Imp5.yaml)。

**proton/pionの閾値は各3.0のまま。** 旧HelixとImp5は保存済みnSigma、標準KFは従来どおりdedx_pullであり、今回PID定義を変更していない。標準KFのKaon=2.0とTOFは有効なので残した。Imp5ではKaon/TOFを使わない。

## 2. 削除した未使用YAML（9ファイル）

以下は対象3 mainconf以外の現行mainconfから参照されていないことを確認してから、設定ディレクトリから除去した。過去のログ・job設定snapshot・archiveは履歴なので書き換えていない。

- `config/cuts/track/track_auau13p5_anaLambda.yaml`
- `config/cuts/track/track_auau13p5_anaLambda_KFParticle.yaml`
- `config/cuts/pid/pid_auau13p5_anaLambda.yaml`
- `config/cuts/pid/pid_auau13p5_anaLambda_KFParticle.yaml`
- `config/cuts/v0reco/v0_auau13p5_anaLambda.yaml`
- `config/cuts/v0reco/v0_auau13p5_anaLambda_KFParticle.yaml`
- `config/cuts/mixing/mixing_auau13p5_anaLambda.yaml`
- `config/cuts/mixing/mixing_auau13p5_anaLambda_KFParticle.yaml`
- `config/maker/maker_auau13p5_anaLambda_KFParticle.yaml`

削除前の16設定ファイル（上記9件＋整理した既存7件）を [復元用archive](../share/figure/auau13p5_config_cleanup_20260908/config_before_cleanup_20260908.tar.gz) に保存した。元の相対パスのまま格納している。確認・復元する際は新しい作業用ディレクトリへ展開し、現在のmainconfを不用意に上書きしないこと。

archive SHA256: `351a8c75c2140c96876f185859004a1ff3243d85c6a84ce12eb1f961fca645cf`。

旧 `maker_auau13p5_anaLambda.yaml` は実際に使われ、`anaLambdaNuclearId` とも共有されているため削除・変更していない。他のデータセット用設定や汎用テンプレートも削除していない。

## 3. 残すYAML内の未使用・重複条件を削除

### Event設定

- [旧Helix event](../config/cuts/event/event_auau13p5_anaLambda.yaml): Makerで参照する `maxNTr: 0` だけにした。未使用のminVz/maxVz/maxVr/minRefMult/maxRefMult/maxVzDiff/maxAbsVzVpdを削除。bad-run/pileup/centrality設定は別のcentrality YAMLで従来どおり。
- [標準KF event](../config/cuts/event/event_auau13p5_anaLambda_KFParticle.yaml): `vertexByMode` に上書きされていたflat minVz/maxVz/maxVrを削除。有効なmode別頂点カット・RefMult/VPD/track数条件は維持。
- [新設Imp5 event](../config/cuts/event/event_auau13p5_anaLambda_KFParticle_Imp5.yaml): `maxNTr` と `qaVertexByMode.<mode>.center` だけを保持。選別しないVz/半径/RefMult/VPDの閾値を記載しない。FXTのQA中心(-0.4,-2.0) cmは不変で、これはvertex cutではない。

### KF設定

Imp5 YAMLから以下の14項目を削除:

`minNHitsDedx`, `minPt`, `maxPt`, `minEta`, `maxEta`,
`minDedxError`, `maxDedxError`, `nSigmaKaon`,
`tofCalibrationProfile`, `tofNSigma`, `tofPMax`,
`tofKaonPMin`, `tofKaonPMax`, `primaryProbCut`。

標準KF YAMLからは、kaon cleaningが無効なため未使用の `tofKaonPMin/PMax` と、外部PicoPV経路では選別に作用しない `primaryProbCut` を削除した。

`primaryProbCut` の扱いは実装を追って確認した。PicoDstのPVを外部入力し、PV再構成は呼ばない。確率から設定したFinder側の値も、独立した `finderChiPrimary2D` で直後に上書きされる。したがって、この経路で有効なカットとして表示しない。実際の `interfaceChiPrimaryCut` と `finderChiPrimary2D`、upstream Λ PV χ²/NDF < 3は変更していない。

さらに無効値だったoptional final boundsも入力YAMLから削除した（標準KF9項目、Imp5 6項目）。元と同じ無効デフォルトが維持される。実効metadataでは出力QAとの互換性のため、これらだけは「負値=disabled」と明記して保存する。

`useTof: false`、`strictTofPid: false`、`cleanKaonsWithTof: false`、`useHftTracksOnly: false` などの機能スイッチは、無効状態を明示しデフォルトで誤有効化しないため残している。共通C++設定クラスのフィールドも、他profileや過去YAMLを読む互換性のため残すが、今回の未使用値を有効カットとして表示しない。

## 4. コード・手順の整理

| ファイル | 変更内容 |
|---|---|
| `src/cuts/KfParticleCutConfig.cpp` | Dumpでprofile非使用の数値を出さない。実際のカット計算・デフォルト・読み込み互換性は維持 |
| `StMaker/kfparticle/KfEventSelection.h/.cxx` | Imp5専用QA中心のみのevent形式を追加。標準KFの頂点カットを保持。旧Imp5形式も読み込み可能 |
| `tests/check_kfparticle_output.C` | 新旧Imp5 metadataに対応し、標準KFの範囲検査は維持 |
| `script/setup_config_from_analysisinfo.py` | manual指定mainconfを再生成から保護 |
| `tests/kfparticle_pico_adapter.cxx`, `tests/kfparticle_event_selection.h` | 最小設定・無効値省略・新旧互換・不正設定拒否を検査 |
| 新設 `tests/test_manual_config_guard.py` | 再生成保護の一時環境試験 |
| `docs/REFERENCE.md`, `StMaker/kfparticle/README.md` | 現在の参照先と有効条件に説明を統一 |
| 本ファイル | 削除・整理内容と検証結果の記録 |

旧 `StLambdaMaker` / `StLambda`、両解析マクロ、KFのPID・組合せ・fit・最終選別コードは変更していない。主要Maker/adapter/helperソースのSHA256も整理前と一致する。共通 `ConfigManager` の挙動も変更していない。

3つのmainconfに以下の正確な1行を追加:

```yaml
# config-generator: manual
```

この指定があると、`setup_config_from_analysisinfo.py` は **ファイルのコピー・mode更新・マクロ生成より前に停止**し、未使用設定を作り直さない。`--force` でも保護する。名前による解析固定ではなく明示的なmarkerによる保護で、markerのない設定は従来どおり。

通常の `setup.sh`、make、解析runnerはこの保護の対象外なので、そのまま使用できる。カット変更は既存mainconfが指すYAMLを直接編集する。

注意: genericなConfigManagerは省略された `track/pid/v0/mixing/lambda` 等に従来のwarningを出す。この対象解析では意図した省略で、generic PIDのデフォルト値が選別に使われるという意味ではない。設定切替試験は共有singletonの残存値を避けるため、すべて別ROOTプロセスで行った。

## 5. 実行・検証ログ

ROOT 5.34/38 / STAR SL24y / GCC4.8.5 / 既存Singularity環境で、以下が正常終了（exit 0）:

- `singularity_make.sh ... --no-clean all -j4`。
- `test-kfparticle-full-chain` / `test-kfparticle-pico-adapter`（新しいsparse設定試験を含む）。
- `python3 -B tests/test_manual_config_guard.py`: 6テスト。通常/force・書込み前停止・類似comment・旧動作など。一時コピーのみで実行。
- 旧Helix20、標準KF1000、Imp5 KF10000の解析と全histogram比較。
- 標準KF/Imp5の実データ出力QA。

検証用出力（以前の解析ROOTは上書きしていない）:

- [Helix20](../rootfile/auau13p5_config_cleanup_validation_20260908/helix20_after_cleanup.root)
- [標準KF1000](../rootfile/auau13p5_config_cleanup_validation_20260908/standard_kf1000_after_cleanup.root)
- [Imp5 KF10000](../rootfile/auau13p5_config_cleanup_validation_20260908/imp5_10000_after_cleanup.root)

Helix20は再構成17イベント。標準KF1000は957イベント、Λ3189個＋anti-Λ463個。Imp5 KF10000は9744イベント、Λ3135個。これらは整理前と一致した。

[整理後設定・ソース・検証ログarchive](../share/figure/auau13p5_config_cleanup_20260908/cleanup_validation_20260908.tar.gz) に使用設定、変更コード、実行ログ、差分監査、ハッシュを保存。元ログは `/tmp/star-cut-cleanup-20260908.CpAMYN/`。入力は前回と同じ `/tmp/star-kf-compare-input-20260907.c7jFYa/local.list` で、前回の入力manifest・ID監査を再利用した。

再実行例（出力名は新しいものを指定）:

```bash
./script/singularity_make.sh config/mainconf/main_auau13p5_anaLambda_KFParticle_Imp5.yaml --no-clean all -j4
./script/singularity_make.sh config/mainconf/main_auau13p5_anaLambda_KFParticle_Imp5.yaml --no-clean test-kfparticle-full-chain test-kfparticle-pico-adapter KF_TEST_CUTS=config/cuts/kf/kf_auau13p5_anaLambda_KFParticle.yaml -j2
./script/singularity_run_anaLambda_KFParticle.sh /tmp/star-kf-compare-input-20260907.c7jFYa/local.list rootfile/auau13p5_config_cleanup_validation_20260908/imp5_recheck.root cleanupRecheck 10000 config/mainconf/main_auau13p5_anaLambda_KFParticle_Imp5.yaml
```
