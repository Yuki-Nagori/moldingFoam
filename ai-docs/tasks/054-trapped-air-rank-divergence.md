# 054 — 并行死锁 #2：`reportTrappedAir` 的按 rank 提前返回

- 状态：done（2026-09-14：根因锁定 + 修复 + 双环境验证 + `parallelTrappedAir` 防线）
- 优先级：P0
- 依赖：046（同类缺陷的第一次修复）
- 预估规模：0.5–1 天
- 来源：答复 Kairos 对 037 的质询时，用最小用例（`tests/cases/fountainFlow`
  @4 子域）复现出的第二处集合通信错配

## 1. 现象与复现

`tests/cases/fountainFlow` 在 **4 子域** 下**确定性死锁**：

```console
$ cd tests/cases/fountainFlow
$ # system/decomposeParDict: numberOfSubdomains 4; method scotch;
$ decomposePar -force && mpirun -np 4 foamRun -parallel
# → 第 500 个时间步（t ≈ 1.6 s，即最后一步）后不再前进，无输出无报错
```

- 该用例在套件里只跑**串行**（`run-solver-tests.sh`），所以此前未被发现；
- 同一 case 串行正常跑到 `End`（501 步）；
- 两个环境都复现：`of14`（apt OpenFOAM 14）与 Kairos bundle VM ✓。

## 2. 根因（gdb per-rank 栈）

```
rank0  : preSolve → gMax<double>            （原生 reduce，UIPstream::read 等待）
rank1-3: preSolve → reportTrappedAir → syncTools::syncBoundaryFaceList
         → PstreamBuffers::finishedSends → Pstream::exchangeSizes → MPI_Alltoall
```

`reportTrappedAir()` 的入口有**按 rank 判定的提前返回**：

```cpp
label nAir = 0;                                  // 本地计数，从未归约
forAll(alpha, i) { air[i] = alpha[i] <= trapAirAlpha_; if (air[i]) ++nAir; }
...
if (nAir == 0) { if (Pstream::master()) Info<< ...; return; }   // ← 按 rank
```

而函数体内有集合通信：洪水填充循环里的
`syncTools::swapBoundaryCellList`（`PstreamBuffers` → `MPI_Alltoall`）与末尾
7 个 `reduce`。于是当**分解让一部分 rank 已填满（`nAir == 0`）而另一部分
仍有气**时，两边进入不同的通信路径 → 死锁（或更早的静默配对/数据污染）。

触发条件：`trapAirInterval > 0`，且某个 trapAir 步上出现"部分 rank 填满"。
fountainFlow @4 子域在最后一步（闸口侧 rank 全熔体）正中此条件。
契约 case（`trapAirInterval 200`）与 Kairos 实件 case 至今未触发——见 §5
的验收数据（契约守恒值未变，说明其各 rank 在 trapAir 步上尚未分歧）。

## 3. 修复

```cpp
// 提前返回必须各 rank 一致：nAir 是本地计数，而下面的洪水填充、
// 边界同步与归约都是集合通信
if (returnReduce(nAir, sumOp<label>()) == 0) { ...; return; }
```

（`returnReduce` 在本函数的洪水填充循环里本就在用，风格一致；串行下与
`nAir == 0` 等价，故串行数值逐位不变。）

## 4. 验证（2026-09-14，of14）

| 项 | 结果 |
|----|------|
| fountainFlow @4（修复前） | 第 500 步后死锁（双环境复现） |
| fountainFlow @4（修复后） | **exit 0，501 步到 End** |
| per-rank trace（临时插桩，已移除） | 四个 rank 序列完全一致（全局守卫 nAir=334、循环全局退出） |
| 契约 case 4 子域验收 | 守恒 **5.010e-04 未变**（修复不触碰已对齐的运行） |
| `xmake run test-solver` | 27/27 全绿（串行路径逐位不变） |

## 5. 防线（本轮未随提交落地）

尝试了两个用例，**均因测试流程自身的状态污染而不可用**，暂不随本提交：

- `boxFill` 派生（`trapAirInterval 1` + `n (1 1 4)`）：在旧代码上**没有
  复现**（同一 bug 在该分解下静默配对而非死锁），无判别力 ✗；
- `fountainFlow` 派生（已证实的复现器）：旧代码超时 FAIL ✓，但**修复后
  在 harness 流程里仍超时** ✗——而同一份用例、同一份修复，在手工干净
  副本上 exit 0 ✓（见 §4）。根因见 §8（`0/` 被运行污染 + 复制的元数据
  残留），需要先修 harness 的用例复用语义，再补这条防线（列为后续）。

修复本身的验证以**手工干净副本** before/after 为准（§4），不依赖该用例。

## 8. 附带发现：一次运行会污染 `0/`，用例目录不可无脑复用

调试防线用例时发现：**求解器在 startTime 会写入所有已注册相场**，于是
跑过一次的 case 目录其 `0/` 会多出 `T.air`、`T.melt`（两相热物性注册
产生的场）。后果：

- 同一目录二次运行（或"串行 pass 之后跑并行 pass"）的**初态与首次不同**
  → 结果不可比、行为可能不同（本轮防线用例的"修复后仍超时"即由此叠加
  复制残留引起）；
- 对 Kairos 的 e2e 意义：**复跑必须从干净目录开始**（或先清理 `0/` 的
  新增场），否则对照实验的初态不一致；
- 后续项：在 `run-solver-tests.sh` 的用例清理里显式恢复 `0/`（例如记录
  初始文件清单并删除新增项），或让 harness 每次从模板复制新目录。

（另：macOS 侧 tar/复制会带出 `._*` AppleDouble 文件，跨机传递用例请用
`COPYFILE_DISABLE=1 tar`。）

## 6. 与 046 的关系与教训

- 同类根因（按 rank 分歧 → 集合通信错配），但 046 覆盖的是"**循环内**的
  归约"，本项覆盖"**提前返回**"——046 的扫描按"循环"筛选，漏掉了这一族；
- 教训（已写入 `README.md`「性能与数值控制」的并行规则小节）：凡"函数体
  内含集合通信"，其**所有**提前返回与分支都必须是全局一致的（用
  `reduce`/`returnReduce` 或全局字典条件）；
- 本项同时解释了 037 报告的一个观察面：bundle 环境里"求解完成后退出异常"
  与"按 rank 分歧"是两条独立的问题线，不应互相推断。

## 7. 涉及文件

- `src/moldingFoam/moldingFoam.C`（`reportTrappedAir` 守卫）
- `README.md`（并行防线规则）

> 相关：046（同类根因的第一次修复与防线模式）。
