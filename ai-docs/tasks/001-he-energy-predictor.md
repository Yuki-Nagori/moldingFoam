# 001 — he 型能量预报器（启用潜热）

- 状态：in-progress（2026-09-10：覆写机制 + 半隐式潜热线性化已落地，
  latentHeat 缺省 0；he 型矩阵重设计未完成，见第 2 节实验记录）
- 优先级：P0（功能完整注塑求解器的核心缺口）
- 依赖：无
- 预估规模：1–2 周（含验证）

## 1. 背景与现状

潜热物理已在热力学层完成：`hMeltThermo`（`src/thermo/hMeltThermo.*`）
在常 Cp 上叠加表观 Cp 潜热峰——`Cp_app = Cp0 + latentHeat·dw/dT`，其中
`w` 是 Tait 混合的 C1 平滑熔体权重（跨带单位积分），`hs` 相应含
`L·(w(T) − w(Tref))` 平台。热力学层由 modelTests 5 项测试覆盖。

**未被启用的原因**：`compressibleVoF::thermophysicalPredictor()`
（`/opt/openfoam14/applications/modules/compressibleVoF/thermophysicalPredictor.C`）
按 **T 矩阵**求解能量方程：

```
correction( Cv1·T-transport + Cv2·T-transport )   ← 隐式修正项
+ fvc::ddt(αiρi, ei) + fvc::div(αρφi, ei) − contErri·ei   ← 显式 e 输运
− fvm::laplacian(κeff, T)
+ totalInternalEnergy 压力功/体积功项
```

Tait EOS 的 `Tt(p) = b5 + b6·p` 使固化前沿随压力移动，带内
`CpMCv = T·α²/ψ` 被 `wp·(vm − vs)/ψ` 项主导（量级 1e5–1e6），于是

```
Cv = Cp − CpMCv < 0   （带内深部）
```

负 Cv 同时污染 T 矩阵对角与 `Te` Newton 斜率，T 解出发散
（实测：latentHeat=2e5 时 T 解出 −155173 K / −32188 K，
`thermoI.H:212` "Negative initial temperature T0"）。

## 2. 已否决方案与实验记录（勿重复尝试）

| 方案 | 结果 | 原因 |
|------|------|------|
| 修正项系数取 `max(Cv, Cp)` | 解仍发散（−32188 K，0.147 填充分数） | 破坏 `correction()` 拆分的雅可比一致性：修正项隐式系数（Cp）与被替换的显式项系数（Cv）不一致，等式不再是能量方程 |
| `latentHeat` 保持 0（现状） | 稳定但不含潜热 | 当前默认；本任务的终点是解除它 |

### 2026-09-10 补充实验（半隐式潜热容量线性化 + limitTemperature）

在 moldingFoam 覆写的能量预报器（上游 T 矩阵体 + `SuSp` 追加项）上：

1. **`SuSp(α1ρ1·latentCpW/dt, T)`**（有效热容法：带内潜热储存率的
   隐式欧拉离散，对角抬升项）——单独使用不足以稳定：带内深部
   `CpMCv ≈ 6e5` 超过潜热峰 `latentCp ≈ 3e5`（两者都来自前沿项），
   对角仍为负；
2. **`limitTemperature` fvConstraint**（min/max 钳制 + 矩阵条件化）
   —— 与 compressibleVoF 的双 thermo 结构不兼容：`phase melt` 使
   约束绑定 `T.melt` 字段（不存在，钳制永不生效，日志
   "Constraint limitT defined for field T.melt but never used"），
   不设 phase 则查找 `physicalProperties`（不存在，FatalError）。

当前结论：T 矩阵形式的能量方程与"压力相关 Tt + 潜热"存在根本性
冲突（带内等容响应 `Cv` 本征为负，物理真实而非数值错误）。下一步
应实现真正的 he 变量能量预报器（对角 = α·ρ/dt 恒正），传导项显式
（扩散数估算见 4.1，无约束）、压力功显式搬运，`Te` Newton 反演注意
带内斜率符号。覆写机制与半隐式线性化代码已保留（`latentHeat 0` 时
完全惰性），作为该研发的起点。

结论：**必须更换隐式变量**——以 he（每相显能/焓）为矩阵变量，对角
系数为 ρ（恒正），Cv 不再出现在对角中。

## 3. 目标与非目标

### 目标
1. `moldingFoam` 内覆写 `thermophysicalPredictor()`，以每相 he 为
   隐式变量（`fvm::ddt(αi, ρi, hei) + fvm::div(αρφi, hei) − Sp`），
   对角恒正；
2. `latentHeat = 2e5` 下契约 case 全项验收通过，且**顶出时刻明显
   变长**（潜热生效的物理证据）；
3. 显式传导的时间步限制对当前与可预见网格不构成约束（见 5.1 估算）；
4. README 第 6 节限制警告解除，契约变更日志记录。

### 非目标
- 不实现模具共轭传热（任务 002）；
- 不引入上游 patch；
- 不改变 M1/M2 的流-压-黏度耦合结构；
- 不追求 he 矩阵下的通用多相适用性（只服务 moldingFoam 的两相 + 共享 T）。

## 4. 技术方案

### 4.1 变量与方程

对每相 i ∈ {1(melt), 2(air)}：

```
fvm::ddt(αi, ρi, hei) + fvm::div(αρφi, hei) − fvm::Sp(contErri, hei)
= 显式项：传导 + 压力功/体积功 + fvModels 源
```

- `hei = mixture_.thermoi().he()`（已有注册场，含边界条件，直接作为
  隐式变量）；
