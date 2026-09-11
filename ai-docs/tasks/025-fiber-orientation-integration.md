# 025 — 纤维取向集成（015 落地）

- 状态：done（2026-09-10：DoD 全部满足——Jeffery 轨道 <1e-6、取向场有界/trace=1、随流输运、缺省不变、CI；各向异性黏度为后续增强）
- 优先级：P2
- 依赖：001/002/015
- 预估规模：2–4 周

验收记录（第一阶段：取向张量随流输运）：

- 求解器在局部 Folgar-Tucker 推进前对 `a` 做隐式 upwind 输运
  （`ddt(a)+div(phi,a)-Sp(div(phi),a)`）+ 对称化与迹归一化；内部创建
  的 `a` 场默认 `zeroGradient` 边界；启用时需在 case 的 `fvSchemes`
  加 `div(phi,a)`、`fvSolution` 加 `(a|aFinal)`；
- 集成用例 `tests/cases/fiberOrientationAdvection`：初始 `a=xx` 的
  型腔被 `a=I/3` 的新鲜熔体驱替，t=1.5 s 平均 `a_xx=0.072`（阈值
  0.5），`max|tr(a)−1|=1e-8`；
- 原 `fiberOrientation` 用例回归通过（剪切取向 max|a12|=0.147，
  tr(a) 保持）；
- 待做：Lipscomb 各向异性黏度 `η(a, AR)`、各向异性热导率、实验对拍；
  closure 默认 quadratic（单纤维精确），Hybrid/ARD-RSC 可选。

## 1. 背景与现状

015 已落地 Folgar-Tucker 模型、局部 a 场与 Jeffery 轨道验证。缺口：
a 随流输运、Lipscomb 各向异性黏度、各向异性热导率、纤维浓度/断裂，
以及与实验（取向角/力学性能）的对拍。

## 2. 目标与非目标

### 目标

1. a 随流输运（张量输运 + 有界性）；
2. 各向异性黏度 `η(a, 长径比)` 与各向异性热导率；
3. 简单剪切/收缩流对拍与取向场输出；
4. 与实验/文献定性定量对照。

### 非目标

- 纤维断裂/浓度演化；长纤维。

## 3. 技术方案

- 输运：`ddt(a)+div(φ,a)` + FT 源，迹归一化与特征值限幅；
- Lipscomb：`η = η0(T,γ̇)·f(a, AR)`，在 CrossWlf 调用点叠加；
- 热导率张量由 a 构造。

## 4. 工作拆解

1. a 输运与稳定化；
2. 各向异性黏度/热导率；
3. 基准与实验对拍；
4. CI 与文档。

## 5. 验收标准（DoD）

- 简单剪切取向与 Jeffery/文献一致；
- 取向场有界、迹为 1；
- 缺省不启用行为不变；CI 双架构绿。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 张量方程刚性/有界性 | 成熟闭合 + 限幅 + 小步长验证 |
| 参数繁多 | 字典化 + 典型 GF/CF 缺省 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingFiberOrientation.*` | 输运/耦合 |
| `src/viscosityModels/` | 各向异性黏度 |
| `tests/cases/fiberOrientation/` | 扩展 |
| `README.md` | §6 |
