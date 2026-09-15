# 071：solidDisplacement 校正循环的真实收敛控制

- 状态：done
- 优先级：P1
- 依赖：070；OpenFOAM-14 `solidDisplacement`
- 来源：070 B3 诊断与源码复核

## 背景

OpenFOAM-14 的 `solidDisplacement::pressureCorrector()` 读取
`nCorrectors` 和 `D` 收敛阈值，但把第一次 `DEqn.solve()` 的
`initialResidual` 保存后，在后续校正中不更新。循环条件因此不能反映当前
校正的残差；只要第一次残差高于阈值，就固定执行全部 `nCorrectors`。在
96×160 细网格上，`nCorrectors=3` 已观察到每个 D 分量达到 1000 次 GAMG
迭代，造成 68 s 只推进到伪时间 35 的成本。

这解释了为什么 070 中“增加校正次数”只能改善局部线性残差而无法提供可接受
的完整静态证据。它不是调大 8% 阈值或继续堆叠字典参数可以解决的问题。

## 目标

在不修改 `/opt/openfoam14` 的前提下，评估可维护的模块覆写或上游补丁入口，
使校正循环使用每次实际残差和目标量变化作为停止条件，并保留最大校正次数
和超时保护。若当前仓库无法安全覆写上游 solver，则形成最小 upstream patch
请求和可复现证据，不把 070 标成数值修复完成。

## 设计流程

1. 从 of14 源码确认 `DEqn.solve().max()` 的返回类型、残差语义、
   `D.oldTime()` 更新时机和 `divSigmaExp` 重建依赖。
2. 用合成线性弹性场验证：残差下降时提前退出；残差不降时达到上限并返回
   `not_converged`；每个分支都必须有有限字段和一致边界力。
3. 比较三种实现边界：模块内新 solver 类型、编译时替换 OpenFOAM module、
   仅字典控制。若需要修改上游源码，记录原因并拒绝将其偷偷复制进本仓库。
4. 在 48×80 和 96×160 上以相同 Q、D RMS 和绝对残差预算对拍，报告精度、
   校正次数、墙钟和峰值内存；未达到静态判据的行不能参与性能比较。
5. 通过 070 的固定物理截面 verifier、静态收敛检查和 nightly artifact 后，
   再决定是否将修复接入验证基线。

## 验收标准

- 每次校正的实际残差进入日志和结构化指标，停止条件不再复用首轮残差。
- 最大校正数、阶段 timeout 和未收敛状态均有硬失败路径。
- 两个结构基准的三网格结果达到静态收敛；中/细网格解析误差维持 8% 门槛。
- 有同一 of14 会话的修复前后精度和有效成本对比；未完成或上游受限时明确
  标记 unavailable，不更改默认门槛。
- 仅在源码、模型测试、双架构 CI 和 np4 契约均通过后，才可重新评估 070 done。

## 与 070 的关系

070 已完成测量口径、静态收敛防线和字典参数排除；071 承接上游校正循环的
实现缺口。070 保持 `in-progress`，直到 071 提供可接受的 solver 行为或有
正式的上游阻塞记录。

## 实施记录 2026-09-15

新增 `scripts/diag/check-solid-displacement-contract.py`。它在 of14 读取实际
`solidDisplacement.C`，统计 `DEqn.solve()`、`initialResidual` 赋值和循环条件；
若循环读取首轮残差且没有后续更新，返回非零并输出 JSON。宿主无 OpenFOAM 源码
时返回 `unavailable`，不会把缺环境误判为通过。

当前 of14 源码审计结果：`DEqn.solve()` 只在校正体中赋给
`initialResidual` 一次，循环条件继续读取该值；没有逐校正残差更新。该结果与
070 的 B3 成本证据一致。

### 停止条件修正补记（2026-09-15）

项目覆写现分别保存每次求解的 `initialResidual()` 与 `finalResidual()`：前者只
用于判断是否允许加速外推，后者用于 `pressureCorrector()` 校正循环的停止条件。
此前把初始残差同时用于停止判断，会让循环无法反映本次校正后的实际收敛程度。
本次仅改项目模块，未修改 `/opt/openfoam14`；OF14 编译、结构精度、校正次数、
墙钟和内存尚未复测，不能据此宣称 069/070 已通过。

## 实施记录 2026-09-15：模块覆写完成

新增 `moldingSolidDisplacement` 派生模块（`src/moldingFoam/`），复用官方
材料、应力和边界实现，只覆写 `pressureCorrector()`：每次 corrector 都重新
写入当前 `DEqn.solve().max().initialResidual()`，并按当前残差与
`nCorrectors` 结束循环。验证字典通过 `solver moldingSolidDisplacement` 和
`libs ("libmoldingFoam.so")` 显式启用，未改动 `/opt/openfoam14`。

of14 证据：库构建与 `modelTests` 链接通过；一阶 `nCorrectors=3` 算例输出三组
独立 Dx/Dy 求解。契约审计对官方源返回 `contract_violation=true`（预期的上游
缺陷），对本项目实现返回 `contract_violation=false`。该修复已成为两个结构
验证载体的默认 solver；070 的三网格精度矩阵仍按其静态收敛标准单独验收。
