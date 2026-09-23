# MelonRind 🍉

> 在 Minecraft 基岩版上还原经典 Java 版 **AppleSkin（苹果皮）** 体验的 HUD 增强模组。

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Client](https://img.shields.io/badge/Platform-Client--Only-brightgreen.svg)](#特性)
[![Loader: LeviLamina](https://img.shields.io/badge/Loader-LeviLamina-blue.svg)](https://github.com/LiteLDev/LeviLamina)

---

## 📖 项目简介

**MelonRind（西瓜皮）** 是一个基于 [LeviLamina](https://github.com/LiteLDev/LeviLamina) 客户端的轻量 HUD 增强模组，致力于在基岩版客户端上还原 Java 版 [AppleSkin](https://github.com/squeek502/AppleSkin) 的核心功能。

它能在游戏原版 HUD 界面直观显示玩家隐藏的**饱和度**数值，并在手持食物时动态预览预计恢复的**饱食度、饱和度与生命值**。

> [!IMPORTANT]
> **仅客户端模组（Client-Only）**
> 
> MelonRind 完全运行在客户端，**无需**在服务端安装任何插件或模组。无论是单人世界还是加入多人服务器，均可在客户端直接获得流畅的 HUD 提示体验。

---

## ✨ 效果演示与特性

![MelonRind HUD 效果演示](./assets/preview.png)

## 支持显示
* 饱和度
* 预计恢复饱食度
* 预计恢复饱和度
* 预计恢复生命值

---

## 📦 安装与使用

### 方式 1：使用 lip 安装（推荐）

通过 LeviLamina 官方包管理器 `lip` 一键安装：

```bash
lip install github.com/LL-LSE-Lab/MelonRind
```

### 方式 2：手动下载发行版

1. 前往 [Releases](https://github.com/LL-LSE-Lab/MelonRind/releases) 页面下载预编译的发布包（ZIP）。
2. 解压并将 `MelonRind` 文件夹放入客户端的 `mods/` 目录下。
3. 启动游戏，模组及其自带纹理包将由 LeviLamina 自动加载。

---

## ⚙️ 配置说明

模组配置文件位于 `mods/MelonRind/config/config.json`，修改后重启游戏生效。

默认配置项如下：

```json
{
  "showSaturation": true,
  "showFoodPreview": true,
  "showRegeneration": true,
  "pulse": true,
  "opacity": 0.8
}
```

| 配置项 | 默认值 | 说明 |
| :--- | :---: | :--- |
| `showSaturation` | `true` | 是否在饱食度栏上显示金色饱和度轮廓 |
| `showFoodPreview` | `true` | 是否在手持食物时预览预计恢复的饱食度和饱和度 |
| `showRegeneration` | `true` | 是否在生命值栏预览预计自然恢复的血量 |
| `pulse` | `true` | 是否开启预览图标的呼吸脉冲闪烁动画 |
| `opacity` | `0.8` | 覆盖层图标的不透明度（0.0 ~ 1.0） |

---

## 🛠️ 编译构建

如果你希望自行从源代码构建 MelonRind：

### 环境要求
* Windows x64
* Visual Studio 2022 C++ 工具链（MSVC）
* [xmake](https://xmake.io/) 3.0 或更高版本

### 构建命令

```powershell
# 配置客户端构建目标
xmake f -p windows -a x64 -m release --target_type=client

# 编译模组本体
xmake build MelonRind

# 运行单元测试
xmake build model-tests
xmake run model-tests

# 打包为发行包（产物位于 dist/ 目录）
./tools/package.ps1
```

---

## 📄 许可证

本项目采用 [MIT](LICENSE) 许可证开源。

## 🙏 致谢

* 灵感来源：[squeek502/AppleSkin](https://github.com/squeek502/AppleSkin)
* 模组加载器：[LiteLDev/LeviLamina](https://github.com/LiteLDev/LeviLamina)
