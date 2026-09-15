# ai-docs：研发缺口与路线任务

本目录是 moldingFoam 求解器研发任务的唯一计划来源，面向 AI 开发会话与
新加入的工程师。每个任务文件包含：背景与现状、目标与非目标、技术方案
（含已否决的方案及原因）、工作拆解（文件级）、验收标准（DoD）、风险与
缓解、涉及文件清单。

## 使用约定

1. **只写计划，不写实现**：任务文件中的接口草图/公式仅用于设计评审，
   落地代码一律进 `src/`、`tests/`、`case-contract/`，并走正常提交与
   CI 流程。
2. **状态字段**：每个任务文件头部有 `状态`（planned / in-progress /
   done / rejected）与 `优先级`（P0 > P1 > P2 > P3）。开工时改为
   in-progress，完成时改为 done 并附验收记录（提交哈希 + 验收输出摘要）。
3. **完成定义（DoD）**：所有任务共享——
   - `xmake` 编译零告警新增；
   - `xmake run test` 全部 PASS（新增功能必须带新测试）；
   - `MOLDINGFOAM_PARALLEL=4 xmake run case-contract` 全项验收通过
     （质量守恒 < 1e-3）；
   - README 与 `case-contract` 契约字典同步更新（契约键变更须记入
     README 第 8 节变更日志）；
   - CI 双架构绿。
4. **上游零补丁**：任何功能通过模块内覆写/自注册实现（参见
   README 第 6 节与 moldingStage/hMeltThermo 的既有模式），不得修改
   `/opt/openfoam14` 或引入 upstream patch。
5. **验证环境**：Linux（本仓库 Multipass VM `of14`，Ubuntu 24.04
   arm64 + openfoam14 官方二进制）。宿主 macOS 只做编辑，构建一律
   `scripts/vm-sync.sh` 后在 VM 内执行（该脚本依赖宿主挂载
   `~/moldingFoam`；挂载未激活时 VM 内该目录为空，需先重新挂载或
   改用 tar 同步到 VM 原生目录）。
6. **引用代码用函数/符号名，不写行号**：行号随每次编辑失效
   （本目录已清理过一批）；跨文件引用写 `<类名>::<函数>()` 或文件路径。
7. **历史不改写，更正用"补记"**：任务/报告文档是带日期的记录，后续
   结论变化时追加「复核/补充 YYYY-MM-DD」小节，不修改原文数字。
8. **阴性同样落文档**：试用无效/被否决的方案必须写清"测了什么、数字
   多少、为什么不用"，避免后人重复。
9. **数字的取证口径**：墙钟类数字必须标"同会话对照"（VM 跨会话方差
   20–25%），守恒/物理量类给到提交哈希；只给结论不给口径的数字视为
   未取证。
10. **用例目录不可无脑复用**：求解器会把已注册相场写进 `0/`，跑过的
    目录初态已变；测试统一走 `mktemp -d` 新鲜副本（`run-solver-tests.sh`
    的每个 pass），`run-case.sh` 在 git 树内恢复 `0/`（任务 055）。
11. **文档体检（手动，不入 CI）**：`python3 scripts/docs/check-docs.py .`
    （位于 `scripts/docs/`，与跑 case 的脚本分开）检查索引覆盖、状态字段、
    失效行号/用例/章节引用；整理文档后顺手跑一次。
12. **测试口径**：本地只跑受影响的最小用例（秒级/分钟级），完整套件
    （`test-solver` 全量、契约验收、并行矩阵）交给 nightly CI；墙钟类
    结论必须同会话对照。
13. **排查口径（最小复现优先）**：定位问题先把现场压成秒级复现
    （砍 `endTime`/步数/网格，试串行与并行），再做**单变量对照矩阵**
    并落一行证据表；准备一个已知干净的对照运行以区分"通病"与"特定"；
    探针先在有界与自证上过关（先在干净运行上跑）；阴性结论同样落文档。
    完整手法见 [`diagnostics.md`](diagnostics.md)。

## 文档地图

