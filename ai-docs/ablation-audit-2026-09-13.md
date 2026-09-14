# 仓库级消融与代码检查（2026-09-13）

审计对象：本仓库 `src/`、`tests/cases/`（基线 commit `53b2a45` 工作区）。
方法：

- **消融**：对代表性用例关闭/篡改其关键特性，随后跑该用例自己的检查
  （`expectedPatterns` + `system/verifyScript`）——「消融后仍 PASS」意味着
  该用例的断言**对该特性不敏感**（弱断言）；
- **代码检查**：删除全部对象文件做干净全量重编译（`-Wall -Wextra`）收集
  告警；静态扫描遗留标记/调试输出/仅出现一次的未引用函数与成员；沿用
  覆盖审计的「lookup 键是否被用例赋值」扫描。

## 1. 消融结果

| 用例 | 消融项 | 基线 | 消融后 | 判定 |
|------|--------|------|--------|------|
| `crystallization` | `crystallinityShrinkage 0.02 → 0`（χ↔收缩耦合） | PASS | FAIL | **sensitive** |
| `crystallization` | `latentHeat 2e5 → 0`（潜热） | PASS | PASS | **INSENSITIVE** |
| `fiberOrientation` | `lipscombRatio 3 → 1`（Lipscomb 各向异性黏度） | PASS | PASS | **INSENSITIVE** |
| `freezeOffGuard` | `freezeOffTemperature 418 → -1e6`（冻死守卫） | PASS | FAIL | **sensitive** |
| `voidCavitation` | `closure true → false`（汽蚀闭锁上限） | PASS | FAIL | **sensitive** |
| `moldSteady` | 追加 `wallResistance 1e-3`（深层热阻） | PASS | PASS | **INSENSITIVE** |
| `boxFill` | 追加 `pressureRamp 0.2`（保压斜坡） | PASS | PASS | **INSENSITIVE** |

- **sensitive（3）**：χ-收缩耦合、冻死守卫、汽蚀闭锁——断言有效，改动该
  特性会被抓住；
- **INSENSITIVE（4）**：潜热、各向异性黏度、深层热阻、保压斜坡——用例
  检查对这些特性不敏感，属审计 G3/G4/G9 家族的**实证确认**（此前只有静态
  的「键未被赋值」证据，现在有了「改了也不报警」的直接证据）；
- 其中汽蚀闭锁一行同时验证了 039 的验证器扩展（`closure off` 触发质量
  判据 FAIL，即消融本身被门禁捕获）。

## 2. 代码检查结果

- **编译告警（干净全量重建）**：修复前 **9 条**，全部为 `-Wreorder`
  （`moldingStage` 构造函数 1 处、`moldingFoam` 构造函数 2 处：成员初始化列表与
  声明顺序不一致）；按声明顺序重排后 **0 条**（`moldingFoam.H` 中
  `pressureRamp_/gateSealRamp_`、`freezeOffTemperature_/trapAirAlpha_`、
  `condAniso_/aInitial_` 三对）。重排只改列表顺序，实际初始化顺序本就是
  声明顺序，**无语义变化**，`modelTests` 全过；
- **静态扫描（57 个 `src` 文件）**：`TODO/FIXME/XXX/HACK/TEMP/WIP` 标记
  **0**；`Info<<` 调试输出残留 **0**；疑似未使用成员 **0**；
  `moldingFiberOrientation::conductivityTensor` 曾被列为疑似死代码，核实
  后由 `tests/modelTests.C` 调用（非死代码；其求解器侧集成属 025 的
  「各向异性热导率」待做项，已在 025 文档登记）；
- **键覆盖**：22 个 lookup 键仍无用例赋值（此前审计的分类不变，见
  `coverage-audit-2026-09-13.md` §3）。

## 3. 结论与跟进

断言强度呈「安全关键项敏感、增强项不敏感」的分布：守恒/守卫/闭锁类特性
都有敏感断言；不敏感的 4 项恰好是**增强/可选路径**（潜热对 χ 轨迹的影响、
各向异性黏度、深层热阻、保压斜坡），与覆盖审计的缺口清单一致，已登记：

