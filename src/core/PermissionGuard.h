#ifndef PERMISSIONGUARD_H
#define PERMISSIONGUARD_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include "PermissionInfo.h"

class PermissionGuard : public QObject
{
    Q_OBJECT

public:
    explicit PermissionGuard(QObject* parent = nullptr);

    void watch(const QString& moduleKey);
    void watch(const QStringList& moduleKeys);

    bool canRead(const QString& moduleKey)  const;
    bool canWrite(const QString& moduleKey) const;

    QStringList watchedKeys() const;

signals:
    void changed();

private slots:
    void refresh();

private:
    int currentUserId() const;
    QMap<QString, PermissionInfo> m_state;
};

#endif // PERMISSIONGUARD_H
