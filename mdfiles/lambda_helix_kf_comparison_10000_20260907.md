# AuAu13p5: 同一10,000イベントのHelix / KFParticle Λ mass比較

実行日: 2026-09-07 (EDT)。farmへのjob投入なし、SL24y / ROOT 5.34/38のSingularity環境でローカル実行。

## 成果物と結果

同じ入力の先頭10,000イベントを既存 `anaLambda` と新 `anaLambda_KFParticle` で処理した。既存ROOTファイルの上書き、解析コード・物理カットの変更は行っていない。

| 項目 | Helix (旧) | KFParticle (新) |
|---|---:|---:|
| 入力読み取りイベント | 10000 | 10000 |
| 再構成まで通過したイベント (`hN` entries) | 9744 | 9728 |
| 図の範囲内のΛ候補 | 4914 | 31248 |
| 比較元histogram | `hLambda_InvMass` | `hKfLambdaMassSelected` |
| histogram underflow / overflow | 0 / 2302 | 0 / 0 |
| ローカル解析終了コード | 0 | 0 |

候補数は信号フィット・背景差し引き後のΛ収量ではない。範囲は `1.05 <= M < 1.25 GeV/c²`、bin幅は1 MeV/c²。新KFのanti-Λ 4731件は比較に含めない。KF側の互換histogram `hLambda_InvMass` はΛとanti-Λを合わせた35979件なので、旧側とそのまま重ねない。

- [比較図 PNG](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle/lambda_mass_compare_10000_20260907.png)
- [比較図 PDF](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle/lambda_mass_compare_10000_20260907.pdf)
- [比較canvasと4本のhistogram ROOT](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle/lambda_mass_compare_10000_20260907.root)
- [旧Helix解析ROOT](../rootfile/auau13p5_anaLambda/local_compare_10000_20260907.root)
- [新KFParticle解析ROOT](../rootfile/auau13p5_anaLambda_KFParticle/local_compare_10000_20260907.root)
- [入力監査・設定snapshot・実行ログ](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle/comparison_provenance_20260907.tar.gz)

図は青実線がHelix、赤破線がKFParticle。`Draw("HIST SAME")` で重ね描きした。左は候補実数、右はそれぞれの図示範囲の積分で面積規格化した形状比較。追加cut・mass fit・背景差し引き・効率補正は行っていない。両方にΛ質量付近のピークが見える一方、現設定のKF側はピーク外の候補が多い。定量的なsignal/backgroundや質量分解能は今回フィットしていない。

## 同一入力の確認

元リスト: `config/picoDstList/auau13p5GeV.list`。リスト順を変更せず先頭7ファイルのみをローカルに用意した。

| 順序 | ファイル末尾 | 全イベント数 | 今回使用 |
|---:|---|---:|---:|
| 1 | `raw_6500002.picoDst.root` | 1479 | 1479 |
| 2 | `raw_6000008.picoDst.root` | 1601 | 1601 |
| 3 | `raw_7000011.picoDst.root` | 1557 | 1557 |
| 4 | `raw_7500008.picoDst.root` | 1558 | 1558 |
| 5 | `raw_6000019.picoDst.root` | 1567 | 1567 |
| 6 | `raw_6500018.picoDst.root` | 1531 | 1531 |
| 7 | `raw_7000021.picoDst.root` | 1545 | 707 |
| 合計 | | 10838 | 10000 |

- 共通local list: `/tmp/star-kf-compare-input-20260907.c7jFYa/local.list`。
- 全7ファイルでPicoDst treeとEvent / Track / TrackCovMatrix branchを確認。
- 両解析のログで同じ7ファイルの添付順を確認。両マクロは先頭から `i=0..9999` を処理し、出力の入力read counterも10000件と一致した。
- 独立した読み取り専用監査で先頭10000件の `(inputIndex, runId, eventId)` を保存。index欠落・runId/eventId重複なし。
- 最初のID `(21033026,337)`、最後のID `(21033026,6279507)`。
- ID一覧SHA256: `aeb0e8c9da2d62e1c3fed9dd7e1897d473af766887a38a1d05539ed5618da4d3`。
- local list SHA256: `a93ab236f3357bbe95eac2a2b15ad9b0b8a65ba4279991f8dfefd8ed763fe1cb`。
- 元URI・ローカルpath・bytes・件数を `input_manifest.tsv` に記録。入力7ファイル計501445309 bytes。
- ROOT 5のNetx未導入を回避するためhostのxrdcpでローカル化した。ファイル2/3はコピー後のサーバchecksum照会が未対応でexit54だったが、サイズが元と一致し、ROOT読み取り監査は成功。ローカルSHA256は保存したが、サーバ認証済みSHA256とは主張しない。

入力ID一覧は**入力の監査**であり、各Makerが保存した全イベントIDの照合ではない。同一性の根拠は共通リスト、入力順監査、両ループの処理範囲、完了ログ、出力read counterの組み合わせである。

## 今回の図の解釈

現在のコード・設定をそのまま比較しているため、**同じ入力イベントであっても、同じ選別後のサンプルではない**。候補数の比をHelix対KFParticleの再構成効率比と解釈してはいけない。

