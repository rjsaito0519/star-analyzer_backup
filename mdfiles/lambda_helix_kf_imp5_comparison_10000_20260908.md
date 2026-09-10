# AuAu13p5: Imp5共通カットに合わせたHelix / KFParticle比較（2026-09-08）

## 結果

同じ入力の先頭10,000イベントで、KFParticleをImp5専用設定にしてローカル再実行した。farmへのjob投入はしていない。旧Helixは前回の出力を再利用し、入力順・入力内容・旧ソースと設定・旧ROOTのSHA256不変を再確認した。

| 項目 | 旧Helix（現行Imp5） | KFParticle（今回Imp5） |
|---|---:|---:|
| 入力イベント | 10000 | 10000 |
| 再構成処理まで進んだイベント | 9744 | 9744 |
| 図示質量範囲のΛ候補 | 4914 | 3135 |
| 比較範囲 / bin幅 | 1.05 <= M < 1.25 GeV/c² / 1 MeV/c² | 同左 |

- [比較PNG](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle_Imp5/lambda_mass_compare_10000_20260908.png)
- [比較PDF](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle_Imp5/lambda_mass_compare_10000_20260908.pdf)
- [比較canvas / histogram ROOT](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle_Imp5/lambda_mass_compare_10000_20260908.root)
- [新KF解析ROOT](../rootfile/auau13p5_anaLambda_KFParticle_Imp5/local_compare_10000_20260908.root)
- [旧Helix解析ROOT（変更なし）](../rootfile/auau13p5_anaLambda/local_compare_10000_20260907.root)
- [設定・ソース・ログ・入力監査の保存archive](../share/figure/auau13p5_Lambda_Helix_vs_KFParticle_Imp5/comparison_provenance_20260908.tar.gz)

青実線がHelix、赤破線がKFParticle (Imp5)。左は無規格化の候補数、右は図示範囲をそれぞれ面積1へ規格化した形状比較。旧 `hLambda_InvMass` と新 `hKfLambdaMassSelected` を `Draw("HIST SAME")` で重ねており、anti-Lambdaは含めない。

前回の標準KF（範囲内31248候補）に比べ、今回の共通カット適用後はピーク外の候補が減り、約1.115 GeV/c²のΛピークが明瞭になった。旧Helixとの面積規格化図でもピークの占める割合は高く見える。一方、無規格化のピーク付近の候補数も減っている。**背景フィット・信号収量抽出をしていないため、S/B、S/√(S+B)、質量分解能の改善率、再構成効率の数値は主張しない。** 閾値が一致してもKF固有条件と再構成量の違いは残る。

## Imp5の基準と実装方針

ユーザーから「現在のanaLambdaがImp5、proton/piはnSigmaのみ、TOFなし」と指定された。そのため基準は、実際の [StLambdaMaker](../StMaker/StLambdaMaker/StLambdaMaker.cxx) と [maker_auau13p5_anaLambda.yaml](../config/maker/maker_auau13p5_anaLambda.yaml) の**実行時に使われる条件**とした。

