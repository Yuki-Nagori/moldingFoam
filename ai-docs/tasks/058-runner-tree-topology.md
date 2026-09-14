# 058 — 一维流道拓扑扩展：任意树 + 逐浇口时序

- 状态：planned
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
