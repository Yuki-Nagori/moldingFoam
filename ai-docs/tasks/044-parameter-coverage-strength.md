# 044 — 参数/系数覆盖补齐与模式-only 用例强化

- 状态：planned
- 优先级：P2
- 依赖：038（pressureRamp）、006/012（周期用例）、016/002/015/003（次要键）
- 预估规模：1–2 天
- 来源：`ai-docs/coverage-audit-2026-09-13.md` 缺口 G3、G8、G9

## 1. 背景与现状

- **G3 `packing.pressureRamp`（038）**：自适应缺省在用例中执行但**无断言**；
  显式值分支与负值 `FatalIOError` 未覆盖；启动回显
  （`pressureRamp = auto (...)`）无断言；038 报告的 E1–E7 实验矩阵未跑
  （其中 E1/E3/E7 是稳定性与跟随性对照，E2 复现失稳）；
- **G8 模式-only 用例**：`tests/cases/cycleReset`、`gateFreeze` 仅有
  `expectedPatterns`，其余 20/22 用例都有数值验证器；
- **潜热断言缺失（2026-09-13 消融新发现）**：`crystallization` 的
  `latentHeat 2e5 → 0` 与 `boxFill` 的同类消融后，用例检查仍 PASS →
  现有验证器不断言潜热对温度/χ 轨迹的影响（潜热属仓库核心特性，001 的
  契约走的是 case-contract 口径，求解器用例侧无回归）。**标定尝试
  （同日）**：`coldWall` 的模具温度（moldingMoldTemperature 集总 BC）
  在开/关潜热下分离仅 300.021 vs 300.015 K（3 mK，相对 40% 但绝对量级
  太小，跨平台不稳）；改用熔体体积平均温度需给用例加 `volFieldValue`
  函数对象——首次尝试该 FO 被正确选中但未产生输出（`log true` 的
  操作名/输出格式待查，见 `postProcessing/` 或 FO 文档），故**未提交
  未验证的断言**。**第二次尝试（同日）**：查明失败原因——v14 的
  `volFieldValue` 被当作 `generatedCellZone` 解析（`system/functions!
  meltTemperature` 报 `generatedCellZone::read`），补 `regionType all;`
  后仍无输出，需按 v14 的 region 语法（`regionType`/`cellZone` 组合）再
  核对。下一步：确认该 FO 在 v14 的最小可用配置 → 用 `postProcessing/
  meltTemperature/*/volFieldValue.dat` 的时间序列标定（候选判据：末态
  熔体平均温度或穿越阈值的时刻，开/关分离度须 ≥10× 阈值余量）；
- **G9 次要分支键无用例配置**：runnerNetwork 的 `powerLaw`（K/n）、
  moldingMoldTemperature 的 `Nu` 相关式（C/m/n/Re/Pr/k/D）、fiber 的
  `lambda` 覆盖、`trapAirAlpha`、`hsRef`、moldingPrghPressure 的
  `relaxation`、runnerNetwork 的 `wallTemperature`（模型级有解析覆盖，
  case 级无）。

## 2. 目标

1. pressureRamp：显式值 + 回显断言；负值报错路径断言；E1/E3/E7 的
   稳定性对照有最小复现（可在同一用例的参数化变体中完成）；
2. `cycleReset`/`gateFreeze` 补数值验证器（复位前后守恒量、封冻时刻的
   通量归零）；
3. 次要键：至少覆盖 `powerLaw` 与 `Nu` 相关式两条**代码分支**（其余键
   以「缺省=解析极限」在文档标注有意不覆盖）。

## 3. 技术方案

- pressureRamp：`tests/cases/boxFill` 变体设 `pressureRamp 0.2`，断言
  「切换后闸口压力单调升至目标、无 NaN」，并断言回显行；另一变体
  `pressureRamp -1` 断言 `FatalIOError` 文本；
- 复位序列断言：把 `cycleReset` 扩展为验证器（对比复位前后
  `alpha.melt`/`U` 的 L1 差与 `T` 保留量）；
- 封冻断言：`gateFreeze` 增加「封冻后 `sum(inlet)` ≈ 0」检查（可从
  日志通量行读取）；
- 次要键：`runnerNetwork` 变体用 `powerLaw` 对拍解析压降；
  `moldSteady` 变体用 `Nu` 相关式对拍 `htcFromNu` 的解析值；
- 有意不覆盖清单：`lambda`（形状因子派生）、`trapAirAlpha`、`hsRef`、
  `relaxation`、`wallTemperature` 在 016/002/015/003 文档标注。

## 4. 工作拆解

1. pressureRamp 两个断言用例（1 天内的主要部分）；
2. cycleReset/gateFreeze 验证器（各 ≤2 小时）；
3. powerLaw 与 Nu 两条分支用例（各 ≤2 小时）；
4. 文档与审计标注。

## 5. 验收标准（DoD）

- pressureRamp 的三条路径（自适应/显式/负值）均有断言；
- 潜热断言：至少一个求解器用例在 `latentHeat` 关掉后 FAIL（消融可验）；
- 22 个 solver 用例全部带数值或定量断言（模式-only 归零）；
- `powerLaw`/`Nu` 分支有解析对拍（≤1%）；
- 有意不覆盖清单落文档。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 变体用例增多使套件变慢 | 全部复用既有几何，控制在秒级 |
| 负值报错用例需要非零退出码的 runner 支持 | run-solver-tests.sh 已按 `foamRun` 非零退出计 FAIL，可直接用 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `tests/cases/boxFill*/`、`cycleReset/`、`gateFreeze/`、`runnerNetwork/`、`moldSteady/` | 变体与断言 |
| `scripts/verify-cycle-reset*.py`、`verify-gate-freeze.py`（新）等 | 数值验证器 |
| `ai-docs/tasks/{038,006,012,016,002,015,003}-*.md`、审计文档 | 记录与缺口关闭 |