| 文档 | 作用 | 阅读时机 |
|------|------|----------|
| 本文件 | 任务索引 + 约定 + 基线速查 | 每次开工 |
| `tasks/NNN-*.md` | 单任务：背景/方案/DoD/验收记录 | 接活与验收时 |
| `review-2026-09-12.md` | 历史快照：完成度/精度债务/性能瓶颈 | 了解全局由来 |
| [`review-2026-09-15.md`](review-2026-09-15.md) | 当前代码复核：闭合/耗散、组合状态、覆盖与效率缺口 | 查本轮审查结论及最小证据 |
| `coverage-audit-2026-09-13.md` | 四层测试通路与缺口 G1–G11 | 查测试覆盖 |
| `ablation-audit-2026-09-13.md` | 特性消融与断言灵敏度 | 查断言强度 |
| `uncertainty.md` | 收敛性/定标/换算（活文档） | 做精度与门槛决策 |
| `diagnostics.md` | 排查手册：最小复现、对照矩阵、探针纪律（活文档） | 定位崩溃/挂起/环境类问题 |
| `report-037-heap-crash.md` | bundle 退出崩溃取证 | 动 bundle/验收判据 |
| `report-038-kairos-v023-retest.md` | Kairos 侧复测记录 | 对接 Kairos |

## 基线速查（一句话版，权威值在 README）

- 契约 case：**v1.29** → 守恒 **5.010e-04**（阈值 1e-3）、全周期
  **15,212 步**（完整口径与历史见文末「背景速览」与 README 第 8 节）；
- 数值定标：守恒误差 **∝ dt**（同网格）且固定 dt 下随加密一阶变差；
  余量↔成本换算见 `uncertainty.md`；
- 并行规则（三条血泪）：① 循环内的集合通信不能随 rank 变化的 patch/
  分支数变化（046）；② 函数体内的集合通信要求**所有**提前返回与分支
  全局一致（054）；③ 运行期求解器库只能有一份映射（两处各一份实体
  `libmoldingFoam.so` → np4 双载 → 退出堆破坏，串行不复现；056）。防线
  用例：`parallelMassBudget`；`parallelTrappedAir`
  待 harness 用例复用语义修复后落地（054 §5/§8）；库重载用
  `scripts/diag/check-lib-duplication.sh` 自检；
- 性能：墙钟热点是界面机制（050 剖面）；已落地杠杆见 README 第 7 节，
  阴性清单见 `tasks/041` §4y 与 052/053。

## 任务索引

| 编号 | 标题 | 优先级 | 状态 |
|---|---|---|---|
| [063](tasks/063-review-closure-state-consistency.md) | 审查问题收口：闭合、能量与历史状态一致性 | P1 | in-progress（R1–R8 本地修复与最小回归完成；完整 CI 待验收） |

### 当前专项收口（按风险分类，2026-09-15）

这些任务来自最近一次整体复核，按“结果正确性与状态恢复”“多物理耦合”“效率与验证”
分类。状态以各任务文件为准；`in-progress` 表示已有可审查的增量，但仍未满足共享 DoD。

#### 结果正确性与状态恢复（P1）

| 编号 | 标题 | 优先级 | 状态 |
|---|---|---|---|
| [064](tasks/064-checkpoint-cycle-contract.md) | 检查点完整性与多周期时间语义 | P1 | in-progress（非零 startTime 与串行续跑已验收；np2 重分解待验收） |
| [065](tasks/065-tait-domain-derivative-consistency.md) | Tait 有效域、截断导数与逆求解稳健性 | P1 | in-progress（导数/逆解/极端负压有限性已验收；正式越界策略待 nightly） |

#### 多物理耦合稳定性

| 编号 | 标题 | 优先级 | 状态 |
|---|---|---|---|
| [066](tasks/066-viscoelastic-transport-stability.md) | 粘弹性输运、材料限步与相区一致性 | P1 | in-progress（本地 transport/重启/两相最小闭环通过；高 We 与 CI 待验收） |
| [067](tasks/067-energy-accounting-coupling.md) | 完整能量账目与耦合精度约束 | P2 | in-progress（温度裁剪诊断已提交；完整能量账目待验收） |

#### 效率与验证基础设施

| 编号 | 标题 | 优先级 | 状态 |
|---|---|---|---|
| [068](tasks/068-runner-convergence-cache.md) | 流道非线性收敛诊断与重复求解优化 | P2 | in-progress（收敛告警与缓存失效键已验收；墙钟基准待 nightly） |
| [069](tasks/069-validation-harness-uncertainty.md) | 验证工具稳健性与精度覆盖收口 | P2 | in-progress（矩阵 nightly 入口已提交；九行组合待执行） |

#### 未完成项跟进顺序

