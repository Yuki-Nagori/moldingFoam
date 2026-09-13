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
  `expectedPatterns`，其余 20/22 用例都有数值验证器；**cycleReset 已补
  （2026-09-13）**：新增 `scripts/verify-cycle-reset.py`——断言喷射周期数
  等于 `nCycles`、复位日志的 `min(rho)` 与初始状态的 Tait EOS 密度一致
  （实测 746.291 vs 746.28177，相对 1.18e-05，阈值 1e-3 ≈ 85× 余量）、
  写出态的 `alpha.melt`=1 与密度复原；**gateFreeze 已补**：新增
  `scripts/verify-gate-freeze.py`——自校准不变量（无需绝对阈值）：
  封冻（含 `gateSealRamp` 窗口）后熔体质量逐步恒定（实测 99 个闭合样本
  的逐步变化 **0.00e+00**，阈值 1e-6），并断言封冻事件与斜坡回显；
  用例同步把 `ejectionTemperature` 设为不可达（原 500 K + `nCycles 1`
  使运行在封冻瞬间结束，闭合窗口无从采样），endTime 0.005 → 0.02 s；
  **G8 两项（cycleReset/gateFreeze）至此均具备数值验证器**；
- **潜热断言缺失（2026-09-13 消融新发现 → 同日交付）**：`crystallization`
  的 `latentHeat 2e5 → 0` 消融原本仍 PASS。**交付**：用例新增
  `volFieldValue` 函数对象（v14 语法为 `cellZone all;`，非
  `regionType`——此前的两次失败均因该语法），记录熔体体积平均温度序列；
  `verify-crystallization.py` 增加断言「末态熔体平均温度 ≥ 400 K」
  （潜热开 429.23 K / 关 380.16 K，分离 49 K，阈值两侧各留 ~20 K ≈ 5×
  跨平台波动余量）。消融复验：开 → PASS、关 → FAIL ✓；
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

- pressureRamp：**显式值 + 回显断言已交付（2026-09-13）**：
  `tests/cases/gateFreeze` 设 `pressureRamp 0.2`（此前只有自适应缺省被
  执行），patterns 断言回显 `pressureRamp        = 0.2`，gateFreeze 的
  自校准验证器不受斜坡取值影响；**顺带修掉一个真实缺陷**：
  `moldingFoam.C` 的启动回显写作 `word(pressureRamp)`，而 `word` 只匹配
  到 `word(char)`（0.2 → char(0) = NUL）——显式值回显为空并往日志里写入
  NUL 字节（grep 判为 binary；若取值恰为 ASCII 码会打印成乱字符）；
  改为 `Foam::name(pressureRamp)`（首版 `name(...)` 被类自身 `name()`
  成员遮蔽，需限定命名空间）。**剩余**：负值 `FatalIOError` 路径需要
  runner 支持「期望失败」标记（`system/expectFailure`），为本批唯一需要
  动 runner 的项；
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

## 8. 交付记录（2026-09-13）

- **负值 pressureRamp 路径 + runner 期望失败支持**：`scripts/run-solver-tests.sh`
  新增 `system/expectFailure`（存在时反转期望：`foamRun` 必须非零退出且
  日志包含指定错误文本），并新增永久用例 `tests/cases/fatalPressureRamp`
  （`packing.pressureRamp -1` → 断言 `The packing pressure ramp must be
  non-negative`）。实测 runner 输出
  `PASS: fatalPressureRamp: expected failure reproduced`（All 1 solver
  case(s) passed）；插入块内补 `caseFailed=0` 修 `set -u` 下的 unbound。
  pressureRamp 三条路径（自适应/显式+回显/负值）至此全部交付；
- 交付顺序回顾：潜热断言 → cycleReset/gateFreeze 数值验证器（G8 清零）
  → pressureRamp 显式值+回显（并修复 `word(scalar)` 回显缺陷）
  → 负值路径 + runner 支持。剩余：powerLaw / Nu 相关式两个分支。
