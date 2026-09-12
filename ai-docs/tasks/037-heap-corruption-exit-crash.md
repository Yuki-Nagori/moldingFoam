# 037 — 求解完成后退出阶段堆破坏崩溃（bundle exit 132/134/139）

- 状态：done（repo 侧：防线+ASan 验证完成；bundle 侧定位移交 Kairos，报告 `ai-docs/report-037-heap-crash.md`）
- 优先级：P0（驱动方拿到非零退出码，Kairos 只能按输出标记兜底）
- 依赖：无（libmoldingFoam.so 初始化路径）
- 预估规模：1–3 天

## 1. 现象

求解正常打印 "End" 后，进程在**析构阶段**崩溃：

```
malloc_consolidate(): unaligned fastbin chunk detected
libOpenFOAM.so(Foam::argList::~argList) -> dictionary::~dictionary -> free
```

- 串行、`mpirun -np 4`、成功路径、`FOAM FATAL` 路径**全部复现**；
- `MALLOC_CHECK_=3` 下同样报堆破坏；退出码 132/134/139；
- 环境：Ubuntu 24.04 arm64（Multipass/Apple M4）、bundle v0.2.0
  （openfoam14/linuxArm64GccDPInt32Opt/20260912）、apt OpenMPI。

## 2. 判别实验（已完成，结论）

- **零步运行**（`endTime = 0`，时间循环体一次不执行）仍崩溃 →
  堆破坏发生在**初始化路径**（字典读取 / 场与边界条件构造 / 模型构造），
  非时间循环累积；
- `FOAM FATAL` 提前退出路径同样出现该堆破坏；
- bundle 自带上游教程 `incompressibleFluid/planarCouette`（不加载
  `libmoldingFoam.so`）exit 0 → OpenFOAM 核心与构建正常，
  **问题在 libmoldingFoam.so**。

## 3. 定位步骤

1. `apt install valgrind`，对零步 case（case-contract，`endTime 0`）跑
   `valgrind --error-exitcode=9 foamRun`，取**第一处** invalid write/free
   的调用栈；
2. 备选：给 libmoldingFoam 加 `-fsanitize=address,undefined` 构建
   （xmake 的 cxxflags/cxflags），ASan 直接指出越界写入；
3. 重点排查构造路径：`new[]/delete` 配对、字典解析定长缓冲区写越界、
   注册场/边界条件越界写、EOS/黏度模型构造中的静态表。

## 3a. bundle 根因确认与修复（2026-09-13，用户 A/B 实证）

**根因（已在真实 v0.2.0 linuxArm64 bundle 上 A/B 验证）**：bundle 的
`platforms/linuxArm64GccDPInt32Opt/lib/` 下有**两份独立构建的同一模块**：

| 文件 | 大小 | 构建时间 | 来源 |
|------|------|----------|------|
| `libmoldingFoam.so` | 3,375,448 B | 09-12 14:01 | v0.2.0 新构建 |
| `libmoldingFoamSolver.so` | 1,774,664 B | 09-10 00:46 | 上一轮陈旧产物 |

两者 inode 不同、各自注册同一批选择表（`"moldingInletVelocity"` 分别
出现 71/67 次）。运行期两条加载路径同时生效：case 的
`libs ("libmoldingFoam.so")` 与 `foamRun` 的模块探测
`Foam::solver::load("moldingFoam")` → `lib<Solver>Solver.so`。同一模块
代码在进程内存在两份 → 18 条 `Duplicate entry … in runtime selection
table` → 退出期 `~argList` 堆破坏（`malloc_consolidate`，exit 134/139）。

A/B：删除 case 的 libs 行（只加载陈旧份）→ **exit 132 SIGILL**（陈旧份
含 M4 不支持的指令）；把 `libmoldingFoamSolver.so` 改为指向
`libmoldingFoam.so` 的符号链接（只加载一份）→ **exit 0**，完整链路
（`decomposePar -force` → `mpirun -np 4 foamRun -parallel` →
`reconstructPar`，Time=2s）全通。

本地为何不复现：本地 `libmoldingFoamSolver.so` 本就是符号链接（glibc
按 inode 去重只加载一份），且本地实验用 apt openfoam14 二进制而非
bundle 内环境树——两者合起来刚好绕开。

**仓库侧修复（本次落地）**：

1. `xmake.lua` bundle 目标：staging 后先
   `rm -f <tree>/platforms/<wmo>/lib/libmoldingFoam*.so` 清掉环境树里
   可能残留的陈旧安装，再拷贝新构建的 `libmoldingFoam.so` 并建立
   `libmoldingFoamSolver.so -> libmoldingFoam.so` 符号链接；随后用
   `stat -c %i` 断言两者 **inode 相同**，否则打包失败；
