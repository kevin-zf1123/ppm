#include "projectsettings_common.h"

#include <QMessageBox>
#include <QObject>
#include <QSet>
#include <QPair>
#include <QHash>

ProjectSettings tmpSettings;

std::function<void()> g_onDirtyChanged;

void markDirty() {
  tmpSettings.dirtyFlag = true;
  if (g_onDirtyChanged) g_onDirtyChanged();
}

void refreshTmpSettingsFromProject() {
  tmpSettings.projectTitle = WorkflowManager::instance()->projectTitle();
  tmpSettings.projectDescription =
      WorkflowManager::instance()->projectDescription();
  // パスは、相対パスのまま編集・保存できるよう、解決前の設定値を使う
  tmpSettings.projectRootPath =
      WorkflowManager::instance()->projectRootPathSetting();
  tmpSettings.logRootPath = WorkflowManager::instance()->logRootPathSetting();
  tmpSettings.projectInfoFolderPath =
      WorkflowManager::instance()->infoFolderPathSetting();
  tmpSettings.cutNumberPrefix = WorkflowManager::instance()->cutNumberPrefix();
  tmpSettings.useSequence     = WorkflowManager::instance()->useSequence();
  tmpSettings.gasUrl          = WorkflowManager::instance()->gasUrl();
  tmpSettings.chatDestinations =
      WorkflowManager::instance()->chatDestinations();
  tmpSettings.sceneThumbnailPath =
      WorkflowManager::instance()->sceneThumbnailPath();
  tmpSettings.hasEpisodes  = WorkflowManager::instance()->hasEpisodes();
  tmpSettings.episodeNames = WorkflowManager::instance()->episodeNames();
  tmpSettings.workflows.clear();
  for (int wfId = 0; wfId < WorkflowManager::instance()->workflowCount();
       wfId++)
    tmpSettings.workflows.append(
        WorkflowManager::instance()->getWorkflow(wfId));
  // パスのマクロ
  tmpSettings.pathMacros.clear();
  tmpSettings.pathMacros = WorkflowManager::instance()->pathMacros();

  tmpSettings.flowGroupInfos = WorkflowManager::instance()->flowGroupInfos();
  // カット情報（前回ダイアログを開いた際に複製したタグを解放してから作り直す）
  for (auto infoTag : tmpSettings.sceneInfoTags) delete infoTag;
  tmpSettings.sceneInfoTags.clear();
  for (auto infoTag : WorkflowManager::instance()->sceneInfoTagList())
    tmpSettings.sceneInfoTags.append(infoTag->clone());

  // フローチャートUIのノード配置情報
  tmpSettings.flowChartLayout = WorkflowManager::instance()->flowChartLayout();
  // 役職（フローチャートUIの役職パレット）
  tmpSettings.roles = WorkflowManager::instance()->roles();

  // 読み込み直後は未変更の状態から開始する
  tmpSettings.dirtyFlag = false;
}

