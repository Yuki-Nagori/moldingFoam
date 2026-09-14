# 046 — 并行死锁：边界 patch 循环内的归约（issue #7）

- 状态：done（2026-09-14：根因定位 + 修复 + 回归防线；bundle v0.2.5
  环境复测通过——见 §5）
- 优先级：P0
- 依赖：无
- 预估规模：1–2 天
- 来源：上游 issue #7（真实件体素网格 4 进程死锁）

## 1. 现象与复现

真实件 case（体素网格，5115 面≈2560 单元；`repro-moldingfoam-np4`）在
**4 进程**下第 1 个时间步后死锁：time 目录停在 `0`，4 rank 常驻
~100% CPU、无输出、无报错、不退出；同一 case 1/2 进程可跑到 endTime。
gdb attach 显示两组 rank 停在不同集合通信里：

```
rank0/rank2: preSolve → VoFSolver::preSolve → twoPhaseVoFSolver::correctCoNum
             → basicFluidSolver::correctCoNum → gMax → UIPstream::read
rank1/rank3: moldingFoam::postSolve → gSum → reduce → PMPI_Allreduce
```

即部分 rank 已在下一步的 `preSolve`，另一部分仍停在 `postSolve` 的归约
里——同一 case 的**归约次数按 rank 不同**，典型的通信错配。

## 2. 根因

**分解后的网格，每个 rank 的边界 patch 个数不同**：真实 patch
（inlet/vent/walls）在每个 rank 上都有，processor patch 只属于该 rank
参与的界面。本 case 的 scotch 4 分解为每 rank 5/6/5/6 个 patch：

| rank | patch 列表 |
|------|-----------|
| 0 | inlet, vent, walls, procBoundary0to1, procBoundary0to3（5） |
| 1 | inlet, vent, walls, procBoundary1to0, procBoundary1to2, procBoundary1to3（6） |
| 2 | inlet, vent, walls, procBoundary2to1, procBoundary2to3（5） |
| 3 | inlet, vent, walls, procBoundary3to0, procBoundary3to1, procBoundary3to2（6） |

于是**任何 `forAll(boundaryField(), patchi)` 循环里放集合通信**，其归约
次数就按 rank 不同：`moldingFoam::postSolve` 的熔体质量通量循环
（`flux += gSum(abf[patchi])`）在 rank0/2 调用 5 次、rank1/3 调用 6 次
（实测 trace：见 §5 的 5 vs 6）。MPI 的 `sumOp<scalar>` 归约走
`MPI_Allreduce`（OpenFOAM 对 scalar 求和有专门实现），`maxOp` 等走
OpenFOAM 自带的 linear/tree 通信——两组 rank 的归约序列相差一次后便
互相等待，形成死锁。

由此也解释了「样例立方体 case 4 进程正常」：其分解每个 rank 的
processor patch 数相同（对称分解），归约次数恰好一致；真实件的
分解不对称才暴露。

受影响代码（同一根因，全部修复）：

| 位置 | 循环 |
|------|------|
| `moldingFoam::postSolve` | `flux += gSum(abf[patchi])`（熔体质量通量） |
| `moldingFoam::postSolve` | `fluxVol += gSum(phi.boundaryField()[patchi])` |
| `moldingFoam::postSolve` | `fluxAlpha += gSum(abfA[patchi])` + 逐 patch 打印 |
| `moldingCoolantFluid::thermophysicalPredictor` | `qNet += gSum(...)`、`mf = gSum(phip)`、`mdotT += gSum(...)` |

按 patch 类型过滤的循环（`gatePressure`、排气封口 `gMax(a1p)`、
填充速度预警）不受影响：被过滤的 patch 在所有 rank 上都存在、类型一致，
归约次数相同——本次只修「遍历全部 patch」的循环。

## 3. 修复

- 累加量改为**循环内局部求和 + 循环外一次归约**（
  `sum(abf[patchi])` … `reduce(flux, sumOp<scalar>())`），归约次数与
  patch 循环彻底解耦；
- 循环跳过 processor patch（`isA<processorFvPatch>`）：它们是全局网格的
  内部面，不承载边界通量；旧代码在并行下把不同 rank 的**不同 patch**
  按同一序号混在一起求和，数值本身也是错的。跳过它们使并行口径与串行
  完全一致（串行无 processor patch，故串行数值不变）；
