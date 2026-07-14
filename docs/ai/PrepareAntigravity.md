# Antigravity (Gemini) スキルのセットアップ (tcsh/csh)

このリポジトリに定義されているカスタムスキル（日報ログ作成、ヒストグラム追加手順、コンテナビルドなど）を、Antigravity（Gemini IDE）でもシステムレベルで自動認識させるためのセットアップ手順です。

## セットアップ手順

プロジェクトディレクトリのルートで、以下のスクリプトを実行してください。

```tcsh
tcsh script/setup_antigravity_skills.csh
```

### スクリプトの処理内容
1. プラグイン配置用ディレクトリ `~/.gemini/config/plugins/` を作成します（存在しない場合）。
2. プロジェクト内の `.cursor` ディレクトリから `~/.gemini/config/plugins/star-analyzer` へのシンボリックリンクを作成します。
3. 既に古いリンクやファイルが存在する場合は、自動的にバックアップを取得して上書きします。

## 反映の確認

スクリプトを実行後、**Antigravity のセッションをリロードまたは再起動**してください。
正しく設定されると、AIとの対話時にシステムに以下のカスタムスキルが自動的にインジェクションされます：

* `daily-analysis-log`
* `add-new-analysis`
* `stmaker-add-histograms`
* `update-readme-scripts`
* `singularity-local-build-run`

## 仕組み

- `.cursor/plugin.json` を配置したことで、`.cursor/` ディレクトリ全体が Antigravity のプラグインフォルダとして機能するようになります。
- プラグイン内の `skills/` フォルダ（`.cursor/skills/`）にある `SKILL.md` を Antigravity が自動認識します。
- これにより、**CursorユーザーとAntigravityユーザーの間で全く同じスキル定義（Source of Truth）を共有**することができます。
