# 036 — 三维冷却水流动（019 遗留）

- 状态：planned
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
