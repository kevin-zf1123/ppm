[日本語](translations.ja.md)

# UI Language and Translation Files

The PPM user interface is written in English.
To show it in another language, PPM uses a **translation file** for that language.
PPM comes with a Japanese translation file, `PPM_ja.qm`.

## Choosing the language

Each staff member chooses the language with **Language** in **Preferences** (see [Initial Setup](../getting-started/initial-setup.md)).

- The change is applied the next time PPM starts.
- Choosing "English" shows the user interface in English, without a translation.
- If no language has ever been chosen, the display language of Windows is used. If there is no translation file for that language, English is used.
- The initial value for the whole studio can be set with `LanguageCode` in `defaultusersettings.ini` in the Stuff folder (see [The Stuff Folder](stuff-folder.md)).

## Where to put translation files

Put translation files in the `loc` folder of the Stuff folder, named `PPM_languagecode.qm`.

```
stuff/
└─ loc/
   ├─ PPM_ja.qm      … Japanese
   └─ PPM_fr.qm      … French (an example of an added language)
```

- **Language** in Preferences lists "English" and the languages of the translation files in the `loc` folder. Each language name is shown in that language itself (for example, "français" for `fr`).
- If the translation file of the chosen language cannot be found, a warning is shown at startup and PPM starts in English. **Language** in Preferences then shows "(translation file not found)" after the language name.
- If the Stuff folder is shared, putting a translation file in the `loc` folder makes it available to everyone.

## What depends on the language

| Contents | Language |
|---|---|
| The PPM user interface: buttons, menus, messages, and so on | The language chosen by each staff member |
| The heading and date format of scene logs, and words such as `[FAILED]` | The language of the PPM of **the person who did the operation** (see "Tracing what happened from the log" in [Backup and Recovery](backup.md)) |
| Names and descriptions of rename script rules | The language, if the script has a name in that language (see [Rename Scripts](../integrations/rename-scripts.md)) |
| Names of workflow groups, workflows, actions, and scene info tags, Chat messages, and so on | Not translated. Shown as written by the project designer |

The workflow names and so on in the sample project are written in Japanese.

## Adding a language

To create a translation file for a new language, use the Qt 6 translation tools (Qt Linguist, `lupdate`, and `lrelease`).
They are included in a Qt installation.

The files that translations are made from (`.ts`) are in the `sources/translations` folder of the source code.
The `.qm` files used by PPM are created from these `.ts` files.

### With a build environment

1. In `sources/CMakeLists.txt`, add the language code of the new language to the following line.

   ```cmake
   set(LANGUAGES ja fr)
   ```

2. Build the CMake target `translation_PPM`.
   `sources/translations/PPM_fr.ts` is created, and `stuff/loc/PPM_fr.qm` is written.
3. Open `PPM_fr.ts` in Qt Linguist, translate it, and save it.
4. Build `translation_PPM` again, and `PPM_fr.qm` is written with your translation.

### With the tools only

In the source code folder, run the following.

```bash
lupdate sources -ts sources/translations/PPM_fr.ts
```

After translating and saving the created `PPM_fr.ts` in Qt Linguist, write the `.qm` file.

```bash
lrelease sources/translations/PPM_fr.ts -qm stuff/loc/PPM_fr.qm
```

### Using the translation file

1. Put the created `PPM_fr.qm` in the `loc` folder of the Stuff folder.
2. Choose the added language with **Language** in Preferences.
3. Restart PPM.

### When PPM is updated

When PPM is updated, there may be new untranslated text.
If you update the `.ts` file with the same steps as above, the new text is added as "unfinished"; translate it and write the `.qm` file again.
Untranslated text is shown in English.

If you would like your translation to be included in PPM, see [Contributing](../about/contributing.md).

---

[← Back to contents](../index.md)