namespace {
bool variantUsesSequenceMacro(const QVariant& v) {
  switch (v.typeId()) {
  case QMetaType::QString:
    return v.toString().contains("[SEQUENCE]");
  case QMetaType::QVariantList:
    for (const QVariant& child : v.toList())
      if (variantUsesSequenceMacro(child)) return true;
    return false;
  case QMetaType::QVariantMap:
    for (const QVariant& child : v.toMap())
      if (variantUsesSequenceMacro(child)) return true;
    return false;
  default:
    return false;
  }
}

// tmpSettingsのうち、[SEQUENCE]マクロを使っている設定の種類を返す
QStringList settingsUsingSequenceMacro() {
  QStringList ret;
  QStringList macroNames;
  for (const PathMacroInfo& macroInfo : tmpSettings.pathMacros)
    if (macroInfo.path.contains("[SEQUENCE]"))
      macroNames.append(macroInfo.macroStr);
  if (!macroNames.isEmpty())
    ret.append(QObject::tr("Path macros: %1").arg(macroNames.join(", ")));
  if (tmpSettings.sceneThumbnailPath.contains("[SEQUENCE]"))
    ret.append(QObject::tr("Scene Thumbnail Path"));
  for (const FlowGroupInfo& info : tmpSettings.flowGroupInfos) {
    if (info.xdtsFileName.contains("[SEQUENCE]") ||
        info.subfolderForOpenButton.contains("[SEQUENCE]")) {
      ret.append(QObject::tr("Workflow groups"));
      break;
    }
  }
  bool inWorkflows = false;
  for (const Workflow& wf : tmpSettings.workflows) {
    for (const WorkflowProtocol& protocol : wf.protocols) {
      if (protocol.conditionPath.contains("[SEQUENCE]")) inWorkflows = true;
      for (const PPM_Action& action : protocol.actions)
        for (const ActionStep& step : action.steps)
          for (const QVariant& param : step.params)
            if (variantUsesSequenceMacro(param)) inWorkflows = true;
    }
  }
  if (inWorkflows)
    ret.append(QObject::tr("Workflows (condition paths and steps)"));
  return ret;
}

// [SEQUENCE]を使うかの設定が変わる場合に、確認する。続行してよければtrue
bool confirmUseSequenceChange(QWidget* parent) {
  if (tmpSettings.useSequence == WorkflowManager::instance()->useSequence())
    return true;

  QString message;
  if (tmpSettings.useSequence) {
    message = QObject::tr(
        "\"Group Scenes by 100 [SEQUENCE]\" will be turned ON.\n\n"
        "Scene names must begin with a number of 3 or more digits. "
        "Scenes whose names do not satisfy this cannot be used.\n\n");
  } else {
    message = QObject::tr(
        "\"Group Scenes by 100 [SEQUENCE]\" will be turned OFF.\n\n"
        "The [SEQUENCE] macro will be replaced with an empty string, and the "
        "logs and scene information "
        "will be saved without sequence subfolders.\n\n");
    QStringList users = settingsUsingSequenceMacro();
    if (!users.isEmpty())
      message +=
          QObject::tr("The following settings still use [SEQUENCE]:\n - %1\n\n")
              .arg(users.join("\n - "));
  }
  message += QObject::tr(
      "PPM does not reorganize the existing logs and scene information. "
      "The logs, the scene information and the take (retake) information saved "
      "in the old folder structure "
      "will no longer be found, and [TAKE] / [R_TAKE] may point to the wrong "
      "folders. "
      "Changing this setting in the middle of production is not recommended. "
      "If you do, move the folders under the log folder and the \"scene_info\" "
      "folder of the project info folder by hand.\n\n"
      "Do you want to save the project?");
  return QMessageBox::warning(parent, QObject::tr("Warning"), message,
                              QMessageBox::Save | QMessageBox::Cancel,
                              QMessageBox::Cancel) == QMessageBox::Save;
}

// 保存先のppmprjが、PPMが読み込んだ後にほかの人によって書き換えられていた場合や、
// 別のプロジェクトのファイルとして既にある場合に、上書きしてよいかを確認する。
// 保存しない場合はfalseを返す
bool confirmOverwriteChangedProjectFile(QWidget* parent,
                                        const QString& prjName) {
  WorkflowManager* wm = WorkflowManager::instance();
  if (!wm->projectFileChangedSinceLoaded(prjName)) return true;

  QString message;
  if (prjName == wm->currentProjectName())
    message = QObject::tr(
        "The project file \"%1\" has been changed (probably by someone else) "
        "after this PPM loaded it.\n"
        "If you save now, those changes will be lost.\n\n"
        "To keep them, cancel, close the Project Settings without saving, "
        "and reload the project (restart PPM, or switch to another project "
        "and back in the main window).\n\n"
        "Do you want to overwrite the project file?");
  else
    message = QObject::tr(
        "A project with the ID \"%1\" already exists.\n"
        "If you save now, the settings of that project will be replaced.\n\n"
        "Do you want to overwrite the project file?");
  return QMessageBox::warning(parent, QObject::tr("Warning"),
                              message.arg(prjName),
                              QMessageBox::Save | QMessageBox::Cancel,
                              QMessageBox::Cancel) == QMessageBox::Save;
}
}  // namespace

