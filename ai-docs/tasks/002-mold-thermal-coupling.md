# 002 — 模具热耦合（集总参数模温模型）

- 状态：planned
- 优先级：P2
- 依赖：建议在任务 001 之后（都作用于能量预报器，避免并行改动冲突）
- 预估规模：3–5 天

## 1. 背景与现状

契约 case 的模壁温度是恒定 `fixedValue 353 K`（`case-contract/0/T`
的 walls patch，即"模温恒定、模具无限大热沉"假设）。真实注塑中：

- 模具吸收熔体热量，模壁温度在周期内上升数 K 至数十 K；
- 模温直接影响：近壁冻结层生长、压力传导、顶出时间、制品表面质量；
- 模温由冷却水路（循环水 + 比例阀）控制。

当前模型下冷却段顶出时间系统性**偏短**（模壁永远恒温吸热不受限）。

## 2. 目标与非目标

### 目标
1. 模壁温度随时间变化：由壁面热流与冷却水换热共同决定，支持
   "模温先升后受控回落"的典型周期；
2. 契约字典增加可选的模具热参数组（缺省关闭 = 现有恒溫行为）；
3. 顶出时间对模温参数的响应合理（模温升高 → 顶出变慢，反之变快）。

### 非目标
- 模具三维温度场（共轭传热 CHT）——需要多区域网格与上游 cht 工具链，
  工程价值/成本比低；
- 冷却水路几何建模（只做集总换热系数）。

## 3. 技术方案（集总参数 / lumped 模型）

模壁温度 `T_mold` 作为单一状态量（或每 patch 一个），状态方程：

```
C_m · dT_mold/dt = Σ_patches q″_i·A_i − h_water·A_w·(T_mold − T_water)
```

- `C_m = m_mold·c_mold`：模具等效热容（字典参数，默认按钢 + 常用
  模板尺寸给一组合理缺省）；
- `q″_i`：壁面热流，由 `thermophysicalTransport` 的壁面热流场或
  `kappaEff·grad(T)` 插值获得（OpenFOAM 壁面热流 utilities 的标准
  做法）；
- `h_water·A_w`：冷却水侧等效换热（字典参数，缺省关断：`h_water 0`
  时模具绝热升温，现有行为= `C_m → ∞` 恒温）；
- `T_mold` 持久化：参照 `moldingStage` 的 regIOobject 模式
  （`src/moldingFoam/moldingStage.C`），多周期连跑时模温从上一周期
  结束值继续。

### 实现载体选择

| 载体 | 优点 | 缺点 |
|------|------|------|
| `moldingFoam` 内覆写（能量预报器后置钩子 / preSolve+postSolve） | 无新类；与 001 的 he 预报器同处一文件，共享 T/κ 访问 | 功能边界不如独立类清晰 |
| 独立 fvModel（自注册） | 上游标准的源项机制；`fvModels` 字典驱动；契约 case 可选启用 | 需要拿到壁面热流与模温状态的接口 |

建议：先做 fvModel 原型（对 T 场加 `fvm::Sp`/显式源），不行再退回
覆写内实现。注意与 001 的 he 预报器配合：模温变化体现在 T 的边界
条件上，he 方程的传导/边界项自动响应，无需改 he 方程本身。

## 4. 工作拆解

1. 参数与字典设计：`constant/moldingDict` 新增可选 `mold` 组
   （`heatCapacity`、`waterHeatTransferCoefficient`、`waterTemperature`、
   `wettedArea`；缺省关闭）→ 契约变更日志 v1.3/v1.4；
2. 状态对象：`moldThermalState`（regIOobject，参照 moldingStage 的
   writeData/readData 模式）持久化 `T_mold`；
3. 热流获取：壁面 `q″` 的计算与单元测试（构造解析温度场验证
   `q″ = −κ·grad(T)|w`）；
4. 时间推进集成：`preSolve()` 中以 `runTime.deltaT()` 更新 `T_mold`，
   写回 walls patch 的 T 边界场（fixedValue 值替换）；
5. 契约 case 参数化验证：两组模温参数（绝热 vs 强冷却）各跑全周期，
   顶出时刻单调响应；
6. modelTests：`T_mold` 一阶常微分方程解析解对拍（无熔体热流项时
   指数趋近水温）；
7. 文档：README 第 6 节 + 第 8 节变更日志 + 本文件状态。

## 5. 验收标准（DoD）

- `latentHeat 2e5` + 模温模型关闭：行为与当前基线完全一致（回归）；
- 绝热模参数下模温单调上升且顶出变慢；强冷参数下模温趋近水温；
- modelTests 新增项 PASS；
- CI 双架构绿。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 壁面热流计算依赖 thermophysicalTransport 内部接口 | 退化为 `kappaEff·mag(grad(T))|w` 直接计算（物理一致，精度足够） |
| 与任务 001 的 he 预报器集成顺序 | 本任务排在 001 后合入；模温作为 T 的 Dirichlet 值，与 he 变量解耦 |
| 多周期状态持久化需求不明确 | 先实现单周期；regIOobject 持久化预留 |
