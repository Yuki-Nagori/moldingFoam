# 038 — 样例 case 填充/稳定性诊断与参考配置（Kairos 10 mm 立方体）

- 状态：done（2026-09-13；诊断完成 + 参考用例 `tests/cases/boxFill`
  交付并通过 19/19 套件）
- 优先级：P1
- 依赖：003（排气模型）、006（周期完备化）
- 预估规模：1 天

## 1. 现象（Kairos 生成样例，bundle v0.2.1，10 mm 立方体、5000
四面体、4 进程）

1. 入口 `volumetricFlowRate = 型腔体积/1 s`，但 t=2 s 填充仅 0.75，
   顶层角部 166/500 单元 α≤0.05；
2. `endTime 2.4/6 s` 时 t≈2.05 s 起 α 与 `p_rgh` 全 NaN，发散前门控
   压力 0.26→0.51 MPa；
3. `maxAlphaCo 0.03`（契约值）反而更早发散（t≈0.46 s），0.1 可到
   2.05 s；两者 t<0.45 物理量一致；
4. 对照：case-contract 同环境跑到 t=0.54 s 未发散。

## 2. 诊断结论（2026-09-13，结果场 + 求解器日志 + 复现）

### 2a. 问题 1：入口 BC 与填充度量都正确，停滞 = vent 整面开放逃料

- **入口流量精确**：填充曲线前 0.5 s 与名义流量重合（t=0.1 实测
  0.0968 vs 0.10；t=0.5 实测 0.4967 vs 0.50）；质量预算日志
  `mass budget patch inlet: alphaPhi1 = -1000 m³/s`（= Q 精确）；
- **填充度量口径正确**：`filledFraction = ∫α₁dV/V`（物理熔体体积
  分数）；
- **停滞原因**：vent 是整面（120 m²，与入口同量级）开放边界
  （`pressureInletOutletVelocity` + `inletOutlet`），t≈0.68 前沿触达
  后熔体大量排出——质量预算实测 `mass budget patch vent:
  alphaPhi1 = +922.5 m³/s`（**入口流量的 92% 直接逃逸**）；V/P 切换
  （0.96 / 60 MPa）永不触发（填充 0.75、门控 0.26 MPa），持续灌注/
  排料；
- **网格尺度错配**：生成器把 10 mm 写成原始毫米坐标（求解器读成
  10 m 立方体、腔体 1000 m³），Q=1e3 与 mm³/s 口径自洽但与 SI 物性
  组合后整体放大 1e3（重力静水压 ~1e5 Pa 主导、前沿/逃逸行为非
  10 mm 件物理）；
- **未用 vent 模型**：样例 U 用的是通用 `pressureInletOutletVelocity`
  而非 `moldingVentVelocity`/`moldingVentPressure`，故求解器的
  `ventSealAlpha` 密封逻辑（只识别 moldingVent 系列）从不生效。

### 2b. 问题 2/3：失稳是同一设置的病态终局，不是容差问题

- 爆炸前：`dm = 3190 kg/步`、`dm-(psi+dalpha) = 3193 kg`、
  `alpha/vol = -1.169`（α 通量与体积通量失配）→ 速度爆增至 ~1e6 m/s
  → dt 渐进坍缩（0.0167 → 2e-7 → 1e-13 → 1e-100）→ T 方程先炸、
  随后 p_rgh/α 全 NaN；
- 机理：入口恒定 Q 无处可去（腔体停滞、vent 被熔体覆盖、出口面积
  与 α 出口处理不自洽）→ 压力方程/出口通量失配 → 发散；门控压力
  0.26→0.51 MPa 是出口被覆盖的时刻；
- **maxAlphaCo 0.03 更早发散的原因**：同一物理点（填充平台开始，
  实测 t≈0.458、填充 0.4548 停滞）处，更小的 dt 更早解析出出口通量
  不一致（界面更锐、出口覆盖通量立即失配）；0.1 推迟到 2.05 s 才
  触发。两者在触发前轨迹一致，属**设置驱动的病态**，非轨迹差异或
  时间步容差问题；
- 另注：该样例根本没有 vent 密封（未用 moldingVent BC），问题 2/3
  与 vent 密封事件无关。

## 3. 交付：参考用例 `tests/cases/boxFill`（✅ 19/19）

以同一几何重建**正确配置**：SI 尺度（0.01 m 立方体、1000 六面体）、
入口 `volumetricFlowRate 1e-6`（= V/1 s）、vent 用
`moldingVentVelocity` + `moldingVentPressure`（p0=1e5、CdA=1e-6、
`ventSealAlpha 0.9`）、V/P 切换 0.96、质量预算开启。

实测（`scripts/verify-box-fill.py`）：

- 早期填充（t≤0.3 s）与 Q t 偏差 ≤ **2.3%**；
- **V/P 切换在 t=1.177 s、填充 0.9614** 触发；vent 被熔体密封；
- 最终填充 **0.9786**、无 NaN、稳定跑完；
- vent 积分逃逸 **7.07%**（粗网格 10³ 界面涂抹的数值膜，集中于密封
  瞬间与 V/P 切换瞬态；持续峰值已抑制）——已在验证器中记录为
  已知数值限制；