| 顺序 | 下一步 | 退出条件 |
|---|---|---|
| 1 | 065：先做 Tait 极端负压/逆求解小模型，确定拒绝或正则化策略 | 有限性、单调性和逆残差断言通过 |
| 2 | 066：增加均匀剪切应力输运与材料限步对照 | 串/并行、重启和相区权重结果一致 |
| 3 | 067：建立压力功、黏性耗散、潜热和 clamp 的闭合账目 | 能量残差有界并进入 nightly |
| 4 | 068/069：补缓存失效键及三网格/三时间步验证矩阵 | 小基准通过，完整矩阵交 CI |

所有顺序均遵循 `diagnostics.md` 的最小复现和单变量对照原则；未满足共享 DoD
前不把任务标记为 done。

### 契约周期与工艺（17 项）

| 编号 | 标题 | 优先级 | 状态 | 依赖 | 预估规模 |
|------|------|--------|------|------|----------|
| [001](tasks/001-he-energy-predictor.md) | 潜热能量预报器（启用 latentHeat） | P0 | **done**（根因修正，非 he 方案；周期完备化后质量守恒 9.305e-04） | 无 | 1–2 周 |
| [002](tasks/002-mold-thermal-coupling.md) | 模具热耦合（集总参数模温模型） | P2 | **done** | 建议在 001 之后 | 3–5 天 |
| [003](tasks/003-venting-model.md) | 排气反压/困气模型 | P1 | **done**（A 反压 + B 困气诊断均已验收） | 006 | 3–5 天 |
| [004](tasks/004-molding-dict-runtime-reload.md) | moldingDict 运行时重载 | P1 | **done** | 无 | 半天 |
| [005](tasks/005-nightly-contract-case.md) | CI 夜间契约 case 回归 | P2 | **done**（运行验证待推送后手动触发） | 无 | 半天 |
| [006](tasks/006-contract-cycle-well-posedness.md) | 契约周期物理完备化（排气封堵/压力切换/闸口封冻） | P0 | **done**（质量守恒 9.383e-04，周期单调） | 无 | 2–4 天 |
| [007](tasks/007-viscous-dissipation.md) | 黏性生热（能量方程剪切耗散项） | P0 | **done**（解析 Couette 对拍 4.1e-4；契约 case 已开启） | 001/006 | 2–4 天 |
| [008](tasks/008-mold-conjugate-heat-transfer.md) | 模具三维传热（共轭传热 CHT） | P1 | **done**（路线 A 双区域 + VoF 充填能量守恒 0.24%；冷却水 1D 通道） | 002 之后 | 1–2 周 |
| [009](tasks/009-high-pressure-vof-conservation.md) | 高压可压缩界面守恒（40–100 MPa） | P1 | **done**（018 收口：massFixGlobal + Euler 口径，40 MPa 离散守恒 1.1e-5；标定 case 2026-09-13 入 nightly） | 006 | 1–2 周 |
| [010](tasks/010-gate-freeze-physics.md) | 闸口冻结物理（局部温度/剪切判据） | P2 | **done**（温度判据实现+用例；契约物理触发依赖 016） | 006/016 | 3–5 天 |
| [011](tasks/011-wall-slip.md) | 壁面滑移模型 | P2 | **done**（Navier 滑移 Couette 对拍 3.3e-9） | 无 | 3–5 天 |
| [012](tasks/012-multi-cycle-mold-steady-state.md) | 多周期模温与周期稳态 | P2 | **done**（周期循环 + 400 周期模温收敛验收） | 002/008 | 3–5 天 |
| [018](tasks/018-high-pressure-conservation-closeout.md) | 高压可压缩界面守恒收尾（009 收口） | P0 | **done**（定位 psi/rho 拆分；massFixGlobal 修正，40 MPa 1.1e-5） | 009 | 3–5 天 |
| [018a](tasks/018a-shrinkage-void-model.md) | 封冻后收缩空洞/负压建模（018 拆分） | P1 | **done**（PVT 指标 + voidFraction 场 + 精确 Tait + PVT 对拍 1.6e-6 + 质量预算 4.1e-4） | 018 | 1–2 周 |
| [030](tasks/030-multistage-process-profiles.md) | 多级注射/保压工艺曲线与过程控制（补充） | P1 | **done**（Function1 曲线 + switchTime + 用例） | 006/016 | 1–2 周 |
| [031](tasks/031-pressure-mass-consistency.md) | 保压压力方程质量一致定式（根治 018，去修正器） | P0 | **done**（根因=时间截断 dt^1.6；maxDeltaT 1e-4 → 无修正器 2.58e-4，176×） | 018 | 1–2 周 |
| [039](tasks/039-void-cavitation-closure.md) | 汽蚀空洞的闭锁约束与标定（033 跟进） | P1 | **done**（`moldingVoidClosure` 闭锁上限：void 与 Cv/Cc 无关、质量漂移 ≤0.13%、验证器含质量/闭锁判据；完全退化平衡仍待两场模块） | 033 | 1–2 天 |

