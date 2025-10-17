#include "Log.h"

namespace czmosg
{
    // 定义全局logger实例
    // 需要显式创建并持有，以控制生命周期
    std::shared_ptr<spdlog::logger> g_loggerInstance;

    void initializeLogger() {
        // 如果已经初始化过，直接返回
        if (g_loggerInstance) {
            return;
        }

        // 创建logger实例
        g_loggerInstance = spdlog::get(coLoggerName);
        if (!g_loggerInstance) {
            // 如果不存在，创建一个新的
            g_loggerInstance = spdlog::stdout_color_mt(coLoggerName);
        }

        // 设置日志级别
        g_loggerInstance->set_level(spdlog::level::trace);
        
        // 设置全局日志级别
        spdlog::set_level(spdlog::level::trace);
        
        // 设置默认logger
        spdlog::set_default_logger(g_loggerInstance);
        
        // 设置日志格式
        g_loggerInstance->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
        
        // 设置所有级别都立即刷新
        g_loggerInstance->flush_on(spdlog::level::trace);
        
        // 记录初始化日志
        CO_INFO("Logger system initialized");
    }
}
