# 042 — 防线回归接入 nightly（037 堆退出 + 038 预警/快速失败）

- 状态：planned
- 优先级：P1
- 依赖：037（堆破坏退出防线）、038（守卫与预警）
- 预估规模：0.5–1 天
- 来源：`ai-docs/coverage-audit-2026-09-13.md` 缺口 G1、G7

## 1. 背景与现状

- **G1**：`scripts/smoke-exit.sh`（037 的堆破坏退出防线：把任意 case 拷到
  临时目录跑一遍并检查退出码/`malloc_consolidate` 类报告）未接入任何
  workflow——037 报告自身即写明「建议接入：CI（快）或夜间
  （`bash scripts/smoke-exit.sh case-contract`）」，但至今只能手工跑；
- **G7**：038 的 P1 防线缺回归——`fillVelocityWarn` 阈值无任何用例配置、
  预警文本无断言；`src/moldingFoam/moldingFoam.C:1876` 的「非有限快速
  失败」`FatalError` 从未被触发（属失败路径，可接受，但 README 的
  P1 防线 claim 与测试不匹配）。

## 2. 目标

1. `smoke-exit.sh` 进入 nightly，失败可定位（独立日志 + artifact）；
2. 038 两条防线各有一个用例断言：预警阈值可配且触发时输出可断言；
   非有限快速失败有最小复现（或明确记录「不可稳定复现、仅保留实现」
   的决策）；
3. 缺省行为与既有用例不受影响。

## 3. 技术方案

- **A（接线，优先）**：nightly `contract` job 在 `case-contract` 之后追加
  `bash scripts/smoke-exit.sh case-contract 2>&1 | tee smoke-exit.txt`，
  artifact 增加 `smoke-exit.txt`（该脚本自建临时目录，不污染）
- **B（预警断言）**：在既有 `tests/cases/boxFill` 增加一个参数化变体
  （同几何，`fillVelocityWarn 1e-3` + 入口速度放大到 >1 m/s），
  `expectedPatterns` 断言预警行；默认阈值（20 m/s）由原用例继续覆盖
  「不触发」路径
- **C（快速失败）**：构造最小发散用例（如入口速度 1e3 m/s 的 3 步运行）
  断言 `FatalError` 文本与退出码；若数值上不稳定复现，则在 038/本任务
  文档记录原因并降级为「仅实现、无回归」并在审计文档中同步该决策

## 4. 工作拆解

1. nightly `contract` job 接线 + artifact（G1，约 10 行 YAML）；
2. boxFill 变体用例 + 断言（G7 预警）；
3. 快速失败最小复现尝试（≤1 小时时间盒），成功则加入 solver 用例，
   失败则记录决策；
4. 文档：README CI 表、037/038 交叉引用、审计文档 G1/G7 标注关闭。

## 5. 验收标准（DoD）

- nightly 中出现 `smoke-exit.sh` 步骤且本地在 VM 内手工跑通（PASS）；
- 预警用例在 22+ 用例套件中通过，未触发路径仍是原用例；
- 快速失败有断言或明确的不可复现结论；
- 文档同步（README CI 表 + 审计标注）。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| smoke-exit 在 CI 上因环境差异失败 | 先在 VM 内跑通再接线；失败日志独立成块 |
| 预警用例阈值过敏感导致平台差异误报 | 阈值取能明确触发的量级（1e-3 vs 20），断言只看「出现预警」 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `.github/workflows/nightly.yml` | contract job 增加 smoke-exit 步骤与 artifact |
| `tests/cases/boxFill*`（或新变体） | fillVelocityWarn 断言 |
| `tests/cases/*`（快速失败，若可行） | 断言 FatalError |
| `README.md`、`ai-docs/coverage-audit-2026-09-13.md` | CI 表与缺口标注 |
