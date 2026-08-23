# Fcitx5 English Hint

[![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu-24.04-E95420?logo=ubuntu&logoColor=white)](https://ubuntu.com/)
[![Fcitx5](https://img.shields.io/badge/Fcitx5-5.1.7-blue)](https://github.com/fcitx/fcitx5)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

一个轻量、高性能的 **Fcitx5 插件**：在 Rime / 雾凇拼音候选词旁显示简短自然的英文提示，用日常中文输入顺带学习英语。

> 项目原则：**只做英语辅助，不修改输入法主体，不让输入法变臃肿。**

```text
1. 提高效率 [Improve efficiency]
2. 今天天气不错 [The weather is nice today]
3. 这个方案可以继续推进 [This plan can proceed]
```

选中候选后，真正上屏的仍然只有原始中文。插件不会修改 Rime 词库、候选排序、选词行为或提交文本。

## 特性

- **纯 Fcitx5 Addon**：不修改 Rime / 雾凇拼音，不 fork fcitx5-rime。
- **主线程零网络 I/O**：HTTP 请求只发生在单独 worker 线程，不阻塞键盘输入。
- **批量翻译**：默认一次处理当前页最多 5 个候选，而不是每个候选单独请求。
- **200 ms debounce + latest-wins**：快速连续输入时只保留最新候选快照。
- **内存 LRU + 持久缓存**：常见翻译后续几乎即时显示。
- **内置轻量词典**：常见短词优先本地 exact-match，不调用 LLM。
- **本地模型优化**：短 prompt、`max_tokens=32`、关闭 reasoning、HTTP 连接复用、局域网地址绕过代理。
- **隐私保护**：密码/敏感输入框完全跳过；默认只发送当前候选短句，不发送历史、剪贴板、窗口内容或周边文本。
- **标准 Fcitx 配置**：支持在 Fcitx5 配置工具中修改 Endpoint、Model、API Key 等参数。

## 官方支持范围

当前发布包**只支持并测试以下环境**：

- Ubuntu 24.04 LTS amd64
- Fcitx5 5.1.7
- fcitx5-rime 5.1.4
- Rime / rime-ice（雾凇拼音）

其他 Linux 发行版、其他 Fcitx5 版本可以自行源码编译尝试，但暂不提供兼容性承诺和预编译包。

## 安装

### 推荐：安装 `.deb`

从 GitHub Releases 下载当前 Ubuntu 24.04 amd64 包：

```bash
sudo apt install ./fcitx5-english-hint_0.7.0_amd64.deb
fcitx5 -r -d
```

升级同样直接执行 `apt install` 即可，已有用户配置和持久缓存不会被覆盖。

### 源码编译

安装依赖：

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build extra-cmake-modules \
  libfcitx5core-dev libfcitx5config-dev \
  libcurl4-openssl-dev
```

编译并安装开发版本：

```bash
git clone https://github.com/vdeng-ai/fcitx5-english-hint.git
cd fcitx5-english-hint
./scripts/install-dev.sh
```

开发安装脚本会编译、安装插件、保留已有配置，并真正重启 Fcitx5 以加载新的 `.so`。

## 配置 LLM

插件调用 **OpenAI-compatible `/v1/chat/completions`** 接口。

推荐通过 `fcitx5-configtool` 的 Addons / 插件配置界面修改 **English Hint**。

也可以直接编辑：

```text
~/.config/fcitx5/conf/english-hint.conf
```

示例：

```ini
[General]
Enabled=True
DebounceMs=200
TimeoutMs=2500
MaxTokens=32
CacheSize=4096
MaxBatch=5
Debug=False

[LLM]
Endpoint=http://127.0.0.1:8080/v1/chat/completions
Model=qwen3.8-27b
ApiKey=
```

修改配置后执行：

```bash
fcitx5-remote -r
```

`ApiKey` 可以为空，适合 llama.cpp、vLLM、LiteLLM 等本地 OpenAI-compatible 服务。

## 本地模型优化

0.7.0 针对本地模型做了几项固定优化，不增加额外常驻服务：

- `max_tokens` 默认 **32**：5 个短候选实测足够，避免生成冗余内容。
- 默认发送 `reasoning_effort=none`，避免 Qwen 等模型把延迟浪费在 thinking 上。
- 如果服务端不支持 `reasoning_effort` 并返回 400，会自动回退一次兼容请求。
- 使用同一个 libcurl easy handle，复用 HTTP 连接。
- 开启 TCP keepalive / TCP_NODELAY。
- `127.0.0.1`、`10.x`、`172.16-31.x`、`192.168.x` 等本地/局域网 Endpoint 自动绕过系统 HTTP 代理。
- System prompt 保持固定且尽量短，利于本地推理服务进行 prompt/KV cache 复用。

当前主要验证模型：`qwen3.8-27b`。

## 缓存策略

查询顺序：

```text
内存 LRU
   ↓ miss
持久缓存
   ↓ miss
本地 exact-match 词典
   ↓ miss
异步 LLM
```

持久缓存路径：

```text
~/.cache/fcitx5-english-hint/cache.bin
```

缓存文件权限为 `0600`。打字热路径不访问磁盘；缓存加载发生在启动阶段，新 LLM 结果由 worker 线程异步追加。

## 性能设计

```text
Rime Candidate
      │
      ▼
Fcitx5 OutputFilter
      │
      ├── Cache hit ────────────────→ 中文 [English]
      │
      ├── Local dictionary hit ────→ 中文 [English]
      │
      └── miss
            │
            ▼
       async worker
            │
       200 ms debounce
            │
       latest snapshot
            │
       batch <= 5
            │
            ▼
     local LLM endpoint
            │
            ▼
       cache + UI refresh
```

网络不可达、超时、LLM 返回异常时，插件静默失败，原输入法继续正常工作。

## 构建 `.deb`

仅面向 Ubuntu 24.04 amd64：

```bash
./scripts/build-deb.sh
```

生成文件位于：

```text
build-package/fcitx5-english-hint_0.7.0_amd64.deb
```

GitHub tag `v0.7.0` 会通过 GitHub Actions 在 `ubuntu-24.04` runner 上构建并创建 Release。

## 测试

离线缓存/词典测试：

```bash
./build/tests/english-hint-cache-smoke
```

LLM 实机测试：

```bash
./build/tests/english-hint-llm-smoke
```

LLM smoke test 会读取你的 `~/.config/fcitx5/conf/english-hint.conf`，因此不会在 CI 中自动调用私人模型地址。

## 项目边界

当前版本刻意**不做**：

- 音标、语法解释、长释义
- 生词本和学习历史
- 读取周边文本做上下文翻译
- 独立 GUI / Web 服务
- Python / Node sidecar
- 修改 Rime 词库或输入逻辑

这些功能会增加复杂度、隐私面和输入法运行风险，不符合本项目定位。

## 开源贡献

开发说明见 [DEVELOPMENT.md](DEVELOPMENT.md)，贡献说明见 [CONTRIBUTING.md](CONTRIBUTING.md)。

安全与隐私问题请阅读 [SECURITY.md](SECURITY.md)。

## License

[MIT License](LICENSE)
