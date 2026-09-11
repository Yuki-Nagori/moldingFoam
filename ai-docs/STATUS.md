# moldingFoam 研发状态（会话交接）

本文件是跨会话的浓缩交接快照；细节以任务文档与 README 为准。
**约定：本文件是临时文件，全部任务（done）后删除。**

## 1. 环境与命令

- 仓库：`/Users/yuki/eit/moldingFoam`（宿主编辑）；VM：Multipass `of14`
  （Ubuntu 24.04 arm64 + openfoam14 apt 二进制）；挂载
  `~/moldingFoam` → 构建目录 `~/moldingFoam-build`（VM ext4，xmake）。
- 同步文件到构建目录（示例）：
  `multipass exec of14 -- bash -lc 'cp ~/moldingFoam/<path> ~/moldingFoam-build/<path>'`
- 构建/测试（VM 内、已 source `/opt/openfoam14/etc/bashrc`）：
  - `xmake`（库 + 测试）
  - `xmake run test`（模型测试，秒级）
  - `xmake run test-solver`（17 个求解器小用例）
  - `xmake run couette` / `couetteSlip` / `stefan`
  - `xmake run moldCHT`（5 个 CHT 基准）
  - `MOLDINGFOAM_PARALLEL=4 xmake run case-contract`（并行契约验收）
- 清理用例时必须用 `0.[0-9]* [1-9]*`（不要用 `[0-9]*`，会删掉 `0/`）。

## 2. 任务状态（ai-docs/tasks/）

- **done（25）**：001–008、010–017、013a、020、021、023、024、025、
  026、027、028、030。
- **in-progress（4）**：
  - **009 / 018**（P0 高压守恒，已定位到定式级）：<1e-3 窗口仅到约
    38 MPa（t≈1.30 s，最小 5.02e-4）。**误差窗口分解**：充填 3.2e-4 ✓、
    **保压期唯一超差**（51%）、封冻后 4.6e-6 kg；起始于保压表 37.8 MPa
    斜率断点后。阴性实验（累积 15+）：平滑封冻、浇口通量一致性、
    transonic、容差/外迭代/校正器、上游标准 BC、慢斜坡、零梯度 U、
    半余弦升压、momentumPredictor、目标松弛、maxCo（无效）。**决定性
    分解（新工具 `massBudget`/`massBudgetInterval`）**：
    `dm−(ψ·dp+Δα)=1e-8…1e-10`（密度更新自洽）；`ψ·dp+flux·dt=±1e-7/步
    （~25%）`交替累积到 1.9% → 不一致在**压力方程通量/压缩项拆分**
    （`p_rghEqnComp`，psi/rho 迭代间滞后）；`massFix` 事后修正：full
    降 20× 但 dt 崩溃、relax 0.1/0.25 仅降 13%/28% → **须改压力耦合
    定式**（每迭代重估 comp 项 / 显式质量通量修正）。封冻后空洞/负压
    另见 018a。
  - **018a**（in-progress，阶段 1–3 完成）：PVT 空洞指标 API +
    `voidFraction` 求解器场（默认关）+ 精确 Tait `rho(pv,T)`（缓存
    EOS）+ 密封冷却用例（`tests/cases/voidFraction`，空洞 4.1% 近均匀
    稳定）；验证器含 PVT 逐单元对拍（1.6e-6）与压力路径对拍
    （求解器 −4.23 MPa ≤ pv，与 PVT 差 1.16%）；待做（阶段 4）：
    张力限制（p 下限 pv + 密度限制，需空洞体积/质量记账）、封冻后
    质量守恒（依赖 018）。
  - ~~019~~（done）：四基准完成；三维水区调研结论——v14
    `incompressibleFluid` 为等温模块（无 T），需自研非等温求解器；
    等效冷却已由 `moldingCoolantChannel` + `moldingConvectiveCooling`
    覆盖（任务允许的 1D 对流路径）。
  - ~~022~~（done）：自由翘曲/残余应力/双金属 + **三维热弹性悬臂
    基准**（`validation/thermoelastic`，`xmake run thermoelastic`，
    48×80 挠度误差 5.46% < 10%，网格收敛）；`alphav` 语义=线性膨胀
    系数、参考态 T=0。
  - **029 性能**：流道网络缓存、并行单区域 constant 字典回退
    （重要回归修复）、`scripts/perf-scaling.sh`；**22.3 万单元 4× 加密
    基准**（516 步：4 进程独占 455 s，逐步成本 0.54→1.16 s 随充填
    上升，每步 4 次线性求解均迭代 6.87，吞吐 2.5e5 cell-step/s）。
    剩下：干净加速比（1/2/4 进程独立重跑）与 profiler 热点优化。
- **planned**：018a（封冻后收缩空洞/负压）、013b（结构翘曲落地）。

## 3. 验证基线（最近运行结果）

- 模型测试：全绿（Tait/CrossWlf/D3/潜热/集总模温/冷却通道/结晶
  Nakamura+Giesekus 粘弹性/纤维 Jeffery/收缩/翘曲/残余应力/双金属/
  喷管/Na 关联式）。
- `xmake run test-solver`：17/17 全绿。
- `xmake run moldCHT`：5/5 全绿（导热 0.19%/1.33%；充填能量 0.24%；
  多周期；Robin 冷却 0.314%；002 极限 1.05e-4）。
- 契约 case：串行与 `MOLDINGFOAM_PARALLEL=4` 均 **9.383e-04**（<1e-3）。
- fountainFlow 串/并行一致；高压力 case 如实失败（**不纳入 CI**）。

## 4. 本会话关键新增（约 60 提交）

- 求解器/模型类：`moldingCoolantChannel`、`moldingRunnerNetwork`、
  `moldingCrystallization`、`moldingFiberOrientation`、
  `moldingShrinkage`、`moldingWarpage`、`moldingViscoelastic`、
  `viscoelasticStress`(fvModel)、`moldingConvectiveCooling`(BC)、
  `moldingRunnerTemperature`(BC)。
- 验证 case：`moldCHT-fill/cycle/cooled/lumped`、`weldLine`、
  `processProfile`、`multiGate`、`runnerTemperature`、
  `crystallizationAdvection`、`fiberOrientationAdvection`、
  `shrinkage`、`fountainFlow`、`viscoelasticFlow`、
  `validation/highPressure`（窗口化验证器，如实失败）。
- 基础设施：`scripts/perf-scaling.sh`、`scripts/fit-crystallization.py`、
  `run-case.sh`/`run-moldcht.sh` 的 `system/verifier` 约定。

## 5. 约定

- 一 task 一 commit，不 push；中文文档；精度优先，不因测试放宽阈值。
- 上游零补丁：功能经模块内自注册/覆写实现。
- 提交前至少跑受影响的小用例；大回归留到阶段收口。
