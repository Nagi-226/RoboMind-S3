# Rust 渐进式引入计划

> 状态: 讨论通过，待 Phase 3 启动 | 创建: 2026-06-08 | 关联: [`version_roadmap.md`](version_roadmap.md)

---

## 一、策略概述

**核心理念**: Rust 进入系统的地方，是 C++ "最难信任、最难改动、运维成本最高"的地方。不是替代 C++，而是在安全关键路径上提供编译期保证。

**引入模型**: "Rust 安全岛" — C++ 主框架保持不变，Rust 实现独立的安全关键模块，通过 `extern "C"` FFI 边界通信。

```
         C++ 主框架 (ESP-IDF)               Rust 安全岛
    ┌──────────────────────────┐      ┌──────────────────────┐
    │  main.cpp                │      │  key_vault/           │
    │  wifi_manager            │      │    ├── api_key.rs     │
    │  chat_engine ──────FFI───────►  │    ├── wifi_creds.rs  │
    │  display_driver          │      │    └── ota_verify.rs  │
    │  chat_ui                 │      │                      │
    │  audio_io                │      │  Cargo.toml           │
    │  camera_driver            │      │  (no_std + esp-hal)  │
    │  sd_card                 │      │                      │
    └──────────────────────────┘      └──────────────────────┘
```

---

## 二、Rust on ESP32-S3 生态现状 (2026)

| 维度 | 状态 | 说明 |
|------|------|------|
| `esp-hal` | ✅ 1.1.1 稳定版 | Espressif 官方维护，6 年开发史 |
| ESP32-S3 (Xtensa) | ✅ 全面支持 | 最后的 Xtensa 芯片，已在稳定通道 |
| I2C 驱动 | ✅ 1.0 稳定 | 摄像头 SCCB / 触摸 FT6x06 可用 |
| SPI 驱动 | ⚠️ unstable feature-gate | LCD / 触摸 / SD 卡 SPI |
| GPIO | ⚠️ unstable | 功能完整，API 可能变动 |
| Wi-Fi / BLE | ⚠️ unstable (`esp-wifi`) | 可用，下一批稳定目标 |
| DMA | ⚠️ unstable | 摄像头 DVP / I2S 音频 |
| `no_std` | ✅ 推荐路径 | Espressif 官方推荐所有新项目 |
| `std` (ESP-IDF) | ⚠️ 社区维护 | `esp-idf-hal/svc` 不再由 Espressif 维护 |

---

## 三、安全关键领域识别

### 风险矩阵

| 领域 | 当前状态 | 风险等级 | Rust 收益 | 优先级 |
|------|---------|---------|----------|--------|
| API Key / WiFi 密码管理 | C 字符串明文，日志可能泄露 | 🔴 高 | 零化内存 + 禁止 Debug | **P0** |
| OTA 固件更新 | 未实现 | 🔴 高 | 签名验证 + 回滚保护 | **P0** |
| TLS 证书验证 | 依赖 mbedTLS 运行时配置 | 🔴 高 | 编译期强制证书策略 | P2 |
| HTTP 请求构造 | cJSON 手动拼接 | 🟡 中 | 类型安全序列化 | P2 |
| SSE 流解析 | 手动字符串分割 | 🟡 中 | 状态机保证完整性 | P3 |
| 摄像头帧缓冲 | raw 指针 + DMA 手动管理 | 🟡 中 | 所有权 + 借用检查 | P3 |
| I2S 音频缓冲 | 同上 | 🟡 中 | 同上 | P3 |
| SD 卡文件 I/O | 裸 fopen/fwrite | 🟢 低 | 类型安全 + 错误处理 | P4 |
| LVGL UI | C 库绑定 | 🟢 低 | 无安全增益 | ❌ 不做 |
| display_driver | SPI fire-and-forget | 🟢 低 | 无内存风险 | ❌ 不做 |

---

## 四、分阶段实施

### Phase 1: 基础设施 (v0.5.x, ~2h)

```
目标: Rust 工具链 + CMake 集成 + FFI Hello World
─────────────────────────────────────────────────
□ Cargo.toml 项目初始化
□ esp-hal 依赖配置 (target: xtensa-esp32s3-none-elf)
□ CMake ExternalProject 集成 (Rust → .a 静态库)
□ extern "C" FFI 测试: Rust add(a,b) → C++ 调用 → 验证
□ CI 中添加 Rust 编译检查 (rustup target + cargo build)
```

**风险**: 零 — 不修改任何现有 C++ 代码

### Phase 2: 密钥安全 (v0.5.x, 1-2 days)