- 已并入 `xmake run test-solver`（18 → **19 用例**）。

## 4. 给 Kairos 的修复清单（按优先级）

1. **vent 建模**：改用 `moldingVentVelocity`/`moldingVentPressure`
   （`CdA` 与实际排气面积一致 + `ventSealAlpha`），或把 vent 缩到
   物理面积（避免与入口同量级）；
2. **SI 尺度**：网格坐标转米（mm×1e-3）、Q = V_si/注射时间；
   勿用 mm-as-m 的坐标 + mm³/s 流量与 SI 物性混搭；
3. **V/P 切换**：按材料/机台设定 `switchFraction`（~0.96–0.99）与
   压力表；样例的 60 MPa 只在切换后施加；
4. **网格**：填充方向加密（本参考 10³ 粗网格导致 7% 数值膜逃逸；
   20+ 层可显著抑制）；
5. 诊断入口：`massBudget true` 会逐 patch 打印熔体通量，可直接
   看到 `patch vent: alphaPhi1` 的逃逸量。

## 5. 涉及文件

| 文件 | 改动 |
|------|------|
| `tests/cases/boxFill/`（新） | 参考用例（SI + vent 模型 + 预算） |
| `scripts/verify-box-fill.py`（新） | 验收：早期填充/Q、切换、密封、逃逸积分 |
| `ai-docs/README.md` | 任务索引（031–038 状态刷新） |

## 6. P1 回流跟进（Kairos 真机复测，2026-09-13 晚）

### 6a. T 先 NaN 的机理：速度 runaway 的**后果**，非能量方程本身

- 以 `/tmp/kairos-e2e/box6` 将 Q 放大 10×（等效小浇口/高压力工况）复现：
  `t≈0.017 s`（填充 1.7%，与真实件 1.4% 同量级）时
  `smoothSolver: T: Initial residual = 1 → Final nan, 1000 iters` **先于**
  其他场 NaN；
- 但日志序列显示：**dt 已先坍缩**（8.6e-15）、Co ≈ 1e-7（U ~ 1e7 m/s）、
  门控压力 6.7 MPa（≈ 惯性压降 ρU²/2，物理量级正确）——即
  压力/动量 runaway → dt 坍缩 → 能量方程系数（对流通量）溢出 → T NaN
  → EOS → 全场 NaN。**冷壁梯度本身没问题**（样例盒冷壁 313 K 跑满
  1.0 s 无 NaN）；
- 触发条件是**浇口速度量级**：真实件浇口 U = Q/A 达数百 m/s（熔体声速
  ~200–1000 m/s，局部 Mach ~O(1)），可压缩两相求解器在此失稳；样例盒
  U = 8.3 m/s 在 t≈2.05 s 经 vent 路径进入同一终局。

### 6b. 交付：可行性预警 + 快速失败（求解器两个防线）

1. **启动浇口速度预警**（`fillVelocityWarn`，缺省 5 m/s，0 关闭）：
   填充第一步按 `U = |φ|/A`（moldingInletVelocity patch）计算名义入口
   速度并 `Warning`，给出 Q/A/阈值与建议（检查浇口面积/流量或设机台
   限压 switchPressure）。实测：样例盒 8.33 m/s → 预警；契约 case
   0.5 m/s → 静默；Q×10 83 m/s → 预警；
2. **非有限快速失败**：`postSolve` 检测 T/|p_rgh| 非有限即
   `FatalError`（附 t、max(T)、max|p_rgh| 与处置建议）。实测 Q×10：
   23,000+ NaN 行 → **17 行 + 清晰 FATAL（rc=1）**；
3. **限压保护**（既有 V/P 切换）：`switchPressure` 设为机台限压后，
   流量阶段被压力封顶；注意不可行工况（极低填充即达限压）切换后
   保压表阶跃仍会失稳——此时应判**工况不可行**而非继续（快速失败
   会明确报出）。

### 6c. 给 Kairos 的"填充压力上界"判据

- 名义判据：`U_nom = Q/A_in`、动压 `0.5 ρ U_nom²`。经验阈值：
  - `U_nom < 5 m/s`：正常；
  - `5–20 m/s`：谨慎（粗网格/大 Q 易失稳，加密+小 dt）；
  - `>20 m/s`：不可行（所需注塑压力远超机台，多半在填充早期崩）；
- 更严格：求解器日志的 `p_gate` 超过机台限压的 50% 即预检不通过；
- 求解器已内置上述两个防线，Kairos 可在提交前用 bundle 跑 1–2 步
  读取预警/日志判定。

### 6d. 验证用例形态建议（Kairos 提出，已覆盖情况）

