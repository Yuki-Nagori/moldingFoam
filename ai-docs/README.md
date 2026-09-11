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
   `scripts/vm-sync.sh` 后在 VM 内执行。

## 任务索引

| 编号 | 标题 | 优先级 | 状态 | 依赖 | 预估规模 |
|------|------|--------|------|------|----------|
| [001](tasks/001-he-energy-predictor.md) | 潜热能量预报器（启用 latentHeat） | P0 | **done**（根因修正，非 he 方案；周期完备化后质量守恒 9.305e-04） | 无 | 1–2 周 |
| [002](tasks/002-mold-thermal-coupling.md) | 模具热耦合（集总参数模温模型） | P2 | **done** | 建议在 001 之后 | 3–5 天 |
| [003](tasks/003-venting-model.md) | 排气反压/困气模型 | P1 | **done**（A 反压 + B 困气诊断均已验收） | 006 | 3–5 天 |
| [004](tasks/004-molding-dict-runtime-reload.md) | moldingDict 运行时重载 | P1 | **done** | 无 | 半天 |
| [005](tasks/005-nightly-contract-case.md) | CI 夜间契约 case 回归 | P2 | **done**（运行验证待推送后手动触发） | 无 | 半天 |
| [006](tasks/006-contract-cycle-well-posedness.md) | 契约周期物理完备化（排气封堵/压力切换/闸口封冻） | P0 | **done**（质量守恒 9.305e-04，周期单调） | 无 | 2–4 天 |
| [007](tasks/007-viscous-dissipation.md) | 黏性生热（能量方程剪切耗散项） | P0 | **done**（解析 Couette 对拍 4.1e-4；契约 case 已开启） | 001/006 | 2–4 天 |
| [008](tasks/008-mold-conjugate-heat-transfer.md) | 模具三维传热（共轭传热 CHT） | P1 | **done**（路线 A 双区域 + VoF 充填能量守恒 0.24%；冷却水 1D 通道） | 002 之后 | 1–2 周 |
| [009](tasks/009-high-pressure-vof-conservation.md) | 高压可压缩界面守恒（40–100 MPa） | P1 | **in-progress**（机理与 Pareto 扫描完成；压力耦合重构待做） | 006 | 1–2 周 |
| [010](tasks/010-gate-freeze-physics.md) | 闸口冻结物理（局部温度/剪切判据） | P2 | **done**（温度判据实现+用例；契约物理触发依赖 016） | 006/016 | 3–5 天 |
| [011](tasks/011-wall-slip.md) | 壁面滑移模型 | P2 | **done**（Navier 滑移 Couette 对拍 3.3e-9） | 无 | 3–5 天 |
| [012](tasks/012-multi-cycle-mold-steady-state.md) | 多周期模温与周期稳态 | P2 | **done**（周期循环 + 400 周期模温收敛验收） | 002/008 | 3–5 天 |
| [013](tasks/013-warpage-shrinkage-residual-stress.md) | 翘曲/收缩/残余应力（超模块范围） | P3 | deferred（拆分为 013a done + 013b/022） | 001–012 | 数周起 |
| [013a](tasks/013a-shrinkage-indicators.md) | PVT 一致收缩/残余应力指标 | P3 | **done**（S 与热应力指标；用例 max(S) −0.008→0.298） | 001/002 | 3–5 天 |
| [014](tasks/014-crystallization-kinetics.md) | 结晶动力学（半结晶聚合物） | P3 | **done**（Nakamura/Avrami + χ 场/潜热耦合；Jeffery 级解析验证） | 001 | 1–2 周 |
| [015](tasks/015-fiber-orientation.md) | 纤维取向与各向异性 | P3 | **done**（Folgar-Tucker + Jeffery 轨道 <1e-6；局部 a 场） | 001/002 | 2–4 周 |
| [016](tasks/016-runner-system-coupling.md) | 流道/热流道耦合 | P3 | **done**（1D 网络 + 入口/保压耦合；多浇口分流解析验证） | 001/006 | 1–2 周 |
| [017](tasks/017-benchmark-validation.md) | 基准验证（解析/文献基准） | P1 | **done**（Couette/滑移/Stefan 三项自动验收；商用对拍无授权条件） | 001/002/006 | 1–2 周 |
| [018](tasks/018-high-pressure-conservation-closeout.md) | 高压可压缩界面守恒收尾（009 收口） | P0 | **done**（封冻前守恒 5.02e-4；浇口通量一致性修正） | 009 | 3–5 天 |
| [018a](tasks/018a-shrinkage-void-model.md) | 封冻后收缩空洞/负压建模（018 拆分） | P1 | planned | 018 | 1–2 周 |
| [019](tasks/019-mold-3d-conjugate-heat-transfer.md) | 模具三维共轭传热（008 落地） | P0 | planned | 008/002 | 1–2 周 |
| [020](tasks/020-fountain-flow-benchmark.md) | 喷泉流基准验证 | P1 | **done**（前沿 0.008%、剖面 L2 1.64%、喷泉特征、时间收敛） | 001/006/017 | 1–2 周 |
| [021](tasks/021-weld-line-air-trap-prediction.md) | 熔接痕与气穴预测 | P1 | planned | 006/003 | 1–2 周 |
| [022](tasks/022-warpage-shrinkage-mvp.md) | 翘曲/收缩 MVP（013 分阶段落地） | P1 | **in-progress**（自由翘曲解析模型；结构耦合待做） | 001–012 | 2–4 周 |
| [023](tasks/023-residual-stress-model.md) | 残余应力模型 | P2 | **in-progress**（1D 自平衡弹性解；粘弹性待做） | 022 | 1–2 周 |
| [024](tasks/024-crystallization-integration.md) | 结晶动力学集成（014 落地） | P2 | **done**（随流输运 + η(χ) + DSC 标定工作流） | 001/014 | 1–2 周 |
| [025](tasks/025-fiber-orientation-integration.md) | 纤维取向集成（015 落地） | P2 | **done**（随流输运；各向异性黏度为增强） | 001/002/015 | 2–4 周 |
| [026](tasks/026-runner-system-integration.md) | 流道/热流道耦合集成（016 落地） | P2 | **done**（多浇口分流 + 热流道温度；阀浇口为增强） | 001/006/016 | 1–2 周 |
| [027](tasks/027-viscoelastic-constitutive-model.md) | 粘弹性本构模型 | P2 | **in-progress**（UCM/Giesekus 本构+验证；动量耦合待做） | 001/007 | 1–2 周 |
| [028](tasks/028-pressure-dependent-viscosity.md) | 压力依赖粘度模型 | P2 | planned | 001 | 3–5 天 |
| [029](tasks/029-solver-performance-optimization.md) | 求解器性能优化（并行/内存/大规模算例） | P1 | **in-progress**（网络缓存已落地；基准/profiling 待做） | 009/018 | 2–4 周 |
| [030](tasks/030-multistage-process-profiles.md) | 多级注射/保压工艺曲线与过程控制（补充） | P1 | planned | 006/016 | 1–2 周 |

### 追加任务评估（018–030）

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
  1.3 MPa 保压、黏性生热开启、界面控制 `maxAlphaCo 0.03` /
  `nSubCycles 16`）质量守恒相对误差 **9.383e-04**（阈值 1e-3），
  全部阶段证据 PASS；006 初次落地时为 9.305e-04。
