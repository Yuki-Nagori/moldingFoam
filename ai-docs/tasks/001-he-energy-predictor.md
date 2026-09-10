# 001 — 潜热能量预报器（启用 latentHeat）

- 状态：done（2026-09-10。验收：契约 case `latentHeat 2e5` 全项 PASS，
  质量守恒相对误差 7.722e-05 < 1e-3，顶出 t = 1.366 s；`xmake run test`
  全部 PASS）
- 优先级：P0
- 依赖：无
- 预估规模：1–2 周

## 1. 背景与现状

`compressibleVoF` 的能量预报器按 T 矩阵求解，对角系数为每相
`Cv = Cp - CpMCv`。启用 `latentHeat` 后，`hMeltThermo` 的表观
Cp 潜热峰经 `constTransport` 的 `kappa = Cp·mu/Pr` 泄漏进导热率：
0.5 K 带宽内 κ 被放大两个数量级（实测峰值 7.6e6 W/m/K），温度方程
被空间强非均匀扩散系数摧毁，随后压力/通量反馈崩溃。

`Tait::CpMCv` 已忽略 `b6` 前沿扫掠耦合（量级 0.15 K/MPa），使带内
`Cv` 保持正定（实测最小值 ≈ 2.3e3 J/kg/K），因此根因是导热率污染，
而非热容负值。

## 2. 目标与非目标

### 目标
1. 契约 case 的 `latentHeat 2e5` 稳定运行至顶出，质量守恒 < 1e-3；
2. 潜热峰只进能量方程的热容系数，不进物性（导热率）；
3. hMelt 函数级测试覆盖潜热峰形状、积分与导数一致性；
4. 移除失效的 `limitTemperature` 约束，温度安全钳制由求解器内部完成。

### 非目标
- he/焓型能量预报器（两相共享 T 下欠定，不在本模块覆写范围内）；
- 相变前沿的显式追踪或界面传热模型；
- 多周期潜热与模温强耦合（属于 002）。

## 3. 技术方案

### 3.1 `hMeltThermo` 拆分表观容量与显热容量

`src/thermo/hMeltThermoI.H`：

```cpp
// 物性用：仅显热基值，保证 kappa = Cp*mu/Pr 平滑
Cp(p,T)       = Cp_ + EOS::Cp(p,T);

// 能量方程用：显热基值 + 潜热峰 - (Cp - Cv) 耦合项，带内正定
latentCp(p,T) = latentHeat_*EOS::latentCpWeight(p,T);
Cv(p,T)       = Cp(p,T) + latentCp(p,T) - EOS::CpMCv(p,T);
```

`hs/es` 不变（仍含单位积分跨带的潜热平台），能量守恒物理不变；
Newton 反演 `Tes` 用正定 `Cv` 收敛。

### 3.2 求解器与契约 case

- `moldingFoam::thermophysicalPredictor()` 保持上游 T 矩阵结构，仅保留
  线性求解后的 250–3000 K 安全钳制；
- `case-contract/constant/physicalProperties.melt`：`latentHeat 2e5`；
- `case-contract/system/fvConstraints`：移除 `limitTemperature`
  （熔体/空气共享单一 `T` 场，`phase melt` 绑定 `T.melt` 永不生效）。

### 3.3 精度评估

- 契约 case 带宽 1 K、壁面冷却速率 ~28 K/s、时间步 ~2.8e-4 s，
  单步 ΔT ~ 8e-3 K，潜热峰被 ~60 步解析，不漏潜热；
- 质量守恒误差由历史基线 7.3e-4 降至 7.72e-05（κ 平滑的直接收益）。

## 4. 工作拆解

1. `src/thermo/hMeltThermoI.H`：`Cp` 仅显热；新增 `latentCp`；
   `Cv` 承载潜热峰；
2. `src/thermo/hMeltThermo.H`：语义与文档更新；
3. `src/moldingFoam/moldingFoam.C`：安全钳制与注释（结构不变）；
4. `tests/modelTests.C`：hMelt 潜热测试改测 `latentCp`/`Cv`；
5. `case-contract/constant/physicalProperties.melt`：`latentHeat 2e5`；
6. `case-contract/system/fvConstraints`：移除 `limitTemperature`；
7. 契约 case 与模型测试回归。

## 5. 验收标准（DoD）

- `xmake` 编译零告警新增；
- `xmake run test` 全部 PASS（新增 hMelt 表观 Cv 解析对拍）；
- `MOLDINGFOAM_PARALLEL=4 xmake run case-contract` 全项 PASS，
  质量守恒 < 1e-3；
- README 与契约字典同步更新（v1.3 变更日志）；
- CI 双架构绿。

验收记录：

- 契约 case（`latentHeat 2e5` + `Pr 4`）：全项 PASS，
  `Mass conservation relative error = 7.722e-05`，V/P 切换 t = 1.016 s，
  顶出 t = 1.366 s（平均熔体温度 383.34 K），日志无 NaN；
  4327 步、4 子域并行约 372 s（8 核 ARM64 VM）；
- `xmake run test`：全部 PASS；
- 提交哈希：待提交（随本报告一并提交后回填）。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 更窄带宽/更大时间步下感知不到潜热峰 | 带宽（`smoothBand`）与时间步均可控；必要时在能量预报器内加局部 Picard 子迭代（与 PIMPLE 外层解耦） |
| `Cp` 语义变化影响外部使用 | `Cp` 文档明确为显热值；潜热贡献经 `latentCp`/`hs` 提供；`latentHeat 0` 时与旧行为完全一致 |
| `Cv` 含潜热峰值使能量矩阵条件数变大 | 只影响能量求解迭代数（占比小）；κ 解耦后实测 3 次迭代收敛 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/thermo/hMeltThermoI.H` | `Cp` 仅显热；新增 `latentCp`；`Cv` 承载潜热峰 |
| `src/thermo/hMeltThermo.H` | 语义与文档更新 |
| `src/moldingFoam/moldingFoam.C` | 注释与安全钳制（结构不变） |
| `tests/modelTests.C` | hMelt 潜热测试改测 `latentCp`/`Cv` |
| `case-contract/constant/physicalProperties.melt` | `latentHeat 2e5` |
| `case-contract/system/fvConstraints` | 移除失效的 `limitTemperature` |
| `README.md` | §5/§6/§7/§8 更新，契约日志 v1.3 |
