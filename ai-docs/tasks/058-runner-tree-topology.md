# 058 — 一维流道拓扑扩展：任意树 + 逐浇口时序

- 状态：in-progress（stage 1 树拓扑已落地并验证：模型测试 + 新用例
  `runnerTree`；逐浇口时序/非圆截面口径待做）
- 优先级：P2
- 依赖：016/026（现有单 feed + N 并联 gate 网络）
- 预估规模：1–2 周
- 来源：Kairos 侧 T82/T83 的口径需求（本轮交接明确了两条"现无此选项"：
  任意树状流道、逐浇口流量/阀浇口时序）

## 1. 背景与现状

现有 `moldingRunnerNetwork`（016/026）：

- 拓扑固定为**单 feed 串联 + N 并联 gate**；每段 Hagen–Poiseuille
  `dp = 128·η·L·Q/(πD⁴)`（η 在 `γ̇ = 32Q/(πD³)` 处由 case 流变模型求值），
  并联支路按**等压降**固定点迭代分流；
- 字典键（v1.x 冻结）：`0/U` 的 `moldingInletVelocity.runner {…}` 与
  `0/p_rgh` 的 `moldingPrghPressure.runner`，`feed {length,diameter}`、
  `gates {<name>{length,diameter}}`、`gate <索引>`；
- 已知缺口（README 与交接答复均记录）：① 任意树状流道（多级分流）；
  ② 逐浇口的流量/压力曲线与阀浇口开/关时序；③ 非圆截面仅能按等效
  水力直径近似。

## 2. 目标

1. **任意树拓扑**：新增（不改旧键）
   `tree { node { parent <name>|feed; length; diameter; children (<names>) } }`
   或等价的分段列表，按"每级并联等压降"递归求解；旧 `feed`+`gates`
   键行为**逐位不变**（现有 `runnerNetwork`/`multiGate` 用例为回归门禁）；
2. **逐浇口时序（可选子项）**：每浇口一个 `Function1`（流量或压力目标）
   与阀开/关时刻（`gateOpenTime`/`gateCloseTime`，关闭后该支路流量为 0，
   网络重新分流）；与 030 的总流量曲线并存，语义优先级在文档写明；
3. **验证**：
   - 解析基准：定黏度/幂律下的分流比（含三级树的手算点）；
   - 非对称三浇口树用例（新增 `tests/cases/runnerTree` 或扩展现有）；
   - 现有 `runnerNetwork`/`multiGate`/`runnerTemperature` 用例不变 PASS；
4. **口径文档**：把"非圆截面 → 等效水力直径"的换算与限制写进 README
   （供 Kairos 的 1D/梁数据转换使用）。

## 2a. stage 1 尝试与回退（2026-09-14，补记）

在 `moldingRunnerNetwork` 上实装了树拓扑的第一版：`tree` 子字典解析
（分支名 → parent/`gate` 索引、两遍名字解析、`gates_` 的 gate 索引视图
保持 `gate(g)`/`nGates()` 接口不变）、递归的"子节点共享出口节点"求解
（对公共压降做二分 + 对子树流量做二分反解），并为**解析解**准备了两条
模型测试（定黏度 → 串并联电阻网络；幂律 → 合并系数 `a = Σ L/D^(3n+1)`）。

**回退原因**：嵌套二分（每层 ~30 次 × 反解内层 ~30 次 × 深度）在模型
测试里超时（`modelTests` > 90 s 未完成），说明该求解结构在"每次 BC 更新
都要调用"的生产路径上不可行（预算内未收敛到可接受结构）。已**整体回退**
（`git restore`，仓库回到全绿），本任务退回 planned。

**下一版的设计要点（避免同样的坑）**：
1. 不用"公共压降二分 + 流量反解"，改用**与既有 `split()` 同构的阻力加权
   固定点**（自顶向下：每个节点按 `Q_i ∝ 1/R_i`（`R_i = dp_i/Q_i`）重分配，
   子树的内部流量在递归中同步重分配），带欠松弛；既有 `split()` 在
   定黏度/幂律下都能收敛到解析解，同族的树版本应同样稳定；
2. 迭代次数按层固定（如每层 ≤30、总评估数设上限），并在模型测试里
   断言"求解耗时"上限（防止再次出现超时）；
3. 测试从**两级最小树**起步（1 个 manifold + 2 个 gate）再扩到三级，
   便于定位；
4. 逐浇口时序（子项 2）保持在 tree 求解收敛之后再动。

## 2b. stage 1 v2 落地（2026-09-15）

按 §2a 的要点重做，**没有嵌套求解**：

**实现**（`moldingRunnerNetwork.{H,C}`）：

- 新键 `tree { <节点>{ parent <节点|feed>; length; diameter;
  wallTemperature?; htc? } }`；`parent` 必填，叶子即浇口、按**字典顺序**
  编号并填进既有 `gates_` 视图（名字即节点名），因此
  `gate(g)`/`nGates()`/`gateFlow`/`gateTemperature` 与两个 BC 的按名寻址
  **全部不用改**；
