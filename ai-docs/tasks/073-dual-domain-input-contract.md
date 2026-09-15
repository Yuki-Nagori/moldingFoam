# 073：Dual Domain 输入字典、拓扑校验与独立生成器

- 状态：in-progress
- 优先级：P1
- 依赖：072 约束；向 074–078 提供冻结 schema
- 来源：2026-09-15 用户 Dual Domain / Kairos 扩展需求

## 目标与文件规划

新增 `docs/dual-domain-input.md`、`src/dualDomainFoam/dualDomainMesh/`、
`scripts/dual-domain/generate-case.py` 与 `tests/dualDomain/` 输入 fixtures。
路径是拟新增交付，不表示已有实现。solver 最终读取
`constant/dualDomainMesh`、`constant/dualDomainProperties` OpenFOAM dictionary；
`system/controlDict` 显式选择 dualDomainFoam。无需普通体网格 polyMesh。
受控 JSON/CSV 仅是生成器入口，不能成为 solver 的隐式替代输入。

## 字段设计流程

1. 固定 schemaVersion、SI 单位、坐标系、零起始索引、长度/面积容差；
   nodes、triangles 及稳定全局 ID，记录生成器版本与源数据 SHA-256。
2. thicknessLocation 明确 triangle 或 node，必须恰好一种；
   节点值如何插值到三角形及积分点写清，不允许缺失默认厚度。
3. facePairs 记录中面单元与两侧表面的关联、源表面 ID、方向和匹配质量；
   sourceSurfaces 提供校验所需的几何/引用。冻结映射基数、覆盖率分母、
   重复归属规则和允许误差；首版要求所有活动单元匹配完整。
4. normalsInner/normalsOuter 与三角形绕序、厚度正向一致，建立局部切平面；
   显式检测翻转、相同方向两面、不可定向组件。禁止静默翻面修复。
5. gates/vents/walls 指向合法中面边及必要的区域，明确边界覆盖、
   互斥/冲突规则、连通性、压力与流量边界条件类型。
6. quadrature 引用固定积分规则、节点、权重及正负面温度；
   与 074 一致；记录面积、积分体积、厚度 min/mean/max、匹配率和摘要。
7. 生成器做尺寸换算并写最终 SI 字典；规范化摘要不依赖字典空白或 rank。
   不接受未知 schema；常规 moldingFoam 显式检测并拒绝 DD case。

## 验收标准

- 独立命令从受控输入生成 case，OF14 字典回读与摘要一致，含最小手写示例。
- 解析、索引越界、缺失/零/负/NaN 厚度、重复面、退化三角形、
  非流形边、重复/未覆盖边界、不完整匹配、翻转法向各有失败 fixture。
- 错误包含字典名、键、实体 ID 和原因，返回非零；MPI 输入失败全局退出而不挂起。
- 在求解前拒绝退化单元，不删除单元后继续伪造原网格统计。
  厚度跳跃保留原值，交给 075 的守恒通量；不进行隐式平滑。
- 接口文档给出完整可回读示例与迁移策略，并测试 DD/3D 错用与普通 3D 回归。

## 验证与状态纪律

## 补充：Kairos T102 联调入口（2026-09-15）

下游仓库 `~/eit/kairos`；契约来源
`ai-docs/reviews/t102-dualdomain-contract.md`。本次只读核对工作区
`src-crates/kairos-core/src/models/mesh.rs` 与
`services/dualdomain.rs::solver_input()`，当时 HEAD 为 `316ffe1`；
未运行导出或 solver，不视为 smoke 通过。

### 受控 JSON 入口

| 键 | T102 固定语义 | 适配检查 |
|---|---|---|
| schemaVersion | dual-domain/v1 | 不支持版本显式拒绝 |
| lengthUnit / thicknessUnit | mm / mm | 必须明确且匹配；不能默认为 m |
| nodes | [x,y,z] | 三个有限坐标 |
| triangles | 三节点索引，零起始 | 整数、范围、重复与面积 |
| thickness | 每三角形一个厚度 | 长度完全匹配、有限且严格正 |
| beams | nodes、diameter、kind | 两端索引、正直径、合法类型 |
| couplings | beam、endpoint、node、distance | 梁索引、endpoint 为 0/1、节点索引、有限非负距离 |

本入口不接受节点厚度替代 triangle 数组；073 的通用 schema 可扩展设计
不能改变 T102 v1 的语义。原始 JSON 保持 mm 不改写；生成 OF 字典时执行
显式、可审计的 mm→m 换算（乘 1e-3），记录 sourceUnits 与最终 dimensions，
用 golden 验证体积的 1e-9 比例，不能只改单位标签。
beam diameter 与 coupling distance 的单位、kind 枚举、重复耦合、
悬空端点和距离容差须与 Kairos 确认后冻结，不能凭字段名默认。

### 几何语义与求解门禁

