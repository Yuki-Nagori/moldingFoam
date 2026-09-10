# 006 — 契约注塑周期物理完备化（排气封堵 / 压力切换 / 闸口封冻）

- 状态：done（2026-09-10。验收：契约 case 全项 PASS，质量守恒相对误差
  见验收记录；平均熔体温度单调；001/002 变体可判定）
- 优先级：P0（阻塞 001/002 的物理验收）
- 依赖：无（与 001/002 协同落地）
- 预估规模：2–4 天

## 1. 背景与现状

在 001（潜热）与 002（模温）的低顶出温度验收中，发现契约 case 的
M1/M2/M3 周期并不自洽（`latentHeat 0/2e5` 两组一致）：

| 现象 | 数据 |
|------|------|
| 型腔质量在 V/P 切换后持续流失 | 峰值 2.03e-3 kg → 4.8e-4 kg（原案例） |
| 流失主要经浇口 | 保压倒流 +6.1e-4 kg，释放后 +7.2e-4 kg |
| 平均熔体温度非单调 | 383 K → 405 K（排料导致） |
| 排气口通过聚合物 | 全程 vent 质量流出 ~2e-4 kg |

根因：

1. **排气口全开**：真实排气槽只透气，聚合物到达后冻结封堵；
2. **V/P 切换判据单一**：按填充分数切换时，熔体可能已在排气口封堵，
   流量控制继续注料把型腔过度压缩，切换瞬间压力阶跃引发倒流；
3. **浇口不封冻**：保压曲线结束后浇口仍保持 1e5 开放，压缩/收缩导致
   型腔持续排料或吸料，平均熔体温度失去意义。

## 2. 目标与非目标

### 目标
1. 排气口只透气、遇熔体密封；
2. V/P 切换无压力阶跃，避免型腔过压与切换倒流；
3. 保压结束后浇口封冻，型腔不再排料；
4. 平均熔体温度在冷却段单调，顶出判据可判定；
5. 契约 case 质量守恒 < 1e-3（高保压下）。

### 非目标
- 排气反压/困气压力演化模型（属 003，仍为 planned）；
- 多浇口/多排气口的独立状态；
- 闸口冻结的凝固传热模型（此处按保压结束时刻封冻）。

## 3. 技术方案

### 3.1 相感知排气口（`moldingStage::ventSealed`）

- `moldingStage` 新增 `ventSealed` 状态（持久化），由求解器在
  `preSolve` 检测：排气口 patch 的 `max(alpha.melt) >= ventSealAlpha`
  （`moldingDict` 可选，缺省 0.5）时密封；
- 新边界 `moldingVentVelocity`（`pressureInletOutletVelocity` 派生）：
  未密封时按压力出/入流；密封时置零速度；
- 新边界 `moldingVentPressure`（`mixed` 派生）：未密封时定压 `p0`；
  密封时零梯度（配合零速度实现零通量）。

### 3.2 压力触发 V/P 切换（`packing.switchPressure`）

- `moldingDict` 新增可选 `packing.switchPressure`：闸口压力达到该值
  （保压设定压力）即切换，与 `switchFraction` 取先到者；
- 保压曲线起点取切换压力，消除切换压力阶跃；
- 避免熔体封死排气口后流量控制继续注料造成的过压。

### 3.3 闸口封冻（`moldingStage::gateSealed`）

- 保压目标降至 `cooling.releasePressure` 时置 `gateSealed`（持久化）；
- `moldingInletVelocity` 密封后置零；`moldingPrghPressure` 密封后转
  零梯度；型腔停止排料/吸料。

### 3.4 高压下的质量守恒数值控制

保压压力提高到 MPa 量级后，可压缩 VoF 的界面输运质量误差随密度比
增大。契约 case 将界面子循环与界面库朗数收紧为
`nSubCycles 12 / maxAlphaCo 0.05`，并缩短保压平台（0.15 s）以把
质量守恒相对误差压回 1e-3 以内。

## 4. 工作拆解

