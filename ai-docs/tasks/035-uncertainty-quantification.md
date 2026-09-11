# 035 — 数值不确定性量化（网格/时间收敛，验证器收紧）

- 状态：done（阶段 1：thermoelastic/warpagePlate 收敛数据 + GCI/有界误差报告；阈值 10%→8% 且通过）
- 优先级：P2
- 依赖：017（基准体系）/022/013b（弯曲 5% 未定量）
- 预估规模：1–2 周

## 1. 背景

弯曲类基准（thermoelastic 5.46%、warpagePlate 4.2%）的 FV 离散误差
已知随网格收敛（13.5%→85.5%→94.6%），但缺少系统的收敛阶与不确定度
报告；验证器阈值（10%）比实测宽 2 倍以上。

## 2. 目标

1. 对 thermoelastic、warpagePlate、fountainFlow、moldCHT 做
   网格/时间细化研究（≥3 级），Richardson 外推给出观测阶与
   95% 置信区间；
2. 将相关验证器阈值**收紧**到外推值 ± 置信区间（不放松）；
3. 发布 `ai-docs/uncertainty.md`（方法、数据、结论、局限）。

## 3. 技术方案

- 网格序列（h, h/2, h/4）与时间步序列（maxCo×2）；GCI/Richardson；
- 比较通量/积分量（挠度、前沿位置、界面热流）而非单点；
- 阈值更新后重跑全回归；CI 绿。

## 4. 验收标准（DoD）

- 收敛阶与 GCI 报告；至少 thermoelastic/warpagePlate 阈值 <10%
  （如 7%）且仍通过；
- 现有基准数字不劣化；CI 双架构绿。

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 细化后运行时长增长 | 只选小基准；外推用 3 级 |
| 观测阶低于理论（混合误差） | 如实报告并归因（边界/离散） |

## 6. 涉及文件

| 文件 | 改动 |
|------|------|
| `ai-docs/uncertainty.md`（新） | 报告 |
| `scripts/verify-*.py` | 阈值收紧 |
| `validation/thermoelastic/, warpagePlate/` | 细化网格配置 |
