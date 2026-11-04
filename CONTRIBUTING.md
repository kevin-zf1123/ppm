# Contributing to PPM

[日本語は下にあります](#ppm-の開発に参加する)

Thank you for your interest in contributing to PPM.
Fixes to the source code, translations, and improvements to the manual are all welcome.
Contributions are released under the [BSD 3-Clause License](LICENSE).

The full guide, including how to build from source, is in the manual: [Contributing](docs/about/contributing.md).
To report a bug or request a feature, see [Reporting Bugs and Requests](docs/about/reporting-issues.md).

## Before opening a pull request

1. For large changes, discuss them in an [Issue](https://github.com/opentoonz/ppm/issues) first.
2. Format the C++ code you changed with clang-format:

   ```bash
   cd sources
   ./beautification.sh
   ```

3. If you changed or added a feature, update the manual in `docs` (both `*.ja.md` and `*.md`).

## About pull requests

A pull request can be merged only when the reviewers can understand it.
As more people develop with AI, it has become easy to create pull requests that bundle many changes.
To keep reviews manageable, please help us with the following.

- **One pull request for one improvement.**
- **If a change gets large, consider whether it can be split.** For example, put preparatory refactoring and the new feature in separate pull requests.
- **Write not only what you changed, but why it is needed and what the design intends.** If the code was written with AI, including the prompts you used also helps convey the intent.
- **If you changed or added a feature, also update the manual.** If you did not, say so in the pull request.

The pull request template lists these points. You do not need to get everything perfect. If you are unsure, feel free to ask in the pull request or an Issue.

If you develop with an AI coding tool, it should read [AGENTS.md](AGENTS.md), which summarizes the rules of this repository.

---

## PPM の開発に参加する

PPM の開発に関心を持っていただき、ありがとうございます。
ソースコードの修正、翻訳、マニュアルの改善など、どんな形の協力も歓迎します。
協力していただいた内容は、[BSD 3-Clause License](LICENSE) で公開されます。

ソースからのビルドなどを含む、くわしい説明はマニュアルの [開発に参加する](docs/about/contributing.ja.md) にあります。
不具合の報告や機能の要望は、[不具合報告・要望](docs/about/reporting-issues.ja.md) を参照してください。

## プルリクエストを出す前に

1. 大きな変更をするときは、先に [Issue](https://github.com/opentoonz/ppm/issues) で相談してください。
2. 変更した C++ のコードに、clang-format をかけてください。

   ```bash
   cd sources
   ./beautification.sh
   ```

3. 機能を変えたり追加したりしたときは、`docs` のマニュアル（`〜.ja.md` と `〜.md` の両方）も直してください。

## プルリクエストのお願い

プルリクエストは、レビューする人が内容を理解できて、はじめて取り込めます。
AI を使って開発する人が増え、たくさんの変更をまとめたプルリクエストを手軽に作れるようになりました。
レビューを進めやすくするために、次のことにご協力ください。

- **1つの改良につき、1つのプルリクエストにしてください。**
- **変更が大きくなったときは、分けられないか考えてください。** たとえば、下準備のリファクタリングと、機能の追加を、別のプルリクエストにします。
- **何を変えたかだけでなく、なぜ必要なのかと、設計の意図を書いてください。** AI を使って書いたコードの場合は、使ったプロンプトも添えてもらえると、意図が伝わりやすくなります。
- **機能を変えたり追加したりしたときは、マニュアルも直してください。** 直していない場合は、そのことをプルリクエストに書いてください。

プルリクエストのテンプレートに、これらの項目を並べています。すべてを完璧にそろえる必要はありません。迷ったときは、プルリクエストや Issue で気軽に相談してください。

AI のコーディングツールを使って開発するときは、このリポジトリの決まりをまとめた [AGENTS.md](AGENTS.md) を読ませてください。