| 形态 | 现状 |
|------|------|
| 整面进料 | `tests/cases/boxFill`（+通用 vent 对照） |
| 点浇口 | Q 放大等效复现（速度判据已交付）；建议再建专用小 case |
| 冷壁 + 薄壁 | 样例盒即冷壁 313 K；薄壁专用 case 待补（速度判据先行） |

## 7. P2 契约（本次定义）

### 7a. C5 冷却水路瞬态（数据契约）

既有能力（两条路径，均已验证）：

1. **模壁 1D 通道（推荐，Kairos 现有面板直连）**：模壁 patch 的 T
   BC 用 `moldingMoldTemperature`，通道写在 `coolant` 子字典：
   ```yaml
   coolant
   {
       massFlowRate     0.05;      // [kg/s]
       cp               4180;      // [J/kg/K]
       inletTemperature 293.15;    // [K] 介质入口温度（面板字段）
       direction        (1 0 0);   // 通道轴向
       htc              5000;      // [W/m^2/K] 或改用 Nu 相关式：
       // Nu { C 0.023; m 0.8; n 0.4; Re 6000; Pr 7; k 0.6; D 0.008; }
   }
   ```
   该 BC 逐步求解 1D 活塞流能量平衡（离散能量守恒已验收
   `validation/coolantMold`、moldCHT 套件）；冷却阶段瞬态传热天然
   包含（壁温随水路取热演化）；
2. **三维水路 CHT（需要真实水流场时）**：独立 `moldingCoolantFluid`
   区域 + `coupledTemperature`，见 `validation/coolantWater`（单区）
   与 `validation/coolantWaterMold`（水+模具，界面能量平衡
   1.2e-07）。

Kairos 侧 `cooling_channels`（直径/起止/介质温度）映射：每个通道
的**润湿模壁 patch** 上写 `cooling { ... }`（如上）；直径用于
Nu{...D} 或由 Kairos 折算 `htc`。字段名以本契约为准。

### 7b. C6 翘曲位移/应力场（数据契约）

| 场 | 类型 | 量纲 | 说明 |
|----|------|------|------|
| `D` | volVectorField | `[length]` = **m (SI)** | 位移；显示按 mm ×1000 |
| `sigma` | volSymmTensorField | `[Pa]` | 残余应力张量 |
| `sigmaEq` | volScalarField | `[Pa]` | 等效应力 |
| `T` | volScalarField | `[K]` | 温度（各向同性映射时即本征应变代理） |

- 写出位置：case 时间目录 `<time>/D`（writeInterval 控制），样例见
  `validation/warpagePlate`（4.2%）与 `validation/shrinkBar`（机器精度
  自由收缩）；bundle 内 `xmake run warpagePlate` 可直接复跑生成；
- 流动侧关联场（如启用）：`shrinkage` [-]、`shrinkageTensor` [-]
  （volSymmTensorField）、`voidFraction` [-]；
- 单位说明：OpenFOAM 惯例为 SI（m/Pa/K），Kairos 显示层做 mm 换算。

## 6e. 保压阶跃修复与填充期 T 失稳复现（2026-09-13 晚二）

**保压阶跃（Kairos B/C）**：V/P 切换由填充分数触发时，闸口压力低于
保压曲线首点，切换瞬间形成 ~55 MPa 单步阶跃 → 跨声速 → 失稳。
修复（v1.22 契约）：

- `packing.pressureRamp`（缺省 0.05 s）：BC 在**首次保压更新**时捕获
  实测闸口压力并线性 ramp 到曲线目标（压力触发时起点=首点，无操作）；
- 切换填充率 < 0.90 警告；`fillVelocityWarn` 重标定 20 m/s（SI）。

验证（最小）：`tests/cases/boxFill`（96% 填充、57.7 MPa 阶跃）ramp 后
稳定通过；人为 `switchFraction 0.5`（50% 填充即保压，属不可行工况）
按预期被警告并在保压瞬态快速失败（守护生效）。

**填充期 T 失稳（已修复）**：3-block 槽形点浇口（0.6×10 mm、
A≈6e-6 m²、冷壁 313 K）在填充 ~70% 时 T 残差先 NaN。逐步诊断显示
根因是**冻死短射**：样品熔体导热极高（k=mu·Cp/Pr≈6.3e4 W/mK），冷模
+ 慢充下**可动熔体分数从 t=0.1 起即 0%**（T.melt 峰值 372 K < 固化
418 K），型腔早已冻死而入口仍强制流量 → 冻料被挤过收缩通道 → 局部
速度尖峰（1.3→96 m/s）→ T 降至 269 K（低于壁温）→ 能量方程 NaN。

**修复（v1.23 契约）**：充填期冻死检测——已充体积中可动熔体（T >
`freezeOffTemperature`）占比 < `freezeOffFraction`（缺省 0.01）即判定
短射：**封闸**（零速/零通量）+ 切保压控制 + 打印
`melt freeze-off detected … (short shot)`。验证：复现件（2% 检出、
短射 2.1%）与永久用例 `tests/cases/freezeOffGuard` 稳定跑到 endTime、
无 NaN；21/21 用例 + 模型测试全绿。