| 選別 | 旧Helixで実際に使用 | 新KFParticleで実際に使用 |
|---|---|---|
| Event | `PassEventCuts` はtrack数上限のみ (今回は上限なし)、その後centrality選別 | mode `fxtmult` のVz [198,202]、XY中心(-0.4,-2.0) cmから半径2 cm、RefMult/VPD/track数、その後centrality |
| Daughter acceptance | `LambdaCutConfig` のnHitsFit>=15、hit ratio>=0.52、電荷 | KF専用YAML: nHitsFit>=15、nHitsDedx>=5、hit ratio>=0.52、pT [0.15,10]、lab eta [-2.4,0]、dE/dx errorなど |
| PID | PicoTrack格納 `nSigmaProton/Pion` 各3以内 | SL24y `dedx_pull` とKF設定のTOF参照profile、複数PID仮説 |
| Daughter/PV・topology | proton gDCA>=0.7、pion>=1.0、daughter DCA<=0.5、parent DCA<=0.5 cm、cos pointing>=0.998、helix path長<=100 cm | 共分散を利用したInterface/PV/Finder選別。Finder daughter距離1.5 cm、L=1 cm、chiPrimary2D=3、chi2/NDF=10、L/dL=3。upstream Topo内のPV chi2/NDF<3も有効 |
| 最終mass | 全質量をfill (範囲外はflow bin) | 選別histは [1.05,1.25]、追加Maker topology cutは無効化設定 |

重要: genericな旧 `config/cuts/track/...yaml` のpT/eta/nHitsDedx値は、現在の `StLambdaMaker::PassProtonCuts/PassPionCuts` では使用されていない。KF cut YAML内の既存コメントにはgeneric track設定を「Helix preselection」とする記述があるが、本比較の条件説明ではそのコメントではなく実行コードを確認した。今回、旧解析コードやその挙動を修正していない。

両方のcentrality設定は `fxtmult`、既存Run20テーブル、同じaccepted bins 0..8。新KFのFXT中心とTOF係数は参照profileであり、今回の図作成でRun20データに対する較正を確立したものではない。両massは親Λのmass constraintをかけていないが、KF massもupstreamのtopology選別後である。

## 実行と検証

プロジェクトrootで、以下のローカルrunnerを使用した (`jobid` 引数は識別名でありfarm jobではない)。再実行する場合は既存ROOTを上書きしない新しい出力名に変える。

```bash
./script/singularity_run_anaLambda.sh /tmp/star-kf-compare-input-20260907.c7jFYa/local.list rootfile/auau13p5_anaLambda/local_compare_10000_20260907.root compareHelix10000 10000 config/mainconf/main_auau13p5_anaLambda.yaml
./script/singularity_run_anaLambda_KFParticle.sh /tmp/star-kf-compare-input-20260907.c7jFYa/local.list rootfile/auau13p5_anaLambda_KFParticle/local_compare_10000_20260907.root compareKF10000 10000 config/mainconf/main_auau13p5_anaLambda_KFParticle.yaml
```

比較macro: [common/macro/compareLambdaHelixKF.C](../common/macro/compareLambdaHelixKF.C)。既存の出力を読むだけで、元ROOTを変更しない。

```cpp
compareLambdaHelixKF(
  "rootfile/auau13p5_anaLambda/local_compare_10000_20260907.root",
  "rootfile/auau13p5_anaLambda_KFParticle/local_compare_10000_20260907.root",
  "share/figure/auau13p5_Lambda_Helix_vs_KFParticle/lambda_mass_compare_10000_20260907",
  10000, 1.05, 1.25, 0.001);
```

- Helix正常終了、`StLambdaMaker::Finish() processed 10000 events` を確認。centrality summary: ok=9744、invalidCent=256、badRun/pileup/binRejected=0。旧mass histogramの全entriesは7216件で、図示範囲内4914件とoverflow2302件に分かれる。
- KF正常終了: requested=completed=10000、reconstructed=9728、status=0。
- KF event outcome: Vr除外9、VPD差除外11、centrality invalid252、他の除外/KF errorは0。event cut後9980イベント。
- `tests/check_kfparticle_output.C` はPASS / exit0。finite値、候補treeとsigned histogram/counter、metadata、vertex cut範囲の整合性を確認。
- KF raw候補tree: Λ121173、anti-Λ24184。raw mass histogramには上限1.25より上のoverflow (Λ89925、anti-Λ19453) がある。比較では選別後のΛのみを使用。
- 比較macroはread count、再構成count、signed candidate count、KF completed status、bin境界を検査しPASS / exit0。PNGを開いてレイアウトと重ね描きを確認。
- 比較macro自体をROOT 5のACLiC/CINTで人工データ試験した。片側/両側ゼロ候補とwhole-bin rebin成功、誤read count・非整合bin・上書きの拒否を確認。人工データは実データ結果に混ぜていない。
- 実行前後でmainconfと参照YAML、解析macro、Maker、主要ライブラリのSHA256一致を確認。既存 `StLambdaMaker` / `StLambdaV0Reconstruction` は変更なし。
- 実測RealTime: Helix 1323.67秒、KF 42.0905秒。選別と処理内容が異なるので純粋なアルゴリズム速度比較ではない。

ログ原本は `/tmp/star-lambda-compare-20260907.BhNrNu/`。上記tar.gzには入力manifest/ID一覧/ハッシュ、実行ログ、使用YAML snapshot、比較macro/driverなどを保存した。ローカルPicoDst本体は `/tmp` のため後日消える可能性があり、必要ならmanifestの元URIから再取得する。比較macroは既存PNG/PDF/ROOTがあると上書きを拒否するので、再生成時は新しいoutput stemを指定する。
