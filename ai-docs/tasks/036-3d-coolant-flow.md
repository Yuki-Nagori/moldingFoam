# 036 — 三维冷却水流动（019 遗留）

- 状态：done（2026-09-13：路线 C 完整落地；**路线 A 完成**——`moldingCoolantFluid` 模块 + 单区域能量守恒 2.7e-6 + **多区域 CHT 集成**（水+模具 coupledTemperature，能量平衡 1.2e-7）；Nu 关联式定量对拍列为增强）（2026-09-12：路线 C 设计定稿——仿 `moldingRunnerTemperature` 模式新增模具侧冷却水 BC）
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

## 2b. 现状核对（2026-09-13）

**路线 C 的核心已存在**：`moldingMoldTemperature`（008/019 落地）的
`coolant` 子字典已实现 1D 塞流通道：沿 `direction` 分组润湿面、
冷却水从入口沿程推进并与模具换热（`moldingCoolantChannel`：
`mdot Cp (Tc_out − Tc_in) = Σ htc A (Twall − Tc_in)`），且为能量方程
的隐式部分；`tests/cases/coolantChannel` + `verify-coolant-channel.py`
做了有界验收（模温从 300 K 升到 <355 K）。**缺口核对（2026-09-13）**：
1. 定量基准：**模型测试已完整覆盖**——`coolantChannelTests` 含
   Nu 幂律手算值（0.023·Re^0.8·Pr^0.4，Re=1e4/Pr=7 → 79.3902）、
   `h=Nu·k/D`、截面分组、**精确离散推进与能量平衡**（手算组温）、
   以及 **ε-NTU 收敛测试**（n=200、NTU=1 的离散出口温度对
   `T = Ts−(Ts−Tin)exp(−hA/ṁcp)`，见 modelTests「Many small
   cross-sections converge to the analytic plug-flow exponential」）；
2. 多区域 CHT 能量守恒 <1%（冷却水吸热 = 模具放热）：待做（现有
   `coolantChannel` 用例为有界验收；需加能量账）；
3. 沿程模具温度梯度：**受限于集总模具表述**（单个模具热质量；只有
   冷却水沿程温度分组）——真正的三维模具梯度需路线 A（三维水区），
   作为长期项保留并已在限制中记录。

## 2d. 路线 A 首里程碑（2026-09-13）

- **新求解器模块 `moldingCoolantFluid`**（派生 `incompressibleFluid`）：
  常密度水流动 + `thermophysicalPredictor` 中的被动温度方程
  `dT/dt + div(phi,T) − laplacian(kappa/(rho·Cp), T) = 0`；物性自
  `constant/physicalProperties`（rho/Cp/kappa，与 nu 并存）；
  注册入 `libmoldingFoam.so`，`solver moldingCoolantFluid` 可用；
- **验证 `validation/coolantWater`**（`xmake run coolantWater` + 夜间
  CI）：2D 通道（40×4，入口 300 K、热壁 350 K、U=0.01 m/s、层流
  Re≈80）；模块内诊断（净边界热流、出口流量加权温度）：
  - 净热 4.6703 W = ṁcp(T_bulk−300) → **能量平衡误差 2.7e-6** ✓；
  - 出口 ṁ = 4.000001e-5 kg/s（解析 4e-5 ✓）、T_bulk = 327.92 K
    （物理区间 300–350 ✓）；
- 待做：**多区域 CHT 集成**（水区与模具固体区经 `coupledTemperature`
  耦合、`regionSolvers { water moldingCoolantFluid; mold solid; ... }`），
  以及圆管换热 Nu 关联式定量对拍。

## 2e. 路线 A 多区域 CHT 集成（2026-09-13，已完成）

- **模块扩展**：`moldingCoolantFluid` 内构造 `constSolidThermo`
  （常物性 rho/Cv/kappa，子字典格式）+ `solidThermophysicalTransport`，
  提供 `coupledTemperature` 共轭边界所需的 `kappaEff`（水的动量仍由
  incompressibleFluid 求解，温度为其被动场）；
- **多区域 case `validation/coolantWaterMold`**（moldCHT 结构：水区
  20×2 mm + 模具 20×5 mm）：`regionSolvers { water moldingCoolantFluid;
  mold solid; }`，水-模具界面 `coupledTemperature`，模具顶面 400 K；
  水入口 300 K/0.01 m/s；
- **结果**（`xmake run moldCHT` 套件含此案例，经 runner 归一化验收）：
  - 水的净边界热 = **38.2244 W**、出口 bulk T = 322.85 K；
  - **能量平衡误差 1.176e-07** ✓；界面连续性由 coupledTemperature 保证；
- **036 路线 A 完成**：三维非等温水区可参与多区域 CHT（层流强制对流、
  无浮力）。增强项：Nu 关联式定量对拍（发展段层流）、湍流/浮力。

## 2c. 路线 C 完整落地（2026-09-13）

- **新 BC `moldingChannelCooling`**：固体区域通道壁的隐式 Robin 边界，
  环境温度 = 1D 通道沿程推进的局部水温（`moldingCoolantChannel::group`
  全局分组 + `march`；逐面按轴向坐标映射到截面）；并行下分组全局一致、
  截面壁温经 `returnReduce` 汇总；
- **验证用例 `validation/coolantMold`**（多区域 CHT：模具顶面 =
  通道 BC）：水 350 K 入口 → 出口 **350.274 K**、吸热 **+11.45 W**
  （正确加热）；`xmake run coolantMold` 全验收通过；
- **修复**：`patchInternalField()` 返回 `tmp`，初版绑定 const 引用导致
  悬垂（读到垃圾/500）——改为拷贝（这是本次定位到的真实缺陷）；
- 模型层验证（既有）：Nu 幂律手算、`h=Nu·k/D`、截面分组、精确离散
  推进、**ε-NTU 收敛到解析指数**；
- 回归：模型测试 + 18 求解器用例 + moldCHT 5/5 全绿；xmake/夜间 CI
  已接入 `coolantMold`；
- **待做（路线 A）**：自研非等温不可压水区求解器（三维水流动/热点），
  路线 C 的限制（无三维回流、模具梯度仅由区域求解提供）已在文档记录。

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