当前 Kairos DTO 把 DualDomainMesh 描述为“表面 + 杆系”，另有独立 MidplaneMesh；
solver_input 直接复制表面 nodes/triangles，不执行中面构建。
因此不能把导出成功当成“已获得中面”的证据；先确认两面是否重复覆盖、
厚度配对和实体体积定义，避免 sum(A*h) 双计体积。

T102 JSON 目前不含 facePairs、内外法向、gate/vent/wall 区域、积分规则、
网格摘要。摘要可按输入计算，但双面匹配不能从“厚度为正”推定。
读取/网格摘要 smoke 可以独立通过；生成可求解 case 必须由显式配套字典
或双方确认的版本化扩展提供缺失语义，并通过完整校验。
缺失时报告契约不完整，不能默认匹配率 100%、默认厚度或隐式修补后求解。

### T102 验收顺序（先于材料/工艺接线）

1. JSON schema、单位、索引、厚度长度与 beam/coupling 负例。
2. Kairos sample-box 合成 fixture 回读单测，不依赖 Mug 数据；
   合成 fixture 的求解适用性单独判断。
3. 本地 Mug JSON 网格摘要与读取 smoke，保留真实退出码、时间戳及原始日志。
4. solver 的只读/校验入口确认节点、拓扑、厚度、梁/耦合关系；
   几何语义完整性门禁通过后才接材料与工艺字典。
5. 最后做 078/079 的 fill-pack-cool；读取成功不等于物理求解成功。

## 验证记录规则

### T102 experiment manifest 范围

纳入用户提供的 `tests/fixtures/dual-domain-v1-experiment-manifest.json`，
schema 为 dual-domain-experiment/v1，引用网格 fixture、PP-REF-01、
fill-pack-cool 和冻结工艺数值。相对路径以 manifest 目录为基准；
只读校验不修改 C/MPa/mm，不解析材料数值，不宣称可求解。
待实现的生成层必须明确将温度转 K、压力转 Pa、长度转 m，
并记录原始单位和转换；不可改标签而保留原数值。

### 首批实现记录（2026-09-15）

新增 `scripts/dual_domain_input.py`、`scripts/test-dual-domain-input.py`。
真实 STL 导出已在本地验证为 4,417 节点、8,834 三角形、厚度约
0.034–101.785 mm，但按 T102 资产边界不提交仓库；仓库 fixture 保持合成样例。
真实 manifest 的来源与规模元数据仅作为本地覆盖校验，不成为公开 CI 输入。
只读 JSON 适配校验保持 mm、逐三角形厚度，不生成 case 或求解。
16 个测试通过（宿主 Python，约 1.8 s；数据规模增大后仍仅为输入测试，非 solver 性能）：
正例、缺键/版本/单位、索引、厚度、坐标有限性、退化/重复/绕序、
非流形、beam/coupling 和 CLI 退出码/日志。PR CI 双架构已接线，尚未运行。

修改前：无 moldingFoam T102 读取入口或 fixture 单测。
修改后：合成 fixture 的 8 节点/12 面及厚度数组正确回读；本地真实覆盖输入
另记录 4,417/8,834 规模，但未作为提交内容或精度证据；
非法输入非零退出，成功仍标 solverReady=false。
精度与性能：仅输入契约通过；物理解精度、质量残差、墙钟和内存未测。
未完成：OF14 字典生成/读取与 FOAM IO 错误、配对/法向/中面语义、
边界/积分、MPI、Mug 本地读取和共享 DoD。不得将本批标 done。

补记：加入 manifest 后共 16 个测试通过（同次宿主测试总时间 0.198 s，
仅记录执行成本，不能与 solver 或先前测试数量不同的运行比较性能）。
新增覆盖工艺冻结值、曲线时刻重复/倒序/NaN、非法温度与时长、
材料/单位/阶段不匹配、网格引用与嵌套错误。
原始命令：`python3 scripts/test-dual-domain-input.py`；
fixture 与 manifest CLI 均 rc=0，solverReady=false、materialResolved=false。

遵循 [diagnostics.md](../diagnostics.md)：of14 环境、全新 case 副本、先秒级最小复现、
一次一个变量、失败与阴性结论均留证。本地只跑受影响的小测试；完整矩阵交 nightly，
不得以“已加入 CI”代替通过。遵守索引共享 DoD，精度优先于性能。
每批提交同步本任务状态和证据；尚未执行的项目写未测，不填推测数字。

| 提交/环境 | 输入摘要/规模/步数 | 修改前后精度或契约结果 | 质量/能量残差 | 墙钟/峰值内存 | 原始日志/CI |
|---|---|---|---|---|---|
| 规划阶段 | 未运行 | 未实现；无对比数据 | 未测 | 未测 | 无 |

新功能无旧实现时写“修改前不支持”，用解析解/共享材料基线作精度对照，不虚构加速比。
达到本任务验收标准与共享 DoD 后才能标 done；仅剩 nightly 时也保持 in-progress。
