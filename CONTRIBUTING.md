# 贡献指南

感谢关注 Fcitx5 English Hint。

本项目优先级很明确：**稳定、轻量、高性能，且只作为 Fcitx5 插件工作，不修改输入法主体。**

## 开发环境

官方开发/测试环境：

- Ubuntu 24.04 amd64
- Fcitx5 5.1.7
- fcitx5-rime 5.1.4
- CMake + Ninja + GCC 13

安装依赖：

```bash
sudo apt install \
  build-essential cmake ninja-build extra-cmake-modules \
  libfcitx5core-dev libfcitx5config-dev libcurl4-openssl-dev
```

构建：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

测试：

```bash
./build/tests/english-hint-cache-smoke
```

`english-hint-llm-smoke` 会访问用户配置的 LLM Endpoint，只适合本地手工测试。

## 设计约束

提交新功能前请确认：

1. 不修改 Rime / 雾凇拼音源码或词库。
2. 不在 Fcitx5 主线程执行网络 I/O 或磁盘 I/O。
3. 不引入 Python、Node、独立 daemon 等额外常驻组件。
4. 不默认发送周边文本、剪贴板、历史输入或窗口内容。
5. 网络失败不能影响正常中文输入。
6. 优先使用标准库、Fcitx5 API 和已有 libcurl 依赖，避免为小功能增加重量级依赖。

## Pull Request

PR 请尽量保持单一目的，并说明：

- 修改动机；
- 对输入延迟/线程/缓存的影响；
- 是否改变发送给 LLM 的数据范围；
- 在 Ubuntu 24.04 + Fcitx5 5.1.7 上的测试结果。

其他系统的兼容补丁欢迎提交，但当前项目只承诺 Ubuntu 24.04 官方支持。
