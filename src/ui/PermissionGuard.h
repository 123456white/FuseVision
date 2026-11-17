/** @file
 * @brief PermissionGuard 的公共声明。
 * @ingroup ui_common
 */
#pragma once
#include <QObject>
#include <QStringList>

// Existing widgets keep their page keys; every key follows the same login state.
/** @brief 把会话变化转换为控件刷新信号；parent 管理实例生命周期。
 * @ingroup ui_common
 */
class PermissionGuard : public QObject {
    Q_OBJECT
public:
    explicit PermissionGuard(QObject* parent=nullptr);
    void watch(const QString& key);
    void watch(const QStringList& keys);
    bool canRead(const QString& key) const;
    bool canWrite(const QString& key) const;
    QStringList watchedKeys() const;
    int currentUserId() const;
public slots:
    void refresh();
signals:
    void changed();
private:
    QStringList m_keys;
};
