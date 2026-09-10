# 010 — 闸口冻结物理（局部温度/剪切判据）

- 状态：planned
- 优先级：P2
- 依赖：006（闸口封冻的时序机制已落地）
- 预估规模：3–5 天

## 1. 背景与现状

006 中闸口封冻由 `packing.gateSealTime`（V/P 切换后固定时间）或保压
压力释放触发。真实闸口冻结取决于**局部热历史**：闸口截面小、散热快，
熔体温度降到无流动温度以下即冻结；冻结时刻决定保压补偿的有效时间与
制品重量。

## 2. 目标与非目标

### 目标
1. 以闸口区域（可配置 cellZone/patch 邻域）的**质量加权平均温度**或
   `alpha.melt·solid` 判据判定冻结；
2. 冻结后自动进入封冻分支（复用 006 的 `gateSealed`）；
3. 字典可配：`gateFreezeTemperature`（缺省无流动温度 = Tait `Tt`）或
   时间兜底。

### 非目标
- 闸口凝固潜热的单独求解（复用 hMelt）；
- 闸口几何自动识别（先按 patch/cellZone 配置）。

## 3. 技术方案

- 在 `moldingFoam::preSolve` 中，packing 阶段计算闸口 patch 邻域
  （`patch().patchInternalField()` 或配置的 cellZone）的质量加权
  平均温度 `T_gate`；
- 当 `T_gate <= gateFreezeTemperature` 或 `t - switchTime >=
  gateSealTime` 时置 `gateSealed`；
- 保留 006 的压力/时间兜底语义，缺省行为不变。

## 4. 工作拆解

1. 闸口区域定义（patch 邻域/字典 cellZone）；
2. 温度判据与日志；
3. 与 006 时序兜底的优先级；
4. 验证 case：不同模温下冻结时刻变化合理；
5. modelTests（判据函数）+ 文档。

## 5. 验收标准（DoD）

- 缺省配置行为与 006 一致（回归）；
- 模温降低 → 冻结提前；模温升高 → 冻结延后；
- 冻结后型腔质量不再变化（守恒）；
- CI 双架构绿。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 闸口温度受网格分辨率影响 | 用质量加权/体积加权平均并做网格敏感性检查 |
| 冻结判据与潜热平台相互影响 | 阈值取 Tait `Tt` 以下并留裕度 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingFoam.{H,C}` | 闸口冻结判据 |
| `case-contract/constant/moldingDict` | `gateFreezeTemperature` |
| `README.md` | §6/§8 |