- 解析期做拓扑构建与校验：父节点存在性、深度解析（解析不出来的节点＝
  成环或父链不达 `feed`，直接 FatalIOError）、子表、深度优先序（父在子前）、
  叶序；
- 求解：**单层**固定点——每轮先自上而下推节点温度（`Tin` = 父出口）与
  自下而上的**等效阻力**（`R_eq = R_own + 1/Σ(1/R_eq_child)`），再自上而下
  按 `q_child = q_node·(1/R_eq_child)/Σ(1/R_eq_sibling)` 分配流量，欠松弛
  0.5，收敛判据 `1e-14·max(Q, small)`；迭代上限
  `maxTreeIterations_ = 500`，成本 O(上限 × 节点数)，**与树深无关**；
- 旧路径（`feed`+`gates`）保持原代码不动，运行期按 `dict.found("tree")`
  分支——旧用例数值不变（见下）。

**验证结果**：

| 项目 | 结果 |
|------|------|
| 模型测试（4 项新增） | 全绿：叶序/两级定黏度对串联并联解析（rtol 1e-12）/扁平树与旧 `gates` 一致（1e-10）/三级幂律对合并系数 `a=ΣL/D^(3n+1)`（1e-6，实测到打印精度） |
| 求解耗时上界 | 2000 次三级树求解 **0.0296 s CPU**（断言 < 5 s；首版嵌套二分曾把 `modelTests` 拖超时） |
| 受影响旧用例 | `runnerNetwork`/`multiGate`/`runnerTemperature`/`gateFreeze` 全 PASS，数值与历史一致（multiGate 32.297 / 0.929%，runnerTemperature 499.9712 K / 0.0000%） |
| 新用例 `tests/cases/runnerTree` | 两级树、两浇口各带自己的管段：分流比 **15.381 vs 解析 15.249（0.869%）**，总流量误差 3.32% |

**本轮踩到的两个坑（记录以免重犯）**：

1. **共用管段会从分流比里约掉**：若两个浇口挂在**同一个** manifold 下，
   该段是两支路的公共串联阻力，分流比与扁平网络**完全相同**（都是
   `(D1/D2)^(3+1/n)`）——用例因此先失败（求解器给的 32.27 是对的，我写的
   期望 8.19 是错的）。要让 `tree` 与扁平网络产生可区分的分流比，各支路
   必须**各有自己的管段**；`README` 的「1D 流道网络」小节已写明这条。
2. **幂律解析式的 π 别漏**：`Δp = 128·K·(32/π)^(n-1)·L·Q^n/(π·D^(3n+1))`，
   漏掉最后一个 `/π` 会让期望值偏大 π 倍（模型测试的解析对拍当场抓到）。

**剩余（本任务未完）**：逐浇口 `Function1`（流量/压力目标）与
`gateOpenTime`/`gateCloseTime` 阀时序、关闭支路阻力置 `great` 后的重分流；
非圆截面→等效水力直径的换算口径写进 README；三级树端到端用例（可选，模型
测试已覆盖解析）。

## 3. 技术方案

- 求解器结构：把 `pressureDrop`/`split` 的"feed + 并联 gates"推广为
  递归的段/节点结构；保持等压降迭代与剪切变稀反馈（现实现已验证）；
- 字典解析：新键优先；旧键存在时走原路径（编译期分支不做，运行期判断）；
- 阀关闭：`gateOpenTime/CloseTime` 生效后，该支路的 `Q=0`，其余支路重新
  分配（迭代中把关闭支路的阻力设为 `great`）；
- 与压力边界耦合：`moldingPrghPressure.runner` 的"目标 − 当前流量下的
  压降"沿用；逐浇口压力目标时按支路分别求（仅当子项 2 落地）。

## 4. 验收标准（DoD）

- 旧用例逐位不变（`xmake run test` + `test-solver` 全绿）；
- 新拓扑的解析对拍（分流比 ≤1%）与新用例；
- 文档：README「1D 流道网络」小节更新（拓扑、键、限制、换算口径）。

## 5. 风险与缓解

- 拓扑推广会动到现有的固定点迭代的收敛性 → 保留原路径做 A/B，
  新旧在同一 case 上对拍；
- 阀时序会引入新的不连续（切换时刻）→ 用 time-step 级的时间控制并
  在用例里断言切换前后流量守恒。

## 6. 涉及文件

- `src/moldingFoam/moldingRunnerNetwork.{C,H}`
- `src/moldingFoam/boundaryConditions/moldingInletVelocity/`、
  `moldingPrghPressure/`（字典解析）
- `tests/cases/runnerNetwork/`、`tests/cases/multiGate/`、新增树用例
- `README.md`（§6「1D 流道网络」）

> 相关：016/026（现有网络）、030（工艺曲线）。
