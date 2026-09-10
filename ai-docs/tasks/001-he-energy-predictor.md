# 001 — he 型能量预报器（启用潜热）——完整研发提示词

> **给 AI 开发会话的完整上下文与执行指令**。按本文档逐步实现，
> 不要跳步，不要发明文档未描述的额外功能。

---

## 1. 一句话目标

在 moldingFoam 求解器模块内覆写 `thermophysicalPredictor()`，
将能量方程从 **T 矩阵**（温度为隐式变量，Cv 作对角系数）改为
**he 矩阵**（显能为隐式变量，对角 = α·ρ/dt 恒正），从而使
hMeltThermo 的表观 Cp 潜热峰（`latentHeat` 关键字）可以在
`compressibleVoF` 框架内**稳定运行**。

## 2. 背景：为什么 T 矩阵 + 潜热会发散

### 2.1 发散机理（已实验确证）

`compressibleVoF::thermophysicalPredictor()` 构造的能量矩阵对角
系数 = 每相的 `Cv = Cp − CpMCv`。Tait EOS 的 `CpMCv = T·α²/ψ`
在凝固带（`Tt(p) ± 0.5 K`）内：

- `α`（热膨胀系数）含前沿扫掠项 `wt·(vm − vs)/v`
- `ψ`（等温压缩率）含前沿扫掠项 `wp·(vm − vs)/v`
- 比值 `α²/ψ ∝ wt²·Δv/(wp·v)` — 因为 `wt/wp = 1/b6` 且 b6 极小
  (1.543e-7 K/Pa)，此比值可达 1e5–1e6

导致 `Cv = Cp_app − CpMCv ≈ 3e5 − 6e5 = −3e5` < 0（带内深部）。
T 矩阵不定 → smoothSolver 输出发散解 → `correctThermo` 的 Newton
得到负 T → FatalError。

### 2.2 已否决的修复尝试

| 方案 | 失败原因 |
|------|----------|
| 修正项系数取 `max(Cv, Cp)` | 破坏 `correction()` 拆分的雅可比一致性（隐式系数 Cp ≠ 显式系数 Cv → 求解的是错误方程），解仍发散 |
| `SuSp(ρ·latentCp/dt, T)` 追加对角 | 带内深部 `CpMCv` 的负贡献 ≈ −6e5 超过潜热峰 +3e5，对角仍负 |
| `limitTemperature` fvConstraint | 与 compressibleVoF 的双 thermo 结构不兼容：`phase melt` 使约束绑定不存在的 `T.melt` 字段；不设 phase 则查找不存在的 `physicalProperties` thermo |
| 潜热 = 0（当前状态） | 稳定但不含潜热物理 |

### 2.3 物理本质

带内等容响应 `Cv < 0` 是**真实的物理**：固定体积的单元升温 → 熔化
→ 比容跳升 → 压力剧增 → `Tt(p)` 上升 → 前沿回退 → 有效温度反而
下降。但注塑成型的腔体**不是等容的**（有排气和自由边界），等容
假设在这里不成立。问题出在 EOS 的等容恒等式与求解器的 T 变量
不匹配，而不是物理错误。

## 3. 解决方案：he 型能量预报器

### 3.1 核心思路

用**每相显能 `ei` 作为隐式变量**替代 T。矩阵对角 = `α·ρ/dt`
（恒正），潜热完全包含在 `he(T)` 的非线性中（通过 hMeltThermo
的表观 Cp），不存在负对角问题。

### 3.2 方程

对每相 i ∈ {1(melt), 2(air)}：

```
fvm::ddt(αi, ρi, ei) + fvm::div(αρφi, ei) − fvm::Sp(contErri, ei)
− fvm::laplacian(κeff/Cpi_eff, ei)        ← 传导（用 αEff = κ/Cp 形式）
+ pressure-work / KE 项                    ← 从上游搬运
== fvModels 源项
```

其中 `Cpi_eff = thermo_i.Cp()`（**表观 Cp 含潜热峰**，恒正）。
传导项写作 `κ/Cp × ∇e` 的近似（OpenFOAM 标准做法，如
rhoPimpleFoam 的 `−fvm::laplacian(alphaEff, he)`）。

### 3.3 T 恢复

矩阵求解后，T 由 `species::thermo::Th(he, p, T0)` Newton 反演：
`de/dT = Cv_eff`。**关键**：hMeltThermo 必须覆写 `Cv` 为
**正的表观值** `Cp_app − small`（而非热力学恒等式 `Cp − CpMCv`
的负值），使 Newton 斜率恒正。

## 4. 实现步骤（按顺序执行，每步编译验证）

### 步骤 1：hMeltThermo 添加稳定 Cv