bool saveTmpSettingsToProject(QWidget* parent, const QString& projectName) {
  QString newProjectName = projectName.trimmed();
  if (newProjectName.isEmpty()) {
    QMessageBox::warning(parent, QObject::tr("Warning"),
                         QObject::tr("Enter the project ID at the bottom left. "
                                     "It is used as the file name of the "
                                     "project (<Project ID>.ppmprj)."));
    return false;
  }

  if (!confirmUseSequenceChange(parent)) return false;
  if (!confirmOverwriteChangedProjectFile(parent, newProjectName)) return false;

  WorkflowManager::instance()->setProjectTitle(tmpSettings.projectTitle);
  WorkflowManager::instance()->setProjectDescription(
      tmpSettings.projectDescription);
  WorkflowManager::instance()->setProjectRootPath(tmpSettings.projectRootPath);
  WorkflowManager::instance()->setLogRootPath(tmpSettings.logRootPath);
  WorkflowManager::instance()->setInfoFolderPath(
      tmpSettings.projectInfoFolderPath);
  WorkflowManager::instance()->setCutNumberPrefix(tmpSettings.cutNumberPrefix);
  WorkflowManager::instance()->setUseSequence(tmpSettings.useSequence);
  WorkflowManager::instance()->setGasUrl(tmpSettings.gasUrl);
  WorkflowManager::instance()->setChatDestinations(
      tmpSettings.chatDestinations);
  WorkflowManager::instance()->setSceneThumbnailPath(
      tmpSettings.sceneThumbnailPath);
  WorkflowManager::instance()->setHasEpisodes(tmpSettings.hasEpisodes);
  WorkflowManager::instance()->setEpisodeNames(tmpSettings.episodeNames);
  WorkflowManager::instance()->setWorkflows(tmpSettings.workflows);
  WorkflowManager::instance()->setPathMacros(tmpSettings.pathMacros);
  WorkflowManager::instance()->setFlowGroupInfos(tmpSettings.flowGroupInfos);
  WorkflowManager::instance()->setSceneInfoTagList(tmpSettings.sceneInfoTags);
  WorkflowManager::instance()->setFlowChartLayout(tmpSettings.flowChartLayout);
  WorkflowManager::instance()->setRoles(tmpSettings.roles);

  // プロジェクト名を元のプロジェクトIDから変更していた場合、
  // ここで別IDのppmprjファイルとして保存される（内容を別プロジェクトにコピーする形になる）
  QString errorString;
  if (!WorkflowManager::instance()->saveProject(newProjectName, &errorString)) {
    // 未保存の印（dirtyFlag）は残し、閉じるときにも保存を促せるようにする
    QMessageBox::critical(
        parent, QObject::tr("Error"),
        QObject::tr("Failed to save the project file:\n%1\n\n"
                    "Check that the projects folder of the Stuff folder is "
                    "accessible and not read-only.")
            .arg(errorString));
    return false;
  }

  tmpSettings.dirtyFlag = false;
  return true;
}

int insertPositionForWorkflowGroupEnd(FlowGroupId groupId) {
  QList<FlowGroupId> orderedGroupIds =
      tmpSettings.flowGroupInfos.keys();  // QMapなので昇順
  int groupOrderPos = orderedGroupIds.indexOf(groupId);
  if (groupOrderPos < 0)
    return tmpSettings.workflows.size();  // 未知のグループは末尾に追加する

  for (int i = 0; i < tmpSettings.workflows.size(); i++) {
    int gidOrderPos =
        orderedGroupIds.indexOf(tmpSettings.workflows.at(i).groupId);
    if (gidOrderPos > groupOrderPos) return i;
  }
  return tmpSettings.workflows.size();
}

int moveWorkflowToGroupEnd(int currentIndex, FlowGroupId newGroupId) {
  if (currentIndex < 0 || currentIndex >= tmpSettings.workflows.size())
    return currentIndex;
  Workflow wf = tmpSettings.workflows.takeAt(currentIndex);
  wf.groupId  = newGroupId;
  // insertPositionForWorkflowGroupEnd()はtakeAt後の（現在の）リストを走査するため、
  // 取り除いたことによるインデックスのずれを別途補正する必要はない
  int insertPos = insertPositionForWorkflowGroupEnd(newGroupId);
  tmpSettings.workflows.insert(insertPos, wf);
  return insertPos;
}

//--------------------------------
// キューファイルパス／Step_Rename自動設定機能

QList<ActionLocation> findActionsLeadingTo(WorkflowType targetType) {
  QList<ActionLocation> result;
  for (auto& wf : tmpSettings.workflows) {
    for (int p = 0; p < wf.protocols.size(); p++) {
      const QList<PPM_Action>& actions = wf.protocols.at(p).actions;
      for (int a = 0; a < actions.size(); a++) {
        if (actions.at(a).allNextFlows().contains(targetType))
          result.append({wf.workflowType, p, a});
      }
    }
  }
  return result;
}