### 多物理与材料（19 项）

| 编号 | 标题 | 优先级 | 状态 | 依赖 | 预估规模 |
|------|------|--------|------|------|----------|
| [013](tasks/013-warpage-shrinkage-residual-stress.md) | 翘曲/收缩/残余应力（超模块范围） | P3 | **done**（拆分为 013a/013b，均完成） | 001–012 | 数周起 |
| [013a](tasks/013a-shrinkage-indicators.md) | PVT 一致收缩/残余应力指标 | P3 | **done**（S 与热应力指标；用例 max(S) −0.008→0.298） | 001/002 | 3–5 天 |
| [013b](tasks/013b-structural-warpage.md) | 结构翘曲集成（顺序耦合，022 落地） | P2 | **done**（MVP：PVT 自由应变映射 + 双层收缩板基准 4.2% + 应力自平衡） | 013a/022 | 2–4 周 |
| [014](tasks/014-crystallization-kinetics.md) | 结晶动力学（半结晶聚合物） | P3 | **done**（Nakamura/Avrami + χ 场/潜热耦合；Jeffery 级解析验证） | 001 | 1–2 周 |
| [015](tasks/015-fiber-orientation.md) | 纤维取向与各向异性 | P3 | **done**（Folgar-Tucker + Jeffery 轨道 <1e-6；局部 a 场） | 001/002 | 2–4 周 |
| [016](tasks/016-runner-system-coupling.md) | 流道/热流道耦合 | P3 | **done**（1D 网络 + 入口/保压耦合；多浇口分流解析验证） | 001/006 | 1–2 周 |
| [019](tasks/019-mold-3d-conjugate-heat-transfer.md) | 模具三维共轭传热（008 落地） | P0 | **done**（四基准：多周期/Robin 冷却/002 极限/周期稳态；2026-09-13 多周期回归修复：`nCycles 6` + 验证器按顶出事件计周期、≥3 增量硬要求；三维水区受上游模块限制） | 008/002 | 1–2 周 |
| [022](tasks/022-warpage-shrinkage-mvp.md) | 翘曲/收缩 MVP（013 分阶段落地） | P1 | **done**（解析翘曲/残余应力 + `solidDisplacement` 三维悬臂基准 5.46%） | 001–012 | 2–4 周 |
| [023](tasks/023-residual-stress-model.md) | 残余应力模型 | P2 | **done**（1D 自平衡弹性解 + 解析验证） | 022 | 1–2 周 |
| [024](tasks/024-crystallization-integration.md) | 结晶动力学集成（014 落地） | P2 | **done**（随流输运 + η(χ) + DSC 标定工作流） | 001/014 | 1–2 周 |
| [025](tasks/025-fiber-orientation-integration.md) | 纤维取向集成（015 落地） | P2 | **done**（随流输运；各向异性黏度为增强） | 001/002/015 | 2–4 周 |
| [026](tasks/026-runner-system-integration.md) | 流道/热流道耦合集成（016 落地） | P2 | **done**（多浇口分流 + 热流道温度；阀浇口为增强） | 001/006/016 | 1–2 周 |
| [027](tasks/027-viscoelastic-constitutive-model.md) | 粘弹性本构模型 | P2 | **done**（本构+解析验证+fvModel 动量耦合） | 001/007 | 1–2 周 |
| [028](tasks/028-pressure-dependent-viscosity.md) | 压力依赖粘度模型 | P2 | **done**（CrossWlf D3 压致增稠验证 517.92→533.91 Pa·s） | 001 | 3–5 天 |
| [033](tasks/033-void-tension-field-model.md) | 空洞/张力场建模（018a 阶段 4） | P1 | **done**（会计式交付：tensionLimit 账目 + Tait 逆 + 用例预算 4.1e-4 无修正器；场耦合钉需两场模块，九次实验定论） | 018a/031 | 1–2 周 |
| [034](tasks/034-warpage-full-chain.md) | 结晶/纤维/粘弹耦合的收缩-翘曲全链 | P2 | **done**（全链 3a–3e + 自由收缩条机器精度；哑铃以 warpageAniso+shrinkBar 替代） | 024/025/027/013b | 3–5 周 |
| [036](tasks/036-3d-coolant-flow.md) | 三维冷却水流动（019 遗留） | P3 | **done**（路线 C `moldingChannelCooling`+coolantMold；路线 A `moldingCoolantFluid`+coolantWater/coolantWaterMold CHT 能量平衡 1.2e-7） | 019/008 | 3–6 周 |
| [040](tasks/040-tensor-eigenstrain-source.md) | 非均匀张量本征应变的域内源（034 跟进） | P2 | **done**（`moldingEigenstrain` 域内源 + 符号修正；永久基准 `eigenstrainGraded` 偏差 0.83%<1.5%，接入 xmake/nightly） | 034 | 1–2 天 |
| [062](tasks/062-prescribed-gate-flow.md) | 流量受控浇口（逐浇口给定流量，其余按阻力分配） | P2 | planned（Kairos T82/T83 的流量曲线；**已做一轮实现尝试并回退：模型层 4 条断言全绿证实语义可实现，用例层因 `PtrList` 含空项不可拷贝（BC 的 `clone()` 会拷贝网络）而全红；四条实测教训与显式拷贝构造的结论已写入 §7**；058 曾以"边界层已可表达"为由不做，**该论据对流量不成立**——有 runner 时 patch 曲线是网络总流量而非该浇口流量；语义/两个求解器改法/阀优先级/退化定义/测试与敏感性实验已全部写进文档，可直接执行） | 016/026/058/030 | 2–3 天 |
| [061](tasks/061-restart-continuity.md) | 重启续跑（`startFrom latestTime`）的连续性与状态持久化 | P2 | **done**（全部 case 原来都是 `startFrom startTime`、README 的"重启续读"声称零覆盖；新增 `validation/restartContinuity`：连续 vs 中途重启对拍，T 3.3e-8 / p 1.0e-7（写入精度量级），并**据此抓到并修复一个真 bug**——重启时 `moldingStage` 的 `gateSealTime_` 丢失（封闸 ramp 不一致）；状态行 `1 0 1 1 0.02` 两边逐字段一致，packing/闸口封冻/排气封堵三条路径都覆盖；顺带纠正 README 的状态载体表述——持久化走 patch 字典的 `T` 条目而非 `UniformDimensionedField` 本身；密封状态持久化留待带流动的载体） | 012/026/019 | 1 天 |
| [060](tasks/060-anisotropy-assertions.md) | 各向异性耦合的断言缺口（Lipscomb 黏度 / 各向异性热导率） | P2 | **done**（两条耦合的判据都已落地并各自验证敏感性：`validation/anisoConduction` 本征模衰减率对拍解析（0.036%，关掉实现 11.3% FAIL）、`validation/anisoViscosity` 壁面力增幅下界（ratio 4 为 +17.97%、关掉实现 0% FAIL）；均已入 nightly） | 034/040/025/043 | 1–2 天 |
| [059](tasks/059-dict-key-coverage-sweep.md) | 字典键覆盖扫描（未赋值键的分诊与取舍） | P3 | **done**（77 键扫描；补 `trapAirAlpha`/`deepMoldTemperature` 两处赋值，其余 8 个各附「不需要用例」的理由；产出排查纪律「改路径就确认覆盖」） | 043/044/058 | 半天 |
| [058](tasks/058-runner-tree-topology.md) | 一维流道拓扑扩展：任意树 + 逐浇口时序 | P2 | **done**（stage 1–3：`tree` 子字典 + 单层阻力加权不动点（上限 500）、`gateOpenTime`/`gateCloseTime` 阀时序、工艺曲线驱动网络总流量、非圆截面等效直径口径；模型测试 6 项 + 用例 `runnerTree`/`runnerValve`/`runnerProfile`） | 016/026/030 | 1–2 周 |

