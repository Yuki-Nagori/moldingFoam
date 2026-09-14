# 048 — 求解成本杠杆组合实测与每步通信削减

- 状态：in-progress（2026-09-14 立项；诊断门控已落地，组合实验进行中）
- 优先级：P1
- 依赖：032（nSubCycles 8）、041（能量容差 1e-5）
- 预估规模：1–2 天
- 来源：046 后续评估——两个已测单项杠杆各自 −26% / −24%，但**组合
  从未实测**，且两者吃同一份守恒余量

## 1. 背景与现状

- `nSubCycles 16→8`：墙钟 −26%（628.7→467.4 s 中位数），守恒
  9.383e-4→9.695e-4（032 §3c）；未改缺省（余量 6%→3%）；
- 能量解容差 1e-6→1e-5：墙钟 −24%（876→666 s），守恒 9.696e-4
  （041 §4b）；未改缺省，记为按需选项；
- 两项的守恒增量分别是 +3.3% 与 +3.3%，叠加后可能贴近或越过
  1e-3 阈值——**必须先测再谈缺省**；
- 另一个每步成本项：`massBudget` 诊断的逐 patch 求和与打印**每步
  都做**（实测 cli10：5,080 步打出 8,092 行 `mass budget patch`，而
  真正被消费的汇总行只有 25 行 = `massBudgetInterval 200` 次），
  这些量只在 interval 步用于打印，可按 interval 门控；
- 已否决（勿重复）：GAMG（慢 13%）、`nCorrectors 2`（破守恒）、
  容差组合调优（无加速）、界面子循环放宽（2.5e-3 破线）、
  `maxCo 0.25`、`ventSealAlpha 0.99`、强制 ddtCorr。

## 2. 目标

1. **同会话同 VM** 测三档：基线（16 / 1e-6）、单位杠杆（8 / 1e-6、
   16 / 1e-5）、组合（8 / 1e-5），每档重复 ≥2 次取中位数，记录墙钟、
   步数、质量守恒、V/P 切换与压力跟随；
2. 若组合守恒 < 1e-3：写入 README §7 作为**长算例生产选项**（不改
   CI 缺省，理由同 041：夜间门禁需要抗漂移余量）；若 ≥1e-3：记录
   失败组合并给出「先根修守恒源（047/031）再谈叠加」的结论；
3. 诊断门控落地并量化：每步集合通信次数、日志行数、墙钟差（预期
   <1%，零风险，主要收益是同步点与日志体积）。

## 3. 技术方案

- 组合实验：`case-contract` 的 **scratch 副本**（不改契约本体），
  只覆盖 `fvSolution` 的 `nSubCycles` 与 `(U|e|T).*` 的 `tolerance`；
  用 `scripts/run-case.sh <scratch> 4` 跑，读 `log.foamRun` 末段验收
  摘要与 `ExecutionTime`；后台顺序执行，同会话对照；
- 诊断门控：`moldingFoam::postSolve` 中 `fluxVol`/`fluxAlpha`/逐 patch
  求和/打印、以及步分解的三个 `reduce`，全部移入
  `runTime.timeIndex() % massBudgetInterval_ == 0` 分支（`flux` 与
  `reduce(m)` 保持每步，因为 `massBudgetIn_` 每步累加）——条件由
  `timeIndex` 与全局字典给出，所有 rank 一致，不引入 rank 相关分支；
- 记录：`python3 scripts/perf-breakdown.py log.foamRun`（迭代分解）
  与 `grep -c` 的日志行数对照。

## 4. 工作拆解

1. 诊断门控代码 + `xmake run test-solver` 回归（半天，已完成）；
2. 组合实验脚本（scratch 副本 + 三档 × 重复）与执行（半天）；
3. 结果表 + 结论写入 README §7、041 与 ai-docs/README 索引（半天）。

## 5. 验收标准（DoD）

- 三档 × ≥2 次的中位数对照表（墙钟/步数/守恒/物理量）；
- 组合是否可用的明确结论与缺省决策（改或不改，理由）；
- 诊断门控落地且回归绿（`test-solver` 27/27、契约并行全项通过）。

## 6. 风险与缓解

- VM 方差 20–25%（032 §3a）：同一会话内交替跑基线/变体，取中位数；
  结论以守恒为主、墙钟为辅；
- 组合若破守恒：不强行推广，转为 047（网格/时间收敛）与 031 的
  输入——「把守恒余量做大」比「吃掉余量换速度」优先。

## 7. 涉及文件

- `src/moldingFoam/moldingFoam.C`（诊断门控）
- `case-contract/`（只读；变体在 scratch 副本）
- 文档：README §7、`ai-docs/tasks/041-memory-traffic-longterm.md`、本文件
