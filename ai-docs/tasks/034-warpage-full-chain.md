# 034 — 结晶/纤维/粘弹耦合的收缩-翘曲全链

- 状态：in-progress（阶段 1：结晶度-收缩耦合；阶段 2a：纤维各向异性收缩张量已完成；Lipscomb 黏度/结构各向异性映射待做）
- 优先级：P2
- 依赖：024（结晶）/025（纤维）/027（粘弹）/013b（结构映射 MVP）
- 预估规模：3–5 周

## 1. 背景

013b 已交付 PVT 自由应变映射与板基准；024 无 `ρ(χ)` 结晶收缩耦合、
025 为各向同性黏度（无 Lipscomb）、027 的本构未接结构、真实制品
映射缺失。全链（结晶/取向/粘弹 → 各向异性自由应变 → 翘曲）未打通。

## 2. 目标

1. `ρ(χ)` 结晶收缩修正（PVT 参考态随 χ）；
2. Lipscomb 各向异性黏度 `η(a, AR)` 与各向异性热导率（与 015/025 协同）；
3. 粘弹/结晶历史 → 结构自由应变的顺序耦合（应力松弛/各向异性收缩）；
4. 哑铃/平板基准与文献偏差 ≤ ±10%；
5. 缺省不影响现有流动求解。

## 3a. 阶段 1（2026-09-12，已完成：结晶度-收缩耦合）

- `moldingShrinkage` 新增 `crystallinityShrinkage`（缺省 0）：
  `S(rho, chi) = S(rho) + k_chi·chi`；求解器在存在 χ 场时自动使用；
- 模型测试：χ=0 与原行为一致、χ=1 增量 = k_chi（精确）、缺省 k_chi=0；
- 结晶用例集成：`tests/cases/crystallization` + `verify-crystallization.py`
  检查 ΔS ≥ 0.5·k_chi·Δχ——实测 **ΔS = 0.019838 = k_chi·Δχ
  （0.02×0.991923）精确一致**；模型测试与用例全绿。

## 3b. 阶段 2a（2026-09-13，已完成：纤维各向异性收缩）

- `moldingShrinkage` 新增 `orientationShrinkage`（β，缺省 0）与
  `anisotropicShrinkage(rho, chi, a)`：
  `eps = S/3·(I − β·(a − I/3))`——**迹守恒**（任意取向 tr(eps)=S），
  取向方向收缩小、横向大；
- 模型测试：a=diag(1,0,0)、S=0.05、β=0.5 → 对角
  (2/3, 7/6, 7/6)·e0、迹 0.05（rtol 1e-12）；β=0 → 各向同性；
## 3c. 阶段 2b（2026-09-13，已完成：张量场输出与回归）

- 求解器：a 场与 `orientationShrinkage != 0` 同时激活时创建并写出
  `shrinkageTensor`（volSymmTensorField，AUTO_WRITE），逐单元
  `anisotropicShrinkage(rho, chi, a)`；周期重置归零；
- `tests/cases/fiberOrientation` 启用 shrinkage（β=0.5）；
  `verify-fiber-orientation.py` 新增检查：末态
  `max|tr(tensor) − S| = 9.0e-9`，且最取向单元 `T_xx < T_yy`
  （取向方向收缩小）；18/18 求解器用例全绿；
- 待做（阶段 3）：结构端各向异性本征应变施加（`solidDisplacement`
  热应变为各向同性，需自定义源项或顺序映射工具）；Lipscomb 黏度
  `η(a, AR)`；哑铃/平板基准对拍。

## 3. 技术方案

- 结晶：自由体积/比容随 χ 的 PVT 混合（`rho(χ, p, T)`），先模型测试；
- 纤维：Lipscomb 封闭（a 的第四不变量）+ 热导各向异性（λ_∥/λ_⊥）；
- 粘弹：027 的 τ 历史 + 自由应变松弛（顺序耦合，单向）；
- 映射：扩展 `scripts/free_strain.py` 支持各向异性应变张量与张量场；
- 基准：哑铃（各向异性收缩）、平板（层间收缩差）。

## 4. 验收标准（DoD）

- 模型测试（ρ(χ)、Lipscomb、松弛）+ 验证器（哑铃/平板 ±10%）；
- 现有 case 不劣化；CI 双架构绿；
- 文档：全链数据流与限制。

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 材料参数（结晶收缩率、AR、粘弹谱）缺乏 | 文献缺省 + 敏感性分析 |
| 各向异性映射复杂度 | 先单向顺序耦合，标量等效起步 |

## 6. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingCrystallization.*` | ρ(χ) |
| `src/moldingFoam/moldingFiberOrientation.*` | Lipscomb/热导 |
| `scripts/free_strain.py` | 各向异性/张量映射 |
| `validation/warpageDumbbell/`（新） | 基准 |
