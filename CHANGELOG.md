# 更新日志

## 0.8.0

- 中文候选恢复为 Rime / Fcitx5 原样显示，不再在候选文本后拼接英文。
- 英文翻译改为统一写入 `InputPanel::auxDown`，当前页最多 5 条，格式为 `1.English  2.English ...`。
- 序号后不留空格，英文项目之间使用 2 个空格，兼顾紧凑与可辨识度。
- 若 Rime 或其他插件已使用 `auxDown`，English Hint 不覆盖原内容。
- 默认 `max_tokens` 提升到 96，适配 5 条中短句批量翻译；模型仍按实际长度提前停止。
- worker 完成后在 Fcitx 主线程直接重建 `auxDown`，修复新翻译要等下一次按键才出现的问题。
- 允许缓存/词典/已完成的 LLM 结果先部分显示，不再等待 5 条全部就绪。
- `finish_reason=length` 时丢弃最后一条可能截断的翻译并只重试缺失项，避免半句进入缓存。
- 新增 `auxDown` 格式 smoke test 和长短句 LLM 回归测试。

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
