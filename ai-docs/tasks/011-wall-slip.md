# 011 — 壁面滑移模型

- 状态：planned
- 优先级：P2
- 依赖：无
- 预估规模：3–5 天

## 1. 背景与现状

当前所有壁面为无滑移（`noSlip`）。高剪切注塑（薄壁、浇口）中聚合物
熔体常出现壁面滑移，表观黏度下降、压降与充填模式改变；无滑移会高估
剪切应力与压降。

## 2. 目标与非目标

### 目标
1. 支持常用滑移模型：Navier 线性滑移（`u_w = β τ_w`）与
   Cross-WLF 经验滑移（`u_s = f(τ_w, T)`）；
2. 作为可配置边界（缺省无滑移，向后兼容）；
3. 与 CrossWlf 的壁面剪切一致评估。

### 非目标
- 滑移层的微观机理建模；
- 壁面滑移引起的压力振荡/不稳定（粘滑）模拟。

## 3. 技术方案

- 新边界类 `moldingSlipVelocityFvPatchVectorField`（动量方程 `U`）：
  以 `τ_w = μ_eff·∂u/∂n` 的局部切向应力计算滑移速度；
  实现为 `mixed`/`directionMixed`（切向 valueFraction 随 β/τ 变化）；
- 参数：`slipCoefficient`（Navier β [m/(Pa·s)]）或滑移模型字典；
- 温度依赖用 `thermo.T()` 的壁面值。

## 4. 工作拆解

1. 滑移本构选型与字典；
2. 新边界类（自注册、拷贝族、`updateCoeffs`）；
3. 解析对拍：Poiseuille/Couette 滑移解析解（滑移长度 b=βμ）；
4. 验证 case：薄壁充填压降对比；
5. 文档与契约日志。

## 5. 验收标准（DoD）

- 缺省无滑移回归；
- Couette/Poiseuille 滑移解析解对拍 rtol < 1e-6；
- 充填压降随滑移系数单调下降；
- CI 双架构绿。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| τ_w 的离散精度在壁面较差 | 用壁面附近的等效梯度/单侧差分；网格敏感性检查 |
| 与 VOF 界面壁面接触角/三相线耦合 | 先做单相验证，再全周期回归 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/boundaryConditions/moldingSlipVelocity/*` | 新增 |
| `src/Make/files` | 新源文件 |
| `tests/modelTests.C` 或新 case | 解析对拍 |
| `README.md` | §6/§8 |
