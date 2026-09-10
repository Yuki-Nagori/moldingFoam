# 010 — 闸口冻结物理（局部温度/剪切判据）

- 状态：done（2026-09-10：温度型封冻判据已实现并用例验收；契约
  case 的固定 480 K 入口不会触发，物理触发依赖 016 流道热耦合）
- 优先级：P2
- 依赖：006（时序封冻已落地）、016（流道/热流道热耦合，温度判据）
- 预估规模：3–5 天

## 0. 评审结论（先读）

在当前契约 case 上直接做"闸口局部温度低于冻结温度"判据**不会触发**：
浇口 `inlet` 的 `T` 是 `fixedValue 480 K`（热流道蓄热假设），且没有
流道/闸口的传热模型，闸口单元始终被 480 K 熔体补热，平均温度不会
降到无流动温度。

可落地的前提（二选一）：

1. 先做 016（流道/热流道热耦合），让入口温度随流道热状态演化；
   再按 `T_gate <= gateFreezeTemperature` 判据封冻；
2. 或改判据为**流动/黏度型**：保压后期闸口流量降到注料流量的小比例
   （或闸口剪切率/黏度超过无流动阈值）即封冻——这与 006 的
   `gateSealTime` 近似同义，可用局部通量替代固定时间，收益有限。

实施结论（2026-09-10）：

- 已实现 `moldingDict` 可选 `gateFreezeTemperature` [K]：保压阶段按
  闸口 patch（`moldingPrghPressure`）单元的质量加权平均温度判据封冻，
  与既有的 `gateSealTime`/releasePressure 兜底并存；
- `tests/cases/gateFreeze` 快速用例：闸口 patch 温度 480 K、
  阈值 485 K → 首个保压步即触发封冻，日志/正则校验通过
  （`xmake run test-solver`）；
- 契约 case 的入口为固定 480 K 热流道假设，闸口不会降温，因此
  契约不配置该键（缺省关闭、行为不变）；真实闸口冻结需要 016 的
  流道/闸口热模型或流动型判据，见下节。

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
