#include "local_dictionary.h"

#include <array>
#include <string_view>
#include <utility>

namespace fcitx::english_hint {
namespace {

using Entry = std::pair<std::string_view, std::string_view>;

// Intentionally small and conservative. Exact-match only: ambiguous phrases
// are left to the LLM instead of guessing from a dictionary entry.
constexpr std::array<Entry, 78> kEntries = {{
    {"效率", "efficiency"},
    {"提高效率", "improve efficiency"},
    {"性能", "performance"},
    {"高性能", "high-performance"},
    {"速度", "speed"},
    {"稳定性", "stability"},
    {"稳定", "stable"},
    {"轻量", "lightweight"},
    {"默认", "default"},
    {"当前", "current"},
    {"最新", "latest"},
    {"版本", "version"},
    {"项目", "project"},
    {"代码", "code"},
    {"功能", "feature"},
    {"插件", "plugin"},
    {"系统", "system"},
    {"用户", "user"},
    {"配置", "configuration"},
    {"设置", "settings"},
    {"参数", "parameter"},
    {"接口", "API"},
    {"模型", "model"},
    {"数据", "data"},
    {"服务器", "server"},
    {"网络", "network"},
    {"文件", "file"},
    {"目录", "directory"},
    {"缓存", "cache"},
    {"请求", "request"},
    {"响应", "response"},
    {"输入", "input"},
    {"输出", "output"},
    {"输入法", "input method"},
    {"候选", "candidate"},
    {"候选词", "candidate"},
    {"翻译", "translation"},
    {"中文", "Chinese"},
    {"英文", "English"},
    {"语言", "language"},
    {"学习", "learning"},
    {"短句", "short phrase"},
    {"图片", "image"},
    {"视频", "video"},
    {"工作流", "workflow"},
    {"提示词", "prompt"},
    {"节点", "node"},
    {"开发", "development"},
    {"测试", "testing"},
    {"计划", "plan"},
    {"结果", "result"},
    {"方法", "method"},
    {"原因", "reason"},
    {"问题", "problem"},
    {"错误", "error"},
    {"失败", "failure"},
    {"成功", "success"},
    {"支持", "support"},
    {"解决", "solve"},
    {"更新", "update"},
    {"安装", "install"},
    {"卸载", "uninstall"},
    {"保存", "save"},
    {"删除", "delete"},
    {"复制", "copy"},
    {"粘贴", "paste"},
    {"打开", "open"},
    {"关闭", "close"},
    {"启用", "enable"},
    {"禁用", "disable"},
    {"开始", "start"},
    {"停止", "stop"},
    {"调试", "debug"},
    {"日志", "log"},
    {"内存", "memory"},
    {"线程", "thread"},
    {"延迟", "latency"},
    {"超时", "timeout"},
}};

} // namespace

bool lookupLocalDictionary(const std::string &text, std::string &translation) {
    for (const auto &[source, target] : kEntries) {
        if (source == text) {
            translation.assign(target);
            return true;
        }
    }
    return false;
}

} // namespace fcitx::english_hint