```
目标: API Key + WiFi 密码安全存储和访问
─────────────────────────────────────────
□ key_vault Rust crate:
    - secrecy::SecretString 零化内存
    - 禁止 Clone / Debug / Display
    - NVS 加密读写
    - opaque handle FFI (C++ 侧看不到明文)
□ 集成到 wifi_manager: WiFi 密码通过 key_vault 获取
□ 集成到 chat_engine: API Key 通过 key_vault 获取
□ 密码/Key 永远不会出现在日志中 (编译期保证)
```

**C++ 侧接口**:
```cpp
// main/key_vault_ffi.h
extern "C" {
    void* kv_create(const char* nvs_namespace);
    void  kv_destroy(void* handle);
    // 返回 opaque handle，C++ 侧无法读取明文
    int   kv_get_api_key(void* handle, char* out_buf, size_t buf_size);
    int   kv_get_wifi_pass(void* handle, char* out_buf, size_t buf_size);
}
```

### Phase 3: OTA 安全 (v0.6.x, 2-3 days)

```
目标: 固件签名验证 + 回滚保护
─────────────────────────────────
□ ota_verify Rust crate (纯 no_std):
    - ed25519-dalek 签名验证
    - SHA-256 固件完整性校验
    - NVS 持久化版本号 (回滚保护)
    - 签名失败 → 拒绝烧录 + 告警
□ 集成到 OTA 下载流程
□ 公钥编译期嵌入 (const embedded)
```

### Phase 4: 帧缓冲安全 (v0.7.x+, 等 esp-hal DMA 稳定)

```
目标: camera_driver + audio_io 缓冲管理
─────────────────────────────────────────
□ 等 esp-hal DMA 驱动稳定化 (1.0+)
□ 摄像头帧缓冲: 所有权从 DMA → JPEG 编码器 → SD 卡
□ I2S 音频缓冲: 借用检查防止 DMA 和 CPU 同时访问
□ 零拷贝传递 (no_std, 纯引用)
```

---

## 五、成本/收益分析

| 维度 | 成本 | 收益 |
|------|------|------|
| 学习曲线 | 5 个 Agent 均需 Rust 训练 | 长期: 更少内存 bug |
| 编译时间 | +30-60s (Rust 编译) | 更少运行时调试时间 |
| 二进制大小 | +50-100KB (Rust core) | 无 GC/异常开销，基本持平 |
| FFI 开销 | 跨边界调用 ~10ns | 仅非热路径 (初始化/配置) |
| API 稳定性 | esp-hal SPI/DMA 仍在 unstable | 1.0 核心已稳定 (I2C/GPIO) |
| 维护负担 | 双语言 CI + 工具链 | 密钥/OTA 模块长期零维护 |
| **安全收益** | — | **消除整类内存 bug** (use-after-free, buffer overflow, null deref, double free) |

---

## 六、对 5-Agent 开发模式的影响

| Agent | 影响 |
|-------|------|
| **Claude Code** | 需学习 `esp-hal` 生态 + Rust 安全模式；FFI 边界设计需同时审查 C++ 和 Rust 侧 |
| **Codex (GPT-5.5)** | ✅ 最大受益者 — GPT-5.5 的 Rust 代码生成质量极高，适合编写安全关键模块 |
| **CodeWhale** | Plan→Agent 继续用于 C++ 日常修复；Rust 模块独立，互不影响 |
| **Reasonix** | 调试双语言增加复杂度；但 Rust 侧 bug 更少，实际调试量可能下降 |
| **Hermes** | 需创建 "Rust on ESP32" 技能，捕获 Rust 编译/FFI/安全模式知识 |

---

## 七、不做的事

- ❌ 全量重写任何现有 C++ 模块
- ❌ 在 `esp-hal` unstable 外设上构建核心路径
- ❌ Rust 化 LVGL / display_driver（纯 C 库绑定，无安全增益）
- ❌ 用 Rust 替换 `esp_http_client`（mbedTLS 已足够稳定）
- ❌ 引入 Rust 异步运行时（ESP32 资源有限，FreeRTOS 任务模型已够用）

---

## 八、决策记录

| 日期 | 决策 | 理由 |
|------|------|------|
| 2026-06-08 | 采用渐进式 Rust 引入策略 | 全量重写风险高；安全岛模式在 C++ 生态中成熟 |
| 2026-06-08 | P0 优先: 密钥管理 + OTA 验证 | 安全收益最大，实施最简单 |
| 2026-06-08 | 暂不 Rust 化 HTTP/显示/UI | 依赖 C 库绑定或 ESP-IDF 原生，无安全增益 |
| 2026-06-08 | 等 esp-hal DMA 稳定后再迁移帧缓冲 | 核心路径不应依赖 unstable API |
