# 062 — 流量受控浇口（逐浇口给定流量，其余按阻力分配）

- 状态：planned（设计已定稿 + **已做过一轮实现尝试并回退**，四条实测教训见 §7，可直接接着做）
- 优先级：P2
- 依赖：016/026（流道网络）、058（树拓扑与阀时序）、030（工艺曲线）
- 预估规模：2–3 天（含两个求解器 + 用例 + 敏感性实验）
- 来源：Kairos 侧 T82/T83 明确要求"逐浇口流量/压力曲线"；058 曾以"边界层
  已可表达"为由不做，**该论据对流量不成立**（见 §1）

## 1. 背景（先纠正一条错误论据）

058 §2d 当时写的是"逐浇口的流量/压力目标在边界层已可表达（每个浇口 patch
有自己的字典）"。**对压力成立、对流量不成立**：

- `0/p_rgh` 的 `moldingPrghPressure` 是**逐 patch** 的 ✓ → 每个浇口的保压
  目标/曲线本来就能各写一份 ✓；
- 但 `0/U` 的 `moldingInletVelocity` 在**有 `runner` 时**把 patch 上的
  `volumetricFlowRate(Profile)` 当作**网络的总流量**（`Qtotal`），再用
  `gateFlow(gate, Qtotal, t)` 取该 patch 的份额 → patch 自己的曲线**不是**
  该浇口的流量 ✗。

所以"某浇口由执行机构给定流量、其余浇口争剩余"这种工况（阀浇口的常见控制
方式）目前无法表达。README §6 与 058 §2d 已按此更正。

## 2. 目标语义（定稿）

浇口（扁平 `gates` 条目或 `tree` 叶子）新增可选 `gateFlowProfile`
（`Function1`，量纲 `[0 3 -1 0 0 0 0]`，单位 m³/s；写 `type constant; value …;`
即常数）：

1. 该浇口的流量 = 曲线值（钳到 ≥ 0），**不参与**等压降分配；
2. **剩余流量**（总流量 − Σ 已给定）在**其余开通浇口**之间按阻力等压降分配
   （树模式下逐节点局部扣除：某节点的子节点里有给定流量的，先从该节点入流里
   扣掉，再对剩下的自由子节点按等效阻力分配）；
3. **阀优先级**：`gateOpenTime`/`gateCloseTime` 判为关闭的浇口流量恒为 0，
   **覆盖**给定曲线（阀门是执行机构的硬约束）；
4. **退化定义**：若某层/网络里**没有自由浇口**，该层的压降取"给定流量浇口的
   流量加权平均压降"（`pressureDrop()` 因此仍有定义，写进文档）；
5. **总流量仍由上游决定**（`totalFlowRate` 或 030 的曲线）✓ 不变；
6. 校验：Σ 给定流量 > 总流量时，自由浇口按 0 处理并打印预警（不静默）；
7. **未写 `gateFlowProfile` 时两条求解路径逐位不变**（与前几轮同样的口径：
   新增分支由 `anyPrescribedFlow_` 守卫，旧算术不动）。

## 3. 实现要点（已勘定位置）

- `moldingRunnerNetwork.H`：`PtrList<Function1<scalar>> gateFlowProfile_`
  （空项 = 自由）、`bool anyPrescribedFlow_`、私有 `prescribedFlow(g, t)`；
- 解析：`readValve()` 里追加 `gateFlowProfile`（`Function1<scalar>::New(...)`）；
- 扁平：`splitValved()` 内先算 `qPrescribed`/`qFree`，迭代只对**自由**开通
  浇口做（给定浇口保持曲线值）；不做则退化为现有行为；
- 树：`solveTree()` 的兄弟分配段（现为 `qTgt[j] = qTgt[i]*(1/Req[j])/invSum`）
  同样先扣给定份额、再对自由子节点分配；根的分配同理；
  `dpChild` 用 `qFreeRoot/invSumRoot`，无自由根时用 §2.4 的退化定义；
- BC **无需改动**（`gateFlow(g, Qtotal, t)` 已经返回该浇口份额 ✓✓）。

## 4. 验证

- 模型测试（`tests/modelTests.C`）：
  - 扁平：两浇口、其一给常数流量 → 该浇口 = 给定值、另一浇口 = 总量 − 给定
    （定黏度下与解析一致，rtol 1e-10）；给定 + 关闭 → 0（阀优先）；Σ 给定 >
    总量 → 自由浇口 0 且总量守恒；
  - 树：给定**叶子**深一层 → 其兄弟按剩余分配、其**上游**管段仍按总流量；
  - 幂律 + 给定流量的守恒性；
- 用例（`tests/cases/runnerPrescribed` 或扩展现有）：两个浇口各挂一个 patch，
  一个走 `gateFlowProfile` 曲线（两段：3e-8 → 1e-7）、另一个自由 →
  验证器核对"曲线段内该入口质量流 = ρ·曲线值"、"另一入口 = 总量 − 曲线值"、
  "切换前后守恒"；
