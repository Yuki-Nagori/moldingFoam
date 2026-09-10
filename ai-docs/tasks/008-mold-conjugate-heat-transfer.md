# 008 — 模具三维传热（共轭传热 CHT）

- 状态：in-progress（2026-09-10：路线 B 第一阶段——模壁热阻 + 深层
  模温；路线 A 第二阶段——`foamMultiRun` 双区域共轭传热（cavity
  `moldingFoam` + `solid` 模具）已落地并解析验证；含流动的注塑周期
  CHT 与冷却水 1D 网络仍待做）
- 优先级：P1
- 依赖：002（0D 集总模温已落地）；建议在 001/006 之后
- 预估规模：1–2 周

## 1. 背景与现状

当前模壁温度是`fixedValue` 恒温或 002 的 0D 集总状态（单一温度、
无限大热沉/单点冷却水）。真实模温在空间上分布很广：浇口附近温度高、
远离浇口低；冷却水路附近低、型芯内部高。模温分布决定：

- 冻结层厚度与充填压降；
- 保压补偿路径与收缩均匀性；
- 顶出时间与翘曲。

## 2. 目标与非目标

### 目标
1. 模具区域作为独立求解区参与共轭传热（型腔壁面两侧温度/热流连续）；
2. 支持冷却水路的等效换热（可先做管道 1D 对流 BC，再考虑三维水）；
3. 与 `moldingFoam` 的 VoF 求解器耦合（分区或整体求解）。

### 非目标
- 冷却水路几何自动生成；
- 模具热疲劳/热应力。

## 3. 技术方案

- 上游 `chtMultiRegionFoam` 模块已有共轭传热框架，但本项目当前是
  单区域 VoF；两条路线：
  - **路线 A（多区域）**：复制/组合 `compressibleVoF` 与
    `solidDisplacement`/`solid` 热求解器为多区域模块，接口边界用
    `compressible::turbulentTemperatureCoupledBaffleMixed` 类；
    改动大，但物理完整；
  - **路线 B（单区域扩展）**：在 VoF 网格中把模具作为"固体相"添加
    （三相 VoF 的简化：固体不动、无速度），用一维壁面热阻 + 深层
    温度边界（如 `externalWallHeatFluxTemperature` + 热阻网络）近似
    三维效应。
- 建议先做路线 B 的最小可用版（壁面热阻 + 冷却水对流 BC），为后续
  路线 A 预留接口；
- 冷却水：`h = Nu·k/D` 的对流关联式，或 1D 管道能量方程。

## 4. 工作拆解

1. 需求与网格评审：单区域近似 vs 多区域；
2. 壁面热阻/共轭边界条件选型与自注册实现；
3. 冷却水换热 BC（`固定 h`→`Nu 关联式`→1D 水温度演化）；
4. 验证 case：一维稳态导热 + 对流冷却解析解对拍；
5. 契约/演示 case：与 002 的模温响应对照；
6. modelTests + 文档（README §6、契约日志）。

## 5. 验收标准（DoD）

- 与一维稳态解析解对拍（界面温度/热流 rtol < 1e-3）；
- 模温空间分布合理（浇口附近高于远端）；
- 与 002 的 0D 结果在极限参数下一致；
- CI 双架构绿。

验收记录（第一阶段：路线 B 最小版——深层模温热阻路径）：

- `moldingMoldTemperature` 新增可选 `wallResistance` [m²K/W] 与
  `deepMoldTemperature` [K]：模壁经该热阻与深层模体换热，与冷却水、
  铸件两条路径合并为单一等效导热加权驱动温度
  （`moldThermalState::Tdrv`）；
- `moldThermalState::Tdrv` 单测：导热加权平均 rtol 1e-12、双零导热
  返回 Ta；`xmake run test` 全部 PASS；
- 每个 patch 可独立配置 `wallResistance`/`deepMoldTemperature`，
  在边界层面近似模具内的温度梯度；
- 多区域共轭传热（路线 A）与冷却水 1D 网络仍待做。

验收记录（第二阶段：路线 A——`foamMultiRun` 双区域 CHT）：

- 无需任何新求解器代码：`moldingFoam` 通过 `regionSolvers` 作为流体
  区域求解器加载（`solver::load("moldingFoam")` 命中本库的
  `libmoldingFoamSolver.so`），模具区域用上游 `solid` 模块，
  `cavity_to_mold`/`mold_to_cavity` 采用 `coupledTemperature`；
- `validation/moldCHT`：20x2 mm 熔体腔（常密度 EOS 以隔离界面物理）
  + 20x5 mm 钢模具，腔体 480 K、模具 353 K、模具顶面恒温 353 K、
  其余绝热；`xmake run moldCHT` 全流程（blockMesh → topoSet →
  splitMeshRegions → foamMultiRun → 验证）；
- 独立一维两层隐式有限差分参考（Tait ρ(T)、Cv(T)）：界面温度最大
  相对误差 **0.19%**、腔体平均温度 **1.33%**（阈值 1%/3%），界面两侧
  温度连续误差 0；5 s 物理时间约 1 s；
- 含 VoF 流动的成型周期 CHT、冷却水对流网络仍待做。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 多区域改造与现有 solver 模块冲突 | 先单区域近似，接口抽象后再评估多区域 |
| 冷却水关联式标定 | 参数全部字典化，先做固定 h |
| 计算量上升（多区域/网格量级） | 模具粗网格 + 界面热阻等效 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/` | 新耦合边界/子求解器 |
| `src/Make/files` | 新源文件 |
| `case-contract/` 或新演示 case | 模具热参数 |
| `tests/modelTests.C` | 一维对拍 |
| `README.md` | §6/§8 |
