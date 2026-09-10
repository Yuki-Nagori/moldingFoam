# 002 — 模具热耦合（集总参数模温模型）

- 状态：done（2026-09-10。验收：`0/T` 模壁改为 `moldingMoldTemperature`
  后绝热/强冷方向正确、离散能量守恒恒等式 PASS、modelTests 全部 PASS、
  契约 case 全项 PASS）
- 优先级：P2
- 依赖：建议在 001 之后（已满足）
- 预估规模：3–5 天

## 1. 背景与现状

契约 case 的模壁温度是恒定 `fixedValue 353 K`（`case-contract/0/T` 的
walls patch，即"模温恒定、模具无限大热沉"假设）。真实注塑中：

- 模具吸收熔体热量，模壁温度在周期内上升数 K 至数十 K；
- 模温直接影响近壁冻结层生长、压力传导、顶出时间与制品表面质量；
- 模温由冷却水路（循环水 + 比例阀）控制。

当前模型下冷却段顶出时间系统性偏短（模壁永远恒温吸热不受限）。

## 2. 目标与非目标

### 目标
1. 模壁温度随时间变化：由铸件跨壁面导热与冷却水换热共同决定；
2. 契约字典可选启用（缺省 `fixedValue` = 现有恒温行为，向后兼容）；
3. 顶出时间对模温参数的响应方向合理；
4. 离散能量守恒、无条件稳定，且与能量方程的时间离散一致。

### 非目标
- 模具三维温度场（共轭传热 CHT）：需要多区域网格与上游 cht 工具链；
- 冷却水路几何建模（只做集总换热系数）；
- 多周期模温持久化（单周期 + 重启续读已够，见 3.3）。

## 3. 技术方案

### 3.1 集总状态方程

模壁温度 `T` 为单一（每 patch 一个）热容状态，`C = heatCapacity`
[J/K]：

```
C dT/dt = Σ_f h_f (T_cell,f − T) + h_A (T_water − T) + Q
```

- `h_f = kappaEff·deltaCoeffs·magSf`：铸件侧面导热系数，与能量方程
  `fvm::laplacian` 的隐式边界系数**完全同源**；
- `h_A = waterHTC·wettedArea` [W/K]：冷却水侧等效换热；
- `Q`：可选 `Function1` 功率源 [W]（与上游
  `lumpedMassTemperature` 一致）。

### 3.2 后向 Euler 离散与实现载体

更新式（与能量方程的 Euler 时间离散一致）：

```
T^{n+1} = (C/dt·T^n + Σ_f h_f T_cell + h_A T_water + Q)
        / (C/dt + Σ_f h_f + h_A)
```

- 无条件稳定：`C/dt` 与 `h_A` 都出现在分母，任意 `dt` 不越界；
- 离散能量守恒严格成立：
  `C(T^{n+1}−T^n) = dt·(Σ h_f(T_cell−T^{n+1}) + h_A(T_water−T^{n+1}) + Q)`；
- 更新式实现为纯函数 `moldThermalState::Tnew(...)`（便于单测），
  由边界条件在每次矩阵装配时**始终从 `UniformDimensionedField`
  的 `oldTime` 值**求值，因此每个外迭代精化耦合、但状态每步只推进
  一次（幂等）。

实现载体选择**边界条件**而非求解器侧显式耦合：后者需要先在
`preSolve` 用旧热流推进模温、再求解能量方程，当 `dt > C/(Σ h_f)`
时（高导热/小热容）显式耦合不稳定且破坏离散能量守恒。边界条件
方案（结构参照上游 `lumpedMassTemperatureFvPatchScalarField`）把
`C/dt`、`h_f`、`h_A` 全部放入隐式分母，稳定且守恒。

### 3.3 持久化

模温存于 `UniformDimensionedField<scalar> T_`，随 `write()` 写入边界
字典的 `T` 项；重启时构造函数从 `T` 读取，续跑不丢状态。

## 4. 工作拆解

1. `src/moldingFoam/moldThermalState.{H,C}`：更新律纯函数；
2. `src/moldingFoam/boundaryConditions/moldingMoldTemperature/
   moldingMoldTemperatureFvPatchScalarField.{H,C}`：自注册边界
   （`fixedValue` 派生、拷贝族、`updateCoeffs`、`write`）；
3. `src/Make/files`：新源文件；
4. `tests/modelTests.C`：离散能量守恒、稳态、极限与守恒测试；
5. 契约 case 验证：缺省回归、绝热、强冷三组；
6. README §5–§8 更新（契约 v1.3）。

## 5. 验收标准（DoD）

- 模壁保持 `fixedValue`（缺省）：行为与当前基线一致（回归）；
- 绝热模参数（`waterHTC 0`）下模温受铸件热流上升；
- 强冷参数下模温向水温回落；
- modelTests 新增项 PASS；
- CI 双架构绿。

验收记录：

- `xmake run test`：全部 PASS（含离散能量守恒恒等式 rtol 1e-12）；
- 缺省契约 case：全项 PASS（见 001 验收记录）；
- 绝热/强冷变体：见"验收摘要"（`/tmp/case-mold-adia`、
  `/tmp/case-mold-cool`）；
- 提交哈希：待提交（随本报告一并提交后回填）。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 边界条件构造/拷贝的 `autoPtr<Function1>` 所有权 | 拷贝构造深拷贝 `Q_`（`clone().ptr()`），避免源对象被置空导致 `write`/`decomposePar` 崩溃 |
| 每个 patch 独立集总量，多 patch 时模温可以不同 | 对单腔小模具适用；多 patch 一致模温可后续加"共享状态"选项 |
| 高导热 `h_f` 下时间步不受限但精度仍是一阶 | 与能量方程 Euler 离散同阶；需要更高阶时可换 Crank-Nicolson |
| 多周期模温持久化 | 单周期 + 重启续读已实现；多周期待需求明确 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldThermalState.{H,C}` | 新增：后向 Euler 更新律 |
| `src/moldingFoam/boundaryConditions/moldingMoldTemperature/*` | 新增：集总模温边界 |
| `src/Make/files` | 新源文件 |
| `tests/modelTests.C` | 模温更新律测试 |
| `README.md` | §5–§8 更新，契约日志 v1.3 |