- 传导项：`−fvc::laplacian(κeff, T)` 显式（T 用上一迭代值）。显式
  扩散数估算：聚合物带内 `α_T = κ/(ρ·Cp_app) ≥ 0.25/(950×3e5) ≈
  8.8e-10 m²/s`，契约网格 `dx ≈ 2.5–5e-4 m` → `dt_max = dx²/(2α_T)
  ≈ 35–140 s`，比求解 dt（~1e-3 s）大两个数量级以上，**无约束**；
- 压力功/体积功/动能项：整体搬运上游 `totalInternalEnergy` 显式块
  （它本来就是 fvc 显式形式，含 `totalInternalEnergy()` 开关分支）；
- `κeff`：来自 `thermophysicalTransport.kappaEff()`（上游同款）。

### 4.2 两相共享 T 的处理

两相仍共享单一 T 场（VoF 混合物假设，与上游一致）。求解后由
`mixture_.correctThermo()` 内部的 `TE(he, p, T)` Newton 反演更新 T：

- hMelt 的 `e(T) = hs(T) − p·vhat(T)` 带内单调递增（`Cp_app ≥ 2400`
  恒正，`p·vT` 项在契约压力下 ≈ 260 J/kg/K，不改变符号）→ Newton
  有唯一根；
- **风险点**：Newton 斜率参数是 `Cv = Cp − CpMCv`（带内可为负或过
  零），收敛路径可能在带边缘 |Cv|≈0 的 ~mK 级薄区内变慢。缓解：
  `T0` 取上一时间步收敛值（默认行为），必要时在 hMeltThermo 增加
  `limit(T)` 带内限幅或实现二分回退（OpenFOAM `thermo` Newton 已有
  maxIter，失败会 FatalError——先观察再处理，不预防性实现）。

### 4.3 与上游的代码关系

- 仅覆写 `virtual void thermophysicalPredictor()`（moldingFoam.H/.C），
  不修改 compressibleVoF；
- 需要的 protected 成员全部可访问：`mixture_`、`alpha1/2`、
  `alphaRhoPhi1/2`、`contErr1/2`、`K`、`p`、`phi`、`U`、`rho`、
  `thermophysicalTransport`（与既覆写实验相同，编译已验证）；
- 已知类型细节：`thermo1().Cv()()` 返回
  `volScalarField::Internal`（不是 volScalarField）；he-form 中不再
  需要（无 Cv 对角），但压力功块里 `rhoPhi`、`contErr` 等的用法照抄
  上游。

### 4.4 稳定性与守恒论证（写入代码注释）

- 对角 = Σ αiρi/dt·V（恒正），无条件优于 Cv 对角；
- he 为守恒输运变量：ddt+div 隐式离散保质量-能量一致；
- 传导与压力功显式 → 时间步条件（见 5.1 估算，约束远宽于 Courant）；
- 潜热经 `he(T)` 完全进入瞬项与对流项，无双重计入（hMelt 的 Cp 峰
  与 hs 平台互为积分关系，离散上正确性由 4.6 的 FD/积分测试保证）。

## 5. 工作拆解

1. **设计笔记定稿**：本文件 4.x 段补齐实测数字（α_T、dt_max 用实际
   网格 spacing 复算）；
2. **moldingFoam.H**：声明 `virtual void thermophysicalPredictor();`
   （含设计注释）；
3. **moldingFoam.C**：实现 heEqn（含 includes
   fvcMeshPhi/fvcDdt/fvmDiv/fvmSup/fvmLaplacian，参照上游
   thermophysicalPredictor.C 的 include 列表）；
4. **冒烟验证**：`latentHeat 2e5`，契约 case 临时 `endTime 0.1`
   （纯填充段）跑通，检查 T 场有界、无 Newton Fatal；
5. **全周期验证**：`endTime 3` 全周期 + verify-case 全项验收，记录
   顶出时刻与质量守恒（预期顶出晚于 latentHeat=0 基线）；
6. **modelTests 补充评估**：he–T 反演一致性属于 specie/thermo 上游
   Newton，若步骤 5 出现带内 Newton 慢收敛，补充 hMelt 的
   `Te` 收敛性专项测试；否则不新增；
7. **文档**：README 第 6 节解除限制警告并描述 he 预报器；第 8 节
   契约变更日志 v1.3（无新键，latentHeat 语义激活）；本文件状态改
   done 并附验收记录。

## 6. 验收标准（DoD）

- `MOLDINGFOAM_PARALLEL=4 xmake run case-contract`
  （`latentHeat 2e5`）全项 PASS；
- 顶出时刻相对 `latentHeat 0` 基线变长 ≥ 5%（物理证据）；
- 质量守恒 < 1e-3；
- `xmake run test` 14 项全 PASS；
- CI 双架构绿（amd64 + arm64）；
- README/契约日志同步。

## 7. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 传导显式导致带内 T 振荡 | 当前 dt/dx 比下扩散数 ≪1（4.1 估算）；若加密网格触发，加 he 子迭代（每步 2–3 次 heEqn.solve + correctThermo 循环） |
| Te Newton 在带边缘慢收敛 | T0 用上一时刻值；必要时 hMeltThermo::limit(T) 限幅或二分回退 |
| 两相 he 方程经共享 T 隐式耦合不足 | 与上游 T-form 相同的耦合水平（κ∇T 显式），不引入新耦合缺陷 |
| 包缓存/CI 时长增加 | 无（he-form 不增加上游依赖；矩阵规模不变） |
