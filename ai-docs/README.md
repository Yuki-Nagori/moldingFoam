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
| [003](tasks/003-venting-model.md) | 排气反压/困气模型 | P1 | **in-progress**（选项 A 反压完成；选项 B 困气诊断待做） | 006 | 3–5 天 |
| [004](tasks/004-molding-dict-runtime-reload.md) | moldingDict 运行时重载 | P1 | **done** | 无 | 半天 |
| [005](tasks/005-nightly-contract-case.md) | CI 夜间契约 case 回归 | P2 | **done**（运行验证待推送后手动触发） | 无 | 半天 |
| [006](tasks/006-contract-cycle-well-posedness.md) | 契约周期物理完备化（排气封堵/压力切换/闸口封冻） | P0 | **done**（质量守恒 9.305e-04，周期单调） | 无 | 2–4 天 |
| [007](tasks/007-viscous-dissipation.md) | 黏性生热（能量方程剪切耗散项） | P0 | **done**（解析 Couette 对拍 4.1e-4；契约 case 已开启） | 001/006 | 2–4 天 |
| [008](tasks/008-mold-conjugate-heat-transfer.md) | 模具三维传热（共轭传热 CHT） | P1 | planned | 002 之后 | 1–2 周 |
| [009](tasks/009-high-pressure-vof-conservation.md) | 高压可压缩界面守恒（40–100 MPa） | P1 | planned | 006 | 1–2 周 |
| [010](tasks/010-gate-freeze-physics.md) | 闸口冻结物理（局部温度/剪切判据） | P2 | planned（评审：温度判据依赖 016 的流道热耦合） | 006/016 | 3–5 天 |
| [011](tasks/011-wall-slip.md) | 壁面滑移模型 | P2 | **done**（Navier 滑移 Couette 对拍 3.3e-9） | 无 | 3–5 天 |
| [012](tasks/012-multi-cycle-mold-steady-state.md) | 多周期模温与周期稳态 | P2 | **in-progress**（周期循环完成；模温稳态验证待做） | 002/008 | 3–5 天 |
| [013](tasks/013-warpage-shrinkage-residual-stress.md) | 翘曲/收缩/残余应力（超模块范围） | P3 | planned | 001–012 | 数周起 |
| [014](tasks/014-crystallization-kinetics.md) | 结晶动力学（半结晶聚合物） | P3 | planned | 001 | 1–2 周 |
| [015](tasks/015-fiber-orientation.md) | 纤维取向与各向异性 | P3 | planned | 001/002 | 2–4 周 |
| [016](tasks/016-runner-system-coupling.md) | 流道/热流道耦合 | P3 | planned | 001/006 | 1–2 周 |
| [017](tasks/017-benchmark-validation.md) | 基准验证（实验/商用软件对拍） | P1 | planned | 001/002/006 | 1–2 周 |

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
  1.3 MPa 保压）质量守恒相对误差 **9.305e-04**（阈值 1e-3），全部
  阶段证据 PASS。
