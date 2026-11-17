#include "DatabaseManager.h"
#include "SessionManager.h"
#include "SettingsManager.h"
#include "Logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QSettings>
#include <QTemporaryDir>
#include <QUuid>

DatabaseManager& DatabaseManager::instance() { static DatabaseManager inst; return inst; }
DatabaseManager::~DatabaseManager() { if(m_db.isOpen()) m_db.close(); }
QString DatabaseManager::hashPassword(const QString& password) {
    return QString::fromLatin1(QCryptographicHash::hash(password.toUtf8(),QCryptographicHash::Sha256).toHex());
}
bool DatabaseManager::initDatabase(const QString& databasePath,QString* errorOut) {
    if(errorOut) errorOut->clear();
    auto fail=[&](const QString& error) {if(errorOut) *errorOut=error;Logger::error(error);return false;};
    const auto legacy=QCoreApplication::applicationDirPath()+"/data/fusevision.db";
    const auto configured=SettingsManager::instance().dbPath();
    const auto path=databasePath.isEmpty()?QFileInfo(configured.isEmpty()?legacy:QDir(configured).filePath("fusevision.db")).absoluteFilePath():databasePath;
    if(m_db.isOpen()) return m_db.databaseName()==path?true:fail(QString::fromUtf8("数据库已打开；路径变更需重启。"));
    if(path!=":memory:" && !QDir().mkpath(QFileInfo(path).absolutePath())) return fail(QString::fromUtf8("无法创建数据库目录：%1").arg(path));
    QSettings settings(QSettings::defaultFormat(),QSettings::UserScope,"FuseVisionTeam","FuseVision");
    if(databasePath.isEmpty() && !QFileInfo::exists(path)) {
        const auto source=settings.value("database/last_opened_file",legacy).toString();
        const bool remembered=settings.contains("database/last_opened_file");
        if(remembered && !QFileInfo::exists(source)) return fail(QString::fromUtf8("上次使用的数据库缺失，拒绝新建账户库：%1").arg(source));
        if(QFileInfo::exists(source)) {
            // VACUUM INTO creates a consistent SQLite snapshot, including committed WAL pages.
            // Keep the source intact and publish only after the snapshot connection is closed.
            QTemporaryDir stage(QFileInfo(path).absolutePath()+"/.fusevision-db-XXXXXX");
            if(!stage.isValid()) return fail(QString::fromUtf8("无法创建数据库迁移暂存目录：%1").arg(path));
            const auto connection="migration-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
            QString error;
            {
                auto snapshot=QSqlDatabase::addDatabase("QSQLITE",connection);
                snapshot.setDatabaseName(source);snapshot.setConnectOptions("QSQLITE_OPEN_READONLY");
                if(!snapshot.open()) error=snapshot.lastError().text();
                else {
                    QSqlQuery query(snapshot);query.prepare("VACUUM INTO ?");query.addBindValue(stage.filePath("snapshot.db"));
                    if(!query.exec()) error=query.lastError().text();
                }
                snapshot.close();
            }
            QSqlDatabase::removeDatabase(connection);
            if(!error.isEmpty()) return fail(QString::fromUtf8("数据库迁移失败，原库保留：%1 → %2：%3").arg(source,path,error));
            if(!QFile::rename(stage.filePath("snapshot.db"),path)) return fail(QString::fromUtf8("无法提交数据库副本，原库保留：%1 → %2").arg(source,path));
        }
    }
    m_db=QSqlDatabase::addDatabase("QSQLITE","fusevision_conn");
    m_db.setDatabaseName(path);
    if(!m_db.open()) return fail("Failed to open database: "+path+"："+m_db.lastError().text());
    if(!createTables() || !seedUsers()) {const auto error=m_db.lastError().text();m_db.close();return fail("Failed to initialize database: "+path+"："+error);}
    if(databasePath.isEmpty()) {
        settings.setValue("database/last_opened_file",path);settings.sync();
        if(settings.status()!=QSettings::NoError) {m_db.close();return fail(QString::fromUtf8("无法保存活动数据库路径：%1").arg(settings.fileName()));}
    }
    return true;
}
bool DatabaseManager::isConnected() const { return m_db.isOpen(); }
bool DatabaseManager::createTables() {
    QSqlQuery q(m_db);
    // Keep the existing schema and any legacy user_permissions table intact.
    return q.exec("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  "username TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL,role INTEGER NOT NULL DEFAULT 0)");
}
bool DatabaseManager::seedUsers() {
    QSqlQuery q(m_db);
    if(!q.exec("SELECT COUNT(*) FROM users") || !q.next()) return false;
    if(q.value(0).toInt()>0) return true;
    q.prepare("INSERT INTO users (username,password_hash,role) VALUES (?,?,?)");
    q.addBindValue("superadmin");q.addBindValue(hashPassword("admin123"));q.addBindValue(2);
    return q.exec();
}
UserInfo DatabaseManager::singleAccount() const {
    QSqlQuery q(m_db);
    // Reuse superadmin and its current password; otherwise reuse the oldest highest-role account.
    q.exec("SELECT id,username,role FROM users ORDER BY CASE WHEN username='superadmin' THEN 0 ELSE 1 END,role DESC,id LIMIT 1");
    if(q.next()) return {q.value(0).toInt(),q.value(1).toString(),q.value(2).toInt()};
    return {};
}
LoginResult DatabaseManager::validateLogin(const QString& username,const QString& password) {
    const auto account=singleAccount();
    if(account.id<0 || account.username!=username || password.isEmpty()) return {};
    QSqlQuery q(m_db);q.prepare("SELECT password_hash FROM users WHERE id=?");q.addBindValue(account.id);
    if(!q.exec() || !q.next() || q.value(0).toString()!=hashPassword(password)) return {};
    return {true,account.id};
}
QList<UserInfo> DatabaseManager::allUsers() const {
    QList<UserInfo> users;QSqlQuery q(m_db);q.exec("SELECT id,username,role FROM users ORDER BY id");
    while(q.next()) users.append({q.value(0).toInt(),q.value(1).toString(),q.value(2).toInt()});
    return users;
}
int DatabaseManager::getUserId(const QString& username) const {
    QSqlQuery q(m_db);q.prepare("SELECT id FROM users WHERE username=?");q.addBindValue(username);
    return q.exec() && q.next()?q.value(0).toInt():-1;
}
bool DatabaseManager::changePassword(int userId,const QString& oldPassword,const QString& newPassword) {
    const auto& session=SessionManager::instance();
    if(!session.isLoggedIn() || userId!=session.currentUserId() || newPassword.size()<6) return false;
    const auto account=singleAccount();
    if(account.id!=userId || !validateLogin(account.username,oldPassword).success) return false;
    QSqlQuery q(m_db);
    // Compare-and-update preserves the old password if another writer changed it in between.
    q.prepare("UPDATE users SET password_hash=? WHERE id=? AND password_hash=?");
    q.addBindValue(hashPassword(newPassword));q.addBindValue(userId);q.addBindValue(hashPassword(oldPassword));
    return q.exec() && q.numRowsAffected()==1;
}