- **敏感性（按 061/diagnostics §4c 的纪律）**：必须让**实现**失效、期望不变
  （例如把 `qPrescribed` 清零）→ 判据 FAIL；
- 回归：`runnerNetwork`/`multiGate`/`runnerTree`/`runnerValve`/`runnerProfile`
  数值不变，模型测试全绿。

## 5. 风险

- 树模式下的"逐节点扣减"会改变**上游管段**的流量语义吗？——不会：上游段仍
  携带该子树的全部流量（给定 + 自由），只是"分配方式"变了；用例要专门断言
  上游段流量（例如通过闸口压力耦合反推）；
- 与 `pressureDrop()` 的耦合：保压期压力边界用网络压降，若某层无自由浇口
  会走 §2.4 的退化定义 → 文档必须写明，避免使用者误以为那是等压降结果。

## 6. 涉及文件

- `src/moldingFoam/moldingRunnerNetwork.{H,C}`
- `tests/modelTests.C`、`tests/cases/runnerPrescribed/`（或扩展）
- `scripts/verify-runner-*.py`、`README.md` §6、`xmake.lua`/nightly

> 相关：058（树拓扑/阀时序；§2d 的错误论据已补记更正）、030（总流量曲线）、
> 061 与 `diagnostics.md` §4c（敏感性实验的纪律）。

## 7. 实现尝试记录（2026-09-15，已回退；四条实测教训）

按 §2/§3 实做了一遍（扁平 `splitValved` + 树 `solveTree` + 三处派发 + 解析 +
4 条模型测试），结果：

- **模型层全绿**：给定浇口保流量（1e-06）、自由浇口吃剩余（2e-06）、
  **阀优先于给定值**（关阀后 0、自由浇口吃满 3e-06）、树里给定叶子
  （8e-07 + 2.2e-06）、无自由浇口时压降取流量加权平均（3.65e+06 Pa）——
  §2 的语义在模型层已被证实可实现；
- **用例层全红**：五个 runner 用例（multiGate/runnerNetwork/runnerProfile/
  runnerTree/runnerValve）启动即崩。根因：**`PtrList` 的拷贝构造在含空项时
  直接 `abort`**，而 `moldingInletVelocity` 的 `clone()` 会拷贝
  `moldingRunnerNetwork`（`new moldingRunnerNetwork(*pivpvf.network_)`），
  我把 `gateFlowProfile_.setSize(n)` 造出 n 个空项 → 默认拷贝构造炸掉。
  模型测试不拷贝网络 → 绿；用例一拷贝 → 红（这个"模型绿、用例红"的分裂
  很有诊断价值）。

**四条教训（下次直接照做）**：

1. `Function1s::unitSets` **没有默认构造** → 用
   `Function1<scalar>::New(name, unitSet(dimTime), unitSet(dimVolume/dimTime),
   dict)`（已编译通过）；
2. `Function1::New(name, …)` 的 `name` 是**字典里的条目名**（它按该名去 dict
   里找子字典）→ scalar 简写 `gateFlowRate` 必须包成条目
   （`{"gateFlowRate": {"type": "constant", "value": q}}`），否则报
   `keyword gateFlowRate is undefined`；
3. `PtrList` 必须按闸口数 `setSize`（且 `prescribed()` 要做边界检查），否则
   无 profile 的网络会越界索引；
4. **阻塞点**：`PtrList` 含空项时不可拷贝 → 必须给 `moldingRunnerNetwork`
   写**显式拷贝构造**（逐成员列出）或换容器（如按闸口名索引的
   `HashTable<autoPtr<Function1<scalar>>>`）；且验收**必须包含用例层**，
   只跑模型测试会给出假绿。

回退原因：本轮余量不足以完成"显式拷贝构造 + 重建 + 五用例回归 + 敏感性"
这一整套验证，按本仓库标准不提交未验证的网络改动。**语义设计不变**（§2），
代码从 §3 的位置接着做即可。

## 补记：2026-09-15 设计一致性复核

以下更正保留原规划作为历史记录，下一轮实施应先统一验收口径：

- §2.6 的“超配时自由流量置零”与 §4 的“总量守恒”不能同时成立：若给定
  流量之和已经超过总量，仅把自由流量置零仍会超出总量。建议超配时明确
  拒绝输入；若产品选择按比例缩放，则必须同时修改“给定值严格满足”的
  契约与测试，不能只打印告警后输出不守恒结果。该设计点尚未实现。
- §1 的“逐 patch 压力边界即可各写独立保压曲线”推断也不充分：当前
  `moldingPrghPressureFvPatchScalarField::updateCoeffs()` 从共享
  `moldingStage::pressure()` 取得目标，独立 patch 不等于独立目标曲线。
  逐浇口压力控制需另行定义，不将其混入本任务的给定流量交付。
- 063 已补边界 runner/profile 的写出；本任务增加 Function1 后仍需测
  clone、序列化和重启。非线性残差与缓存规划见
  [068](068-runner-convergence-cache.md)，检查点约束见
  [064](064-checkpoint-cycle-contract.md)。
