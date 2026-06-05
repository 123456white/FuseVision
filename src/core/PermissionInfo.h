#ifndef PERMISSIONINFO_H
#define PERMISSIONINFO_H

// =============================================================================
// PermissionInfo — 模块权限的最小数据单元
// =============================================================================
// 被以下模块通用引用（跨层共享，无外部依赖）：
//   - DatabaseManager   ← 从 DB 查询后填充
//   - PermissionRegistry ← 内存缓存中存储
//   - PermissionGuard    ← Widget 级别代理使用
//
// 语义：
//   canRead  = true → 模块对该用户可见
//   canWrite = true → 模块内的操作按钮启用
//   两者独立控制，可实现"看但不能操作""操作但不可见"等细粒度组合
// =============================================================================

struct PermissionInfo {
    bool canRead  = false;  // 读权限：控制模块可见性
    bool canWrite = false;  // 写权限：控制操作按钮启用
};

#endif // PERMISSIONINFO_H
