/** @file
 * @brief WriteAccess 的公共声明。
 * @ingroup ui_common
 */
#pragma once

#include "core/AccessControl.h"

class QWidget;

/**
 * @brief 写操作的界面授权反馈。
 * @ingroup ui_common
 *
 * 核心 AccessControl 仅判断会话；本接口负责 GUI 线程上的拒绝日志和弹窗。
 * 调用方必须在实际修改前检查返回值，不能只依赖按钮的 enabled 状态。
 */
namespace WriteAccess {
/** @brief 已登录返回 true；否则提示只读并返回 false，不改变会话或业务数据。 */
bool require(const QString& operation, QWidget* parent);
}