2. 发布产物检查（人工/CI）：同一平台 `lib/` 下不得有两份独立构建的
   同一模块（核对 inode/时间戳）；清理 runner 上 09-10 那份陈旧
   `libmoldingFoamSolver.so`，避免再次被打包；
3. CI 金丝雀：`scripts/smoke-exit.sh` 及全部 runner 增加
   `Duplicate entry` 检查（它比退出期堆破坏出现得早、易定位）——
   `Duplicate entry` 即失败。

**修复后 bundle 端到端复测（本 VM，按用户 §验证 步骤）**：解包修复后
的 bundle（`xmake run bundle`，staging 中先植入一个陈旧
`libmoldingFoamSolver.so` 以验证清理逻辑），在 **bundle 自含环境**中：

- 树内 `lib/` 只有一份 `libmoldingFoam.so` + `libmoldingFoamSolver.so`
  符号链接（inode 相同；断言 `stat -L -c %i` 通过，陈旧文件被清除）；
- 零步（`endTime 0`、`MALLOC_CHECK_=3 foamRun`）：**exit 0**，
  `Duplicate entry` 0 处、堆报告 0 处，`End` 正常打印；
- 完整并行链（`decomposePar -force` → `mpirun --oversubscribe -np 4
  foamRun -parallel` → `reconstructPar -latestTime`）：**三步 exit 0**，
  时间目录 `0`、`0.000197989` 齐全，两日志均无 Duplicate/堆报告。

**本地同类事件（混合版本对象，已定位并加守卫）**：第九次实验覆写
`moldingFoam.H` 后 `git checkout` 回退并增量重建，`modelTests` 在
`runnerNetworkTests` 返回时 `*** stack smashing detected ***`。
机制：wmake 经 `src/lnInclude` 的**符号链接**跟踪头文件依赖，回退时
`cp` 只更新了真实头文件的 mtime，链接自身 mtime 不变 → 依赖该链接的
部分 TU 未重编，同一库内混入两种类布局/内联定义（ODR 破坏）。
处理与验证：

- xmake 构建脚本加守卫：编译前扫描 `src/lnInclude/*.H`，用
  `stat`（**不跟随**）比较链接与目标 mtime，仅对过期链接
  `touch -h` 刷新（已实测触发“refreshed N stale lnInclude header
  link(s)”并正确重编）；
- 干净重建后 `All tests passed`；**ASan 重建（lib+tests）跑
  `modelTests`：0 处 ASan 错误、全部通过**——确认当前源码无潜伏
  越界，事件纯属混合对象构建态；
- 教训：头文件级实验回退后，先干净重建（或依赖本守卫）再跑套件；
  CI 每次全新 checkout 构建，天然规避。

## 3b. 取证结果（2026-09-12）

- 详见 `ai-docs/report-037-heap-crash.md`：当前与上一版源码（0b22ce9）
  本地重建后，零步/并行/FATAL + `MALLOC_CHECK_=3` 全部干净
  （源层面不复现）；valgrind arm64 SIGILL 属工具限制；
- **CI 掩盖缺陷已修复**：`run-solver-tests.sh` 的 `|| true` → 退出码
  检查；全部 runner 加堆报告 grep 守卫；
- 新增 `scripts/smoke-exit.sh`（零步 + FATAL 退出路径冒烟）并在本 VM
  验证 PASS；建议接入 CI/夜间；
- bundle 侧假设与复测步骤见报告 §4/§5（ISA/工具链、OpenMPI/ABI
  混用、ASan 重建）。

## 4. 验收标准（DoD）

- 零步 / 正常 / 并行 / `FOAM FATAL` 四类路径退出码均为 0（FATAL 路径
  按 OpenFOAM 约定为非 0 的 FATAL 码，但**不得**出现 malloc 堆破坏）✓
  （本 VM 全部干净）；
- 严格内存验证 ✓：**ASan+UBSan 重建后 ~100 步运行 0 处 ASan 错误**、
  仅 2 处上游 BasicThermo 构造序 vptr 报告（假阳性）；
- 现有全部回归保持绿 ✓（模型测试 + 18 求解器用例）；
- CI/测试脚本退出码判定加固 ✓（`run-solver-tests` 去 `|| true`、
  全 runner 堆报告守卫、新增 `smoke-exit.sh`）；
- bundle 侧（构建 ISA/MPI ABI 假设）与复测步骤见报告，移交 Kairos。

## 5. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/…` | 定位并修复堆破坏 |
| `scripts/`、`xmake.lua` | 退出码判定加固（如需） |
| `ai-docs/tasks/037-*.md` | 本记录 |
