# 合并证据 · ppm-integration → master

- 日期：2026-11-13
- 源分支：`ppm-integration`（`b61a45a`）
- 目标分支：`master`（`995b243`）
- 内容：component 补 2 源文件 + PPM 事件生产者 + wire codec

---

## A · 构建与烧录

| # | 判据 | 结果 |
|---|---|---|
| A1 | `idf.py build` 退出码 | **0**，0 error / 0 warning |
| A2 | **烧的就是编的**（`compile time` 对齐） | `compile time Nov 13 2026 10:03:13` 与构建时间戳一致 |
| A3 | 烧录成功 | `Hash of data verified`，`Wrote 1231216 bytes at 0x00020000` |
| A4 | 读回校验 | 读回 flash 0x20000 与本地 `robo_mind_s3.bin` **逐字节相同**（md5 一致） |

## B · 默认态零影响（特性开关类改动必检）

| # | 判据 | 结果 |
|---|---|---|
| B1 | **宏检查**（不查符号，见 §9 规则 6） | 默认构建 `sdkconfig.h` 中 `CONFIG_ROBOMIND_PPM_MOCK_FIXTURE` 计数 = **0** |
| B2 | 串口零新增行 | `[producer]`=**0**，`[ppm ]`=**0**，`[wire]`=**0** |
| B3 | `[mem]` 五点齐全 | **5** 行 |

## C · 关键行为

| # | 判据 | 结果 |
|---|---|---|
| C1 | 无目标路径（真实场景：镜头前没人） | `frames=300 ok=300 run_fail=0 encode_fail=0 no_target=300 with_target=0` |
| C2 | 有目标路径（mock 夹具） | `frames=200 ok=200 run_fail=0 encode_fail=0 no_target=0 with_target=200` |
| C3 | wire 帧全量校验 | **320 帧，结构失败 0** |
| C4 | payload 分布 | `dg=1` × **253**（无人脸已确认）／`dg=0` × **67**（上电初期 presence 未到首个执行帧，属正常） |

**解码样例**（第一帧，去掉 4 字节长度头后）：

```
1|telemetry|best_effort|0|7537|300|ppm-c510-70ce|vvos|ppm/state||dv=N;dl=0;ev=N;es=-;mv=N;ms=-;ab=0;dg=0
```

11 字段完整，`source` 带 MAC 与 boot 标识，`ttl=300`（状态类），payload 为无目标形态。

## D · 静态资源

| # | 判据 | 结果 |
|---|---|---|
| D1 | `.dram0.bss` | **`0x1cde0`**，与改动前相同 |
| D2 | **新增 TU 的 `.bss`/`.data` 贡献** | `ppm_producer` / `ppm_semantics` / `wire_codec` **均为 0x0**（map 逐行可查） |
| D3 | PPM 静态池仍在（链接锚点有效） | `nm` 可见 `ppm_run` / `ppmi_ref_backend` |

> D2 采用"新增 TU 贡献 = 0"而非"总数不变"：后者会被"删 8 加 8"骗过。

## E · 栈

| 分支 | 峰值 | high_water |
|---|---:|---:|
| 无目标 | **3,696 B** | 2,448 B |
| 有目标 | **3,712 B** | 2,432 B |

两者均 < 6,144 的 61%，`high_water ≥ 1,024` ⇒ **保持 6,144 不升级**。

---

## 证据来源（完整日志留本地，`docs/logs/` 已在 .gitignore 中）

- `docs/logs/u02b-default-zeroimpact-2026-11-13.log`
- `docs/logs/u02b-noface-frames-2026-11-13.log`
- `docs/logs/u02b-fixture-frames-2026-11-13.log`
- `docs/logs/idf-build-env-2026-11-13.md`

## 独立验证记录

C4 的 `dg` 分布由主理人**直接解码日志中的 hex** 得出，未依赖工程师转述：

```bash
grep '\[wire\]' docs/logs/u02b-noface-frames-2026-11-13.log \
  | sed 's/.*hex=//' \
  | while read h; do echo "$h" | xxd -r -p | tail -c +5; echo; done \
  | grep -oE 'dg=[0-9]+' | sort | uniq -c
# → 253 dg=1 / 67 dg=0
```

---

## 结论

**A–E 全部通过，符合合入 master 的条件。**

破例项：无。