namespace {
// Step_ConditionalBranchByInfo/FileExistenceのparamsに埋め込まれた入れ子ActionStepを取り出す
// （projectsettingsdialog.cppのpackActionStep()/unpackActionStep()と同じ表現規約）
ActionStep unpackNestedActionStepForCueFilePaths(const QVariant& packed) {
  QVariantList list = packed.toList();
  if (list.size() < 2) return ActionStep{Step_Unidentified, {}};
  return ActionStep{(ActionStepType)list[0].toInt(), list[1].toList()};
}

// Step_Rename/Step_Copy/Step_CreateCueFileで生成されるファイルパスをresultへ集める。
// Step_ConditionalBranchByInfo/FileExistenceについては、真偽どちらの分岐に設定されていても
// （両方に設定されていなくても）条件パスとして拾えるよう、両方の入れ子ステップを再帰的に辿る
void collectGeneratedCueFilePathsInto(const ActionStep& step,
                                      QStringList& result) {
  switch (step.type) {
  case Step_Rename:
  case Step_Copy:
    for (auto& param : step.params) {
      QString path = param.toString();
      if (!path.isEmpty()) result.append(path);
    }
    break;
  case Step_CreateCueFile:
    if (!step.params.isEmpty()) {
      QString path = step.params.at(0).toString();
      if (!path.isEmpty()) result.append(path);
    }
    break;
  case Step_ConditionalBranchByInfo:
    // params: [infoIdName, infoValue, trueStep(packed), falseStep(packed)]
    if (step.params.size() >= 4) {
      collectGeneratedCueFilePathsInto(
          unpackNestedActionStepForCueFilePaths(step.params.at(2)), result);
      collectGeneratedCueFilePathsInto(
          unpackNestedActionStepForCueFilePaths(step.params.at(3)), result);
    }
    break;
  case Step_ConditionalBranchByFileExistence:
    // params: [filePath, trueStep(packed), falseStep(packed)]
    if (step.params.size() >= 3) {
      collectGeneratedCueFilePathsInto(
          unpackNestedActionStepForCueFilePaths(step.params.at(1)), result);
      collectGeneratedCueFilePathsInto(
          unpackNestedActionStepForCueFilePaths(step.params.at(2)), result);
    }
    break;
  default:
    break;
  }
}
}  // namespace

QStringList collectGeneratedCueFilePaths(const PPM_Action& action) {
  QStringList result;
  for (auto& step : action.steps)
    collectGeneratedCueFilePathsInto(step, result);
  result.removeDuplicates();
  return result;
}

QString insertSuffixBeforeExtension(const QString& path,
                                    const QString& suffix) {
  int slashIdx = qMax(path.lastIndexOf('/'), path.lastIndexOf('\\'));
  int dotIdx   = path.lastIndexOf('.');
  if (dotIdx <= slashIdx)  // 拡張子なし
    return path + suffix;
  return path.left(dotIdx) + suffix + path.mid(dotIdx);
}

