# 043 — 未触发可选分支的用例覆盖（gateSealRamp / 深层热阻 / χ-η 耦合 / 质量修正器）

- 状态：in-progress（2026-09-13：gateSealRamp 分支已交付并断言；
  深层热阻 / χ-η 耦合 / 质量修正器三项待做，见 §8）
- 优先级：P2
- 依赖：018（gateSealRamp）、008（深层热阻）、024（χ-η）、031（修正器口径）
- 预估规模：1–2 天
- 来源：`ai-docs/coverage-audit-2026-09-13.md` 缺口 G2、G4、G5、G6

## 1. 背景与现状

审计确认以下**已实现特性没有任何用例触发**（均为「可选/非缺省路径」，
故缺省回归不受影响，但功能效果无测试证据）：

| 分支 | 现状 | 影响 |
|------|------|------|
| `packing.gateSealRamp`（018 平滑封冻，缺省 0） | 全仓库无用例赋值 → 非零斜坡分支从未执行 | 高压/大件场景的稳定性依赖该路径 |
| `moldingMoldTemperature` 的 `wallResistance`/`deepMoldTemperature`（008 路线 B 深层热阻） | 用例与模型测试均未配置（读取存在、缺省 0/300） | 模壁深层热阻的等效模型无证据 |
| CrossWlf 的 `crystallinity` 子字典（`chiInfinity`/`exponent`） | 无用例启用 → 024 的 η(χ) 只有因子函数级模型测试 | 求解器内 χ-黏度耦合路径无 case 覆盖 |
| `massFix`/`massFixRelaxation`/`massFixGlobal` 开启路径 | 无用例开启（highPressure 以注释关闭，验的是 031 的「无修正器」口径） | 修正器打开时的守恒行为无回归保护 |

## 2. 目标

1. 每个分支至少一个用例触发，并带**定量或模式断言**（优先扩展现有
   用例的参数化变体，避免用例数量膨胀）；
2. 关闭的缺省路径行为不变（原用例逐字节通过）；
3. 若某分支实测不可用/不推荐（如修正器已被 031 取代），在任务文档记录
   并在审计文档标注「有意不覆盖」，而不是留下沉默缺口。

## 3. 技术方案

- **gateSealRamp**：在 `tests/cases/gateFreeze`（或 boxFill 变体）设
  `gateSealRamp 0.01`，断言封冻期内 `sum(inlet)` 通量按斜坡衰减
  （模式：封冻时刻被推迟/通量单调趋零）；
- **深层热阻**：`tests/cases/moldSteady` 变体设 `wallResistance`（与
  `deepMoldTemperature`），与 002 集总模型的稳态温度对拍（模型测试已有
  集总解，可直接移植为 Python 计算）；
- **χ-η 耦合**：`tests/cases/crystallization` 的 `CrossWlfCoeffs` 增加
  `crystallinity { chiInfinity 1; exponent 2; }`，验证器断言同 χ 下
  黏度高于无耦合路径（比值 ≥ 1+ε）；
- **修正器**：为 `massFixGlobal on` 增加一个短用例（或 highPressure 的
  参数化变体）断言质量残差被压到机器量级；`massFix`（逐单元）若确认
  已废弃，在 018/031 文档标注并由本任务在审计中标记「有意不覆盖」。

## 4. 工作拆解

1. 四个分支各一个最小配置 + 断言（每个 ≤2 小时）；
2. 回归：22+ 用例、模型测试、契约不劣化（本地定向 + CI 全量）；
3. 文档：各分支所属任务（008/018/024/031）补验收记录，审计缺口关闭。

## 5. 验收标准（DoD）

- 四类分支均有用例断言（或明确的「有意不覆盖」记录）；
- 既有 22+ 用例与契约 case 全绿；
- README/任务文档同步。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 新增变体使 CI 时间增长 | 变体保持秒级（复用几何，仅改字典） |
| χ-η 断言量级受网格影响 | 用同一网格比较「开/关」比值而非绝对值 |
| 深层热阻的解析参照不明确 | 复用 002 集总模型（已有 Python 实现与 1e-4 级对拍） |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `tests/cases/gateFreeze/`、`moldSteady/`、`crystallization/` | 变体配置与断言 |
| `scripts/verify-*.py` | 对应定量断言 |
| `ai-docs/tasks/{008,018,024,031}-*.md`、审计文档 | 验收记录与缺口关闭 |

## 8. 交付记录（2026-09-13）

- **gateSealRamp 分支（G2）已交付**：`tests/cases/gateFreeze` 设
  `packing.gateSealRamp 0.01`（此前全仓库无任何用例赋值 → 非零斜坡
  分支从未执行），`expectedPatterns` 新增启动回显断言
  `gateSealRamp        = 0.01`（回显来自 stage 构造，证明键被消费）。
  标定：斜坡开/关两变体封冻时刻同为 1e-4 s、回显分别为 0.01 / 0；
  `gateSealFactor` 已确认接线到 `moldingInletVelocity` 与
  `moldingPrghPressure` 两个 BC（非死代码）。实测 patterns 全 PASS。
  **未做**：斜坡效应对通量衰减曲线的定量断言（本次标定未取到稳定
  信号，留作后续增强）；
- **待做**：`wallResistance`/`deepMoldTemperature`（深层热阻）、CrossWlf
  `crystallinity` 子字典（χ-η 耦合）、`massFix*` 开启路径三项，手法同
  上（扩展现有用例 + 定量或回显断言）。
