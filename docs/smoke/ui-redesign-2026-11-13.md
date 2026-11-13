# UI 重设计 · 验收证据（feat/ui-redesign）

> 日期：2026-11-13
> 分支：`feat/ui-redesign`（基于 `6c7fc46`，含 PPM producer）
> 设计依据：`docs/UI_DESIGN.md`（496 行）
> 三个提交：`c2b8d95`（批1）/ `a091e5f`（批2）/ `357a49b`（批3）

## 决策前提

用户拍板 **UI 使用英文**。这条有一个连带项：屏幕字体是 `lv_font_montserrat_*`（纯拉丁字形），
中文会渲染成方框。界面固定文案本来就是英文，但**聊天正文来自 AI 回复** ——
所以必须在系统提示词里强制英文，否则屏幕上仍是满屏方框。

处置：`CONFIG_ROBOMIND_AI_SYSTEM_PROMPT` 追加
`Always respond in English only, regardless of the language the user writes in.`

---

## A · 构建与烧录

| 项 | 结果 |
|---|---|
| 批1 构建 | exit 0 / 0 error / 0 warning（336 s，含 LVGL 因新增字体重建） |
| 批2 构建 | exit 0 / 0 error / 0 warning（32 s 增量） |
| 批3 构建 | exit 0 / **0 error / 0 warning**（32 s 增量） |
| 烧录 | `Hash of data verified` × 4 段全过 |
| 烧的就是编的 | 烧录前 `robo_mind_s3.bin` 时间 12:41，与本轮最终构建一致 |

> 批3 首轮曾有 2 条 `-Wenum-compare` 警告（我的 `static_assert` 跨两个枚举类型比较），
> 已改为 `static_cast<int>` 后归零。

---

## B · 默认态零影响

producer 默认关闭（`CONFIG_ROBOMIND_PPM_PRODUCER_ENABLED=n`）：

| 现象 | 期望 | 实测 |
|---|---|---|
| `[producer]` 行 | 0 | **0** |
| `[wire]` 行 | 0 | **0** |
| `[ppm ]` 行 | 0 | **0** |
| `[mem]` 五点 | 齐全 | **5 点齐全** |

`[mem] after_chat`：`free=17696 largest=7680 min_free=9000 psram_free=8076628`

---

## C · 关键行为

| 项 | 结果 |
|---|---|
| 开机 splash → 主界面 | `ShowSplashScreen` 2 s 后自删；`display: LVGL loop start`，无复位/panic |
| 三档字号被实际使用 | 标题 16 / 正文 14 / 状态栏+角色标签+胶囊 12 —— 全部有引用点 |
| 三档字体进镜像 | `lv_font_montserrat_12` / `14` / `16` 均在 ELF |
| 布局常量与真实屏幕一致 | 未触发 `layout may be clipped` 告警（`kScreenH=240` 与实际相符） |
| 胶囊在 producer 关闭时 | 走 `ppm_producer_latest()==false` 分支 ⇒ 显示 **`N/A`** + 灰色圆点 |

> ⚠️ 最后一项是**代码路径 + 串口推断**，不是目视确认（见下方"未完成项"）。

---

## D · 静态资源

### UI 对象自身

`chat_ui.cpp.obj` 的 `.data = 0x0`；`.bss` 里**只有 `ChatUI` 单例本身**（本来就在），
没有新增任何静态缓冲：主题是 `const` 结构体（落 `.rodata`），wire 风格缓冲全在栈上。

### `.dram0.bss` 变化（**必须说明来源**）

| 阶段 | `.dram0.bss` | Δ | 来源（逐符号） |
|---|---|---:|---|
| 分支基线（UI 改动前） | `0x1cde8` | — | — |
| 批1 | `0x1cde8` | **0** | 主题结构体为 `const`，不进 .bss |
| 批2 | `0x1cdf8` | **+16** | `lv_font_montserrat_12/16.c.obj` 各 8 B **字形缓存** |
| 批3 | `0x1ce28` | **+48** | `g_latest` 24 B + `g_latest_ok` 1 B + `ChatUI::instance` 80→96 B (+16 B) + 对齐 |

归因手段：`git stash` 对照构建 + `nm` 符号差集，**不是估算**。

> 🔴 **要纠正判据 D 的字面表述**："UI 相关 `.o` 的 `.dram0.bss` = 0x0" 这条**从来就不成立** ——
> `ChatUI` 是函数内静态单例（`GetInstance` 里 `static ChatUI instance;`），
> 它本来就在 `.bss`（80 B，批3 增至 96 B）。
> 判据的真实意图"不许引入**静态缓冲**"是满足的：
> 新增的 16 B 是胶囊必需的 3 个 `lv_obj_t*` + 1 个状态码，不是缓冲。

### 关于 +16 B 字体的一个发现

启用一个新字体不只是 Flash 成本 —— **每个字体还带 8 B 的 `.bss` 字形缓存**。
设计文档 §3.1 的 Flash 估算（~8 KB / ~13 KB）没有提这 8 B/字体。
两档字体合计 16 B，可忽略，但这条要写进设计文档，否则下次有人以为"启字体零 RAM"。

---

## E · 栈与内存

