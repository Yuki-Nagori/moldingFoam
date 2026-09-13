# 测试覆盖完整度审计（2026-09-13）

审计对象：本仓库（基线 commit `c79e16a`）。方法：**静态核对四条链路**——
源码特性/配置键 → 用例（`tests/cases`、`validation`、`case-contract`）→
验证器脚本 → xmake 目标 → workflow；并对 `src/` 的全部 `lookup` 键做
「用例字典是否赋值」扫描（647 个字典文件，按行判定、忽略注释）。
本次未运行任何 case（整体回归由 nightly 承担）。

## 1. 总量与通路

| 层 | 规模 | 门禁 |
|---|---|---|
| 模型测试 `tests/modelTests.C` | 13 组 / 73 个断言点 | per-PR CI（amd64+arm64 `xmake run test`）+ nightly |
| 求解器特性用例 `tests/cases/*` | 22 个（全部有 `expectedPatterns`，其中 20 个带数值验证器） | nightly `solver-cases` |
| 数值验证 `validation/*` | 17 个（全部带验证器） | nightly 全覆盖（thermal-flow 8 / structural 4 / cht 6） |
| 契约验收 `case-contract` | 1 个（`verify-case.py`：质量守恒 < 1e-3） | nightly `contract`（4 子域并行） |
| 验证器脚本 `scripts/verify-*.py` | 37 个（36 被 case 引用 + `verify-case.py` 为契约缺省） | 无孤儿脚本 |

- per-PR CI（`ci.yml`）仅构建 + 模型测试（双架构）；重型回归全在
  nightly（UTC 22:00 或手动触发）。
- CD（`cd.yml`）：双架构构建 + 模型测试 + `bundle`。
- 2026-09-13 起 nightly 覆盖 **17/17** 数值验证 case（此前缺
  `highPressure`，由 `dd97313` 补齐）。

## 2. 特性 ↔ 用例映射（无孤儿）

- **10 个边界条件全部被用例触发**：`moldingChannelCooling`→coolantChannel；
  `moldingConvectiveCooling`→moldCHT-cooled；`moldingInletVelocity`→20+ 用例；
  `moldingMoldTemperature`→moldSteady/moldCycles/coolantChannel/crystallization；
  `moldingPrghPressure`→highPressure 等 22 个；`moldingRunnerTemperature`
  →runnerTemperature；`moldingSlipVelocity`→couetteSlip；
  `moldingTractionDisplacement`→anisoShrinkBar（含 `0/eigenstrain` 场）；
  `moldingVentPressure`/`moldingVentVelocity`→19 个用例。
- `fvModels/viscoelasticStress`→viscoelasticFlow；`moldingCoolantFluid`
  →coolantWater/coolantWaterMold。
- 已落地重点特性均有 case：周期重置（cycleReset/cycleResetFilled/
  moldCycles/moldSteady）、短射冻结防线（freezeOffGuard）、汽蚀
  （voidCavitation）、空洞/张力场（voidFraction）、多级工艺
  （processProfile）、流道网络（runnerNetwork/runnerTemperature）、
  结晶（crystallization/crystallizationAdvection）、纤维取向
  （fiberOrientation/fiberOrientationAdvection）、翘曲/收缩
  （warpagePlate/warpageAniso/shrinkBar/anisoShrinkBar）、高压守恒
  （highPressure）、CHT 全家（moldCHT ×6）。

## 3. 缺口清单（按建议修复顺序）

### P1 — 已具备脚本、只差接线

- **G1 `scripts/smoke-exit.sh` 未进任何 workflow**（037 的堆破坏退出
  防线）。037 报告自身即写明「建议接入：CI（快）或夜间」。最小改动：
  nightly `contract` job 追加 `bash scripts/smoke-exit.sh case-contract`。

### P2 — 已实现特性，无任何用例触发或断言

- **G2 `gateSealRamp`（018 平滑封冻）**：全仓库无用例赋值 → 非零斜坡
  分支从未执行（缺省 0 保持旧行为，故不影响现有 DoD）。
