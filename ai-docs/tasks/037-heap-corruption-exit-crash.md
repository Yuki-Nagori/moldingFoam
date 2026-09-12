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

## 3a. 取证结果（2026-09-12）

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
