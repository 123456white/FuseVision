#include "Logger.h"
#include <QFileInfo>
#include <QDir>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

/*
 * Logger.cpp — PIMPL 实现体，封装所有 spdlog 依赖
 *
 * 日志格式: [日期 时间.毫秒] [级别] [源文件:行号] 日志内容
 * 文件策略: daily_file_sink_mt 每日滚动，不限制文件数
 * 控制台:   stdout_color_sink_mt 彩色输出（Windows 需 ANSI 支持）
 * 刷盘策略: trace 及以上自动 flush（确保崩溃前日志落盘）
 */

struct Logger::Impl
{
    std::shared_ptr<spdlog::logger> logger;
    QString directory;
    QString baseName;
    
    static spdlog::level::level_enum toSpdlog(Logger::Level level)
    {
        switch (level) {
        case Logger::Trace:    return spdlog::level::trace;
        case Logger::Debug:    return spdlog::level::debug;
        case Logger::Info:     return spdlog::level::info;
        case Logger::Warn:     return spdlog::level::warn;
        case Logger::Error:    return spdlog::level::err;
        case Logger::Critical: return spdlog::level::critical;
        case Logger::Off:      return spdlog::level::off;
        default:               return spdlog::level::info;
        }
    }
};

std::unique_ptr<Logger::Impl> Logger::s_impl;

bool Logger::init(const QString& logPath, Level level, QString* errorOut)
{
    // 先释放旧 logger（从 spdlog 全局注册表移除 + 销毁 s_impl）
    try {

    QFileInfo fileInfo(logPath);
    QString dirPath;
    QString baseName;

    // 判断 logPath 是目录还是文件路径：
    // 既有目录和非.log路径均作为目录；只有显式.log文件使用自定义前缀。
    if (fileInfo.isDir() || fileInfo.suffix().compare("log", Qt::CaseInsensitive) != 0) {
        // logPath 是目录路径
        dirPath  = QDir(logPath).absolutePath();
        baseName = "fusevision";
    } else {
        // logPath 是文件路径
        dirPath  = fileInfo.absoluteDir().absolutePath();
        baseName = fileInfo.baseName();
    }

    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    std::string dailyBasePath = (dirPath + "/" + baseName).toStdString();

    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
        dailyBasePath, 0, 0, false, 0);
    file_sink->set_level(Impl::toSpdlog(level));

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(file_sink);  // 文件 sink 始终启用

#ifndef NDEBUG
    // Debug 模式：启用控制台彩色输出（Release 只写文件）
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    sinks.push_back(console_sink);
#endif

    auto logger = std::make_shared<spdlog::logger>("fusevision", sinks.begin(), sinks.end());
    logger->set_level(Impl::toSpdlog(level));
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
    logger->flush_on(spdlog::level::trace);

    spdlog::drop("fusevision");
    spdlog::register_logger(logger);

    s_impl = std::make_unique<Impl>();
    s_impl->logger = logger;
    s_impl->directory=dirPath;
    s_impl->baseName=baseName;

    s_impl->logger->info("Logger initialized (daily rolling, append mode). Log directory: {}",
        dirPath.toStdString());
    return true;
    } catch(const std::exception& error) {
        if(errorOut) *errorOut=QString::fromUtf8(error.what());
        return false;
    }
}

void Logger::setLevel(Level level)
{
    if (s_impl && s_impl->logger) {
        s_impl->logger->set_level(Impl::toSpdlog(level));
    }
}

QString Logger::activeDirectory() { return s_impl?s_impl->directory:QString(); }
QString Logger::activeBaseName() { return s_impl?s_impl->baseName:QString(); }

void Logger::trace(const QString& msg)    { if (s_impl && s_impl->logger) s_impl->logger->trace(msg.toStdString()); }
void Logger::debug(const QString& msg)    { if (s_impl && s_impl->logger) s_impl->logger->debug(msg.toStdString()); }
void Logger::info(const QString& msg)     { if (s_impl && s_impl->logger) s_impl->logger->info(msg.toStdString()); }
void Logger::warn(const QString& msg)     { if (s_impl && s_impl->logger) s_impl->logger->warn(msg.toStdString()); }
void Logger::error(const QString& msg)    { if (s_impl && s_impl->logger) s_impl->logger->error(msg.toStdString()); }
void Logger::critical(const QString& msg) { if (s_impl && s_impl->logger) s_impl->logger->critical(msg.toStdString()); }

void Logger::flush()
{
    if (s_impl && s_impl->logger) {
        s_impl->logger->flush();
    }
}
