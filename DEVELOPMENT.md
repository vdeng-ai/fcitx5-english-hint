# 开发说明

## 目标

Fcitx5 English Hint 是一个**轻量英语辅助插件**。核心要求：

- 不修改 Rime / 雾凇拼音主体；
- 不改变候选排序和普通中文选词行为；仅专用快捷键可以提交已显示的英文；
- 默认中文候选行保持 Fcitx5 / Rime 原样，英文提示写入 `InputPanel::auxDown`；
- 可选竖排中英同行仅装饰显示，保留原候选对象的选词和翻页行为；
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

## 可选竖排中英同行

`General/VerticalBilingual` 默认 `False`。开启后使用 `InlineCandidateList` 包装原列表的 `shared_ptr`，给当前页已有翻译的候选显示文本附加英文并标记 `DontCommit`，同时返回 `CandidateLayoutHint::Vertical`。不修改原词条、标签或排序，候选的 `select()` 继续调用原候选；调用期间持有原列表，允许 Rime 同步替换 InputPanel。没有翻译的候选直接返回原对象。

包装仅暴露原列表支持的 pageable、bulk、modifiable、cursor-movable 接口，并委托原接口执行。全局 bulk 候选保持原显示，避免把页内翻译错配到全局索引。翻页和候选修改先使翻译失效；包装也会核对原页码及中文原文。异步刷新复用同一层包装，读取原列表，不重复拼接英文或将英文发给 LLM。

竖排英文快捷键用当前候选列表的弱引用验证所有权，并核对包装仍显示对应英文。默认模式继续核对 `auxDown`。重新配置时清理所有记录过的输入上下文。关闭竖排后适配器透明委托原列表直到 Rime 重建，因为 InputPanel 的 setter 只接受 `unique_ptr`，不能直接放回已有的 `shared_ptr`；此时返回原布局和原候选对象。

## 英文提交快捷键

使用 Fcitx5 原生布尔配置 `General/CommitEnglishWithAlt`，默认启用，设为 `False` 禁用。在 `PreInputMethod` 阶段监听按键，通过 `rawKey().digitSelection(KeyState::Alt)` 将 `Alt+数字` 映射为当前页的候选索引，支持数字小键盘。读取 raw key 保留额外的 Shift 修饰键，避免归一化后把 `Alt+Shift+小键盘数字` 误认为 `Alt+数字`。普通数字键及带 Ctrl、Shift、Super 的组合不触发。只在启用、Rime、非敏感且有焦点的输入框中处理按下事件。

显示的 ownership 记录同时保存当前页最多 5 个候选的中文原文和已显示的英文。提交时核对辅助行或竖排候选显示仍属于插件、数字指定的位置仍是同一候选，且该位置已有英文。选择完全由数字决定，与当前高亮位置无关；部分翻译缺失时保留原候选位置，不能将翻译列表压缩后重排。未满足条件时原样放行按键，不查缓存、不请求翻译、不等待 worker。

成功时拦截按键，清除插件提示，通过 `InputContext::reset()` 清空 Rime 整段 composition，再 `commitString()` 提交英文。不能调用中文候选的 `select()`，否则可能先上屏中文。此简化方案不保留分段输入中已选择的中文和剩余拼音。

## 线程模型

### Fcitx5 主线程

只允许：

- 监听 `InputContextUpdateUI`；
- 在 `PreInputMethod` 匹配英文提交快捷键，核对显示快照并提交现成英文；
- 判断 Rime / PasswordOrSensitive；
- 读取当前页最多 5 个候选；
- UTF-8 / 汉字快速过滤；
- 内存 LRU 查询；
- 本地 exact-match 词典查询；
- 构造一条 `auxDown` 字符串或最多 5 项的候选显示包装；
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
- 英文选词：Alt+数字与小键盘的索引映射、高亮位置无关、缺失翻译保留序号、占位候选、无效位置、辅助行让出和换页后的旧快照；普通数字与额外修饰键不匹配，禁用配置可保存和读取。
- 竖排显示：标签、原文、不可提交英文、可选候选接口、翻页/修改后的失效、模式关闭与原候选选词委托。

安装 `fcitx5-rime` 后可启用真实输入法集成测试（无需 librime 开发包）：

```bash
cmake -S . -B build -DENGLISH_HINT_RIME_INTEGRATION=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

该测试通过真实 Fcitx 事件管线与 Rime 引擎验证默认/竖排显示、数字选中文、鼠标使用的候选 `select()`、Alt+数字选英文、键盘/UI 翻页和配置重载；所有词库、配置和翻译缓存写入临时目录，测试后清理，不读取私人模型地址。它不操作桌面鼠标，不覆盖主题的渲染与点击区域。

实际输入框手工验证见 [英文提交检查清单](tests/ENGLISH_COMMIT_CHECKLIST.md)。基础离线测试不加载 Rime；可选集成测试覆盖真实 Rime composition 与测试客户端的提交联动，桌面应用和主题仍需要手工验证。

LLM smoke test 调用用户当前配置的模型，并覆盖两种回归场景：48-token 截断时最后一条必须被丢弃；96-token 正常预算下 5 条中短句必须全部返回。

## 发布

官方只支持 Ubuntu 24.04（noble）amd64，并同时维护两条发布产物：

```text
Git tag vX.Y.Z
  ↓
validate: build + offline tests + unsigned source package + lintian
  ├─ GitHub Release: CPack .deb
  └─ Launchpad PPA: signed Debian source package → Launchpad amd64 build
```

GitHub Release 二进制包：

```bash
./scripts/build-deb.sh
```

Launchpad source package 本地验证：

```bash
bash scripts/build-source-package.sh --unsigned
lintian --fail-on error build-source/*_source.changes
```

PPA source version 使用：

```text
X.Y.Z-1~ppaN~ubuntu24.04.1
```

Release workflow 使用 Repository Variable `LAUNCHPAD_PPA` / `LAUNCHPAD_PPA_REVISION` 和 Secrets `LAUNCHPAD_GPG_PRIVATE_KEY` / `LAUNCHPAD_GPG_PASSPHRASE` 完成非交互 OpenPGP 签名及 `dput` 上传。具体一次性配置见 `docs/launchpad-ppa.md`。

## 项目边界

继续保持轻量：不加入音标、生词本、长解释、上下文抓取、独立 GUI、Python/Node sidecar，也不修改 Rime 词库或输入逻辑。
