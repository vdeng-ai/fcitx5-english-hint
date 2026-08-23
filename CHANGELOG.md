# 更新日志

## 0.7.0

- 针对本地 OpenAI-compatible 模型优化请求路径。
- 默认 `max_tokens=32`，适配最多 5 条输入法短候选。
- 使用更短、固定的翻译提示词，减少 prompt token 和首 token 延迟。
- 默认请求 `reasoning_effort=none`；服务端不支持时自动兼容回退。
- 增强 libcurl 连接复用、TCP keepalive 与 TCP_NODELAY。
- 本地/局域网 Endpoint 自动绕过系统 HTTP 代理。
- README 改为中文并补齐开源、隐私、架构与发布说明。

## 0.6.0

- 增加 Ubuntu 24.04 amd64 `.deb` 构建与发布流程。
- 增加 GitHub Actions CI 与 tag Release 工作流。
- 正式支持通过 Fcitx5 配置工具管理插件配置。
- 官方预编译包只面向 Ubuntu 24.04；其他系统仅提供源码。

## 0.4.1

- 增加轻量本地中英 exact-match 词典。
- 查询顺序优化为：内存缓存 → 持久缓存 → 本地词典 → LLM。

## 0.4.0

- 增加 `~/.cache/fcitx5-english-hint/cache.bin` 持久缓存。
- 打字热路径不访问磁盘。

## 0.3.0

- 接入 Fcitx5 原生 Configuration 系统。
- 支持 `fcitx5-configtool` 配置与 `fcitx5-remote -r` 重载。

## 0.2.1

- 增加 latest-wins、stale response 丢弃、批量翻译和 debug 指标。
- 默认最多 5 个候选一次请求。

## 0.2.0

- 完成异步 LLM worker、debounce、LRU cache 和 OpenAI-compatible 客户端。

## 0.1.0

- 验证 Fcitx5 display-only 候选装饰，不修改 Rime 上屏文本。
