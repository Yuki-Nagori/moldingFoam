# 018 — 高压可压缩界面守恒收尾（009 收口）

- 状态：in-progress（2026-09-10：平滑封冻已实现；诊断收敛到 α 质量通量尖峰 + 封冻腔压力衰减，修正待做）
- 优先级：P0
- 依赖：009
- 预估规模：3–5 天

### 进展与诊断（2026-09-10）

- **平滑闸口封冻已实现**：`moldingStage::gateSealFactor(t)` + 可选
  `packing.gateSealRamp`（缺省 0，行为不变）；`moldingInletVelocity`
  与 `moldingPrghPressure` 在斜坡内混合到零通量。40 MPa 标定
  （`gateSealRamp 0.05`）守恒 4.49e-2 vs 瞬时封冻 4.57e-2——**封冻
  瞬态不是主因**；
- **主因定位**：高压斜坡/封冻期闸口 `alphaRhoPhi.melt` 出现巨大
  **虚假出流尖峰**（峰值 0.66 kg/s，而正常注入约 7.5e-5 kg/s；2278 个
  正样本），污染通量积分并对应真实的 α 质量通量不一致；叠加封冻后
  冷却收缩时压力未按 pvT 回落（储存质量 0.4 s 内虚假增长 ~2%）；
- **通量一致性修正已实现**（覆写 `prePredictor`）：浇口处强制
  `alphaPhi1 = alpha1*phi`、`alphaRhoPhi1 = rho1*alpha1*phi`，消除
  α 输运在快速压力瞬态下与总通量的不一致；16 个小用例回归全绿；
  小型 40 MPa 密封腔实验（全熔体）显示剩余尖峰源在 `phi` 本身
  （压力-速度耦合的瞬态），量级为物理压缩注入（~4e-3 kg/s）的数倍，
  需与封闭腔压力-密度耦合一并解决；
- 因此收口需要：(a) 闸口 α 质量通量一致性修正（用质量通量校正 α
  通量/抑制边界尖峰，保持 MULES 有界性）；(b) 封闭腔压力-密度-温度
  耦合的稳定离散。两项均在模块内覆写实现。

## 1. 背景与现状

009 已完成机理研究与界面 Pareto 扫描：现状 1.3 MPa 契约 case 质量
守恒 9.38e-04；40 MPa 标定（`validation/highPressure`）显示两条路径——
含气约 5% 时稳定但误差 3.3e-3，全熔体（残余气 0.25%）时封冻/减压段
数值失稳（NaN）。瓶颈是**封冻后近不可压缩熔体的压力-密度-温度耦合**
与界面压缩通量的一致性。

## 2. 目标与非目标

### 目标

1. 40 MPa（可扩展至 100 MPa）保压 case 质量守恒 < 1e-3；
2. 全周期稳定（含封冻、减压、冷却）；
3. 给出误差随保压压力单调可解释的标定曲线；
4. 现有 1.3 MPa 契约 case 误差不劣化，纳入 CI 夜间回归。

### 非目标

- 界面锐化算法重写；
- 相变/多组分输运。

## 3. 技术方案

1. **平滑闸口封冻**：`moldingStage` 暴露封冻斜坡系数，`moldingInletVelocity`
   与 `moldingPrghPressure` 在斜坡内混合到零通量，消除瞬时封冻的压力波；
2. **封冻腔压力-密度耦合**：为封闭域引入参考压力/`pcorr` 式处理或
   ψ 的隐式线性化，稳定近不可压缩熔体的冷却收缩；
3. **α 质量通量一致性修正**（若 1/2 不足）：在模块内覆写修正项，
   保持 MULES 有界性；
4. 用 `validation/highPressure` 标定 10/20/40/80 MPa 误差曲线。

## 4. 工作拆解

1. 复现 40 MPa 全熔体失稳，定位到具体项；
2. 实现平滑封冻并验证稳定性；
3. 实现/验证封冻压力耦合；
4. 标定误差-压力曲线，更新 case 与 CI；
5. 文档（README §6/§8、009 收口记录）。

## 5. 验收标准（DoD）

- 40 MPa case 质量守恒 < 1e-3（阈值不放松）；
- 1.3 MPa 契约 case 不劣化；
- 误差-压力曲线单调可解释；
- `xmake run test` / `test-solver` / `case-contract` / 新高压 case 全绿；
- CI 双架构绿。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 平滑封冻改变 V/P 切换物理 | 斜坡可配、缺省 0 保持旧行为 |
| 压力耦合重构影响低压 case | 以 1.3 MPa 契约回归为门槛 |
| 修正项破坏 MULES 有界性 | 小步长对照 + 有界性断言 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/moldingStage.{H,C}` | 封冻斜坡状态 |
| `src/moldingFoam/boundaryConditions/moldingInletVelocity/` | 斜坡混合 |
| `src/moldingFoam/boundaryConditions/moldingPrghPressure/` | 斜坡混合/压力耦合 |
| `src/moldingFoam/moldingFoam.C` | 压力-密度耦合修正 |
| `validation/highPressure/` | 标定 case 与验证器 |
| `README.md`、`ai-docs/tasks/009-*.md` | 收口记录 |