namespace {
Workflow* mutableWorkflowByType(WorkflowType type) {
  for (auto& wf : tmpSettings.workflows)
    if (wf.workflowType == type) return &wf;
  return nullptr;
}

// walk(previousWorkflow,
// previousProtocol)を再帰的に行う。visitedは(workflowType, protocolIdx)の
// 訪問済み集合で、疑似コードには無いが循環接続時の無限再帰を防ぐための安全策
void walkRenamingChain(QWidget* dialogParent, WorkflowType wfType,
                       int protocolIdx, QSet<QPair<WorkflowType, int>>& visited,
                       RenamingChainSummary& summary) {
  QPair<WorkflowType, int> visitKey(wfType, protocolIdx);
  if (visited.contains(visitKey)) return;
  visited.insert(visitKey);

  Workflow* wf = mutableWorkflowByType(wfType);
  if (!wf || protocolIdx < 0 || protocolIdx >= wf->protocols.size()) return;
  // conditionPathはこの後の再帰でも参照するため、参照ではなく値としてコピーしておく
  // （下のループ内でtmpSettings.workflowsへ要素を追加することは無いので実際には安定しているが、
  // 意図を明確にするため値渡しにする）
  QString conditionPath = wf->protocols.at(protocolIdx).conditionPath;
  MediaType mediaType   = wf->protocols.at(protocolIdx).mediaType;
  if (conditionPath.isEmpty()) return;

  QList<ActionLocation> previousActions = findActionsLeadingTo(wfType);
  for (auto& prevLoc : previousActions) {
    Workflow* previousWorkflow = mutableWorkflowByType(prevLoc.workflowType);
    if (!previousWorkflow) continue;
    if (prevLoc.protocolIdx < 0 ||
        prevLoc.protocolIdx >= previousWorkflow->protocols.size())
      continue;
    WorkflowProtocol& previousProtocol =
        previousWorkflow->protocols[prevLoc.protocolIdx];
    if (prevLoc.actionIdx < 0 ||
        prevLoc.actionIdx >= previousProtocol.actions.size())
      continue;
    PPM_Action& previousAction = previousProtocol.actions[prevLoc.actionIdx];

    if (previousProtocol.mediaType != mediaType) continue;

    // previousActionにcondition_pathをパラメータとするStep_Renameを、もしなければ追加する
    int renameStepIdx = -1;
    for (int i = 0; i < previousAction.steps.size(); i++) {
      if (previousAction.steps.at(i).type == Step_Rename) {
        renameStepIdx = i;
        break;
      }
    }
    if (renameStepIdx < 0) {
      ActionStep newStep;
      newStep.type   = Step_Rename;
      newStep.params = {conditionPath};
      previousAction.steps.append(newStep);
      summary.stepsAdded++;
    } else {
      ActionStep& renameStep = previousAction.steps[renameStepIdx];
      bool alreadyHasPath    = false;
      for (auto& param : renameStep.params) {
        if (param.toString() == conditionPath) {
          alreadyHasPath = true;
          break;
        }
      }
      if (!alreadyHasPath) {
        QString oldPathsTxt;
        for (auto& param : renameStep.params)
          oldPathsTxt += QObject::tr("\n - %1").arg(param.toString());
        QMessageBox::StandardButton ret = QMessageBox::question(
            dialogParent, QObject::tr("Change Existing Step_Rename?"),
            QObject::tr(
                "\"%1\" already has a Step_Rename step with a different "
                "destination:%2\n\n"
                "Replace it with the following single destination?\n - %3")
                .arg(previousAction.actionName, oldPathsTxt, conditionPath),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes) {
          renameStep.params = {conditionPath};
          summary.stepsChanged++;
        }
      }
    }

    if (previousWorkflow->suffix.isEmpty()) continue;

    // previousProtocol内のpreviousAction以外のアクションが、wf以外の工程と繋がっている場合、
    // previousWorkflowがここで枝分かれしているとみなし、これ以上は遡らない
    bool hasOtherBranch = false;
    for (int a2 = 0; a2 < previousProtocol.actions.size(); a2++) {
      if (a2 == prevLoc.actionIdx) continue;
      for (auto next : previousProtocol.actions.at(a2).allNextFlows()) {
        if (next != wfType) {
          hasOtherBranch = true;
          break;
        }
      }
      if (hasOtherBranch) break;
    }
    if (hasOtherBranch) continue;

    if (!previousProtocol.conditionPath.isEmpty()) {
      QMessageBox::StandardButton ret = QMessageBox::question(
          dialogParent, QObject::tr("Overwrite Condition Path?"),
          QObject::tr("The condition path of \"%1\" will be "
                      "changed:\n\nFrom:\n - %2\n\nTo:\n - %3")
              .arg(previousWorkflow->name, previousProtocol.conditionPath,
                   insertSuffixBeforeExtension(conditionPath,
                                               previousWorkflow->suffix)),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (ret != QMessageBox::Yes) continue;
    }

    previousProtocol.conditionPath =
        insertSuffixBeforeExtension(conditionPath, previousWorkflow->suffix);
    summary.conditionPathsSet++;

    walkRenamingChain(dialogParent, prevLoc.workflowType, prevLoc.protocolIdx,
                      visited, summary);
  }
}
}  // namespace

RenamingChainSummary autoConfigureRenamingChains(QWidget* dialogParent,
                                                 WorkflowType clickedWfType) {
  RenamingChainSummary summary;
  QSet<QPair<WorkflowType, int>> visited;

  Workflow* wf = mutableWorkflowByType(clickedWfType);
  if (!wf) return summary;

  bool anyConditionPath = false;
  for (int p = 0; p < wf->protocols.size(); p++) {
    if (wf->protocols.at(p).conditionPath.isEmpty()) continue;
    anyConditionPath = true;
    walkRenamingChain(dialogParent, clickedWfType, p, visited, summary);
  }

  if (!anyConditionPath) {
    QMessageBox::information(
        dialogParent, QObject::tr("Auto-configure Renaming Chains"),
        QObject::tr("This workflow has no protocol with a condition path."));
    return summary;
  }

  QMessageBox::information(dialogParent,
                           QObject::tr("Auto-configure Renaming Chains"),
                           QObject::tr("Step_Rename added: %1\nStep_Rename "
                                       "changed: %2\nCondition paths set: %3")
                               .arg(summary.stepsAdded)
                               .arg(summary.stepsChanged)
                               .arg(summary.conditionPathsSet));

  return summary;
}
