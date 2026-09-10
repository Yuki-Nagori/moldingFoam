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
| [001](tasks/001-he-energy-predictor.md) | he 型能量预报器（启用潜热） | P0 | **in-progress**（提示词已就位，待实现） | 无 | 1–2 周 |
| [002](tasks/002-mold-thermal-coupling.md) | 模具热耦合（集总参数模温模型） | P2 | planned | 建议在 001 之后 | 3–5 天 |
| [003](tasks/003-venting-model.md) | 排气反压/困气模型 | P3 | planned | 无 | 3–5 天 |
| [004](tasks/004-molding-dict-runtime-reload.md) | moldingDict 运行时重载 | P1 | **done** | 无 | 半天 |
| [005](tasks/005-nightly-contract-case.md) | CI 夜间契约 case 回归 | P2 | **done**（运行验证待推送后手动触发） | 无 | 半天 |

## 背景速览（新会话必读）

- 构建与依赖：`xmake`，OpenFOAM-14 解析顺序 `--of_src` >
  `/opt/openfoam14`（apt 官方二进制）；详见 README 第 2、3 节。
- 求解器：`foamRun` 模块 `moldingFoam`（继承 `compressibleVoF`），
  三阶段 M1 填充 / M2 保压 / M3 冷却顶出，阶段状态由
  `moldingStage`（regIOobject，注册于网格）承载，边界条件经网格
  注册表读取阶段。
- 材料模型：Tait 双域 PVT（`src/equationOfStates/Tait/`，单遍
  `blendedDerivs` 求值）、Cross-WLF 黏度、hMelt 潜热热力学
  （`src/thermo/hMeltThermo.*`，`latentHeat` 缺省 0）。
- 已知核心缺口：**潜热尚未在求解器中启用**——`compressibleVoF` 的
  能量预报器按 T 矩阵求解，带内 `Cv` 为负导致发散，需要 001 号任务
  的 he 型能量预报器。失败实验记录（max(Cv,Cp) 线性化）见 001 号
  文件的"已否决方案"。
- 验收基线：契约 case（`MOLDINGFOAM_PARALLEL=4`）质量守恒相对误差
  ≈ 7.3–7.4e-4（阈值 1e-3），全部阶段证据 PASS。