### 验证与不确定度（8 项）

| 编号 | 标题 | 优先级 | 状态 | 依赖 | 预估规模 |
|------|------|--------|------|------|----------|
| [017](tasks/017-benchmark-validation.md) | 基准验证（解析/文献基准） | P1 | **done**（Couette/滑移/Stefan 三项自动验收；商用对拍无授权条件） | 001/002/006 | 1–2 周 |
| [020](tasks/020-fountain-flow-benchmark.md) | 喷泉流基准验证 | P1 | **done**（前沿 0.008%、剖面 L2 1.64%、喷泉特征、时间收敛） | 001/006/017 | 1–2 周 |
| [021](tasks/021-weld-line-air-trap-prediction.md) | 熔接痕与气穴预测 | P1 | **done**（fillTime/airTrap 场 + 熔接痕对称性 0.554%） | 006/003 | 1–2 周 |
| [035](tasks/035-uncertainty-quantification.md) | 数值不确定性量化（网格/时间收敛，验证器收紧） | P2 | **done**（GCI/有界误差报告；阈值 10%→8%） | 017/022/013b | 1–2 周 |
| [043](tasks/043-untested-optional-branches.md) | 未触发可选分支的用例覆盖（gateSealRamp/深层热阻/χ-η/质量修正器） | P2 | **done**（四项全交付：gateSealRamp、massFixGlobal、深层热阻、χ-η 耦合） | 018/008/024/031 | 1–2 天 |
| [044](tasks/044-parameter-coverage-strength.md) | 参数覆盖补齐与模式-only 用例强化（pressureRamp 等） | P2 | **done**（潜热断言、G8 清零、pressureRamp 三路径、Nu 分支均交付；powerLaw 核实为扫描假阳性、原已被执行） | 038/006/012/016 | 1–2 天 |
| [049](tasks/049-external-benchmark-calibration.md) | 外部精度标定 MVP（公开 benchmark 对拍） | P2 | **done**（1D 润滑参照入 fountainFlow：实测 −3.09%，残差=离散壁面剪切 −3.13% 解析解释） | 017/020 | 1–2 天 |
| [051](tasks/051-accuracy-explanation-cleanup.md) | 精度解释缺口与验证强度清理（小项合集） | P3 | **done**（① 结案：细网格 L2 反升=验证器测站假象；②③④ 记录不做的理由） | 020/021/035/036/043 | 1–2 天 |

