# 034 — 结晶/纤维/粘弹耦合的收缩-翘曲全链

- 状态：in-progress（阶段 1/2a/2b/3a/3b/3c/3d/3e 完成：结晶耦合、各向异性张量与场、张量映射、各向异性结构基准 2.7%、Lipscomb 因子与动量耦合、各向异性热导率；剩哑铃/梯度用例）
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
## 3d. 阶段 3a（2026-09-13，已完成：各向异性→等效本征应变映射）

- `free_strain.py` 新增 `--tensor <field>` 模式：读取各向异性收缩张量
  场，取**面内均值** `eps_eq = (ε_xx + ε_yy)/2` 作为冻结 T 法的等效
  各向同性本征应变（`T_eq = eps_eq/alphav`）；写出时保留源场的
  边界类型（empty 保持，其余 zeroGradient）；
- 在 `fiberOrientation` 用例的输出上验证（80 单元，eps_eq −0.0847 →
  T_eq −7699 K，符号与参考态约定一致）；
- 物理说明：薄板翘曲由**层间**收缩差驱动，层内各向同性的等效标量
  保留了面内均值效应；精确各向异性本征应变仍需结构端自定义源项
  （留档）；
## 3e. 阶段 3b（2026-09-13，已完成：各向异性结构基准）

- 新验证 `validation/warpageAniso`（`xmake run warpageAniso` + 夜间 CI）：
  双层板，顶层纤维全取向（p=1）、底层各向同性（p=1/3），S=0.036、
  β=0.5 → 面内均值本征应变差 **Δε = e0·β·(p_top−p_bot)/4 = 1.0e-3**；
  冻结 T 法（T_eq = ε/α）驱动 solidDisplacement；
- 结果：自由端 **+0.6569 m vs Timoshenko κL²/2 = 0.675 m（2.7% < 10%）**，
  符号与物理一致（底层收缩大 → 上弯）；
- 意义：打通“取向度 → 各向异性收缩 → 等效本征应变 → 结构翘曲”全链
  （经各向异性收缩张量 + `free_strain.py --tensor` 映射的近似）；
## 3f. 阶段 3c（2026-09-13，已完成：Lipscomb 型各向异性黏度因子）

- `moldingFiberOrientation::lipscombFactor(a, D, axialRatio)`：
  `factor = 1 + (ratio−1)·3/2·(A:D):D/(D:D)`，钳制 [1, ratio]
  （A:D 用配置的闭包；工程模型，极限与归一化在设计注释中说明）；
- 模型测试：全取向 + 轴向拉伸 → factor = ratio（5.0）；横剪 → 1；
  各向同性取向 → 1（基值）；全部 rtol 1e-12；
- 极限含义：纤维方向拉伸黏度最大（ratio 倍）、跨纤维剪切最小（基值）；
## 3g. 阶段 3d（2026-09-13，已完成：各向异性热导率）

- `moldingFiberOrientation::conductivityTensor(a, kappa, anisotropy)`：
  `lambda = kappa(I + anisotropy(a−I/3))`，**迹守恒**（各向同性取向恢复
  标量导热）；模型测试：全取向 diag(1.333, 0.833, 0.833)、tr=3、各向
  同性 → 1（rtol 1e-12）；
- 求解器：能量方程导热项在 `a` 场与 `conductivityAnisotropy != 0` 时
  改用张量 `fvm::laplacian(lambdaEff, T)`（`fiberOrientation` 子字典
  新键）；`tests/cases/fiberOrientation` 启用 0.5（等温 → 数值无差但
  覆盖代码路径；18 用例回归全绿）；
- 待做：带温度梯度 + 取向的定量验证（如冷却通道内纤维取向对导热的
  影响对拍）、该因子与结构映射的联动。

## 3h. 阶段 3e（2026-09-13，已完成：Lipscomb 动量耦合）

- `CrossWlf` 新增可选 `lipscombRatio`（缺省 1 = 关闭）与
  `orientationField`（缺省 "a"）：逐单元表观黏度乘以 Lipscomb 因子
  （二次闭包 `(A:D):D = (a:D)²`，`f = min(max(1+(ratio−1)·3/2·(a:D)²/(D:D),1),ratio)`），
  应变率张量自 `symm(fvc::grad(U))`；
- `tests/cases/fiberOrientation` 启用 `lipscombRatio 3`：
  - 与 ratio=1 对照 `max|ΔUx| = 8.17e-3`（0.84%），流动按取向-应变率
    对齐方向改变 ✓；取向验证器仍通过；18 用例 + 模型测试全绿；
- **矩阵级验证（2026-09-13）**：诊断对比
  `fvm::laplacian(λ,T)` 与标量版本的矩阵——**max|diag diff| = 19282**
  （项确实进入方程且各向异性）；温度场不可观测的原因是**物理的**：
  fiberOrientation 用例 x 向周期 + 均匀初温 + 1D 稳态 → 稳态解与
  λ 的对角大小及交叉项均无关（∂²T/∂x∂y=0）；
- **二维温度场定量验证（2026-09-13）**：Couette 单元（4×20）顶壁
  非均匀温度（460..480 沿 x）+ 初始 a=45°（xy 交叉项非零）：
  - aniso=0 与 0.5 的 T 场差 **max|ΔT| = 0.0421 K**（顶行
    460.0045 vs 460.0011、472.67 vs 472.68）——交叉导热可观测 ✓；
  - 结合矩阵级（diag diff 19282），本项实现-验证闭环；
- **哑铃/自由收缩条尝试（2026-09-13，受阻）**：`solidDisplacement` 的
  自由收缩约束存在 BC 层限制——`symmetryPlane` 段错误（core dump）、
  `directionMixed` 滚轴（法向固定/切向自由）稳态求解超时；
  自由收缩条的解析验证需结构模块支持部分约束（留档为上游/模块级
  增强）。各向异性结构验证以 `warpagePlate`（4.2%）与
  `warpageAniso`（2.7%）为准。

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