| 消融发现的缺口 | 跟进任务 |
|---|---|
| 潜热断言缺失（crystallization/boxFill） | 044（参数覆盖补齐，新增子项） |
| `pressureRamp` 无断言 | 044（G3，已列） |
| 深层热阻无断言 | 043（G4，已列） |
| Lipscomb 各向异性黏度无断言 | 043/044（G9 家族，已列） |

## 附：复现命令

```sh
# 消融（VM 内，OpenFOAM 环境）
python3 /tmp/ablate.py      # 用例级消融驱动（patterns + verifyScript 判定）
# 编译告警
rm -rf src/Make/*Opt tests/Make/*Opt && xmake 2>&1 | grep -c warning:
# 静态扫描
# 见本文件 §2 的扫描口径：遗留标记 / Info 调试输出 / 单次出现函数与成员
```

## 7. 新增用例的消融（2026-09-14，040/043/044 交付件）

对本次会话新增的断言逐一做消融（同 §1 口径：消融后检查必须 FAIL）：

| 用例/变体 | 消融项 | 消融后 | 说明 |
|---|---|---|---|
| `eigenstrainGraded` | 移除 `constant/fvModels`（模型关闭） | **FAIL** | 尖端位移回落到 0.59375（解析 0.750，−20.8%）→ 验证器拒绝；首轮实现还会因未守卫的字典读取抛 traceback，已补存在性守卫 |
| `eigenstrainGraded` | 模型符号取反（历史缺陷） | FAIL（已修复） | 差分 −0.150000 vs 要求 +0.150000；修复后 0.833% |
| `crystallinityViscosity` | 移除 `crystallinity` 子字典 | **FAIL** | 配置守卫拦截（η 由 5.62e10 回落到 6.36 Pa·s 量级） |
| `nuCorrelation` | `C`×2（等效 htc 2×） | **FAIL** | 模温终值 306.877 → 312.027 K（+5.15 K，为 0.05 K 公差的 100×） |
| `massFixGlobal` | `massFixGlobal false` | **FAIL** | 配置守卫拦截；残差由 −8.2e-15 回落到 −2.5e-08 kg |
| `voidCavitation` | `closure false` | FAIL | 质量判据拦截（onset 损失 24.7%） |
| `pressureRamp`（fatalPressureRamp） | 去掉负值条目 | 反向 | `expectFailure` 反转期望：移除后 foamRun 成功 → runner 判 FAIL |
| `case-contract`（041） | 容差放宽 10× | 仍 PASS | 计时 −24% 且验收通过（非强度消融，属增益标定，见 041 §4b） |

**结论**：本次新增的全部断言均通过消融（关掉被断言的对象后检查失败），
未出现"永远通过"的假测试；其中 `eigenstrainGraded` 的消融还顺带暴露出
验证器未守卫字典读取的健壮性问题，已修复。

## 8. 本地回归（2026-09-14，本会话 29 笔改动）

本会话的改动（新 fvModel ×2、新用例 ×4、runner 与脚本修复、文档）在推送前
做了一次本地范围回归（VM arm64，当前源码）：

| 项 | 结果 |
|---|---|
| `xmake` 干净重建 | BUILD_EXIT=0 |
| `modelTests` | All tests passed ✓ |
| `run-solver-tests.sh tests/cases` | **All 26 solver case(s) passed** ✓（22 原有 + fatalPressureRamp / massFixGlobal / crystallinityViscosity / nuCorrelation） |
| `run-validation.sh validation/eigenstrainGraded` | PASS（0.833% < 1.5%） |

合计 159 个 PASS 断言、0 FAIL。**未在本地跑**：17 个重型数值验证（CHT 等）
与 4 子域契约——按既定分工留待 nightly；由于本地提交尚未推送，CI 侧
尚未验证这批改动（这是当前唯一未闭环的验证面）。

---

## 复核 2026-09-14

- 消融矩阵未全量重跑（043/044 已补齐当年 INSENSITIVE 的四项断言并逐项
  验证）；重跑留到下一次大规模改动前，见 `tasks/051` §1a ④；
- 本会话新增的界面/并行改动（048 v1.28、052 v1.29、054 修复）已各自带
  验收与回归证据，未纳入本快照的消融范围。