| 判据 | 要求 | 实测 | 结论 |
|---|---|---|---|
| `[mem] after_chat` 的 `largest` | ≥ 5,632 | **7,680** | ✅ |
| LV_MEM 池 | `free_biggest` 不低于 8 KB | **57,680**（used 12% / frag 0%） | ✅ 余量极大 |
| LV_MEM 碎片率 | < 30% | **0%** | ✅ |

`LV_MEM used=12%` 是三点新增控件的实测代价（胶囊 3 个 obj + 定时器），
远低于设计文档 §10 预估的 ≈1.8 KB。

---

## 对比度复算（§12 判据 3）

全部用 WCAG 相对亮度公式重算，**不采信文档数字**（结果与文档一致）。

| 前景 on 背景 | 比值 | 判定 |
|---|---:|---|
| `text_primary` on `bg` | 16.69 | ✅ |
| `text_primary` on `surface2`（气泡正文） | 13.32 | ✅ |
| `text_secondary` on `surface2`（角色标签） | **6.71** | ✅ 修掉旧的 4.13 |
| `bg` on `primary`（**发送键**） | **5.74** | ✅ 修掉旧的 3.30 |
| `text_primary` on `primary` | 2.91 | ❌ **禁止使用**（白字蓝底） |
| `success/accent/danger` on `surface2`（胶囊） | 6.46 / 8.02 / 4.84 | ✅ |
| `text_disabled` on `surface2` | **3.11** | ❌ 文字不合格 ⇒ **只用于圆点**（图形门槛 3.0） |

> `text_disabled on surface2 = 3.11` 是**设计文档色板表没核算的组合**，实现时发现。
> 处置：胶囊"N/A/Invalid"的**文字**用 `text_secondary`（6.71），**圆点**用 `text_disabled`（3.11 ≥ 3.0）。
> 既守 AA，又保住"灰 = 不知道"的语义。

---

## 构建期闸门：AI 提示词必须含英文约束（`chat_engine.cpp`）

### 为什么需要它

`sdkconfig` 被 gitignore，而 IDF 的 `sdkconfig.defaults` **只补尚未存在的键**。
别人 clone 后只拿到 defaults 的改动，本地 `sdkconfig` 里的旧值（中文提示词、字体未启用）继续生效
⇒ **"UI 英文"决策静默失效**，编译、烧录、串口全都看不出来，只有盯着屏幕才发现是方框。

所以把它变成**编译错误**，而不是靠人记得同步配置。

### 实现

`chat_engine.cpp` 里用 `constexpr` 字符串查找 + `static_assert`：

```cpp
static_assert(StrContains(CONFIG_ROBOMIND_AI_SYSTEM_PROMPT, "English"),
              "[构建中止] … 不含 'English' 约束 …处置二选一 …");
```

（C 预处理器的 `#if` 无法判断字符串内容，所以用 C++ `constexpr` 在编译期做。）

### 双向验证（不是"能编译就算数"）

| 对照 | 输入 | 期望 | 实测 |
|---|---|---|---|
| **负向** | 中文提示词 `"你是一个车载机器人助手…"` | 编译失败 | ✅ `error: static assertion failed: [构建中止] 不含 English 约束` |
| **正向** | `"You are RoboMind. Always respond in English only."` | 编译通过 | ✅ exit 0 |

真实固件构建：exit 0 / 0 error / 0 warning，`.dram0.bss` 不变（`0x1ce28`）。

> 这条闸门的意义：它是本项目第一个**拦在编译期**的静默失效防护。
> 之前的防护（门禁检查、冒烟测试）都依赖"有人记得跑"，而这条不依赖。

---

## 🔴 未完成项：`docs/screenshots/` 三张图

**我没有能力对屏幕拍照或截屏**，这是硬限制，不是遗漏。

原因：
- 板子有摄像头（OV5640），但拍屏幕需要物理对准 + 光照，且我无法观察取景；
- LVGL 8.4 的 `lv_snapshot` 未启用（`LV_USE_SNAPSHOT`），且显示用的是**分块缓冲**
  （两块 38,400 B PSRAM），`buf_act` 里没有完整一帧 ⇒ 无法从内存还原整屏。

**可行的补齐方案**（需批准，属于新增调试代码）：
1. 开 `LV_USE_SNAPSHOT` + 加一个 `lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR)`，
   把整屏渲染到一块 PSRAM 缓冲，再按 RGB565 逐行 dump 到串口，PC 侧重组成 PNG；
   代价 ≈ 153,600 B 临时 PSRAM（可用 PSRAM 充足）+ 一段约 60 行的调试代码。
2. 或由用户用手机对屏幕拍照，放进 `docs/screenshots/`。

**在补齐之前，UI 的视觉呈现（间距/圆角/对齐是否美观）实际上**没有**被任何人验证过** ——
上表证明的是"能跑、不越界、对比度达标、字号到位"，**不证明"好看"**。
这条请在设计验收时显式区分。

---

## 结论

| 项 | 状态 |
|---|---|
| A 构建 + 烧录 | ✅ |
| B 默认态零影响 | ✅ |
| C 关键行为（除目视项） | ✅ 代码路径与串口证据齐全 |
| D 静态资源 | ✅ 意图满足（+64 B 已逐符号归因）；判据字面表述需修正 |
| E 栈/内存 | ✅ `largest=7680`、`LV_MEM frag=0%` |
| 截图基线 | ❌ **未完成**（无拍照能力），方案与代价已给出 |