### 性能与并行（12 项）

| 编号 | 标题 | 优先级 | 状态 | 依赖 | 预估规模 |
|------|------|--------|------|------|----------|
| [029](tasks/029-solver-performance-optimization.md) | 求解器性能优化（并行/内存/大规模算例） | P1 | **done**（22.3 万单元基准、强扩展 1.49×@4、弱扩展 59%、内存报告） | 009/018 | 2–4 周 |
| [032](tasks/032-parallel-performance.md) | 并行与线性求解性能优化（百万单元预算） | P1 | **done**（1M 单元 2.0 GB/6.5 s·步；nSubCycles8 −26%；perf-scaling 含内存/每步；带宽受限为长期项） | 029/018 | 2–4 周 |
| [041](tasks/041-memory-traffic-longterm.md) | 内存流量优化的长期跟踪（032 跟进） | P3 | **done**（量化入口 + 计时矩阵：能量容差 10× → 墙钟 −24%、验收通过；未改缺省，记为按需选项） | 032 | 周级 |
| [045](tasks/045-platform-parallel-coverage.md) | 平台与并行覆盖范围（arm64 重型验证/并行矩阵） | P3 | **done**（口径决策 B：数值回归基线 x86_64、arm64 仅构建+模型测试、差异 1–10% 记录在案） | CI/032/041 | 1–3 天（含 CI 时间成本评估） |
| [046](tasks/046-parallel-boundary-patch-collectives.md) | 并行死锁：边界 patch 循环内的归约（issue #7） | P0 | **done**（根因＝processor patch 数按 rank 不同；4 进程真实件复测通过 + `parallelMassBudget` 防线） | 无 | 1–2 天 |
| [047](tasks/047-contract-grid-time-convergence.md) | 契约 case 网格/时间收敛（1e-3 余量归因） | P1 | **done**（误差 ∝ dt^1.0（三点 0.97–1.06）×网格一阶；自适应 dt 使加密更稳但墙钟 ~20–30×） | 031/035 | 1–2 天 |
| [048](tasks/048-cost-lever-combination.md) | 求解成本杠杆组合实测与每步通信削减 | P1 | **done**（组合 −43.6%；**E=组合+dt÷2 支配基线：−23% 墙钟且余量 6%→48%**，契约变更建议见 §3c） | 032/041 | 1–2 天 |
| [050](tasks/050-million-cell-capacity-profiling.md) | 百万单元产能：通信占比 profiling 与定位决策 | P2 | **done**（通信 7–10% ≪ 30%：不投上游通信改造；定位 10⁵ 单元级） | 032/041 | 2–4 天 |
| [052](tasks/052-interface-cost-levers.md) | 界面机制的下一批杠杆：alpha 修正器与 MULES 策略 | P1 | **done**（`nCorrectors 2→1`：同批 −30% 墙钟、守恒 5.010e-04，已入 v1.29；`MULESCorr yes` 阴性） | 047/048/050/009 | 1–2 天 |
| [053](tasks/053-adaptive-alpha-subcycles.md) | 自适应 alpha 子循环表（nSubCycles 的 Function1） | P1 | **done**（阴性 −2%，机理=填充期占 95–99% 步数；顺带量化 (nSubCycles,nCorrectors) 与熔接痕对称性的取舍） | 052 | 1 天 |
| [054](tasks/054-trapped-air-rank-divergence.md) | 并行死锁 #2：`reportTrappedAir` 的按 rank 提前返回 | P0 | **done**（fountainFlow @4 复现/修复前后对照；防线用例受 harness 状态污染影响，列后续；§8 记录 `0/` 污染发现） | 046 | 0.5–1 天 |
| [057](tasks/057-io-diagnostics-cost.md) | 性能收口：I/O 与诊断开销量化 + 剩余旋钮盘点 | P2 | **done**（两项都在噪声内：I/O 花磁盘不花时间、trapAir/writeFillTime 不可测） | 050/041/048 | 1 天 |

