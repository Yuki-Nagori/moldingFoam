# 061 — 重启续跑（`startFrom latestTime`）的连续性与状态持久化

- 状态：**done**（连续性 + 密封状态持久化已实测固化，并**据此修掉一个真 bug**：
  重启时 `gateSealTime_` 丢失；仅剩 `ventSealed=1` 路径未覆盖）
- 优先级：P2
- 依赖：012（多周期）、026（闸口密封）、008/019（模温状态）
- 预估规模：1 天
- 来源：查"文档声称 vs 有无证据"时发现——**全部 51 个 case 都用
  `startFrom startTime`**，而 README 两处明确声称"随场持久化、重启续读"
  （集总模温状态 §6、`moldingStage` 的 `ventSealed`/`gateSealed` §6）→ 长跑
  必然重启，这条零覆盖是安全相关的

## 1. 做了什么

`validation/restartContinuity`（`xmake run restartContinuity`，已入 nightly）
+ `scripts/verify-restart-continuity.py`：

- case 本体是一个**薄载体**（取 `coolantChannel` 的物理：静止纯导热 + 集总
  模温边界，dt 固定 0.02、endTime 10）；真正干活的是验证器；
- 验证器把 harness 已经跑完的那一次当**连续参考**，再把 case 的输入
  （`0/`、`constant/`、`system/`）复制到临时目录，跑到 `endTime/2`，然后
  `startFrom latestTime` 续跑到 `endTime`，逐场比对末时刻：

| 场 | 连续 vs 重启 的相对差 | 相对初值移动 |
|----|----------------------|--------------|
| `T` | **3.26e-08** | 2.19% |
| `p` | **9.99e-08** | 0.08% |
| `p_rgh` | **9.99e-08** | 0.08% |
| `alpha.melt` | 0 | 0（恒为 1） |

差值与 `writePrecision 8` 的**写入精度同量级**（即两条轨迹在数值上一致）；
判据：每场相对差 ≤ 1e-6，且**至少一个场**相对初值移动 ≥1e-4（否则比对是
空的——`alpha.melt` 恒 1 就是这种情况）。

## 2. 机制澄清（读代码确认，README 已改）

原表述"温度状态经 `UniformDimensionedField` 随场写出、重启续读"**机制不
准确**：`T_`（`UniformDimensionedField`）是**进程内**状态，构造时只给了
`IOobject(name, instance, registry)` → 默认 `NO_READ/NO_WRITE`（运行目录的
`uniform/` 里只有 `time`，没有它）。真正的持久化路径是：

- 写：边界把状态写进**patch 字典的 `T` 条目**（`writeEntry(os, "T",
  T_.value())`，随 `0/T`→`10/T` 一起写）；
- 读：重启时构造边界，`dict.lookup<scalar>("T")` 把状态读回。

行为本身是对的（本 case 就是证据），只是 README 把载体写错了。§6 已按实测
机制改写并指向本任务。

## 3. 另一个实测发现（无害，但会绊人）

派生相温场 `T.melt`/`T.air` 在重启后写成 `uniform 300`，而连续跑写
`nonuniform List<scalar> 4(300 300 300 300)` ——**数值相同、表示不同**。所以
"逐字节比对时间目录"跨重启会失败。验证器把字段按值解析（`uniform` 与
`nonuniform` 都吃），并把这条写进了脚本注释。

## 3a. 本轮发现的真 bug 并已修复：`gateSealTime_` 重启丢失

把"密封状态持久化"纳入判据后立刻抓到问题：同一个 case，连续跑末状态是
`1 0 1 0 0.02`，重启跑是 `1 0 1 0 -1`（字段顺序：`stage==packing`,
`switchTime_`, `gateSealed_`, `ventSealed_`, `gateSealTime_`）。

**根因**（`moldingStage` 构造函数）：`regIOobject` 基类构造期会读文件并调用
`readData()`（它**确实**读了 `gateSealTime_`），但构造函数体里又**手写了一段
恢复逻辑**（`headerOk()` 分支），那段只读了前四个字段、**漏了
`gateSealTime_`** ——于是重启后 `gateSealTime_` 停在未置位的 `-1`，而
`gateSealed_` 被恢复成 true，闸口只在"未封→封"的转变时才调用
`sealGate(t)`（不再发生），封闸时刻因此永久丢失。

**影响**：`packing.gateSealRamp > 0` 的工况里，重启后 `gateSealFactor` 会按
`1-(t+1)/ramp` 直接钳到 0（视为早已封完），而连续跑此刻还在 ramp 中——重启
点附近闸口密封过程不一致（`gateSealed_` 标志本身是对的，所以此前没人注意到）。

**修复**（`src/moldingFoam/moldingStage.C`）：在构造函数体的恢复块里补读
`gateSealTime_`（可选字段，缺失时保持哨兵值，兼容旧的重启数据）。修复后
重启状态行与连续跑逐字段一致 ✓。

**敏感性证据**：该判据在修复**前** FAIL（`-1` vs `0.02`）、修复**后** PASS
——即"实现失效时判据会失败"这条要求由这次真实 bug 直接证明了。

## 4. 敏感性（如实记录：本轮未做实现侧实验）

按 061 的口径，判据应在**实现失效**时 FAIL。本 case 的状态丢失**不会静默**：
把写出的 `T` 条目去掉后，重启时的 `lookup("T")` 会直接 FatalError（响亮失败）
——所以这条判据的价值主要在"确认精确连续"而不是"捕捉静默错误"。要做实现侧
演示（把 `writeEntry` 注释掉再重建）留待 §5 的下一轮一起做。

## 5. 剩余覆盖与已知边界

1. ~~`moldingStage` 的密封状态持久化~~：**已全部覆盖**（见 §3a）——载体虽然
   静止，但 α≡1 使 `max(alpha) ≥ ventSealAlpha` 与 V/P 切换都触发；再把
   `hotEnd` 改成排气口（`moldingVentVelocity` + `moldingVentPressure`）后，
   实测末状态 **`1 0 1 1 0.02`**：`packing`、`gateSealed`、`ventSealed` 三者
   皆 1，且连续跑与重启跑的状态行**逐字段一致**（精确比对，无需容差）；
2. **自适应时间步下的重启不可逐位复现**：`adjustTimeStep on` 时 dt 的历史
   不持久化（重启从字典的 `deltaT` 重新起算，上游同行为），所以这类 case 的
   重启只能到容差级一致——值得写进 README 的"重启"说明，避免被误读为
   "重放式可复现"；
3. 实现侧敏感性演示（见 §4）。

## 6. 涉及文件

- `validation/restartContinuity/`（薄载体，`system/verifier` →
  `verify-restart-continuity.py`）
- `scripts/verify-restart-continuity.py`
- `README.md` §5 验证表 + §6 模温小节的机制表述
- `xmake.lua`、`.github/workflows/nightly.yml`（注册）

> 相关：059/060（"改了路径就确认覆盖"与"敏感性实验要让实现变"两条纪律，
> 本任务正是顺着前者的思路查出来的）。
