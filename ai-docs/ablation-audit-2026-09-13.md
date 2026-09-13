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
  （`moldingStage.C:39` 1 处、`moldingFoam.C:109` 2 处：成员初始化列表与
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
