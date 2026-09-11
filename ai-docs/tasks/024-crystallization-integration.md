# 024 — 结晶动力学集成（014 落地）

- 状态：in-progress（2026-09-10：χ 随流输运已落地；η(χ)/ρ(χ) 耦合与 DSC 数据对拍待做）
- 优先级：P2
- 依赖：001/014
- 预估规模：1–2 周

验收记录（第一阶段：χ 随流输运）：

- 求解器在局部 Nakamura/Avrami 演化前，用混合体积通量对 χ 做隐式
  upwind 输运（`ddt(chi)+div(phi,chi)-Sp(div(phi),chi)`）并限幅
  [0,1]；启用结晶的 case 需在 `fvSchemes` 加 `div(phi,chi)` 与
  `fvSolution` 加 `(chi|chiFinal)` 求解器（已写入示例 case）；
- 无 `0/chi` 时内部创建的 χ 场默认 `zeroGradient` 边界（隐式对流
  需要可用的边界条件）；
- 集成用例 `tests/cases/crystallizationAdvection`：初始 χ=1 的型腔被
  χ=0 的新鲜熔体驱替，t=1.5 s 全充满后熔体加权平均 χ=0.029（阈值
  0.05，残量为壁面滞流熔体）；原 `crystallization` 用例回归通过；
- `η(χ)` 修正接口（第二阶段）：`CrossWlfCoeffs` 可选
  `crystallinity { chiInfinity; exponent; }`，动量黏度按
  `(1−χ/χ∞)^(−a)`（上限裁剪 1e6）放大；model test 手算点
  （1/4/25/裁剪）全部通过；结晶用例启用后回归通过（χ 仍单调到
  0.99999）；
- 待做：`ρ(χ)` 修正、DSC 数据对拍与参数标定流程。

## 1. 背景与现状

014 已落地 Nakamura/Avrami 模型、局部 χ 场与潜热能量耦合（用例 χ
单调有界到 0.99999）。缺口：χ 随流输运、结晶度对黏度/密度的修正、
DSC 数据对拍与参数标定流程。

## 2. 目标与非目标

### 目标

1. χ 随流输运（保守形式）；
2. `η(χ)`（如 `(1−χ/χ∞)^-a`）与 `ρ(χ)` 修正接口；
3. 等温/非等温 DSC 曲线对拍（rtol < 5%）；
4. 参数标定流程文档化。

### 非目标

- 晶体形态学/球晶尺寸分布。

## 3. 技术方案

- χ 输运：`ddt(αχ)+div(αφχ)` + 源项，保持有界；
- 黏度修正在 `CrossWlf` 调用点叠加 `f(χ)`；
- DSC 标定：由等温曲线拟合 n/K(T)，非等温验证。

## 4. 工作拆解

1. χ 输运实现与有界性；
2. 黏度/密度修正；
3. DSC 对拍与标定；
4. CI 与文档。

## 5. 验收标准（DoD）

- 等温结晶曲线与 DSC rtol < 5%；
- 非等温演化定性正确；
- 缺省不启用行为不变；CI 双架构绿。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 与固定潜热平台重复计量 | 启用 χ 时强制 `latentHeat 0` |
| 输运有界性 | MULES/限幅 + 断言 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingCrystallization.*` | 输运/耦合 |
| `src/viscosityModels/CrossWlf/` | η(χ) 接口 |
| `tests/cases/crystallization/` | 扩展 |
| `README.md` | §6 |
