# 073：Dual Domain 输入字典、拓扑校验与独立生成器

- 状态：planned
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

遵循 [diagnostics.md](../diagnostics.md)：of14 环境、全新 case 副本、先秒级最小复现、
一次一个变量、失败与阴性结论均留证。本地只跑受影响的小测试；完整矩阵交 nightly，
不得以“已加入 CI”代替通过。遵守索引共享 DoD，精度优先于性能。
每批提交同步本任务状态和证据；尚未执行的项目写未测，不填推测数字。

| 提交/环境 | 输入摘要/规模/步数 | 修改前后精度或契约结果 | 质量/能量残差 | 墙钟/峰值内存 | 原始日志/CI |
|---|---|---|---|---|---|
| 规划阶段 | 未运行 | 未实现；无对比数据 | 未测 | 未测 | 无 |

新功能无旧实现时写“修改前不支持”，用解析解/共享材料基线作精度对照，不虚构加速比。
达到本任务验收标准与共享 DoD 后才能标 done；仅剩 nightly 时也保持 in-progress。

