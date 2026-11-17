#include "WriteAccess.h"
#include "core/Logger.h"

#include <QMessageBox>

bool WriteAccess::require(const QString& operation, QWidget* parent)
{
    if (AccessControl::canWrite(operation))
        return true;

    Logger::warn(QString::fromUtf8("只读模式拒绝写操作：%1").arg(operation));
    QMessageBox::warning(parent, QObject::tr("只读模式"),
                         QObject::tr("请先登录管理账户，再执行修改操作。"));
    return false;
}
