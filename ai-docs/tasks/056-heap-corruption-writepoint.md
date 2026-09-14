# 056 — 037 堆破坏：写入点定位（模块二分 + 内存诊断）

- 状态：in-progress（2026-09-14：kairos 侧模块二分已启动，结果待收）
- 优先级：P1
- 依赖：046/054（并行正确性已修，038/037 报告）
- 预估规模：1–2 天（含 VM 机时）
- 来源：Kairos 侧对 037 的质询（要求栈证据与复现条件）；本轮已给出栈与
  边界（`ai-docs/report-037-heap-crash.md` 的「补充 2026-09-14」）

## 1. 背景与现状

**已确认**（勿重复结论）：

- 栈：`free()` → `__run_exit_handlers` → `__GI_exit` → `UPstream::exit`
  ← `argList::~argList()`（4 rank 完全一致）→ 退出处理器阶段 free 到已
  损坏的 fastbin 块（写入在更早）；
- 边界：仅在 **bundle 环境 + np4 + 打印 `End` 之后**观察到；同一份源码在
  apt 环境（of14）不崩；bundle 树单一 inode（037 双载防线在位）、无 apt
  混用；串行与秒级小 case 干净；
- 输出完整性：崩溃运行的步数/切换点/质量预算残差与干净运行**逐位一致**，
  最终时间目录场文件齐全、`reconstructPar` 退出 0。

**未确认**（本任务要回答）：损坏写入发生在**我们的代码**、上游 OpenFOAM、
还是 MPI teardown 交互；以及能否在非 bundle 环境复现。

## 2. 目标

1. 在可复现环境（kairos VM + bundle 树 + 手工 `wmake` 构建的当前源码，
   见 `scripts/vm-sync.sh`/tar 同步）里稳定复现；
2. **模块二分**：在崩溃配置上按字典开关逐项关闭 moldingFoam 的可选路径，
   找出"崩溃消失"的边界：
   `writeFillTime`/`trapAirInterval`/`massBudget`(+`massFix*`)/
   `viscousDissipation`/`crystallization`/`fiberOrientation`/
   `viscoelastic`(`fvModels.viscoelasticStress`)/`moldingVoidClosure`/
   边界条件族（`moldingInletVelocity`、`moldingVent*`、
   `moldingRunnerTemperature`、`moldingChannelCooling`）
   每组合 ~4 min（cli10 @4），优先二分非默认项；
3. **内存诊断**：
   - `MALLOC_PERTURB_=<nonzero>` 看是否改变崩溃形态（提示 UAF/越界）；
   - gdb：`break malloc_printerr` + `bt` 已在手；再对**被 free 的块**做
     `x/8gx`（若 `MALLOC_CHECK_` 早退可拿到现场）；
   - 备选：`LD_PRELOAD` 分配器 shim（记录 alloc/free 调用栈，崩溃时 dump
     最近 N 次操作）——本轮验证过 shim 手段可行；
4. 产出：写入点（库/函数）或"最小复现 + 排除清单 + 下一步假设"。

## 3. 技术方案要点

- 复现脚本固化到 `scripts/`（例如 `repro-037.sh`：构建/同步/运行/抓栈），
  使结论可被他人重放；
- 若二分指向某个 fvModel/BC：把该模块的**构造/析构路径**重点审
  （注册表双删、`regIOobject` 生命周期、`autoPtr` 与网格注册表的交互），
  并与上游同名类对照；
- 若二分无结论：用 gdb 在 `__run_exit_handlers` 处 `catch syscall exit_group`
  之前 dump 堆（`heap`/`malloc_info`）并记录损坏块的邻居，交给 Kairos 侧
  对齐他们的现场。

## 4. 验收标准（DoD）

- 一条可复现命令（在 kairos VM 上）与二分结果表；
- 明确结论：写入点定位，或最小复现 + 排除清单 + 后续假设（含需要的
  Kairos 侧输入，如他们的原始栈/环境快照）。

## 5. 风险与缓解

- 环境相关、可能只在对方 VM 复现 → 保留 `18db784` 后新增的复现脚本与
  环境清单（VM 规格/OpenMPI 4.1.6/库构建方式），并请 Kairos 侧提供现场；
- 二分组合数多 → 先关掉差异最大的非默认项（fvModels、writeFillTime、
  trapAir），再细分。

## 6. 涉及文件

- 新增 `scripts/repro-037.sh`（或同义脚本）
- `ai-docs/report-037-heap-crash.md`（补记结论）

> 相关：054（并行的另一条线，已修）、038（Kairos 复测记录）。
