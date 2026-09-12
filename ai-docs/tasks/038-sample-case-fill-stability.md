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
