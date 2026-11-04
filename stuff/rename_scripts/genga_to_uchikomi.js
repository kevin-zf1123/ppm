// PPM - rename script (sample)
// PPM - リネームスクリプト（サンプル）
//
// This script is used by the "Rename Rule" setting of the "Copy Folder Contents" step.
// 「フォルダの内容をコピー」ステップの「Rename Rule」から選べるリネームルールです。
//
// How to write a rename script / リネームスクリプトの書き方:
//   - Put the script in the "rename_scripts" folder inside the Stuff folder (file extension: .js).
//     スクリプトは、Stuffフォルダ内の rename_scripts フォルダに置きます（拡張子は .js）。
//   - Define "rule" (name and description) and the function "rename".
//     「rule」（名前と説明）と、関数「rename」を定義します。
//   - "name" and "description" are either a string or an object with a string for each language.
//     If there is no string for the current language, the first registered string is used.
//     「name」と「description」は、文字列か、言語ごとの文字列のオブジェクトです。
//     現在の言語の文字列が無い場合は、最初に登録されている文字列が使われます。
//   - rename(path, context) receives the path of a file relative to the source folder
//     (including the subfolder when the file is in one) and returns the path of the copy
//     relative to the destination folder (including the subfolder, if any).
//     Return an empty string when the file is not a target of this rule.
//     rename(path, context) は、コピー元フォルダからの相対パス（サブフォルダに入っている場合はそのフォルダを含む）を受け取り、
//     コピー先フォルダからの相対パス（サブフォルダに入れる場合はそのフォルダを含む）を返します。
//     このルールの対象外のファイルには、空の文字列を返します。
//   - context has sceneNumber, sequence and episode (you don't have to use it).
//     contextには、sceneNumber、sequence、episode が入っています（使わなくても構いません）。
//   - The script cannot access files or the network.
//     スクリプトから、ファイルやネットワークにはアクセスできません。

var rule = {
  name: {
    en: "Exported Genga images to Uchikomi folder",
    ja: "書き出したデジタル原画を打ち込みフォルダへ"
  },
  description: {
    en: "[Rename target]\n" +
        "- Sequentially numbered image files without justification.\n" +
        "  (1.png, 2.png, 3.png ...)\n" +
        "- Files must be in a subfolder.\n" +
        "\n" +
        "[Rename rule]\n" +
        "1. The file names directly under the folder are set to [folder name]_[4-digit justified sequential number].\n" +
        "\n" +
        "2. When \"_\" is added to a folder name, the file name in the folder should be\n" +
        "   [before the first \"_\" in the folder name]_[4-digit justified number]_[after the first \"_\" in the folder name].\n" +
        "\n" +
        "[Example]\n" +
        "  a/1.png -> a/a_0001.png\n" +
        "  b_s/1.png -> b_s/b_0001_s.png\n" +
        "  c_trace_shitei/1.png -> c_trace_shitei/c_0001_trace_shitei.png",
    ja: "[リネーム対象]\n" +
        "- 桁揃えなしの連番画像ファイル\n" +
        "  (1.png, 2.png, 3.png ...)\n" +
        "- ファイルはサブフォルダに入っていなくてはならない\n" +
        "\n" +
        "[リネームルール]\n" +
        "1.フォルダ直下のファイル名について\n" +
        "  [フォルダ名]_[4桁揃え連番]  とする。\n" +
        "\n" +
        "2.フォルダ名に\"_\"が付く場合については、そのフォルダ内のファイル名は\n" +
        "  [フォルダ名の1つ目の\"_\"より前]_[4桁揃え連番]_[フォルダ名の1つ目の\"_\"より後] とする\n" +
        "\n" +
        "[例]\n" +
        "  a/1.png -> a/a_0001.png\n" +
        "  b_s/1.png -> b_s/b_0001_s.png\n" +
        "  c_trace_shitei/1.png -> c_trace_shitei/c_0001_trace_shitei.png"
  }
};

var imageExtensions = ["png", "tga", "jpg", "jpeg", "tif", "tiff", "sgi", "rgb"];

function rename(path, context) {
  var parts = path.split("/");
  // フォルダの直下にあるファイルだけが対象。
  // フォルダに入っていないファイルや、サブサブフォルダのファイルは対象外
  if (parts.length !== 2)
    return "";
  var folderName = parts[0];
  var fileName = parts[1];

  // ファイル名が数字から始まる画像ファイルである必要がある
  var firstDot = fileName.indexOf(".");
  if (firstDot < 0)
    return "";
  var extension = fileName.substring(fileName.lastIndexOf(".") + 1);
  if (imageExtensions.indexOf(extension) < 0)
    return "";
  var baseName = fileName.substring(0, firstDot);
  var numberMatch = baseName.match(/^\d+/);
  if (!numberMatch)
    return "";

  // 動画番号（4桁揃え）と、その後ろの文字列
  var number = numberMatch[0];
  while (number.length < 4)
    number = "0" + number;
  var suffix = baseName.substring(numberMatch[0].length);

  // フォルダ名を、最初の"_"の前後に分ける（例："b_s" -> "b" と "_s"）
  var separatorPos = folderName.indexOf("_");
  var folderFront = (separatorPos < 0) ? folderName : folderName.substring(0, separatorPos);
  var folderBack = (separatorPos < 0) ? "" : folderName.substring(separatorPos);

  return folderName + "/" + folderFront + "_" + number + suffix + folderBack + "." + extension;
}