文件：`src/thermo/hMeltThermoI.H`

在类中添加（或修改已有的）：

```cpp
//- Return heat capacity at constant volume [J/kg/K]
//  Uses the apparent Cp (positive everywhere including the
//  latent-heat peak) rather than the thermodynamic identity
//  Cp - T*alpha^2/psi (which gives negative Cv inside the band)
inline scalar Cv(scalar p, scalar T) const
{
    return Cp(p, T);  // Cv ≈ Cp for condensed phases
}
```

如果 `hMeltThermoI.H` 中已有 `Cv()`（检查：搜索 `Cv(p, T)`），
修改为返回 `Cp(p, T)`。

### 步骤 2：moldingFoam 覆写 thermophysicalPredictor

文件：`src/moldingFoam/moldingFoam.C`

在 `preSolve()` 之后添加：

```cpp
void Foam::solvers::moldingFoam::thermophysicalPredictor()
{
    // he 型能量预报器：以每相显能为隐式变量（对角恒正），
    // 传导用 alphaEff·∇e 形式（近似 κ∇T），压力功/KE 显式
    const volScalarField& rho1(mixture_.rho1());
    const volScalarField& rho2(mixture_.rho2());
    const volScalarField& e1(mixture_.thermo1().he());
    const volScalarField& e2(mixture_.thermo2().he());

    const fvScalarMatrix e1Source(fvModels().source(alpha1, rho1, e1));
    const fvScalarMatrix e2Source(fvModels().source(alpha2, rho2, e2));

    volScalarField& T = mixture_.T();

    // 表观 Cp（含潜热峰）用于 alphaEff = kappaEff/Cp
    const volScalarField Cp1(mixture_.thermo1().Cp());
    const volScalarField Cp2(mixture_.thermo2().Cp());

    // 传导系数 α = κ/Cp（边界场）
    const volScalarField::Boundary& kappaBf =
        thermophysicalTransport.kappaEff()().boundaryField();
    // ...

    fvScalarMatrix e1Eqn(
        fvm::ddt(alpha1, rho1, e1) + fvm::div(alphaRhoPhi1, e1)
      - fvm::Sp(contErr1(), e1)
      - fvm::laplacian(kappaEff/Cp1_interp, e1)
      + pressure work terms...
      == fvModels source
    );
    // 同理 e2Eqn
}
```

**关键**：`fvm::laplacian` 的系数是 `kappaEff/Cp`（surfaceField），
`e1` 是被输运的场。Conduction 项写成 `−laplacian(κ/Cp, e)` 而非
`−laplacian(κ, T)`，与 rhoPimpleFoam 的做法一致。

### 步骤 3：编译 + 模型测试

```bash
cd ~/moldingFoam-build && xmake && xmake run test
```

### 步骤 4：契约 case（latentHeat 2e5）

```bash
# case-contract/constant/physicalProperties.melt 设 latentHeat 2e5
MOLDINGFOAM_PARALLEL=4 xmake run case-contract
```

### 步骤 5：物理验证

- 顶出时刻比 `latentHeat 0` 基线**晚** ≥ 5%（潜热生效的证据）
- 质量守恒 < 1e-3
- 带内 T 场连续无跳变

## 5. 已知陷阱（所有都在本轮实测踩过）

1. **不要用 correction(Cv·...) + max(Cv,Cp)**：破坏雅可比一致性
2. **不要用 SuSp(ρ·latentCp/dt, T)**：对角抬升不足（CpMCv 的
   前沿项 6e5 > 潜热峰 3e5）
3. **不要用 limitTemperature fvConstraint**：与双 thermo 不兼容
   （`phase melt` 绑定不存在的 `T.melt`；不设 phase 则查找
   不存在的 `physicalProperties`）
4. **hMeltThermo 的 CpMCv**：如果不覆写，继承 Tait::CpMCv
   的前沿项值（≈6e5），导致 Cv 负。**必须覆写**（返回 0 或
   正的小值）
5. **wmake rules 的 `-mcpu=native`**：在 Arm64 上会被 shim
   剥离（见 moldingFoam.C 的 on_build），防止 CI runner
   CPU 特性编入产物
6. **根作用域闭包**：xmake.lua 中根作用域闭包无 execv/raise，
   必须放在 target 回调体内

## 6. 文件清单

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingFoam.C` | thermophysicalPredictor() 覆写 |
| `src/moldingFoam/moldingFoam.H` | 声明 |
| `src/thermo/hMeltThermoI.H` | Cv() 返回正的表观值 |
| `case-contract/constant/physicalProperties.melt` | latentHeat 2e5 |
| README §6 | 潜热描述更新 |
