# Launchpad PPA 发布配置

Fcitx5 English Hint 从 v0.9.0 开始支持在同一个 Git tag 发布流程中同时发布：

```text
Git tag vX.Y.Z
  ↓
GitHub Actions
  ├─ build / offline tests / source-package validation
  ├─ GitHub Release + Ubuntu 24.04 amd64 .deb
  └─ signed Debian source package
          ↓
       Launchpad PPA
          ↓
      apt update / apt upgrade
```

官方 PPA 只接收 Debian **source package**，不会接收 GitHub Release 中预构建的 `.deb`。Launchpad 收到签名 source package 后，在自己的构建环境里生成并发布 amd64 二进制包。

当前项目只发布到 Ubuntu 24.04 LTS（`noble`）。

## 1. 创建或准备 Launchpad 账号

打开 Launchpad 并登录你的账号。记下 Launchpad ID，例如：

```text
your-launchpad-id
```

后续 PPA 地址会使用这个 ID，而不是 GitHub 用户名（两者可以相同，也可以不同）。

## 2. 创建 PPA

推荐 PPA 名称：

```text
fcitx5-english-hint
```

最终 dput target 形如：

```text
ppa:your-launchpad-id/fcitx5-english-hint
```

PPA 只需要启用 amd64，因为本项目官方只支持 Ubuntu 24.04 amd64。

## 3. 准备 OpenPGP 签名 key

Launchpad 要求上传的 `.dsc` / `.changes` 使用已登记到 Launchpad 账号的 OpenPGP key 签名。

建议为自动发布准备一个专用签名 key，并设置密码。示例：

```bash
gpg --full-generate-key
```

查看 fingerprint：

```bash
gpg --list-secret-keys --keyid-format LONG
```

把该 key 的**公钥**添加到你的 Launchpad 账号，并完成 Launchpad 的验证流程。

导出私钥供 GitHub Actions 使用：

```bash
gpg --armor --export-secret-keys YOUR_KEY_FINGERPRINT
```

> 不要把私钥写入仓库、issue、README 或普通 GitHub Variable。它只能保存到 GitHub Actions Secret。

## 4. 配置 GitHub Repository Variable

在 GitHub 仓库：

`Settings → Secrets and variables → Actions → Variables`

新增：

### `LAUNCHPAD_PPA`

例如：

```text
ppa:your-launchpad-id/fcitx5-english-hint
```

### `LAUNCHPAD_PPA_REVISION`（可选）

默认：

```text
1
```

正常每个新的 Git tag 都保持 `1` 即可。

如果同一个 upstream 版本已经被 Launchpad 接收，但必须重新发布 packaging revision，可以把它改为 `2`、`3` 等。生成的 Debian 版本例如：

```text
0.9.0-1~ppa2~ubuntu24.04.1
```

## 5. 配置 GitHub Actions Secrets

在：

`Settings → Secrets and variables → Actions → Secrets`

新增：

### `LAUNCHPAD_GPG_PRIVATE_KEY`

粘贴完整 ASCII-armored private key，包括：

```text
-----BEGIN PGP PRIVATE KEY BLOCK-----
...
-----END PGP PRIVATE KEY BLOCK-----
```

### `LAUNCHPAD_GPG_PASSPHRASE`

填写该私钥的密码。

如果你的专用 CI key 没有密码，这个 Secret 可以为空，但更推荐使用带密码的专用签名 key。

## 6. 本地验证 source package

安装 packaging 工具：

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build extra-cmake-modules pkgconf \
  libfcitx5core-dev libfcitx5config-dev libcurl4-openssl-dev \
  devscripts debhelper lintian
```

构建未签名 source package：

```bash
bash scripts/build-source-package.sh --unsigned
```

输出位于：

```text
build-source/
```

v0.9.0 默认生成类似：

```text
fcitx5-english-hint_0.9.0.orig.tar.gz
fcitx5-english-hint_0.9.0-1~ppa1~ubuntu24.04.1.debian.tar.xz
fcitx5-english-hint_0.9.0-1~ppa1~ubuntu24.04.1.dsc
fcitx5-english-hint_0.9.0-1~ppa1~ubuntu24.04.1_source.changes
fcitx5-english-hint_0.9.0-1~ppa1~ubuntu24.04.1_source.buildinfo
```

检查：

```bash
lintian --fail-on error build-source/*_source.changes
```

## 7. 正式发布

只有当 `main` CI 成功、实际输入测试通过后再打 tag：

```bash
git tag -a v0.9.0 -m "Fcitx5 English Hint v0.9.0"
git push origin v0.9.0
```

Release workflow 会依次执行：

1. 验证 tag 与 `CMakeLists.txt` 版本完全一致；
2. 编译与离线测试；
3. 构建 unsigned source package 并运行 lintian；
4. 创建 GitHub Release 和 `.deb`；
5. 导入 GitHub Secret 中的 GPG 私钥；
6. 构建并验证 signed source package；
7. `dput` 到 `LAUNCHPAD_PPA`。

如果 PPA variable 或私钥 Secret 没配置，tag workflow 会明确失败，而不是静默跳过 PPA。这确保一次正式 tag 要么完整发布，要么明确显示未完成。

## 8. 用户通过 PPA 安装

PPA 第一次收到 package 后，Launchpad 需要完成构建和发布。完成后用户可以：

```bash
sudo add-apt-repository ppa:your-launchpad-id/fcitx5-english-hint
sudo apt update
sudo apt install fcitx5-english-hint
```

之后新版本发布到同一个 PPA，可以正常：

```bash
sudo apt update
sudo apt upgrade
```

已有插件配置和 `~/.cache/fcitx5-english-hint/cache.bin` 不会因为 apt 升级而被覆盖。

## 9. 关于重复上传

Launchpad 不允许在同一 PPA 中重复使用完全相同的 Debian package version。

如果某个版本已经被 Launchpad 接收，不能简单重新上传同一个版本。需要提高：

```text
LAUNCHPAD_PPA_REVISION
```

例如：

```text
1 → 2
```

再重新触发对应发布流程。
