# 078：Kairos 输出契约与可重复 Mug 样例

- 状态：planned
- 优先级：P1
- 依赖：073 schema；074 材料；075/076 求解；077 并行验收
- 来源：2026-09-15 用户 Dual Domain / Kairos 扩展需求

## 文件与资产计划

新增 `docs/dual-domain-kairos.md`、`samples/dualDomain/Mug/`、
`scripts/dual-domain/run-mug-smoke.sh` 与结构化输出/parser fixtures。
核对已有 `report/mug-moldflow/mug.stl` 的摘要、来源、授权和长度单位。
索取/接入 Kairos 的中面、厚度、双面匹配数据与有使用授权的材料系数，
不反向实现专有网格格式，不从报告“通用 PP”名称猜材料数据。

## Mug 工艺字典（由 case 传入）

| 参数 | 指定值 | solver 内 SI 值/约束 |
|---|---|---|
| 熔体温度 | 220 °C | 493.15 K |
| 型腔侧/型芯侧温度 | 各 50 °C | 各 323.15 K |
| 注射时间 | 5.5 s | 5.5 s；实际填满时间独立输出 |
| 保压曲线 | (0 s,0.9229 MPa)、(0.2 s,27.6282 MPa)、(315.0797 s,27.6282 MPa) | 压力为 922900、27628200、27628200 Pa；核实表压/绝压 |
| 冷却时间 | 20 s | 与保压的阶段关系按 076 明示并核对参考定义 |
| 参考体积 | 1163.6855 cm³ | 0.0011636855 m³ |
| 参考 DD 规模 | 33418 nodes、66830 triangles | 参考元数据，不强制改网格凑数 |

保压时间以 VP 为零点，参考工艺语义核对后冻结。
实际 DD 体积=sum(A*h)；与参考体积差异必须报告，不能缩放厚度偷偷对齐。
独立生成命令固定输入摘要、参数、输出 schema；缺失资产时明确未满足的验收。

## Kairos 字段映射设计（拟定键，需契约确认）

| 字段键 | 意义/单位 | 必须冻结的口径 |
|---|---|---|
| fillEndTime | 填充结束 s | 完成阈值和事件插值 |
| vpTime / vpPressure | VP 时间 s/压力 Pa | 全局时间、入口面积平均压力 |
| maxInjectionPressure / inletPressure | 注射期最大入口压力/当前入口压力 Pa | 多浇口聚合及压力基准 |
| meanFillTemperature | 平均填充温度 K | 已填充熔体质量加权，指定时刻 |
| maxShearStress / maxShearRate | Pa / s^-1 | 活动熔体所有厚度点及壁面重构极值 |
| partMass | 制件质量 kg | 制件区域、是否排除浇道 |
| clampForce / pressureIntegral | 锁模力 N/等效压力积分 N | 锁模方向、投影面积、参考压力、双面不重复计数 |
| massResidual / massResidualKg | 归一化/绝对残差 | 与 075 预算一致 |
| time / fillFraction | 每步 s/体积填充率 | sum(alpha*A*h)/sum(A*h) |
| solverVersion / schemaVersion | 版本 | 构建提交与 case schema 分开 |
| meshDigest / materialDigest | 摘要 | 规范化规则与算法 |
| endStatus / rawLog | 结束状态/日志路径 | completed、smoke_completed、failed、incomplete |

使用版本化 summary JSON 与每步 JSONL（最终以 Kairos 协议确认），非有限值不能
写伪合法 JSON。未到填满/VP/顶出的 smoke 字段写 null + reason，不能写 0 冒充测量。
原始 solver 日志明确打印名称/version/schema、节点/面数、厚度统计、匹配率、
所有 Cross-WLF/Tait 有效参数、保压曲线、冷却时间、质量预算和结束状态。

## 验收标准

- 真实 Mug DD 数据由独立脚本生成并真实启动 dualDomainFoam，
  有界 smoke 正常结束；仅有 STL/合成薄板不能替代该验收。
- smoke 与完整参考设置分开，截短时间/减小网格分别记录差异；
  参考网格缺失或材料未核实不能宣称 Mug 完全一致基线。
- parser schema 测试与 Kairos 实际消费者或其可执行契约 fixture 通过，
  字段覆盖、null 语义、单位/时间/压力口径一致；记录兼容版本。
- 失败返回非零并保留原始日志，summary 不覆盖失败状态。

## 验证与状态纪律

遵循 [diagnostics.md](../diagnostics.md)：of14 环境、全新 case 副本、先秒级最小复现、
一次一个变量、失败与阴性结论均留证。本地只跑受影响的小测试；完整矩阵交 nightly，
不得以“已加入 CI”代替通过。遵守索引共享 DoD，精度优先于性能。
每批提交同步本任务状态和证据；尚未执行的项目写未测，不填推测数字。

| 提交/环境 | 输入摘要/规模/步数 | 修改前后精度或契约结果 | 质量/能量残差 | 墙钟/峰值内存 | 原始日志/CI |
|---|---|---|---|---|---|
| 规划阶段 | 未运行 | 未实现；无对比数据 | 未测 | 未测 | 无 |

新功能无旧实现时写“修改前不支持”，用解析解/共享材料基线作精度对照，不虚构加速比。
达到本任务验收标准与共享 DoD 后才能标 done；仅剩 nightly 时也保持 in-progress。

