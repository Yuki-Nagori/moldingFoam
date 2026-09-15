# 074：共享材料公式与厚度积分契约

- 状态：planned
- 优先级：P1
- 依赖：073 schema；072 版本核对
- 来源：2026-09-15 用户 Dual Domain / Kairos 扩展需求

## 现状与目标

复用 `src/viscosityModels/CrossWlf/` 的静态 readCoeffs/eta/etaValue 接口，
以及 `src/equationOfStates/Tait/`。先核对接口是否依赖 fvMesh，
必要时抽取 mesh-independent kernel，让三维与 DD 调用同一实现；
禁止复制第二套公式。拟新增 `src/dualDomainFoam/thicknessQuadrature/`
及 `tests/dualDomain/` golden fixture。

## 设计与工作拆解

规划取证（2026-09-15）：本地 v1.1.0 指向
`8cfbeaf546aaac6e82b1f39c353ce42706c476d3`；对该 tag 与当前 HEAD 的
CrossWlf/Tait 目录执行 git diff 无差异。CD 的 Set the release version
步骤在 bundle 前注入 tag，不回写开发默认值。此记录确认源码比较，
不替代未来共享实现重构后的 golden 和 Kairos 协议验收。

1. 核对 v1.1.0 release、当前源码与 Kairos 的材料协议；记录哈希、公式、
   单位、压力基准、温标、限制域与差异，未核实不得写“完全一致”。
2. Cross-WLF 明确 n、tauStar、D1、D2、D3、A1、A2；
   Tait 明确 b1m、b2m、b1s、b2s、b3、b3s、b4、b4s、b5、b6、C、smoothBand。
   同步记录 etaMin/etaMax、gammaDotMin、截断、可选增强项与默认值，
   不能仅比较上述主参数而遗漏改变结果的限幅。
3. 首版设计采用固定 8 点 Gauss–Legendre 积分，局部坐标 xi 属于 [-1,1]，
   z=h*xi/2，权重和=2，物理积分权重=h*w/2；正向固定为内侧到外侧。
   两表面边界位于 xi=±1，不误当成内部 Gauss 点。
   thermal 配点/通量算子需另行推导，不能直接把积分点当均匀网格。
4. 首版规则不运行时自适应；拒绝节点/权重与规则不符、非正权重、
   非有限数。增点需新规则版本和收敛证据。研究 4/8/16 点误差决定
   8 点是否够用；若不足，冻结 schema 前显式修订本设计而非放宽精度门槛。
5. 厚度突变使用单元自身 h 与守恒边通量，退化/非正 h 输入阶段拒绝。
   两面不同温度、黏度随厚度变化都必须参与积分。

## 验收标准

- quadrature 的对称性、归一化、常数/多项式积分 golden 独立于被测实现，
  8 点规则验证至 15 次多项式；物理厚度映射另测。
- Cross-WLF 高低剪切/压力/温度及限幅，Tait 熔体/固体/过渡带及密度导数
  均有独立高精度参考值、来源、容差，不由被测函数生成期望。
- 同一参数集的 DD/3D 材料值一致，抽取前后现有 modelTests 通过；
  无未授权 PP 参数混入仓库。材料摘要覆盖所有有效参数与模型版本。
- 提交材料对照表和积分阶数精度/成本记录；未核实 v1.1.0/Kairos 的条目不能 done。

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