- **G3 `packing.pressureRamp`（038）**：自适应缺省在用例中执行但无断言；
  显式值分支与负值 `FatalIOError` 未覆盖；启动回显（`pressureRamp =
  auto (...)`）未被任何模式/验证器断言；038 报告的 E1–E7 实验矩阵未跑。
- **G4 `wallResistance` / `deepMoldTemperature`（008 路线 B 模壁深层
  热阻）**：源码读取、用例与模型测试均未配置 → 该功能效果无测试证据。
- **G5 CrossWlf 的 χ-黏度耦合子字典 `crystallinity`（及 `chiInfinity`/
  `exponent`）**：无任何用例启用 → 024 宣称的「η(χ)」目前只有因子函数
  级模型测试（`modelTests` 的 `crystallinityFactor`），求解器内耦合路径
  无 case 覆盖。
- **G6 质量修正器开启路径**：`massFix`/`massFixRelaxation`/`massFixGlobal`
  无用例开启（highPressure 以注释关闭 `massFixGlobal`，验证的是 031 的
  「无修正器 2.58e-4」口径）→ 修正器打开时的守恒行为无回归保护。
- **G7 038 防线断言**：`fillVelocityWarn` 阈值从未被配置、预警文本无
  断言；`moldingFoam.C:1876` 的「非有限快速失败」`FatalError` 路径
  从未被触发（属失败路径，可接受，但与 README 的 P1 防线claim不完全对应）。

### P3 — 强度不足或次要旋钮

- **G8** `cycleReset`、`gateFreeze` 两个 solver 用例仅有模式断言，
  无数值验证器（其余 20/22 为数值验证）。
- **G9** 次要分支参数无用例配置（模型级有解析覆盖）：runnerNetwork 的
  `powerLaw`（K/n）、moldingMoldTemperature 的 `Nu` 相关式
  （C/m/n/Re/Pr/k/D）、fiber `lambda` 覆盖、`trapAirAlpha`、`hsRef`、
  moldingPrghPressure 的 `relaxation`、runnerNetwork 的 `wallTemperature`。
- **G10 平台与并行范围**：重型验证只在 nightly 的 x86_64；arm64 仅构建
  + 模型测试；并行仅 nightly `contract`（4 子域）覆盖（本次修复期间实测
  两平台数值存在 1–10% 量级差异，见 019 文档）。
- **G11** `perf-scaling.sh` 为手工基准（设计如此，041 跟踪长期项）。

## 4. 结论

结构完整度高：三层测试 + 契约验收齐备，验证器无孤儿、目标无漏排
（17/17 验证 case 进 nightly），边界条件与模型全部有触发用例。剩余
缺口集中在「防线/推进器类参数的 CI 触发器」与「可选子路径参数」，不
影响当前 DoD 的可信度；建议按 **G1 → G2/G4/G5 → G6/G7 → G8/G9** 的
顺序补齐（多数只需 1 个用例或 1 行接线）。

## 附：复现命令

```sh
# 用例/验证器总量
ls -d tests/cases/*/ | wc -l; ls -d validation/*/ | wc -l; ls scripts/verify-*.py | wc -l
# 各 case 的验证器接线
for d in validation/*/ tests/cases/*/; do [ -f "$d/system/verifier" ] && echo "$d verifier"; \
  [ -f "$d/system/verifyScript" ] && echo "$d verifyScript"; done
# 未在用例中赋值的 lookup 键（Python 扫描，见审计方法）
# 边界条件 → 用例
for bc in moldingChannelCooling moldingConvectiveCooling moldingInletVelocity \
  moldingMoldTemperature moldingPrghPressure moldingRunnerTemperature \
  moldingSlipVelocity moldingTractionDisplacement moldingVentPressure \
  moldingVentVelocity; do echo "== $bc"; grep -rl "$bc" validation/ tests/ case-contract/; done
```
