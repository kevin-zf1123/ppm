[English](contributing.md)

# 開発に参加する

PPM は、オープンソースのソフトウェアです。
ソースコードの修正、翻訳、マニュアルの改善など、どんな形の協力も歓迎します。
協力していただいた内容は、[ライセンス](license.ja.md) のとおり、BSD 3-Clause License で公開されます。

## 進め方

PPM のリポジトリは、GitHub の [opentoonz/ppm](https://github.com/opentoonz/ppm) です。

1. 大きな変更をするときは、先に [Issue](reporting-issues.ja.md) で相談してください。考え方の違いで、作業がむだにならないようにするためです。
2. リポジトリをフォークして、作業用のブランチで変更します。
3. ソースコードを変えたときは、整形してからコミットします（下の「コードの書き方」を参照）。
4. `main` ブランチに向けて、プルリクエストを出します。

## プルリクエストのお願い

プルリクエストは、レビューする人が内容を理解できて、はじめて取り込めます。
AI を使って開発する人が増え、たくさんの変更をまとめたプルリクエストを手軽に作れるようになりました。
レビューを進めやすくするために、次のことにご協力ください。

- **1つの改良につき、1つのプルリクエストにしてください。** 関係のない修正が混ざっていると、一部だけを取り込むことができません。
- **変更が大きくなったときは、分けられないか考えてください。** たとえば、下準備のリファクタリングと、機能の追加を、別のプルリクエストにします。
- **何を変えたかだけでなく、なぜ必要なのかと、設計の意図を書いてください。** 制作のどんな場面で役に立つのか、ほかの方法と比べてなぜこの方法にしたのか、といったことです。
  AI を使って書いたコードの場合は、使ったプロンプトも添えてもらえると、意図が伝わりやすくなります。
- **機能を変えたり追加したりしたときは、マニュアルも直してください。** 直していない場合は、そのことをプルリクエストに書いてください（下の「マニュアルへの協力」を参照）。

プルリクエストを作ると、説明欄に、これらの項目を書くためのテンプレートが入ります。
すべてを完璧にそろえる必要はありません。迷ったときは、プルリクエストや Issue で気軽に相談してください。

AI のコーディングツールを使って開発するときは、リポジトリ直下の `AGENTS.md` を読ませてください。このリポジトリの決まり（変更の範囲、コードの書き方、マニュアルの更新など）をまとめています。Claude Code は、`CLAUDE.md` を通して自動で読みます。

## ソースからのビルド

### 必要なもの

| もの | 内容 |
|---|---|
| OS | Windows 10 / 11（64ビット） |
| コンパイラ | Visual Studio 2022（「C++ によるデスクトップ開発」） |
| CMake | 3.21 以上 |
| Qt | Qt 6.8 以上（MSVC 2022 64-bit 版）。開発では Qt 6.12 を使っています。インストールするときに、追加のライブラリの **Qt 5 Compatibility Module** も選んでください。 |

使う Qt のモジュールは、Qt Widgets、Qt Network、Qt Core5Compat、Qt Concurrent、Qt QML、Qt Linguist Tools です。

### ビルドの手順

リポジトリのフォルダで、次のように実行します。
`QTDIR` には、Qt をインストールしたフォルダ（`bin` や `lib` があるフォルダ）を指定します。
環境変数 `QTDIR` を設定しておくこともできます。

```bash
cmake -S sources -B build -G "Visual Studio 17 2022" -A x64 -DQTDIR=C:/Qt/6.12.0/msvc2022_64
```

```bash
cmake --build build --config Release
```

`build/Release/ppm.exe` ができます。
`build` フォルダにできる `ppm.sln` を Visual Studio で開いて、ビルドすることもできます。

### 動かす

`ppm.exe` を動かすには、Qt の DLL と、`conf.ini` が必要です。

1. Qt の `windeployqt` で、必要な DLL を `ppm.exe` の隣にコピーします。

   ```bash
   C:/Qt/6.12.0/msvc2022_64/bin/windeployqt.exe build/Release/ppm.exe
   ```

2. `ppm.exe` の隣に `conf.ini` を作り、リポジトリの `stuff` フォルダを Stuff フォルダとして指定します（[インストール](../getting-started/installation.ja.md) を参照）。

   ```ini
   [General]
   StuffFolderPath=../../stuff
   ```

`stuff` フォルダには、サンプルのプロジェクトとデータがあります。
ビルドした PPM の動作は、まず [チュートリアル：カットを1つ最後まで進める](../getting-started/tutorial.ja.md) の手順で確かめてください。

### macOS でビルドして動かす

Xcode Command Line Tools、CMake 3.21 以上、上記のモジュールと Qt SVG を含む macOS 用 Qt 6.8 以上を使います。動作する macOS のバージョンと CPU アーキテクチャは、ビルドに使う Qt と SDK によります。`QTDIR` に Qt のインストール先を指定するか、`CMAKE_PREFIX_PATH` を使ってください。

```bash
cmake -S sources -B build-macos -G Ninja -DQTDIR=/path/to/Qt/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build-macos --target deploy_macos
open build-macos/macos/ppm.app
```

このジェネレーターを使うには Ninja が必要です。`-G Ninja` を省略すると既定のジェネレーターを使います。インストール済みの Qt と既定の Xcode SDK が対応しない場合は、`-DCMAKE_OSX_SYSROOT=/path/to/MacOSX.sdk` で対応する SDK を選んでください。

配布用アプリは `build-macos/macos/ppm.app` です。CMake は `ppm.app` の隣に `conf.ini` を作り、リポジトリの `stuff` フォルダを指定します。既存の `conf.ini` は上書きしません。複数の構成を持つジェネレーターでは、配布前のアプリと設定は選んだ構成のフォルダ（例：`build-macos/Release`）に置かれます。配布用の出力先は常に `build-macos/macos` です。サンプルプロジェクトは `stuff/sample_data` に進捗とログを書き込むため、ファイル操作のテストには Stuff の別コピーを使ってください。

`deploy_macos` はアプリをビルドし、リンクに使った Qt と同じインストール先からプラグインをコピーして `macdeployqt` を実行し、ローカル実行用のアドホック署名を付けます。再ビルド後、Qt のない Mac に移す前にも実行してください。

```bash
cmake --build build-macos --target deploy_macos
```

できたアプリと一緒に、アプリの外に `conf.ini` と書き込み可能な `stuff` フォルダを置き、設定を `StuffFolderPath=stuff` にします。配布用ツールは Qt のライブラリとプラグインを組み込みます。Developer ID による署名と公証は別の配布手順です。[Qt の macOS 配布ガイド](https://doc.qt.io/qt-6/macos-deployment.html) も参照してください。

macOS のアプリアイコンは `sources/macos/ppm.icns` です。プロジェクトのライセンスのもとで、既存の `sources/ppm.ico` から生成します。macOS で再生成するには `python3 sources/macos/create_icon.py` を実行してください。通常のビルドはリポジトリ内のアイコンを使うため、Python は不要です。

プラットフォーム動作のテストは次のように実行します。

```bash
cmake -S sources -B build-macos -DPPM_BUILD_TESTS=ON
cmake --build build-macos --target ppm_platform_tests
ctest --test-dir build-macos -R '^ppm_platform_tests$' --output-on-failure
```

### コードの書き方

プルリクエストを出す前に、変更した C++ のコードに、必ず clang-format をかけてください。
整形の設定は `sources/.clang-format` にあります。

`sources` フォルダで、次のように `beautification.sh` を実行します（Git Bash などで実行します）。

```bash
cd sources
./beautification.sh
```

`main` ブランチから変更した `.cpp`、`.h` ファイルに、まとめて clang-format がかかります。
clang-format は、Visual Studio に含まれているものや、LLVM のものを使えます。PATH の通った場所に置いてください。

画面に表示する文は、英語で書き、`tr()` で囲みます。日本語の訳は、翻訳ファイルに入れます（下を参照）。

## 翻訳への協力

PPM の画面の言語は、翻訳ファイルで追加できます。
翻訳ファイルの作り方は、[UI の言語と翻訳ファイル](../administration/translations.ja.md) の「言語を追加する」で説明しています。

- 翻訳を PPM に取り込んでほしいときは、翻訳した `.ts` ファイル（`sources/translations/PPM_言語コード.ts`）をプルリクエストで送ってください。新しい言語のときは、`sources/CMakeLists.txt` の `LANGUAGES` にも言語コードを足します。
- 今ある翻訳（日本語）の誤りや、わかりにくい訳の指摘も歓迎します。

## マニュアルへの協力

このマニュアルは、リポジトリの `docs` フォルダにある Markdown のファイルです。

- 各ページに、日本語版（`〜.ja.md`）と英語版（`〜.md`）があります。どちらかを直したときは、もう一方も合わせて直してください。片方しか直せない場合は、プルリクエストにそう書いてもらえれば、もう一方はこちらで直します。
- 画面のキャプチャは `docs/images/ja` に置きます。英語版のページも、日本語版のキャプチャを使います。
- マニュアルは、PPM の今の動きのとおりに書きます。動きがおかしいと思った点は、マニュアルを変えるのではなく、[Issue](reporting-issues.ja.md) で報告してください。

---

[← 目次に戻る](../index.ja.md)