参照指定の [Notionページ](https://app.notion.com/p/2026-07-01-3902284fda2880c4b97cd2fa9798edb3?source=copy_link) はアクセスを試みたが本文を取得できなかった。したがってNotionの記載を独立に照合したとはしていない。もし同ページと現行コードに相違があれば、今回の比較の基準は現行コードである。

既存の `StLambdaMaker` / `StLambda` / Helix設定、標準KF設定は変更せず、明示的な `selectionProfile: lambda_imp5` を新設した。通常は `kf_reference` がデフォルトで従来動作を保つ。

- [新mainconf](../config/mainconf/main_auau13p5_anaLambda_KFParticle_Imp5.yaml)
- [新KFカット](../config/cuts/kf/kf_auau13p5_anaLambda_KFParticle_Imp5.yaml)
- [新analysis-info](../config/analysis/analysis_info_auau13p5_anaLambda_KFParticle_Imp5.yaml)

新mainconfのその他のconcernファイルは既存のKF専用コピーを読み取り専用で参照する。通常KFやHelix設定へ比較用の変更を混入させない。

| 共通条件 | Imp5に合わせた今回の扱い |
|---|---|
| Daughterの電荷 | proton正、pion負。今回anti-Lambda再構成は無効 |
| nHitsFit | >= 15 |
| nHitsFit / nHitsMax | >= 0.52。ただし旧コードどおりnHitsMax > 0のときだけ検査 |
| PID | 保存済み `nSigmaProton/Pion` の絶対値 <= 3、両仮説は独立に評価 |
| TOF / kaon仮説 | 使用しない。TOF参照係数も空 |
| proton / pionのPVからの距離 | 元PicoTrackの `gDCA(pv.X(),pv.Y(),pv.Z())` >= 0.7 / 1.0 cm |
| 元helixの経路長 | 両daughterで絶対値 <= 100 cm |
| daughter間距離 | <= 0.5 cm |
| parentとPVの距離 | <= 0.5 cm |
| cos pointing | >= 0.998 |
| pT / eta / nHitsDedx / dEdxError | 旧Makerで使っていないので今回のImp5 profileでは選別しない |
| Event | 現行旧MakerのmaxNTr条件、同じbad-run / centrality選別。旧MakerにないVz/Vr/RefMult/VPD条件を追加しない |

genericなtrack/PID/V0 YAMLは今回の旧Makerの実効条件ではない。例えば旧V0 YAMLのdecay-length範囲や狭いmass windowを、新しい共通カットとして追加していない。数値の有限性、非ゼロ運動量、共分散・入力IDの妥当性などKF計算に必要な安全確認は保つ。

### 距離・経路長の定義

元daughterのgDCAは、旧Makerと同じ**3スカラー引数**のPicoTrackメソッドを呼ぶ。この値をKFのPV χ²や、新たに計算したhelixの最近接DCAへ置き換えていない。

元helixの経路長も旧Makerと同じStPhysicalHelixDの構築方法・磁場単位・`pathLengths`で求める。ただし対象は**Finder/Topoが見つけた候補**のみで、全trackの手動組合せへの切替ではない。KFのtransportパラメータdSを長さcmとして使うことはしない。この経路長確認でKFのmassやvertexをHelix値に置き換えない。

一方、最後のdaughter間距離・parent/PV距離・pointingは**KF再構成状態から計算した量**へ同じ閾値を適用する。旧解析は元helixとその中点などから計算しているため、同じ候補でも値が完全一致するとは限らない。

### Event policyと従来のmode切替

現行 `StLambdaMaker::PassEventCuts` はmaxNTrのみを検査する（今回maxNTr=0で上限なし）。今回の明示的Imp5 profileだけ `legacy_lambda_imp5` としてこれに合わせた。

ROOT metadataには `vertexCutsApplied/refMultCutsApplied/vpdCutsApplied: false` を記録する。analysis.mode=fxtmultとvertexByModeの整合性検証は残し、中心(-0.4,-2.0) cmはQA座標に使うが、Imp5ではVz/Vr範囲を選別に使わない。**標準KFのcollider / fixed-target modeによるvertex cut切替は従来どおり有効であり、今回無効化したのではない。**

### 維持したKF独自条件

`StPicoKFParticleInterface → KFParticleTopoReconstructor → KFParticleFinder` の全再構成経路を保持し、scalar二体組合せへ戻していない。ROOT 5 / SL24yの既存構成で実行した。

- 不正共分散を除外。position/momentum variance上限100 / 1。
- interfaceChiPrimaryCut=3、primaryProbCut=0.0001。
- Finder: max daughter distance=1.5 cm、L cut=1 cm、chiPrimary2D=3、chi2/NDF=10、L/dL=3。
- 採用したupstream Topo内のΛ PV χ²/NDF < 3もそのまま。
- 親Λのmass constraintなし。表示用の最終mass範囲[1.05,1.25]は従来KFと同じ。
- 今回のデータを見て閾値を最適化したのではなく、共通閾値は旧Imp5、上記KF閾値は前回設定から引き継いだ。

## カット適用と出力QA

入力read=10000、centrality invalid=256、それ以外のevent除外/KF error=0、reconstructed=9744。旧Helixのevent summaryと一致した。

KF raw tracks=3159854、不正共分散4765、Topo Λ候補44121、候補変換等の妥当性検査後のraw Λ=42871、selected Λ=3135、anti-Λ=0。

出力treeの診断量で最終カットを再評価した結果:

| 累積条件（診断のための評価順） | 残存Λ候補 |
|---|---:|
| Finder/Topoと入力PID/gDCA適用後のvalid raw | 42871 |
| + 元helix経路長 | 42222 |
| + KF daughter間距離 <= 0.5 | 18574 |
| + KF parent/PV距離 <= 0.5 | 7398 |
| + cos pointing >= 0.998 | 4762 |
| + mass範囲 | 3135 |
| + その他の設定済み最終KF条件 | 3135 |

これはMaker内部の実行順を表すcut-flow histogramではなく、保存済みraw候補へ順序を定めてカットを再適用した診断。各段の減少数は順序依存で、単独カットの効率や信号・背景別の効率を表さない。

今回の追加tree量は `protonDcaToPv`、`pionDcaToPv`、`protonHelixPathLength`、`pionHelixPathLength`（Double_t）、`imp5PathValid`（Bool_t）。標準KFではこれらは未測定のデフォルト値/falseで、追加選別に使わない。

## 検証と非退行確認

すべて正常終了（exit 0）:

1. ROOT 5.34/38 / SL24y / GCC 4.8.5で `make all --no-clean` 成功。
2. `test-kfparticle-full-chain` と `test-kfparticle-pico-adapter` 成功。Imp5の明示設定、未知キー拒否、境界値、PID/DCAの独立性、TOFを見ないこと、hit ratioの分母ゼロ、通常profileへの復帰、event policy、共通設定の非変更などを検査。
3. 旧Helix先頭20イベントを変更前後で実行: 46 histograms / 2469952 cells、軸・label・entries・内容・誤差・flow binsの**差分0**。
4. 標準KF先頭1000イベントを再実行し前回と比較: 68 histograms / 2536396 cells、同じ検査の**差分0**。read=1000、reconstructed=957、selected Λ+anti-Λ=3652。
5. Imp5実データ100イベントsmokeおよび出力QA成功。
6. Imp5実データ10000イベント、`check_kfparticle_imp5_output.C` 成功。保存PID/gDCA、TOFフラグ、経路長・最終topology・質量条件を検査し、再評価selected件数3135が一致。
7. 比較macro成功。保存済み比較ROOTを読み直し、4比較histogramとcanvas内の4曲線について200 binずつ元histogramと内容/誤差/境界を比較して差分0。PNGの重ね描き・凡例も目視確認。
8. 元Helixソース/スクリプト7ファイルと設定10ファイルのSHA256不変。旧10000出力のSHA256不変。

3/4はhistogramの比較であり、treeや追加metadataのバイト一致を主張するものではない。既存 `StLambdaMaker` / `StLambda` を変更していない。今回の生成物には別ディレクトリ/別名を使い、前回の解析ROOT/比較図も上書きしていない。

## 入力の再現性

入力は `config/picoDstList/auau13p5GeV.list` 先頭7ファイルの前回のローカルコピー。リスト:

`/tmp/star-kf-compare-input-20260907.c7jFYa/local.list`

最初の6ファイル計9293イベント＋7番目の先頭707イベント。7ファイルと順序、入力内容のSHA256を再確認し、今回KFログ・前回Helixログ・local.listの添付順が一致した。

- 先頭ID: (runId,eventId)=(21033026,337)。
- 最終ID: (21033026,6279507)。
- 入力ID一覧SHA256: `aeb0e8c9da2d62e1c3fed9dd7e1897d473af766887a38a1d05539ed5618da4d3`。
- local.list SHA256: `a93ab236f3357bbe95eac2a2b15ad9b0b8a65ba4279991f8dfefd8ed763fe1cb`。
- 旧Helix ROOT SHA256: `1d8bf45a0446ce7ac7353d0f55d7e6386a1f000ab6adb8f65dba30a16ba580b6`。

入力ID一覧は入力監査であり、各Makerが保存した全イベントIDを比較したものではない。同一性の根拠は入力内容・順序・ループ範囲・正常完了ログ・read counterの組合せである。/tmpのPicoDstコピーは後日消える可能性があるため、元URIを含むmanifestをarchiveにも保存した。

## 実行コマンド

プロジェクトrootで実行。再実行時は未使用の出力名を指定する。

```bash
./script/singularity_make.sh config/mainconf/main_auau13p5_anaLambda_KFParticle_Imp5.yaml --no-clean all -j4

./script/singularity_make.sh config/mainconf/main_auau13p5_anaLambda_KFParticle_Imp5.yaml --no-clean test-kfparticle-full-chain test-kfparticle-pico-adapter KF_TEST_CUTS=config/cuts/kf/kf_auau13p5_anaLambda_KFParticle.yaml -j2

./script/singularity_run_anaLambda_KFParticle.sh /tmp/star-kf-compare-input-20260907.c7jFYa/local.list rootfile/auau13p5_anaLambda_KFParticle_Imp5/local_compare_10000_20260908.root imp5Compare10000 10000 config/mainconf/main_auau13p5_anaLambda_KFParticle_Imp5.yaml
```

既存のsynthetic試験には標準profileのTOF検査も含むため、`KF_TEST_CUTS`は標準KF YAMLを指定する。Imp5用のfixtureは試験内で明示的に構成する。

同じSTAR/ROOT 5環境で:

```cpp
root4star -b -q 'tests/check_kfparticle_imp5_output.C("rootfile/auau13p5_anaLambda_KFParticle_Imp5/local_compare_10000_20260908.root",true)'

// ROOT macro / driver内で実行:
Int_t result = 1;
result = compareLambdaHelixKF(
  "rootfile/auau13p5_anaLambda/local_compare_10000_20260907.root",
  "rootfile/auau13p5_anaLambda_KFParticle_Imp5/local_compare_10000_20260908.root",
  "share/figure/auau13p5_Lambda_Helix_vs_KFParticle_Imp5/lambda_mass_compare_10000_20260908",
  10000, 1.05, 1.25, 0.001);
gSystem->Exit(result);
```

比較macroは `common/macro/compareLambdaHelixKF.C`。完全な実行driver、QA、build/runログ、終了コード、使用設定/ソースとSHA256をarchiveに保存した。元ログは `/tmp/star-kf-imp5-20260908.BB8dyv/`。今回KFのRealTime=35.5334秒だが、前回と選別が異なるため純粋な速度比較ではない。

参照: [前回の標準KF比較](lambda_helix_kf_comparison_10000_20260907.md)、[実行手順](../docs/REFERENCE.md)、[adapter仕様](../StMaker/kfparticle/README.md)。