### 流程/防线与发布（5 项）

| 编号 | 标题 | 优先级 | 状态 | 依赖 | 预估规模 |
|------|------|--------|------|------|----------|
| [037](tasks/037-heap-corruption-exit-crash.md) | 退出阶段堆破坏崩溃（bundle 非零退出码） | P0 | **done**（bundle 根因=双份 .so 混载；打包清理+符号链接+inode 断言+金丝雀；E2E 复测通过） | 无 | 1–3 天 |
| [038](tasks/038-sample-case-fill-stability.md) | 样例 case 填充/稳定性诊断与参考配置（Kairos 10 mm 立方体） | P1 | **done**（诊断 + boxFill 19/19 + P1 防线：浇口速度预警/非有限快速失败 + P2 契约：冷却通道/D·sigma·sigmaEq 场） | 003/006 | 1 天 |
| [042](tasks/042-defence-regression-wiring.md) | 防线回归接入 nightly（037 堆退出 + 038 预警/快速失败） | P1 | **done**（smoke-exit 入 nightly contract；boxFill 断言预警；快速失败记录为不回归） | 037/038 | 0.5–1 天 |
| [055](tasks/055-case-reuse-semantics.md) | 测试流程的用例复用语义（`0/` 污染）与 054 防线落地 | P1 | **done**（每 pass 新鲜副本 + `parallelTrappedAir` 两端口验证：pre-054 FAIL / 修复后 PASS） | 054/046 | 0.5–1 天 |
| [056](tasks/056-heap-corruption-writepoint.md) | 037 堆破坏：写入点定位（模块二分 + 内存诊断） | P1 | **done**（根因＝库被映射两次：两处各有一份实体 `libmoldingFoam.so`；单份即 rc=0、串行不复现；19 步复现器 + 库重载自检入库） | 046/054/038 | 1–2 天 |
（001–054 已完成；055–058 为 2026-09-14 的跟进项（测试流程复用语义、037 写入点定位、I/O 与诊断开销、流道拓扑扩展），059 为 2026-09-15 的键覆盖收尾，060 为同轮延伸核查（各向异性耦合断言），061 为 2026-09-15 的重启续跑、062 为同轮发现的"流量受控浇口"缺口（设计已定稿），状态见上表。）

审计与报告（按时间）：

- [`review-2026-09-12.md`](review-2026-09-12.md)——完成度审计、精度债务、
  性能瓶颈（历史快照）；
- [`report-037-heap-crash.md`](report-037-heap-crash.md)——bundle 退出
  阶段堆破坏取证（含 2026-09-14 的栈与边界补充）；
- [`report-038-kairos-v023-retest.md`](report-038-kairos-v023-retest.md)
  ——Kairos 侧 v0.2.3 复测记录；
- [`uncertainty.md`](uncertainty.md)——收敛性、定标与余量↔成本换算
  （活文档）；
