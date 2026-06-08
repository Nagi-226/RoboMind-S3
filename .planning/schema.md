# .planning/ — RoboMind-S3 自动化调度基础设施

## 目录结构

```
.planning/
├── tasks.json           # 任务队列: 当前所有待执行任务
├── loop_state.json      # 运行时状态: Agent 忙闲 / 版本进度 / 变更追踪
├── schema.md            # 本文档: 格式说明 + 使用指南
└── archive/             # 已完成任务记录归档
```

## tasks.json Schema

```json
{
  "_schema": "版本号",
  "_updated": "ISO 时间戳",
  "loop_config": {
    "mode": "sequential | parallel",           // 调度模式
    "stop_on_failure": true,                    // 任务失败是否暂停
    "max_retries_per_task": 3,                  // 单任务最大重试
    "notify_claude_on_batch_complete": true,    // 批次完成通知
    "auto_advance_version": false,              // 是否自动推进版本
    "require_claude_review_after_each_version": true  // 每版本后暂停
  },
  "task_queue": [{
    "id": "CX-v0.0.9-01",                // 唯一 ID: {Agent前缀}-{版本}-{序号}
    "agent": "codex|codewhale|reasonix|hermes",
    "model": "GPT-5.5|DS-v4-pro",
    "priority": "P0|P1|P2",
    "status": "ready|in_progress|done|failed|blocked",
    "category": "implementation|quality|audit|knowledge|fix",
    "description": "人类可读任务描述",
    "detail": "详细说明(可选, 供 Agent 理解上下文)",
    "affected_files": ["文件列表"],
    "prompt_ref": "prompts/xxx.md",
    "validation": {
      "script": "验证命令",
      "ci_jobs": ["关联 CI job 名"],
      "auto_accept": true,           // true = CI 通过即标记完成
      "manual": "人工审查说明"        // false = 需要 Claude 手动审查
    },
    "depends_on": ["前置任务 ID"],
    "retries": 0
  }],
  "agent_registry": { /* Agent 能力定义 */ },
  "auto_routing": { /* 任务前缀 → Agent 映射 */ }
}
```

## 任务分类体系

| 类别 | 分配 Agent | 说明 |
|------|-----------|------|
| `implementation` | Codex | 新功能、新模块、复杂协议 |
| `quality` | CodeWhale | 代码风格、Kconfig、文档一致性 |
| `audit` | Reasonix | 错误路径、并发安全、生命周期 |
| `knowledge` | Hermes | 知识图谱、跨会话记忆、技能创建 |
| `fix` | CodeWhale (默认) | Bug 修复 |
| `debug` | Reasonix | 复杂调试、内存泄漏、死锁 |

## 自动化边界

### ✅ 自动化环节

1. **任务状态追踪**: tasks.json 由引擎自动更新，Claude 只需审查
2. **CI 结果判定**: pr-checks.yml 输出通过/失败，引擎自动关联到任务
3. **结构验证**: Python 验证脚本通过 → 自动标记 `done`
4. **MD 版本同步**: 版本完成时，引擎自动更新 CLAUDE.md / README.md 版本标记
5. **Agent 提示词生成**: 从 tasks.json + 模板，参数化生成提示词
6. **Worktree 生命周期**: 引擎自动创建/清理

### ❌ Claude 独占环节

1. **架构设计**: 需要跨模块创造性推理
2. **版本规划**: 需要全局判断 + 硬件状态感知
3. **语义代码审查**: 需要理解代码意图
4. **最终验收**: 任何 `auto_accept: false` 的任务
5. **Agent 间上下文传递**: 前一个 Agent 的输出如何影响下一个的输入

## 版本状态转换

```
ready → in_progress → done  (自动)
                     → failed → retry (自动, ≤3次)
                              → blocked (通知 Claude)
done → Claude 审查 → verified (手动)
                   → rejected (手动, 重置为 ready + 附审查意见)
```

## 使用示例

```bash
# Claude 规划完 v0.1.0 后, 生成任务:
# 1. 编辑 .planning/tasks.json, 添加 v0.1.0 的 CX-* 任务
# 2. 更新 loop_state.json 的 current_version
# 3. 对每个任务: 写提示词 → prompts/v0.1.0-{agent}-{task}.md
# 4. 手动启动调度: "Codex, 执行 CX-v0.1.0-01"
# 5. Codex 完成后, 引擎自动更新 tasks.json 状态
# 6. 所有 CX-* 完成 → 引擎通知 Claude: "v0.1.0 全部完成, 请最终审查"
```