1. `moldingStage`：`gateSealed`/`ventSealed` 状态、方法与持久化；
2. 新边界 `moldingVentVelocity`、`moldingVentPressure`（自注册）；
3. `moldingInletVelocity`、`moldingPrghPressure` 增加封冻分支；
4. `moldingFoam::preSolve`：排气密封检测、压力触发切换、闸口封冻；
5. 契约 case：`0/U`/`0/p_rgh` vent 类型、`moldingDict` 新键与保压曲线、
   `fvSolution`/`controlDict` 数值控制；
6. README §6–§8 与契约日志更新；
7. 全周期回归（潜热开/关、模温变体）。

## 5. 验收标准（DoD）

- 契约 case 全项 PASS，质量守恒 < 1e-3；
- 平均熔体温度在冷却段单调下降；
- 排气密封、V/P 切换、闸口封冻日志齐备；
- 潜热开/关与模温绝热/强冷变体行为方向正确；
- CI 双架构绿。

验收记录：

- 契约 case（`latentHeat 2e5` + `Pr 4`，`MOLDINGFOAM_PARALLEL=4`）：
  全项 PASS，`Mass conservation relative error = 9.305e-04`；
  V/P 切换（压力触发）t = 1.0175 s（p_gate = 1.286e6 Pa）；排气口密封
  t = 1.0727 s；闸口封冻 t = 1.1698 s（切换后 0.152 s，p_target =
  1.273e6 Pa）；顶出 t = 1.2709 s（平均熔体温度 360.63 K）；8834 步、
  约 687 s（8 核 ARM64 VM）；
- 冷却段质量加权平均熔体温度单调：t=0.8/0.9/1.0/1.1/1.2/1.275 s 对应
  382.1 / 372.9 / 364.2 / 361.3 / 360.2 / 360.1 K；
- 提交哈希：待提交（随本报告一并提交后回填）。

## 6. 验收摘要

```
moldingFoam: V/P switch: filled fraction = 0.9000044, p_gate = 1285791 Pa
  (switchFraction = 0.9, switchPressure = 1300000 Pa), at t = 1.017521 s
moldingFoam: vent sealed by the melt front: max(alpha.melt) = 0.9000164 >= 0.9 at t = 1.072692 s
moldingFoam: gate sealed at t = 1.16977 s (0.1522494 s after the V/P switch, p_target = 1273007 Pa)
moldingFoam: ejection criterion met: average melt temperature = 360.6303 K <= 383.15 K at t = 1.270859 s
Mass conservation relative error = 9.305e-04
All acceptance checks passed
```

## 7. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 排气密封阈值过低会过早困气、过高会漏料 | `ventSealAlpha` 可调；契约取 0.9，实测密封在 V/P 切换附近 |
| 高保压提高界面密度比、质量误差上升 | 收紧界面库朗数/子循环并缩短保压平台；必要时进一步降低保压压力 |
| 封冻后型腔全封闭、压力无参考 | 型腔可压缩（Tait），压力由质量/能量决定，无需 Dirichlet 参考 |
| 与 001/002 的改动耦合 | 三者按顺序落地并整体回归 |

## 8. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingStage.{H,C}` | 封冻/密封状态与持久化 |
| `src/moldingFoam/boundaryConditions/moldingVentVelocity/*` | 新增：相感知排气速度 |
| `src/moldingFoam/boundaryConditions/moldingVentPressure/*` | 新增：相感知排气压力 |
| `src/moldingFoam/boundaryConditions/moldingInletVelocity/*` | 闸口封冻分支 |
| `src/moldingFoam/boundaryConditions/moldingPrghPressure/*` | 闸口封冻分支 |
| `src/moldingFoam/moldingFoam.{H,C}` | 密封检测、压力切换、闸口封冻、日志 |
| `case-contract/0/U`、`0/p_rgh` | vent 边界类型 |
| `case-contract/constant/moldingDict` | `switchPressure`、`ventSealAlpha`、保压曲线 |
| `case-contract/system/fvSolution`、`controlDict` | 界面数值控制 |
| `README.md` | §6–§8 与契约日志 v1.4 |
