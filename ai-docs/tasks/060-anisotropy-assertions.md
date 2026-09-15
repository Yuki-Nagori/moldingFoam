# 060 — 各向异性耦合的断言缺口（Lipscomb 黏度 / 各向异性热导率）

- 状态：in-progress（本轮完成核查与实测取证：两个耦合生效但效果 <1%、
  该 case 无法作为判据，判据须另设计；尚未实现新判据）
- 优先级：P2
- 依赖：034/040（两个耦合的实现）、025（取向集成）、043/044（断言强度补齐）
- 预估规模：1–2 天（含两个验证 case + 解析判据）
- 来源：059 的键覆盖扫描之后的延伸核查——顺着"用例是否赋值"往下一层问
  "**赋了值，但效果有人断言吗**"

## 1. 现状（已核实，勿重复调查）

两个各向异性耦合**都已实现、且在 `tests/cases/fiberOrientation` 里被执行**，
但**没有任何断言**：

| 耦合 | 实现位置 | 用例是否执行 | 断言 |
|------|----------|--------------|------|
| Lipscomb 各向异性黏度 | `CrossWlf::nu`：`coeffs_.useOrientation && mesh.foundObject<volSymmTensorField>(orientationField)` 时给 `nu` 乘 Lipscomb 因子（`(a:D)²/(D:D)` 二次闭包，`lipscombRatio` 截断） | 是（`momentumTransport` 设 `lipscombRatio 3`，`a` 场由求解器创建） | **无** |
| 各向异性热导率 | `moldingFoam::energyPredictor`：`condAniso_ != 0 && a_.valid()` 时把 `fvm::laplacian(kappaEff, T)` 换成张量 `lambdaEff = kappaEff (I + condAniso (a − I/3))` 的隐式 laplacian | 是（`moldingDict` 设 `conductivityAnisotropy 0.5`） | **无** |

现有断言只覆盖：取向输运（`max|a12|`、`tr(a)` 漂移）与各向异性**收缩**张量
（`verify-fiber-orientation.py`）。也就是说：若这两个耦合的**激活条件**或
**因子**回归（例如 `conductivityAnisotropy` 被忽略、符号写反、`lambdaEff`
退化成标量），现有 26+ 用例与全部模型测试**全绿**。

**本轮实测（同 case、同一时间点比较，2026-09-15）**——两个耦合确实生效，
但在这个 case 里都**太弱、无法作为判据**：

| 配置 | 稳态 `max|a12|` |
|------|-----------------|
| 出厂（`conductivityAnisotropy 0.5` + `lipscombRatio 3`） | 0.14749 |
| 关热导率耦合（`conductivityAnisotropy 0`） | 0.14887（+0.9%） |
| 关黏度耦合（`lipscombRatio 1`） | 0.14823（+0.4%） |

**两个坑（本轮踩过，务必避免）**：

1. **日志第一条取向样本是无用的**：`max|a12|` 的第一条 log（0.193526）在两种
   配置下**完全相同**——此时温度场还均匀，∇T = 0，各向异性 λ 不起作用。
   若拿一次运行的第一条去比另一次的第二条，会得出"耦合有 30% 效果"的**错误**
   结论（本轮真踩到过）。跨运行比较必须取**同一物理时刻**的量；
2. 因此这两个耦合的判据**必须是设计出来的**（让耦合成为主导物理），
   见 §2；`fiberOrientation` 这条例只会增加无区分度的断言。

`modelTests` 只覆盖了**因子函数本身**（`conductivityTensor` 构造、
`lipscombFactor` 的端点行为），不是求解器内的使用路径。

## 2. 目标

各补一条**有区分度**的断言（能区分"耦合生效"与"耦合被忽略"），优先用
解析值而不是"两次运行互比"（后者要求同一 case 跑两遍，harness 一 case
一次运行）。

1. **各向异性热导率**（解析，推荐先做）：纯导热、定常、取向冻结的平板——
   `a = diag(1,0,0)`（完全沿 x 排列）、左壁 `T_hot`、右壁 `T_cold`、无流动
   （`alpha=1` 充满、速度 0）。定常热流
   `q = lambda_xx (T_hot − T_cold)/L`，其中
   `lambda_xx = kappa (1 + 2/3 · conductivityAnisotropy)`（各向同性时
   `= kappa`）。用上游 `wallHeatFlux` 函数对象记录壁面热流，验证器按上式
   对拍（判别度：`aniso 0.5` 时 `lambda_xx/lambda_iso = 1.333`，远超网格/
   离散噪声）。注意选温区**高于**熔体冻结温度，避免相变把纯导热搅进来；
   `kappa` 取 case 物性里的常数。
2. **Lipscomb 黏度**（对照式或解析）：同样用定常 Couette/槽道剪切，
   `a` 初始化为与剪切面成固定角度并冻结（无取向源时 FT 源≈0），
   解析因子 `f = 1 + (ratio−1)·3/2·(a:D)²/(D:D)` 可由 case 的 `a` 与剪切率
   算出 → 与实测压降（或入口压力）对拍；若数值噪声过大，退化为"同一 case
   在 `lipscombRatio 1`（等价关闭）与 3 之间压降差 ≥ 解析差的 80%"这类
   单调性+量级断言（需要两条 case 目录，或把 ratio 做成 case 的两个变体）。

## 3. 技术方案要点

- 两个 case 都从"冻结取向 + 单一物理"出发，避免与输运、相变、收缩耦合；
- 取向场用 `0/a` 给定（求解器 `a` 场是 `READ_IF_PRESENT`，且无流动时不被
  改写）——必要时把 `fiberOrientation` 的源项关闭（核对字典项）；
- 判据写进 `scripts/verify-*.py`，并接入 `xmake`/nightly 的既有通路；
- README §6 的 fiberOrientation 小节补上"各向异性黏度/热导率已实现 + 由
  哪个用例的哪条判据覆盖"。

## 4. 验收标准（DoD）

- 两条断言都能在**故意关掉耦合**时失败（用一次实验证明敏感性：临时把
  `conductivityAnisotropy` 置 0 / `lipscombRatio` 置 1，判据必须 FAIL）；
- 既有用例与模型测试全绿，`xmake run test` / `test-solver` 不受影响；
- README 与 025 的"待做"清单同步（025 里的"各向异性热导率"应改为"已实现，
  判据见 060"）。

## 5. 风险

- 纯导热定常 case 在 VoF 求解器里需要 `alpha=1` 且无流动，可能触发求解器的
  填充/保压状态机 → 用 `injection`/`packing` 设置让状态机停在一个稳定分支
  （参考 `validation/stefan`、`validation/couette` 的做法）；
- Lipscomb 的剪切 case 里 `a` 会被剪切流动带动（Jeffery 项）→ 若不关闭
  取向演化，解析因子要用**实测** `a` 反算，判据改为"实测 η 比 = 实测 a 给出
  的因子"（仍然有区分度，只要耦合被忽略就对不上）。

## 6. 涉及文件

- `validation/`（新增两个基准）或 `tests/cases/`（若做成求解器用例）
- `scripts/verify-aniso-conduction.py`、`scripts/verify-lipscomb.py`（名字待定）
- `src/viscosityModels/CrossWlf/CrossWlf.{H,C}`、`moldingFiberOrientation.{H,C}`
  （只读，用于对齐因子定义）
- `README.md` §6、`ai-docs/tasks/025-fiber-orientation-integration.md`

> 相关：025（取向集成）、034/040（两个耦合的实现）、043/044（断言强度）、
> 059（键覆盖扫描，本条是"再往下一层"的核查）。