- [`diagnostics.md`](diagnostics.md)——排查手册：最小复现优先、单变量对照
  矩阵、探针纪律、环境/打包变量、机时纪律（活文档，配 §7 的 037/056 复盘）。
测试覆盖审计：[`coverage-audit-2026-09-13.md`](coverage-audit-2026-09-13.md)
（四层测试通路核对；缺口 G1–G11 已落为任务 042–045，见上表）。
消融与代码检查：[`ablation-audit-2026-09-13.md`](ablation-audit-2026-09-13.md)
（7 项特性消融的断言灵敏度；-Wreorder 告警清零与死代码扫描）。

### 追加任务评估（018–030，历史规划记录）

- **018/019 是 P0 收口**：009 的压力耦合重构（018）与 008 的完整周期
  三维 CHT 落地（019）是后续所有增强功能的物理基础，建议最先推进；
- **024/025/026 是已落地首阶段的“集成”任务**：014/015/016 已完成模型
  与局部场耦合，集成任务的实际范围是**随流输运、黏度/密度/热导率
  各向异性耦合、多浇口/热流道/阀浇口 case 与实验对拍**（见各任务
  背景节），不是从零开始；
- **022 承接 013a**：013a 已提供收缩/热应力指标，022 只需补结构求解
  与脱模释放；023 再补粘弹性残余应力；
- **新增 030（工艺曲线/过程控制）**：当前注射速度为常数、保压为
  table 曲线，多级速度/位置切换与阀浇口时序是真实工艺的缺口，且是
  016 多浇口的自然延伸，建议列为 P1；
- 其余补充评估：025 内应含各向异性热导率；029 应覆盖新增场（χ/a/
  shrinkage）的并行与 I/O 开销；020 的文献数据数字化精度需多源交叉。

建议顺序（与用户给定一致）：018 → 019 → 020/021 → 022 → 029 →
023–028（含 030）。

## 背景速览（新会话必读）

- 构建与依赖：`xmake`，OpenFOAM-14 解析顺序 `--of_src` >
  `/opt/openfoam14`（apt 官方二进制）；详见 README 第 2、3 节。
- 求解器：`foamRun` 模块 `moldingFoam`（继承 `compressibleVoF`），
  三阶段 M1 填充 / M2 保压 / M3 冷却顶出，阶段状态由
  `moldingStage`（regIOobject，注册于网格）承载，边界条件经网格
  注册表读取阶段。
- 材料模型：Tait 双域 PVT（`src/equationOfStates/Tait/`，单遍
  `blendedDerivs` 求值）、Cross-WLF 黏度、hMelt 潜热热力学
  （`src/thermo/hMeltThermo.*`）：潜热峰只进能量方程的表观 `Cv`，
  显热 `Cp` 保持平滑，避免 `kappa = Cp·mu/Pr` 在带内放大。
- **潜热已启用**：契约 case `latentHeat 2e5` 稳定运行至顶出（001 号
  任务已完成；根因与最终方案见其完成报告）。
- 集总参数模温模型已落地（002 号任务）：`0/T` 模壁可选类型
  `moldingMoldTemperature`（自注册边界，后向 Euler 隐式更新，离散
  能量守恒），缺省 `fixedValue` = 恒温模壁。
- 注塑周期已物理完备化（006 号任务）：排气口只透气、遇熔体密封；
  V/P 压力触发无阶跃；保压结束闸口封冻，冷却期不再排料，平均熔体
  温度单调。
- 验收基线：契约 case（`MOLDINGFOAM_PARALLEL=4`，`latentHeat 2e5`、
  1.3 MPa 保压、黏性生热开启、界面控制 `maxAlphaCo 0.015` /
  `nSubCycles 8` / alpha `nCorrectors 1` / 能量 `tol 1e-5`（v1.29））
  质量守恒相对误差 **5.010e-04**（阈值 1e-3，余量 50%；v1.27 的
  9.383e-04、v1.28 的 5.164e-04 为历史值），
  全部阶段证据 PASS；006 初次落地时为 9.305e-04。
- 回归面（2026-09-13 起）：nightly 覆盖模型测试、22 个求解器用例、
  **全部 17 个数值验证 case**（含此前漏排的 `highPressure`）与 4 子域
  契约；PR 级 CI 仅双架构构建 + 模型测试。多周期 CHT 验证器要求
  ≥4 个完成周期（≥3 个增量）——用例 `moldCHT-cycle` 为 `nCycles 6`、
  `endTime 60`，勿再压缩周期数。
