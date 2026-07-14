# star-submit 最小テスト

root4star を使わず、ワーカーでテキストファイルを 1 つ書き出して終えるジョブで、star-submit と SUMS の動作確認をするためのテストです。既存の分析用ディレクトリ・ファイルには一切手を加えません。

## やり方

```bash
cd /star/u/oura/work/star-analysis/job/run
./test_star_submit/submit_test.sh
```

- 1 ファイルだけ入力として取得し、コマンドは `$SCRATCH` に `test_result_<JOBID>.txt` を書き出して終了します。
- 成功すると SUMS がそのファイルを次の 2 か所にコピーします：
  - `job/run/test_star_submit/result/`
  - `/gpfs01/star/pwg/oura/star_submit_test/`（gpfs01 テスト用）

## 事前準備（gpfs01 出力を使う場合）

gpfs01 にもコピーするため、ワーカーから書き込めるようにテスト用ディレクトリを用意してください（なければ作成）：

```bash
mkdir -p /gpfs01/star/pwg/oura/star_submit_test
```

## 確認

- **結果ファイル（NFS）**: `job/run/test_star_submit/result/test_result_<JOBID>.txt`
- **結果ファイル（gpfs01）**: `/gpfs01/star/pwg/oura/star_submit_test/test_result_<JOBID>.txt`
- **標準出力**: `job/run/test_star_submit/log/stdout.<JOBID>.out`
- **標準エラー**: `job/run/test_star_submit/log/stderr.<JOBID>.err`

ここにファイルができ、中身に "Star-submit test OK at ..." と日時が出ていれば、ジョブ完了〜出力コピーまで正常です。（ワーカーは csh のため、コマンドは csh 用の `date` を使っています。）

## 注意

- 入力に 1 ファイル使うため、xrootd で 1 ファイル取得するぶんだけ時間がかかります。
- テスト用のため、`job/run/test_star_submit/` 以外の既存ディレクトリ・ファイルは変更しません。
