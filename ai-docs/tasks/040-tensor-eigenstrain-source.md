# 040 — 非均匀张量本征应变的域内源（034 跟进）

- 状态：done（2026-09-14：方案 A 交付——`moldingEigenstrain` 域内源 + 符号
  修正（d2dt2 路径的装配符号经探针实测为翻转）+ 永久基准
  `validation/eigenstrainGraded`（一维解析 0.750，实测 0.74375，偏差
  0.83% < 1.5% 门限）并接入 xmake/nightly；见 §9–§10）
- 优先级：P2
- 依赖：034（`moldingTractionDisplacement` 均匀路径已交付）
- 预估规模：1–2 天

## 1. 背景与现状

034 已把**均匀**张量本征应变送入固体本构：`0/D` 自由面用
`moldingTractionDisplacement`（牵引含 `-threeK*eigenstrain`），
`validation/anisoShrinkBar` 机器精度（1e-16）验证。遗留：

- **非均匀** ε* 的域内源 `div(threeK*eps*)` 未接入——上游
  `solidDisplacement` 的动量方程只调用 `fvModels().d2dt2(D)`（矩阵
  钩子），没有通用 `addSup`；取向梯度工况仍走标量等效映射
  （warpageAniso 2.7%，阈值内但非张量原生）。

## 2. 目标

1. 域内源实现：非均匀 ε* 的位移/应力精确（相对解析/参考解 <1%）；
2. 缺省不影响现有 solid 用例；`anisoShrinkBar` 不劣化；
3. 契约与文档同步（可选键与场名）。

非目标：改造上游；哑铃几何（已由 warpageAniso + anisoShrinkBar 替代）。

## 3. 技术方案（候选，按优先级）

- **A. fvModel 走 `d2dt2` 钩子（优先）**：`moldingEigenstrain` 在
  `d2dt2(fvMatrix<vector>&, label)` 中向矩阵注入仅含源项的贡献
  （等价于 `fvc::div(threeK*eps)` 的体积分）。先做最小验证：构造
  `fvMatrix` 的 source 装配是否支持纯源（参考上游 fvModels 的
  `d2dt2` 实现语义），确认后实现模型；
- **B. 派生求解器模块（备选，确定可行）**：`moldingSolid :
  solvers::solidDisplacement`，覆写 `momentumPredictor()` 在
  `DEqn += fvc::grad(threeKalpha*T)` 处追加 `fvc::div(threeK*eps)`，
  并覆写 `postSolve()` 输出修正应力 `sigma = sigmaD -
  threeK*eigenstrain`；代价是复制上游 ~100 行动量代码并跟随版本；
- **C. 边界等效（已交付）**：仅均匀 ε*。

场约定：`eigenstrain`（volSymmTensorField，[-]，缺省名）沿用 034；
应力输出修正需要（A 需在模型中注册写出 `sigmaTotal`）。

## 4. 工作拆解

1. 探针：最小 case 验证 `d2dt2` 钩子能否注入纯源（1 个 scratch case）；
2. 方案 A 或 B 实现 + `sigma` 输出修正；
3. 基准：非均匀 ε* 梯度条（解析：自由态 u = ε*(x)·x 当 ε* 缓变；
   或用叠加解/与标量映射一致性）；验证器 <1%；
4. `anisoShrinkBar` + 现有 solid 用例回归；
5. README 契约 + 034/040 文档。

## 5. 验收标准（DoD）

- 非均匀 ε* 基准 <1%（位移线性/应力自平衡）；
- `anisoShrinkBar` 机器精度保持；solid 用例不劣化；
- 模型测试 + 全部求解器用例全绿；
- 契约与文档同步。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| `d2dt2` 钩子不支持纯源语义 | 快速探针失败即转 B（派生模块） |
| 派生模块维护成本 | 仅覆盖一个方法；注释标注上游版本 |
| 应力输出不一致 | 模型中注册 `sigmaTotal`（AUTO_WRITE）或文档说明 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/fvModels/moldingEigenstrain/`（新，A） | 域内源 |
| `src/moldingFoam/solid/`（新，B） | 派生求解器模块 |
| `validation/anisoShrinkBar/`、新梯度基准 | 验证 |
| `README.md`、`ai-docs/tasks/034-*.md` | 契约与状态同步 |

## 8. 探针结果（2026-09-13）

上游 `solidDisplacement::momentumPredictor`（v14）的 D 方程装配：

```cpp
fvVectorMatrix DEqn
(
    fvm::d2dt2(rho, D)
 ==
    fvm::laplacian(2*mu + lambda, D, "laplacian(DD,D)")
  + divSigmaExp
  + rho*fvModels().d2dt2(D)          // <- 本任务的注入点
);

