# 051 — 精度解释缺口与验证强度清理（小项合集）

- 状态：planned
- 优先级：P3
- 依赖：020、021、035 阶段 1、036、043/044
- 预估规模：1–2 天
- 来源：046 后续评估——四处「有数字但没结论」或「断言强度未复核」
  的小项，逐项结清或明确转为长期项

## 1. 清单与现状

1. **喷泉流细网格 L2 反升（020）**：粗网格 L2 1.64%，152×16 细网格
   6.94%，归因写的是「单元值 vs 点值度量」但未定论 → 需给出口径结论
   （换点值/插值口径重算一次即可判定）；
2. **双层板网格非单调（013b/035）**：48×16 / 96×32 / 192×64 误差
   4.2% / 6.1% / 4.9%，报告为「有界 ≤6.1%，不用 Richardson」，但
   界面相位误差未做解析归因 → 给出归因或明确写为「已知有界误差，
   不做归因」的决策；
3. **三维水区 Nu 关联式定量对拍（036）**：`coolantWater` 能量平衡
   2.7e-6、`coolantWaterMold` 1.176e-7 都是自洽守恒，缺「圆管/矩形管
   Gnielinski/Dittus-Boelter ≤10%」的独立对拍 → 补一个解析对拍或
   记录不做的理由；
4. **消融矩阵未全量重跑（043/044 后）**：`ablation-audit-2026-09-13`
   发现 4 项 INSENSITIVE（crystallization 潜热、fiber Lipscomb、
   moldSteady wallResistance、boxFill pressureRamp），043/044 补了
   断言但是否对全部 26+ 用例重跑消融未记录 → 重跑一次并更新审计表；
5. （并入）**`gateSealRamp` 定量断言未做**（043 记录标定未取到稳定
   信号）→ 要么找到稳定信号补断言，要么明确降级并在审计文档注明。

## 2. 目标与 DoD

每一项给出「结论 + 证据」之一：

- 有结论：数字/口径说明写入对应 README 小节或 `ai-docs/uncertainty.md`；
- 不做：写明不做的理由与检索/尝试范围，并从「待办」清单移除。

DoD 汇总：五项各有归属；`ai-docs/coverage-audit-2026-09-13.md` 与
`ablation-audit-2026-09-13.md` 更新到与本任务一致。

## 3. 技术方案（按项）

- 020：在 `verify-fountain-flow.py` 增加「点值插值」口径（面心插值到
  解析剖面），两种口径并列输出 → 若点值口径随网格下降则结论成立；
- 013b：把界面位置的相位误差写成解析表达式（层间间断处高斯积分
  相位），或在 `verify-warpage-plate.py` 增加「误差 vs 相位」扫描；
- 036：用 Gnielinski/Dittus-Boelter 在 `coolantWater` 的直管段做一次
  对拍（管道几何已知、Re/Pr 可算），阈值 ≤10%；
- 消融：用现有 `ablation-audit` 方法（`git stash` 式开关）对 27 个
  用例重跑，记录灵敏度表；
- gateSealRamp：在 `processProfile` 或 `gateFreeze` 用例上加一段可观测
  的斜坡（如 sealRamp 时间常数 vs 通量衰减），取稳定信号后补断言。

## 4. 风险与缓解

- 多项都是「验证验证器」，容易滑入无限细化 → 每项时间盒 0.5 天，超时
  即降级为「记录结论 + 长期项」，不留悬空；
- 消融重跑会改动被测源码（临时），必须在干净工作树 + 单独分支上做，
  避免污染基线。

## 5. 涉及文件

- `scripts/verify-fountain-flow.py`、`scripts/verify-warpage-plate.py`
- `validation/coolantWater/`（或新增验证脚本）
- `ai-docs/ablation-audit-2026-09-13.md`、`ai-docs/coverage-audit-2026-09-13.md`
