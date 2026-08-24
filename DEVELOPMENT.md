# 开发说明

## 目标

Fcitx5 English Hint 是一个**显示层英语辅助插件**。核心要求：

- 不修改 Rime / 雾凇拼音主体；
- 不改变候选排序、选词行为和最终提交文本；
- 中文候选行保持 Fcitx5 / Rime 原样；
- 英文只写入 `InputPanel::auxDown`；
- Fcitx5 主线程不做网络 I/O；
- 网络或模型异常时，原输入法必须继续正常工作。

## 0.8.0 架构

```text
fcitx5-rime
    │
    ▼
CandidateList / InputPanel
    │
    ├──────────────→ Fcitx5 UI 原样渲染中文候选
    │
    ▼
InputContextUpdateUI event
    │
    ├── LRU / persistent cache hit
    │       └── build auxDown
    │
    ├── local dictionary hit
    │       └── put LRU + build auxDown
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
 EventDispatcher → InputPanel UI refresh
```

英文行示例：

```text
1. Improve efficiency 2. Improve benefits 3. Boost efficiency 4. Speed up 5. Improve performance
```

项目之间只使用 1 个空格。

## auxDown 所有权保护

插件不会无条件覆盖 `auxDown`。

- `auxDown` 为空：插件可以写入英文行；
- `auxDown` 等于插件上一次写入内容：插件可以更新；
- `auxDown` 出现其他内容：视为 Rime 或其他 addon 所有，English Hint 立即让出，不覆盖；
- InputContext 销毁时清理插件自己的 ownership 记录。

这样可以保证插件只做附加显示，不抢占输入法主体已有 UI 信息。

## 线程模型

### Fcitx5 主线程

只允许：

- 监听 `InputContextUpdateUI`；
- 判断 Rime / PasswordOrSensitive；
- 读取当前页最多 5 个候选；
- UTF-8 / 汉字快速过滤；
- 内存 LRU 查询；
- 本地 exact-match 词典查询；
- 构造一条 `auxDown` 字符串；
- 已有翻译立即显示，缺失候选继续异步请求；
- 提交最新缺失候选 snapshot。

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
- stale result 处理；
- 延迟/失败统计；
- 持久缓存追加与压缩。

worker 完成后通过 `EventDispatcher` 安全切回 Fcitx event loop，先直接重建当前 InputPanel 的 `auxDown`，再请求 UI repaint。不能只依赖 `InputContextUpdateUI` watcher 再次执行，否则部分 UI 路径会出现“LLM 已完成但要等下一次按键才显示”的现象。

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

只做保守 exact-match，不进行分词、模糊匹配或上下文推理。目标只是让最常见短词跳过 LLM。

## LLM 请求

当前协议：OpenAI-compatible `POST /v1/chat/completions`。

默认：

- batch <= 5；
- `temperature=0`；
- `max_tokens=96`；
- `reasoning_effort=none`；
- 连接复用；
- 本地/局域网 endpoint 绕过系统代理；
- 失败静默。

输出格式：

```text
0<TAB>Improve efficiency
1<TAB>The weather is nice today
```

## 配置

使用 Fcitx5 原生 `Configuration`：

```text
~/.config/fcitx5/conf/english-hint.conf
```

Addon 实现 `getConfig()`、`setConfig()`、`reloadConfig()`，可通过 `fcitx5-configtool` 修改，并使用 `fcitx5-remote -r` 重载。

## 构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 测试

```bash
ctest --test-dir build --output-on-failure
./build/tests/english-hint-llm-smoke
```

离线测试包含：

- cache / dictionary；
- `auxDown` 格式：序号后不留空格，英文项目之间 2 个空格。

LLM smoke test 调用用户当前配置的模型，并覆盖两种回归场景：48-token 截断时最后一条必须被丢弃；96-token 正常预算下 5 条中短句必须全部返回。

## 发布

官方二进制只面向 Ubuntu 24.04 amd64：

```bash
./scripts/build-deb.sh
```

GitHub `v*` tag 由 `ubuntu-24.04` Actions runner 构建 `.deb` 并发布。

## 项目边界

继续保持轻量：不加入音标、生词本、长解释、上下文抓取、独立 GUI、Python/Node sidecar，也不修改 Rime 词库或输入逻辑。
