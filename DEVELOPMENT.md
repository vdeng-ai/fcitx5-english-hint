# 开发说明

## 目标

Fcitx5 English Hint 是一个**显示层英语辅助插件**。核心要求：

- 不修改 Rime / 雾凇拼音主体；
- 不改变候选排序和最终提交文本；
- Fcitx5 主线程不做网络 I/O；
- 热路径只做候选识别、内存查找和字符串装饰；
- 网络或模型异常时，原输入法必须继续正常工作。

## 架构

```text
fcitx5-rime
    │
    ▼
CandidateWord / InputPanel
    │
    ▼
Instance::OutputFilter
    │
    ├── LRU / persistent cache hit
    │       └── append " [English]" to display copy
    │
    ├── local dictionary hit
    │       └── put LRU + append display copy
    │
    └── miss
          │
          ▼
     TranslationWorker
          │
     debounce / latest-wins
          │
          ▼
       LlmClient
          │
     OpenAI-compatible API
          │
          ▼
   persistent cache + LRU
          │
          ▼
 EventDispatcher → UI refresh
```

`OutputFilter` 操作的是 UI 显示副本，不会修改 Rime 原始 CandidateWord 的 `select()` 行为，因此上屏内容保持原中文。

## 线程模型

### Fcitx5 主线程

只允许：

- 判断是否为 Rime candidate；
- 判断密码/敏感 CapabilityFlag；
- UTF-8 / 汉字快速过滤；
- 内存 LRU 查询；
- 本地 exact-match 词典查询；
- 提交最新候选 snapshot；
- UI display string 装饰。

禁止：

- HTTP 请求；
- 文件读写；
- 等待 worker；
- 大规模解析或分词。

### TranslationWorker

单 worker 线程负责：

- 200 ms debounce；
- latest snapshot；
- 批量 LLM 请求；
- 延迟/失败统计；
- 持久缓存追加与压缩。

worker 完成后通过 `EventDispatcher` 安全切回 Fcitx event loop 刷新候选 UI。

## 缓存

### 内存 LRU

默认 4096 项。候选显示热路径只查询内存。

### 持久缓存

路径：

```text
~/.cache/fcitx5-english-hint/cache.bin
```

采用轻量追加式二进制记录。启动时加载；LLM 成功后由 worker 追加；达到大小阈值后在 worker 线程压缩重写。

### 本地词典

只做保守 exact-match。它不是翻译引擎，也不进行分词、模糊匹配或上下文推理。目标只是让最常见短词跳过 LLM。

## LLM 请求

当前协议：OpenAI-compatible `POST /v1/chat/completions`。

默认：

- batch <= 5；
- `temperature=0`；
- `max_tokens=32`；
- `reasoning_effort=none`；
- 连接复用；
- 失败静默。

输出格式保持紧凑：

```text
0<TAB>Improve efficiency
1<TAB>The weather is nice today
```

这样避免复杂 JSON schema 依赖，同时容易用小型专用 parser 处理。

## 配置

使用 Fcitx5 原生 `Configuration`：

```text
~/.config/fcitx5/conf/english-hint.conf
```

Addon 实现：

- `getConfig()`；
- `setConfig()`；
- `reloadConfig()`。

因此可以通过 `fcitx5-configtool` 修改，并用 `fcitx5-remote -r` 重载。

## 构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 测试

```bash
./build/tests/english-hint-cache-smoke
./build/tests/english-hint-llm-smoke
```

第一项完全离线；第二项调用用户当前配置的 LLM。

## 发布

官方二进制只面向 Ubuntu 24.04 amd64：

```bash
./scripts/build-deb.sh
```

GitHub `v*` tag 由 `ubuntu-24.04` Actions runner 构建 `.deb` 并发布。

## 项目边界

0.7.0 之后优先做 bugfix 和性能修复。除非有明确理由，否则不加入：音标、生词本、长解释、上下文抓取、独立 GUI、Python/Node sidecar 等功能。
