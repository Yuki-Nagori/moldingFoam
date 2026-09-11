# 036 — 三维冷却水流动（019 遗留）

- 状态：planned（路线 C 设计已定稿，见下；实现待排期）（2026-09-12：路线 C 设计定稿——仿 `moldingRunnerTemperature` 模式新增模具侧冷却水 BC）
- 优先级：P3
- 依赖：019（调研：v14 `incompressibleFluid` 等温，无温度场）、008
- 预估规模：3–6 周

## 1. 背景

019 的三维水区因上游 `incompressibleFluid` 模块无温度场而搁置，当前
以 `moldingCoolantChannel`（1D）+ `moldingConvectiveCooling`（Robin）
覆盖等效冷却。真实随形冷却水道需要三维对流换热。

## 2. 目标

1. 非等温不可压水区求解器（自研模块或扩展 `incompressibleFluid` 的
  能量方程），可参与多区域 CHT；
2. 基准：圆管/矩形管对流换热（Gnielinski/Dittus-Boelter）偏差 ≤10%；
   模具温度场受水道影响定性正确；
3. 缺省不影响现有 case；CI 双架构绿。

## 2a. 路线 C 设计（2026-09-12，第一实现目标）

用现有 `moldingCoolantChannel`（1D 稳态水沿程温度）驱动**模具侧壁面
温度 BC**，无需三维水求解器即可获得轴向变化的冷却强度：

- 新 BC `moldingChannelCooling`（对标 `moldingRunnerTemperature` 的
  实现模式）：
  - 输入：`coolant`（1D 通道字典：入口温度/流量/直径/htc 关联式）、
    `channelAxis`（模具坐标到通道轴向坐标的映射，如沿 x 的直线）；
  - 壁面温度 = 通道局部水温经通道壁热阻/对流：
    `Twall = Tw(x) + q''/h_w`，或直接取 `Tw(x)` 作为 Robin 的环境温度
    并沿用 `h`；
  - `x` 由 patch 面坐标投影到通道轴获得（`patch().Cf()` 点乘轴 + 原点）；
- 验证：直管模具块（多区域 CHT）——水温沿程温升与 1D 解析
  `Tout = Twall + (Tin−Twall)exp(−hP L/(mdot cp))` 对拍；模具温度沿
  通道轴呈梯度（定性 + 与均匀冷却的差异）；
- 限制（如实记录）：水质点温度无三维回流/热点、无局部 h 分布；
  完整三维水区仍为路线 A（后续）。

## 3. 技术方案

- 路线 A：自研 `moldingCoolantFluid` 模块（PIMPLE + h 方程 +
  热物性常物性/温度相关）；
- 路线 B：两相框架内以固定相（不可压水）模拟，alpha 冻结；
- 路线 C：顺序耦合（1D 网络 → 三维壁面热流映射）——最简，精度有限；
- 基准 case：单管直道 + 模具块（多区域）。

## 4. 验收标准（DoD）

- 圆管换热 Nu 与经验关联式 ≤10%；模具温度场梯度定性正确；
- 多区域 CHT 能量守恒 <1%；现有 case 不劣化；CI 双架构绿。

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 自研求解器工作量大 | 先路线 C 打通数据流，再 A |
| 多区域强耦合稳定性 | 弱耦合/松弛迭代；小 case 起步 |

## 6. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/`（新模块） | 非等温水区 |
| `validation/coolantFlow/`（新） | 圆管换热基准 |
| `xmake.lua`/nightly | 接入 |