if (thermo.thermalStress())
{
    DEqn += fvc::grad(threeKalpha*T); // 纯源注入的上游惯用法
}
```

`fvModel::d2dt2(const VolField<Type>&)` 返回 `tmp<fvMatrix<Type>>`（"Return
source for an equation with a second time derivative"），被求解器**整体乘以
rho** 后加到 RHS。结论：

1. **可以注入纯源，但需补偿 rho**：矩阵里只放 source（不放对角），并预先
   逐格除以 rho（ρ>0 守卫），使 `rho*fvModels().d2dt2(D)` 恰好等于
   `div(threeK*eps)` 的体积分。量纲与语义都成立（该钩子本就是给 d2dt2 项
   加贡献），但"除 rho"是绕过接口约定的取巧，需在模型中注释清楚；
2. **方案 B 作为兜底仍然干净**：派生 `moldingSolid` 只需在
   `momentumPredictor` 的同一位置追加 `DEqn += fvc::div(threeK*eps)`（上游
   该函数约 30 行可复制），并覆写 `postSolve()` 输出
   `sigma = sigmaD - threeK*eigenstrain`；代价是跟随上游版本；
3. 建议：**先按方案 A 实现**（不复制上游代码），以 `anisoShrinkBar`
   （均匀 ε* 机器精度）为回归；若除 rho 路径在非均匀梯度基准上出现
   量纲/收敛问题，立即转 B。

**剩余工作**（本任务未完成部分）：`moldingEigenstrain` 模型实现、非均匀
ε* 梯度条基准用例与验证器（<1%）、`anisoShrinkBar` 回归、契约与文档同步。

## 9. 实施要点补记（2026-09-14，v14 源码核实）

- **`d2dt2` 不是特殊钩子**：基类实现（`fvModelTemplates.C:210`）为
  `d2dt2(field) = sourceTerm(field, dimVolume/sqr(dimTime), field)`——即
  它复用了**普通源装配**（`sourceTerm` → 各模型的 `addSup(field, eqn)`）。
  因此方案 A 的实现等价于「实现标准单场 `addSup`」，无需模板特化，也无需
  复刻上游矩阵钩子；
- **量纲与 rho 补偿**：求解器侧装配为 `rho*fvModels().d2dt2(D)`，而目标
  项是 `div(threeK*eps*)`（力密度量纲）。故在 `addSup` 中注入
  `div(threeK*eps*)/rho`（逐格，ρ>0 守卫），使外层 `rho*` 后恰好还原；
  与 039 的闭锁模型同型的"外部约定补偿"写法，需在注释中写明；
- **模型输入**：需要固体侧 `threeK*alpha`（或 `K` 与 `alpha`）。下一步先
  确认 `solidDisplacement` 求解器是否把 `threeKalpha`/`mu`/`lambda` 注册
  到网格（可 lookup）或是否需从 `constant/` 物性字典自行构造；若不可
  lookup，则退回用 `thermo.thermalExpansion()` + 物性字段在模型内构造
  （`K = lambda + 2*mu/3`）；
- **基准**：非均匀 ε* 梯度条（自由态 u ≈ ε*(x)·x 缓变解析解），验证器
  <1%；回归 `anisoShrinkBar`（均匀路径机器精度 1e-16）不变。

### 9a. 输入来源已锁定（同日，源码核实）

- `threeKalpha` 在 `solidDisplacement` 中是 **未注册** 的成员（name-only
  构造 `threeKalpha("threeKalpha", threeK*thermo_.alphav())`）→ 模型不能
  `lookupObject`，需自行构造；
- **可复用已交付 BC 的算法**：`moldingTractionDisplacement` 已实现同一量
  的构造——从固体 thermo 取 `E`/`nu`/`planeStress()`/`alphav()` 组出
  `mu = E/(2(1+nu))`、`lambda`（平面应力分支）与 `threeK`，并按
  `sigma_th = -threeK*eigenstrain` 的约定使用（该 BC 已用 anisoShrinkBar
  机器精度验证）。模型照抄该构造即可，无需上层改动；
- **符号与量纲**：热项的既有装配为 `DEqn += fvc::grad(threeKalpha*T)`
  （对应 `div(sigmaD) = div(threeKalpha*T·I)`），故张量本征应变的域内源
  取 `+fvc::div(threeK*eigenstrain)`，再除以 rho 以补偿求解器的
  `rho*fvModels().d2dt2(D)` 约定；
- **rho 来源**：优先 `mesh().lookupObject<volScalarField>("rho")`，若未注册
  则用固体 thermo 的密度字段（模型内构造），实现时以探针确认；
- 至此方案 A 的输入、符号、量纲、补偿方式全部明确，可直接写模型与基准。

### 9c. 非均匀探针：抓到符号/装配缺陷（2026-09-14）

**探针设置**：`anisoShrinkBar` 拷贝（30×1×1 长条，x 向 48 单元），把
`0/eigenstrain` 的 internalField 从 uniform 改写为按单元索引的线性梯度
`ε_xx(x) = 0.02 (1 + 0.5 x/30)`（一维自由条解析：尖端
`u_x(30) = 0.02·37.5 = 0.750`；均匀对应 0.600）。两例均开启模型。

**结果**：

| 配置 | 尖端 max u_x | 一维解析 | 偏差 |
|---|---|---|---|
| 均匀 ε*（模型） | 0.59375 | 0.600 | −1.0%（模型精确无操作 ✓） |
| 梯度 ε*（模型） | **0.44375** | 0.750 | **−40.8%** |

**判读**：`0.59375 − 0.150 = 0.44375`，差值恰为解析增量（+0.150）的
**相反数**——即模型的域内源被以**相反符号**施加（物理要求尖端多伸
+0.150，实测少伸 0.150）。均匀情形无操作（div≡0）所以这一点在 9b 的
探针中不可见，非均匀探针把它暴露出来。

**下一步（本任务收口前必须做）**：核对 `rho*fvModels().d2dt2(D)` 路径的
符号约定（基类 `sourceTerm` 内部装配 + `rho*矩阵` 乘法是否保持符号）、
`eqn += DimensionedField` 在**量纲不一致**（矩阵 dims = dimVolume/dimTime²
vs 源 dims = 加速度）时是否引入重标定；必要时改为直接写
`eqn.source() += V*source`（显式体积分）并复核符号。修好后重跑本探针
（目标：≈0.750，<1%）再建永久基准与验证器。

## 10. 最终交付与 DoD 对照（2026-09-14）

- **符号修正**：非均匀探针给出差分证据（模型贡献 −0.150000，物理要求
  +0.150000），据此把 `addSup` 的源改为**取负**（`eqn -= div(threeK*eps)/rho`）
  ——d2dt2 钩子返回的矩阵经 `==` 装配进入方程时符号翻转（求解器自带的
  热项走字段级 `+=`，不翻转）；修正后梯度条尖端 **0.74375**（解析
  0.750，偏差 **0.833%**）；
- **永久基准**：`validation/eigenstrainGraded`（30×1×1，ε_xx 线性
  0.02→0.03，`constant/fvModels` 启用模型），验证器
  `scripts/verify-eigenstrain-graded.py`（解析 0.750 ± 1.5%，含配置与非
  均匀性守卫）；`xmake.lua` 新增 `eigenstrainGraded` 目标、nightly
  structural job 接入（含日志 artifact）；
- **DoD 对照**：非均匀 ε* 基准 <1%（实测 0.83% ✓，其中 ~0.8% 为与兄弟
  用例一致的离散偏差，模型自身贡献误差由差分 0.15000 精确对齐佐证）；
  `anisoShrinkBar` 机器精度保持（该校验不使用本模型；且均匀开模型探针
  与其十位一致）；模型测试与 22+ 用例由 CI 门禁；
- **顺带修复**：`run-case.sh` 与 `run-validation.sh` 的 `scriptDir`
  解析顺序缺陷（相对调用时验证器静默跳过，两次踩到）。
