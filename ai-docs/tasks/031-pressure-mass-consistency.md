# 031 — 保压压力方程质量一致定式（根治 018，去事后修正器）

- 状态：in-progress（2026-09-12：psi 非 stale 已证；下一步=压力方程逐项仪表/显式质量通量投影）
- 优先级：P0
- 依赖：018（诊断完成：ψ/ρ 拆分误差 4.5e-2，`massFixGlobal` 修正到 1.11e-5）
- 预估规模：1–2 周

## 1. 背景

018 的逐步四项分解证明：`dm − (ψ·dp + Δα) = 1e-8…1e-10`（密度更新
自洽），而每步 `ψ·dp + flux·dt = ±1e-7（~25%）` 交替累积到 4.5e-2
——不一致在 `p_rghEqnComp1/2` 的 ddt/div 拆分与 ψ/ρ 在迭代间的滞后。
当前靠 `massFixGlobal` 事后修正（官方 case 1.11e-5），但定式未根治。

## 2. 目标

1. **无修正器**下 40 MPa 标定 case 的 Euler 一致离散质量守恒 <1e-3，
   且干净初场稳定跑过封冻（基线 NaN@1.321 s）；
2. 修正器开启/关闭的解差 <1%（验证事后修正的物理无害性）；
3. 现有 case 结果不劣化；CI 双架构绿。

## 3. 技术方案（候选，按优先级）

- **A. 每校正迭代重估 comp 项**：用最新 ψ/ρ 构造 `p_rghEqnComp1/2`
  的 `ddt/div` 显式项（或在 `correctRho` 后强制重估 psi）；
- **B. 显式质量通量投影**：压力解后解 Poisson 求势 `lambda`
  （`laplacian(D, lambda) = R`，零法向）并修正 `phi`，使
  `ddt(alpha1ρ1) + div(alphaRhoPhi1) = 0` 离散成立（保持边界通量）；
- **C. EOS 一致 `correctRho`**：分段/子迭代积分（Newton）替代
  `rho += ψ·dp`；
- 先 A（改动最小），A 不足再 B/C；每步用 `massBudget` 仪表验证。

## 3a. 首轮调查（2026-09-12）

- 上游核对：`compressibleTwoPhaseVoFMixture::correct()` 只更新混合物
  `rho_/nu_`，**不刷新相 psi**；`psi1/psi2` 在校正器入口取引用、整步
  恒定——压力方程与 `correctRho` 用的是同一 psi（口径自洽，与四项分解
  `dm≈ψ·dp` 一致）。因此拆分误差不在 stale psi，而在压缩项的
  显隐式处理（`correction(fvm::ddt(p_rgh))` + 显式 `ddt/div` 用步初
  rho）与通量插值（`ddtCorr`）的组合；
- **通量分项仪表（2026-09-12）**：新增 dry-run 记录边界
  `phi/alphaPhi1/alphaRhoPhi1` 积分。hpdiag（高压标定，基线无修正器）：
  - 充填早段（界面活跃）`alphaPhi1/phi = 1.09…1.15`（压缩通量），充填后
    段恢复 1.0000；**保压斜坡期再偏离至 0.91…1.04**，并与每步残差
    `dm+flux·dt`（±1e-7 量级）同步——边界 alpha 通量与体积通量的分裂
    是残差的直接关联项；
  - 逐 patch 日志（gate/vent）已加入 `massBudget`；
- **gate+vent 通量一致性实验（2026-09-12，阴性）**：把 gate 一致性修正
  扩展到 vent 边界后，高压标定的每步残差与 a/v 比值**逐位相同**
  （t=1.189 res=−8.205e-8、t=1.276 +6.115e-8；vent 早已封堵，无通量），
  且使熔接痕对称性 0.554%→0.854%；已**回退**为 gate-only（注释记录），
  诊断仪表保留。结论：残差不在边界通量，而在域内压力-密度耦合；
- 下一实验（建议顺序）：
  1. 在求解器内逐项记录压力方程的显式/隐式贡献与最终 phi 的通量积分
     （与 `massBudget` 同口径），定位哪一项与守恒不一致；
  2. 实现**显式质量通量投影**（选项 B）：压力解后解
     `laplacian(D, lambda) = R`（R 为离散质量残差，零法向势）并修正
     `phi`/`alphaPhi1`，使离散质量平衡精确成立；
  3. 仅在 1 的定位结果指向 EOS 非线性时才走选项 C。

## 4. 验收标准（DoD）

- `verify-highpressure.py`（Euler 口径）**无 `massFixGlobal`** PASS
  （<1e-3），且基线不 NaN；
- 与 `massFixGlobal` 结果对比：温度/压力/充填时间差 <1%；
- 模型测试 + 18 求解器用例 + 全套验证回归绿；CI 双架构绿。

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 改动压力方程影响所有 case 稳定性 | 新逻辑先做可选开关，小 case 扫参数 |
| 投影法的边界通量守恒 | 零法向势 + 通量记账（massBudget）逐 case 校验 |

## 6. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingFoam.{H,C}` | pressureCorrector 覆写/校正步 |
| `validation/highPressure/` | 去修正器验收（或双配置对照） |
| `scripts/verify-highpressure.py` | 双口径报告 |
| `ai-docs/tasks/018-*.md` | 结论更新 |
