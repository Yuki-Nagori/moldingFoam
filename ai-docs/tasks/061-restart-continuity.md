# 061 — 重启续跑（`startFrom latestTime`）的连续性与状态持久化

- 状态：**done**（连续性已实测固化；闸口/排气密封状态的持久化留待带流动的载体）
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

## 4. 敏感性（如实记录：本轮未做实现侧实验）

按 061 的口径，判据应在**实现失效**时 FAIL。本 case 的状态丢失**不会静默**：
把写出的 `T` 条目去掉后，重启时的 `lookup("T")` 会直接 FatalError（响亮失败）
——所以这条判据的价值主要在"确认精确连续"而不是"捕捉静默错误"。要做实现侧
演示（把 `writeEntry` 注释掉再重建）留待 §5 的下一轮一起做。

## 5. 剩余（本任务只做了一半的覆盖）

1. **`moldingStage` 的密封状态持久化**（`ventSealed`/`gateSealed`）：
   `moldingStage` 是 `READ_IF_PRESENT` + `AUTO_WRITE` 的 regIOobject，
   运行目录里确实有 `moldingStage` 文件（实测），但本 case 静默、不触发
   密封 → 需要一个**带排气封堵/保压闸口密封的流动载体**（如把
   `runnerNetwork` 的物理接过来）才能验证"重启后密封状态不丢"；
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
