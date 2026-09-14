# 052 — 界面机制的下一批杠杆：alpha 修正器与 MULES 策略

- 状态：done（2026-09-14：F 样本 471/472/577 s、守恒 5.010e-04 三次一致，已入契约 v1.29；G 阴性）
- 优先级：P1
- 依赖：047（守恒误差 ∝ dt×h⁻¹）、048（v1.28 组合）、050（墙钟拆分）、009（界面放宽的阴性矩阵）
- 预估规模：1–2 天
- 来源：046 后续评估 + 050/047 的共同指向——**界面机制既是墙钟热点
  （`libfiniteVolume` 45.6%，`alphaSolve → interfaceCompressionNew::interpolate`
  调用链，最大单符号 `Foam::multiply` 8%+）**，也是守恒误差的来源
  （固定 dt 下误差随加密一阶上升，说明误差由界面单元数产生）。上游补丁
  不可用（README 第 6 节"上游零补丁"），因此只测**字典层旋钮**。

## 1. 上游路径事实（2026-09-14 读源码，OF14）

`twoPhaseSolver::alphaPredictor()`（`applications/modules/twoPhaseSolver/alphaPredictor.C`）：

1. `nAlphaSubCycles = ceil(nAlphaSubCyclesPtr->value(alphaCoNum))` ——
   **`nSubCycles` 是 `Function1`**，按 alpha 库朗数求值；契约里写常数 8；
2. 每个子循环：`alpha1Eqn.solve()` + `correctInterface()`（曲率等）；
3. 子循环之后，另有 `for (aCorr < nAlphaCorr)` 的**显式修正遍数**
   （`nAlphaCorr` = alpha 字典的 `nCorrectors`，契约现为 **2**）：
   - `MULESCorr no`（现缺省）→ 每遍做完整的显式 MULES 更新；
   - `MULESCorr yes` → 只做限幅修正（`MULES::correct`），不重解输运。

即：每步界面工作量 ≈ `nSubCycles ×（解+曲率） + nCorrectors ×（限幅/显式更新）`
——两个旋钮都乘在工作量上，且都未在 v1.28 的紧设置下测过。

## 2. 目标

1. 在 v1.28 契约设置（`nSubCycles 8`、能量 `tol 1e-5`、`maxAlphaCo 0.015`）
   下测三档：`nCorrectors 1`（F）、`MULESCorr yes`（G）、两者组合（H），
   与同会话 E 基线对照：墙钟、步数、质量守恒、完整验收；
2. 给出结论：某档可作生产选项/契约候选，或记录为阴性（守恒破线/无收益）；
3. 记录 `nSubCycles` 为 Function1 这一事实：未来可做**自适应子循环表**
   （低 alphaCo 少子循环、高 alphaCo 多子循环）——本任务只记录，不实现。

## 3. 与 009 阴性矩阵的区别

009 的 Pareto 表放宽的是 `maxAlphaCo`/子循环/`MULESCorr` 的**组合**
（0.10/4 等），守恒 1.66–2.54e-3 破线；本任务**不放松 dt 与子循环数**，
只改修正器遍数与 MULES 策略，属于未被覆盖的维度。

## 3a. 实测（2026-09-14，契约 case @4 子域，同会话）

| 变体 | 墙钟 (s) | 步数 | 质量守恒 | 验收 |
|------|----------|------|----------|------|
| E 基线（v1.28 缺省） | 673 | 15,215 | 5.164e-04 | PASS |
| **F（alpha `nCorrectors 1`）** | **471** | 15,212 | **5.010e-04** | PASS |
| G（`MULESCorr yes`） | 中止 | — | 未测 | — |

- **F：墙钟 −30%**（673 → 471 s，步数几乎不变 15,215 → 15,212，
  即单步成本 44 → 31 ms），**守恒略好**（5.164 → 5.010e-04），完整验收
  PASS（CrossWlf/Tait 回显、V/P 切换、压力跟随、顶出）——机理与源码
  一致：`nAlphaCorr` 的每一遍都是完整的显式 MULES 更新，减半即省掉
  ~30% 的界面工作量（050 的剖面：界面机制占墙钟 45%）；
- **G 被成本支配（阴性）**：`MULESCorr yes` 下 dt 塌缩 ~3×（10,741 步
  才走到 t = 0.66 s，折合 ~16,300 步/物理秒 vs F 的 5,071），单步成本
  也更高（55 vs 31 ms）→ 全程需 ~44 min，约为 F 的 3 倍，在 40% 处
  中止并记为成本阴性（守恒未测——成本已足以否决；这与 009 的
  "界面设置改变会牵动 dt"结论一致）；
- **H（F+G）不再测**：G 的 dt 塌缩由 `MULESCorr` 引入，H 会继承该
  代价 → 预测被 F 支配。

**结论（已由 3 个样本确认）**：α 字典的 `nCorrectors 2 → 1` 是
目前最大的单项收益（−30%，且守恒不劣），建议作为契约 v1.29 候选；
`MULESCorr yes` 在本 case 上不可用。

## 4. 工作拆解

1. 实验脚本扩展（`scripts/lever-experiment.sh` 增 F/G/H 变体，含 alpha 块
   `nCorrectors`/`MULESCorr` 补丁）——已完成；
2. 跑 E（同会话基线）/F/G（各 1 次，~10 min/次）→ 视结果补 H；
3. 结论与文档（本文件 + README §7）；
4. 若某档有收益且守恒安全：按 v1.28 的流程走"契约修订建议"，不直接改
   冻结接口。

## 5. 验收标准（DoD）

- E/F/G(/H) 四档对照表（墙钟/步数/守恒/验收）；
- 明确结论（可用/不可用）与理由；阴性结果同样落文档；
- 若采纳某档，README 与任务 048/041 的成本杠杆表同步。

## 6. 风险与缓解

- 守恒对界面设置敏感（009/047 的证据）→ 每档都跑完整验收（1e-3 阈值），
  破线即判阴性；
- `MULESCorr yes` 会改变 alpha 的对流离散口径（半隐式修正），可能影响
  前沿/熔接痕类指标 → 除守衡外检查 acceptance 的物理判据（压力跟随、
  顶出）与 `weldLine`/`fountainFlow` 用例（后者有时间余量再跑）。

## 7. 涉及文件

- `scripts/lever-experiment.sh`（变体扩展）
- 文档：本文件、README §7、`ai-docs/tasks/048-cost-lever-combination.md`
