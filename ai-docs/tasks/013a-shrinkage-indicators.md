# 013a — PVT 一致收缩/残余应力指标（013 拆分）

- 状态：done（2026-09-10）
- 优先级：P3
- 依赖：001/002
- 拆分说明：013 的结构翘曲需要结构求解器配合（超出单模块范围），
  拆分为 013a（本任务：PVT 一致的体积收缩与残余热应力**指标**）与
  013b（结构耦合翘曲，planned）。

## 目标与验收

- `Foam::moldingShrinkage`：由局部熔体密度给出自由体积收缩
  `S = 1 − ρ_ref/ρ`（ρ_ref 为固态参考密度），并给出约束板残余热应力
  指标 `σ = E/(1−ν)·α·(T_ref−T)`；
- model tests：`S(ρ_ref)=0`、`S(1.05ρ_ref)=1−1/1.05`、低压膨胀为负；
  热应力指标手算点（rtol 1e-12）；
- 求解器：`constant/moldingDict` 可选 `shrinkage` 子字典，创建并写出
  `shrinkage` 场（每步由 `rho.melt` 更新）；周期重置复位；
- 集成用例 `tests/cases/shrinkage`（开放通道冷却）：max(S) 由
  −0.008 增长到 0.298（熔体冷却致密并受压）；`xmake run test-solver`
  9 用例全绿；
- 缺省不写 `shrinkage` 时行为不变。

## 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingShrinkage.{H,C}` | 收缩/应力指标模型 |
| `src/moldingFoam/moldingFoam.{H,C}` | 可选 shrinkage 场 |
| `tests/modelTests.C` | 模型测试 |
| `tests/cases/shrinkage/` | 求解器用例 |
| `scripts/verify-shrinkage.py` | 数值验证 |
| `README.md` | §6/契约日志 |

## 后续（013b）

结构耦合翘曲（热弹性/粘弹性、脱模约束释放）需 `solidDisplacement`
类结构求解器；本任务只提供自由收缩与残余应力指标，不预测翘曲位移。
