# 报告 037 — bundle 退出阶段堆破坏崩溃：取证结论与建议

日期：2026-09-12。对象：`moldingFoam-openfoam14-linuxArm64GccDPInt32Opt-20260912.tar.xz`
（v0.2.0）在“求解正常结束后”析构阶段崩溃（exit 132/134/139，
`malloc_consolidate(): unaligned fastbin chunk detected`）。

## 1. 结论（TL;DR）

1. **源码层面无法复现**：当前源码与上一版源码（`0b22ce9`，bundel
   同期状态）在本 VM 重建后，零步/正常/并行/FATAL 四种路径下
   `MALLOC_CHECK_=3` 全部干净（exit 0 / FATAL 非 0 但无堆报告）。
   因此不是“我们源码里一个稳定必现的堆破坏”，更像是**构建环境/
   运行环境组合相关**的问题（见 §4 假设）。
2. **发现并修复了一个真实的 CI 掩盖缺陷**：`scripts/run-solver-tests.sh`
   用 `foamRun … || true` 吞掉退出码 → “End 后崩溃”会被判为全绿；
   已修复（退出码检查 + 堆报告 grep），并新增 `scripts/smoke-exit.sh`
   及其在夜间 CI 的接入（零步 + FATAL 两路径冒烟）。
3. 其余 runner（`run-case.sh` / `run-moldcht.sh` / `run-validation.sh`）
   本有 `pipefail`，但也已加“堆报告 grep”守卫，保证任何 glibc 堆破坏
   都会使验收失败。

## 2. 复现与取证记录（本 VM，Ubuntu 24.04 arm64）

| 实验 | 结果 |
|------|------|
| 当前源码，零步 case-contract，串行，`MALLOC_CHECK_=3` | **exit 0**，无堆报告 |
| 上一版源码（`git archive 0b22ce9`）重建库，同实验 | **exit 0**，无堆报告 |
| 当前源码，`mpirun -np 4`，`MALLOC_CHECK_=3` | **exit 0**，无堆报告 |
| 当前源码，故意错误 solver（FATAL 路径） | exit 1，无堆报告 |
| `valgrind --num-callers=25 foamRun`（零步） | **SIGILL（exit 132）**——valgrind 3.22 在 arm64 上不支持部分指令（工具限制，非本问题证据） |

注：本 VM 的 OpenFOAM 为 apt 基线二进制；`libmoldingFoam.so` 由本地
wmake（含 `-mcpu=native` 剥离 shim）构建。

## 3. 已落地的防线（repo 侧）

- `scripts/run-solver-tests.sh`：
  - `foamRun` 非零退出 → **FAIL**（不再 `|| true`）；
  - 日志含 `malloc_consolidate` / `corrupted fastbin|size` /
    `free(): invalid` → **FAIL**；
- `scripts/run-case.sh`、`run-moldcht.sh`、`run-validation.sh`：
  验收前同样 grep 堆报告 → 失败；
- `scripts/smoke-exit.sh`（新）：把任意 case 拷到临时目录，跑
  （a）`endTime 0` 零步（应 exit 0 且打印 `End`、无堆报告），
  （b）错误 solver 的 FATAL 路径（应非 0、无堆报告）。
  本 VM 结果：`zero-step exit = 0`、`fatal-path exit = 1`、PASS。
- 建议接入：CI（快）或夜间（`bash scripts/smoke-exit.sh case-contract`）。

## 3c. 根因确认（bundle 实测，2026-09-13）

bundle 的 lib/ 下有两份独立构建（新 `libmoldingFoam.so` + 陈旧
`libmoldingFoamSolver.so`，inode 不同），运行期 `libs` 与
`solver::load` 两条路径各加载一份 → 18 条 Duplicate entry → 退出期
堆破坏；陈旧份单独加载还会 SIGILL（M4 不支持的指令）。把 Solver 名
改为符号链接后全链路 exit 0。仓库侧修复：bundle 目标清残留+符号链接+
inode 断言；runner/冒烟加 Duplicate-entry 金丝雀。详见任务文档 037 §3a。

## 4. 给 Kairos/打包侧的排查假设（按优先级）

1. **构建 ISA/工具链**：CD runner 的 arm64 `-mcpu=native` 已知会注入
   SVE 等指令（repo 里为此加了编译器 shim）。若 v0.2.0 bundle 的
   `libmoldingFoam.so` 或随包 OpenFOAM 在 shim 生效前后混合构建，
   在 Apple M4 上可能出现指令级差异 → 先前的越界/错位写 → 退出时
   `malloc_consolidate`。**建议**：用当前 HEAD 重新出 bundle，确认
   构建日志中 shim 生效（`-mcpu=native` 被剥离）。
2. **OpenMPI/ABI 混用**：bundle 自带 OpenFOAM 与 MPI；若运行环境里
   `LD_LIBRARY_PATH`/`OPAL_*` 残留了 apt 的 OpenMPI（用户描述里
   提到 apt OpenMPI），`argList` 的 MPI 结构与析构会错配——崩溃点
   恰是 `~argList → ~dictionary → free`。**建议**：bundle 启动脚本
   显式清环境（`env -i` 或 unset `LD_LIBRARY_PATH OPAL_* OMPI_*`），
   只用 bundle 内 MPI；`ldd libmoldingFoam.so` 确认 MPI 路径一致。
3. **真实源码 UB（构建相关）**：**已在本 VM 完成 ASan/UBSan 验证**
   （`src/Make/options` 临时 `c++FLAGS += -fsanitize=address,undefined`
   + `LD_PRELOAD=$(gcc -print-file-name=libasan.so)`，否则经 `libs`
   加载时运行库未初始化会段错误）：
   - case-contract，普通运行（~100 步）：**exit 0，0 处 ASan 内存错误**；
   - UBSan 仅 2 处 `BasicThermo` 构造期 vptr 报告（上游
     `static_cast<const MixtureType&>(*this)` 于基类构造期间——已知
     构造序模式、非本库错误）；
   - 结论：**本源码在本地构建下内存干净**；bundle 崩溃更指向
     §4.1/§4.2（构建 ISA / MPI ABI）而非源码堆破坏。

## 5. 关键命令（供 Kairos 复测）

```bash
# 零步复现路径
cp -r case-contract /tmp/t && cd /tmp/t
source <bundle>/openfoam14/etc/bashrc
rm -rf constant/polyMesh log.* [1-9]*        # 注意：勿用 [0-9]*（会删 0/）
blockMesh
sed -i "s/^endTime.*/endTime 0;/" system/controlDict
MALLOC_CHECK_=3 foamRun; echo exit=$?         # 期望 0 且无 malloc_consolidate
bash scripts/smoke-exit.sh case-contract      # 期望 PASS
```

## 6. 状态

- Task 037：repo 侧防线完成（退出码/堆守卫/冒烟脚本）；
  **bundle 侧定位需在 Kairos 环境按 §4 复测**；源码侧在本地两组
  版本上均为干净。
