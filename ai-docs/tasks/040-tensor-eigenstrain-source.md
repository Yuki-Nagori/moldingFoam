# 040 — 非均匀张量本征应变的域内源（034 跟进）

- 状态：planned
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
