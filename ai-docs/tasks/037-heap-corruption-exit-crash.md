# 037 — 求解完成后退出阶段堆破坏崩溃（bundle exit 132/134/139）

- 状态：planned
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

## 4. 验收标准（DoD）

- 零步 / 正常 / 并行 / `FOAM FATAL` 四类路径退出码均为 0（FATAL 路径
  按 OpenFOAM 约定为非 0 的 FATAL 码，但**不得**出现 malloc 堆破坏）；
- `valgrind foamRun`（零步 case）无 invalid write/free（或 ASan 干净）；
- 现有全部回归保持绿；
- 检查并修正 CI/测试脚本对退出码的判定（避免非零退出码被忽略）。

## 5. 涉及文件

| 文件 | 改动 |
|------|------|
| `src/moldingFoam/…` | 定位并修复堆破坏 |
| `scripts/`、`xmake.lua` | 退出码判定加固（如需） |
| `ai-docs/tasks/037-*.md` | 本记录 |
