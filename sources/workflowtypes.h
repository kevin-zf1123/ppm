#pragma once
#ifndef WORKFLOWTYPES_H
#define WORKFLOWTYPES_H

#include <limits>

// 制作工程（ワークフロー）のID。スタジオごとにプロジェクト設定で自由に定義できるよう、
// 固定のenumではなくユーザーが指定する一意の値として扱う。
typedef unsigned int WorkflowType;
// 「未設定」を表す番兵値。自動採番されるIDは0から始まるため衝突しない。
const WorkflowType Wf_Undefined = std::numeric_limits<WorkflowType>::max();

#endif