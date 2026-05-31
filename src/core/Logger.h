#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <QString>

/*
 * 应用程序级日志系统（基于 spdlog + PIMPL 模式）
 *
 * 设计要点：
 * - PIMPL 隔离：头文件不暴露 spdlog 头，避免 20+ 编译单元重复解析
 * - 每日滚动：日志按天切分，文件名格式 fusevision_YYYY-MM-DD.log
 * - 双输出：同时输出到彩色控制台（Debug级）和文件（可配级别）
 * - 运行时调整：可通过 setLevel() 在运行时切换日志等级
 *
 * 使用方式：
 *   Logger::init("/path/to/logs/app", Logger::Debug);
 *   Logger::info("程序启动");
 *   Logger::warn(QString("警告: %1").arg(detail));
 *   Logger::flush();  // 程序退出前刷盘
 */
class Logger
{
public:
    enum Level {
        Trace    = 0,
        Debug    = 1,
        Info     = 2,
        Warn     = 3,
        Error    = 4,
        Critical = 5,
        Off      = 6
    };

    static void init(const QString& logPath, Level level = Info);

    static void setLevel(Level level);

    static void trace(const QString& msg);
    static void debug(const QString& msg);
    static void info(const QString& msg);
    static void warn(const QString& msg);
    static void error(const QString& msg);
    static void critical(const QString& msg);

    static void flush();

private:
    struct Impl;
    static std::unique_ptr<Impl> s_impl;
};

#endif // LOGGER_H
