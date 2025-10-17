#pragma once

// 必须在 #include <spdlog/...> 之前定义 SPDLOG_ACTIVE_LEVEL
// 以防止编译时日志级别限制：SPDLOG_ACTIVE_LEVEL宏可能限制了日志级别。
#ifndef SPDLOG_ACTIVE_LEVEL
#  ifdef CO_DEBUG_MODE
#    define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#  elif defined(CO_RELWITHDEBINFO_MODE)
#    define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#  else
#    define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#  endif
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>

// namespace czmosg
// {
//     inline constexpr const char* coLoggerName = "CESIUM-OSG";

//     inline std::shared_ptr<spdlog::logger>& logger() {
//         static auto instance = spdlog::get(coLoggerName);
//         if (!instance) {
//             // 如果未创建，创建一个默认控制台 logger（或根据需要配置）
//             instance = spdlog::stdout_color_mt(coLoggerName);
//             // 设置日志级别
//         #ifdef CO_DEBUG_MODE
//             instance->set_level(spdlog::level::trace);
//         #else
//             instance->set_level(spdlog::level::info);
//         #endif
//             // 设置时间精确到秒：使用 %S（秒），不要 %e（毫秒）
//             instance->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
//         }
//         return instance;
//     }
    
// } // namespace czmosg

namespace czmosg
{
    inline constexpr const char* coLoggerName = "CESIUM-OSG";

    // 声明全局logger实例
    extern std::shared_ptr<spdlog::logger> g_loggerInstance;

    inline std::shared_ptr<spdlog::logger>& logger() {
        // 使用全局实例确保所有编译单元使用同一个logger
        return g_loggerInstance;
    }
    
    // 初始化函数
    void initializeLogger();
    
} // namespace czmosg

// 日志宏：使用 logger().get() 获取原始指针
#define CO_TRACE(...)   SPDLOG_LOGGER_TRACE(::czmosg::logger().get(), __VA_ARGS__)
#define CO_DEBUG(...)   SPDLOG_LOGGER_DEBUG(::czmosg::logger().get(), __VA_ARGS__)
#define CO_INFO(...)    SPDLOG_LOGGER_INFO(::czmosg::logger().get(), __VA_ARGS__)
#define CO_WARN(...)    SPDLOG_LOGGER_WARN(::czmosg::logger().get(), __VA_ARGS__)
#define CO_ERROR(...)   SPDLOG_LOGGER_ERROR(::czmosg::logger().get(), __VA_ARGS__)
#define CO_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(::czmosg::logger().get(), __VA_ARGS__)