- 逐 patch 诊断打印保留全局值：在「真实 patch」循环内做归约——该集合
  在每个 rank 上一致，归约次数相同。

## 4. 回归防线

- 新用例 `tests/cases/parallelMassBudget`：10 mm 立方体、`massBudget
  true` + `massBudgetInterval 1`、`system/nProcs 4`、
  `system/decomposeParDict` 用 hierarchical `n (4 1 1)` 造出
  4/5/5/4 的不对称 patch 数（两个端 rank 只有 1 个邻居）；
- `scripts/run-solver-tests.sh` 支持 `system/nProcs`：存在时在串行
  pass 之后追加一次 decomposePar + `mpirun -np N foamRun -parallel`
  pass（`MOLDINGFOAM_PARALLEL_TIMEOUT` 缺省 300 s 超时），日志断言与
  串行一致。超时即 FAIL——死锁不再挂住 CI，而是失败退出；
- 该用例在旧代码上实测 FAIL（45 s 超时），修复后 PASS（串行+并行
  19 s），见 §5。

## 5. 验收记录（2026-09-14）

环境：`of14` VM（8 核 ARM64 + apt openfoam14）与 Kairos bundle VM
（5 核 / 12 G，v0.2.5 linuxArm64GccDPInt32Opt）。

| 项目 | 结果 |
|------|------|
| issue 复现 case，np4（bundle v0.2.5 环境，修复后） | **exit 0，5069 步到 endTime 2 s**（修复前：第 1 步后死锁） |
| 同上 np2 / np1 | exit 0，5080 / 5083 步；V/P 切换 1.080167 / 1.080168 s，填充 0.9607489 / 0.9607463；质量预算残差 −2.9e-08 / 7.9e-06 kg |
| 同上 np4 与 np1/np2 对照 | V/P 切换 1.080186 s、填充 0.9607243、残差 −3.8e-05 kg（1–4 进程一致，与 issue 报告的 1/2 进程基线吻合） |
| 37,540 单元大件体素 case，np4（bundle VM） | exit 0，1829 步到 endTime 2 s；V/P 切换 1.4721 s（修复前同样死锁） |
| trace 证据（临时插桩，5 vs 6 次归约） | rank0/2 postSolve 通量循环 5 次、rank1/3 6 次 = 各 rank 的 patch 数 |
| 串行回归（cli10 case） | 修复后与**修复前 bundle 库逐位一致**：5083 步、V/P 切换 1.080168 s、填充 0.9607463、质量预算残差 7.868949e-06 kg |
| `xmake run test` | 全部 PASS |
| `xmake run test-solver`（26 + 新用例） | 27/27 PASS（新用例串行+并行两 pass 均 PASS） |
| `MOLDINGFOAM_PARALLEL=4 xmake run case-contract` | 7692 步全项通过，质量守恒 9.391e-04（阈值 1e-3） |
| `xmake run coolantWater`（串行） | PASS，能量平衡 2.728e-06；净边界热 4.6703 W |
| coolantWater 4 子域并行（hierarchical，patch 5/6/6/5） | exit 0，净边界热 4.6702857 W / 出口 327.91892 K（与串行一致；旧代码在该分解下同样会死锁） |
| 新防线用例在旧代码上 | FAIL（45 s 超时，符合预期） |

已知遗留（非本任务范围）：bundle 环境在求解正常结束后析构阶段仍有
`malloc_consolidate(): unaligned fastbin chunk detected` 退出崩溃
（037，bundle 侧定位移交 Kairos）；本任务的 np4 验收以「求解到
endTime + 输出完整」为准。

## 6. 涉及文件

- `src/moldingFoam/moldingFoam.C`（postSolve 三处循环）
- `src/moldingFoam/moldingCoolantFluid/moldingCoolantFluid.C`（诊断两处循环）
- `tests/cases/parallelMassBudget/`（新用例）
- `scripts/run-solver-tests.sh`（`system/nProcs` 并行 pass + 超时门禁）
- `README.md`（§5 并行用例说明、§6 质量预算口径说明）
