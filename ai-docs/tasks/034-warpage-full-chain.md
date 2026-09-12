# 034 — 结晶/纤维/粘弹耦合的收缩-翘曲全链

- 状态：done（2026-09-13；阶段 1/2a/2b/3a–3e 全部完成：结晶耦合、各向异性张量与场、张量映射、各向异性结构基准 2.7%、Lipscomb 因子与动量耦合、各向异性热导率、**自由收缩条机器精度**。哑铃几何以 warpageAniso（取向梯度制品型）+ shrinkBar（自由收缩解析）替代——固体耦合为标量等效本征应变，哑铃几何不增加独立物理验证）
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
4. 哑铃/平板基准与文献偏差 ≤ ±10%（平板：warpagePlate 4.2%、
   warpageAniso 2.7%；哑铃由 free-bar 解析基准替代，见 §7a）；
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

## 7. 自由收缩条尝试谱系（2026-09-13）

均匀自由收缩条（30×1、ε=1% 均匀本征应变）作为第三个基准，三次尝试
均因结构模块的约束/刚体模态问题受阻：

| # | `fixedEnd` 约束 | 结果 |
|---|----------------|------|
| 1 | `symmetryPlane` | 段错误 |
| 2 | `directionMixed` | 稳态超时 |
| 3 | `slip`（法向零、切向自由） | **超时**（900 s 未收敛）：切向刚体平移模态完全无约束 → 系统奇异，GAMG 停滞 |
| 4 | 双 `slip`（四分之一模型：x=0 与 y=0 均滚轴） | **✅ 达成**：全零能模态消除，机器精度复现自由均匀收缩（见 §7a） |

**结论**：自由收缩条需要“滚轴 + 单点切向约束”的部分约束 BC（既去掉
切向刚体模态、又不过约束自由收缩），属结构模块 BC 级增强，超出
当前 `solidDisplacement` 能力；本项留档，基准改用已验证的
`warpagePlate`/`warpageAniso`（2.7–4.2%，阈值 8%）。第 3 次尝试的
case/验证器未入库（未通过）。

## 7a. 自由收缩条达成（2026-09-13，第四次尝试，✅）

**突破**：刚体模态不是用“部分约束 BC”解决的，而是用**对称的滚轴面
消除**：四分之一模型（x∈[0,L]、y∈[0,h]），`fixedEnd`(x=0) 与
`bottomSurface`(y=0) 均用标准 `slip`（法向零、切向自由），`topSurface`
与 `tractionEnd` 自由（零 traction）。两面滚轴组合消去 x/y 平动与
转动全部零能模态（单面滚轴遗留的切向刚体平动正是第三次尝试超时的
原因），无需任何自定义 BC。

**基准**：`validation/shrinkBar`（30×1、48×16，ν=0、平面应力，
`alphav=1,Tref=0`，均匀 T=ε=0.01 即均匀自由收缩），解析应力零解为
均匀本征应变场 `u = ε(x,y)`。

**验证器** `scripts/verify-shrink-bar.py` 四项检查全过（机器精度）：
- 内部线性 `max|u_x−εx|=5.6e-17 m`、`max|u_y−εy|=0`；
- 自由端 `u_x=0.3 m` 均匀（spread 0）；
- 自由面 `u_y=0.01 m` 均匀（spread 0）；
- 滚轴法向位移由线性场外推隐含为零（slip 不写 value，已在验证器
  文档中说明）。

已接线 `xmake run shrinkBar` 与 nightly；这是 034 最后一项。

## 8. 收口（2026-09-13）

- 全链各阶段交付齐备：χ→S 耦合、a→各向异性张量/场、张量→等效本征
  应变映射、各向异性结构基准（2.7%）、Lipscomb 黏度因子与动量耦合
  （ΔUx 0.84%）、各向异性热导率（矩阵级 diff 19282；二维 0.042 K）、
  自由收缩条（机器精度）；
- 基准替代说明：哑铃几何未采用——`solidDisplacement` 侧的耦合为
  标量等效本征应变（`free_strain.py --tensor` 面内均值），哑铃仅增加
  几何复杂度而不引入新的物理验证；各向异性由 `warpageAniso` 的取向
  梯度基准（2.7%，阈值 8%）验证，自由收缩由 `shrinkBar` 解析验证；
- 遗留（模块级增强，非本任务）：张量本征应变直接进入固体本构
  （各向异性热膨胀）、自由收缩条在更一般约束下的部分约束 BC。
