# 059 — 字典键覆盖扫描（未赋值键的分诊与取舍）

- 状态：**done**（2026-09-15，本轮扫描 + 处置；剩余项均有明确取舍理由）
- 优先级：P3
- 依赖：043/044（此前的键覆盖补齐）、058（本轮发现缺口的手法来源）
- 预估规模：半天
- 来源：058 收尾时发现"`moldingPrghPressure` + `runner` 这条路径没有任何
  用例覆盖"（而当时刚改过它的调用点）→ 顺势把**全部字典键**对用例做一次
  覆盖扫描

## 1. 方法（可复用）

```console
# 源码里读到的键
grep -rhoE '(lookup|lookupOrDefault|found|subDict|subDictPtr|lookupOrDefault<[A-Za-z:]+>)\(\s*"[A-Za-z][A-Za-z0-9_]*"' src/ \
  | grep -oE '"[A-Za-z][A-Za-z0-9_]*"' | tr -d '"' | sort -u
# 逐个看 tests/cases + validation + case-contract 里是否出现
```

**扫描的局限（结论只能当分诊用，不能当证明）**：纯文本匹配——
① 出现在**注释**里也算命中（`massFixGlobal`/`massFixRelaxation` 就有这个
风险，见 §2）；② 不含 `tests/modelTests.C` 的字典字符串，而很多模型键正是
由模型测试覆盖的；③ 不区分"赋了缺省值"与"赋了非缺省值"。

## 2. 扫描结果与处置（77 个键，10 个未在用例中出现）

| 键 | 性质 | 本轮处置 |
|----|------|----------|
| `trapAirAlpha` | 困气判据的熔体分数阈值（**唯一未赋值的求解器键**） | 在 `parallelTrappedAir/constant/moldingDict` 显式赋值（缺省 0.5；空气单元绝对计数无解析预期，故只做读取/分支覆盖，不加断言） |
| `deepMoldTemperature` | 043 点名的 `wallResistance`/`deepMoldTemperature` **一对只接了线一半** | 在 `moldSteady/0/T` 显式赋值 300 K（与该 case 的水温/初温一致，稳态判据 347 K 不变） |
| `gateOpenTime` | 058 阀时序的"晚开"半边 | 已由模型测试覆盖（`runnerValveTreeDictString`），无需用例 |
| `b3s`、`b4s` | Tait 系数 | 模型测试覆盖（PVT 表对拍），无需用例 |
| `relaxation` | `moldingPrghPressure` 的 Dirichlet 松弛（缺省 1） | 模型测试覆盖；用例保持缺省（缺省即推荐口径） |
| `peakTemperaturePressureShift` | 结晶峰温的压力漂移 | 模型测试覆盖，无需用例 |
| `hsRef` | hMelt 的参考焓（缺省 0） | 保持缺省：无物理理由在本库固定一个参考态 |
| `Q` | `moldingMoldTemperature` 的模壁热流 `Function1`（外部热源） | **保留为已知缺口**：该可选热源无 case 覆盖；如需可按 043 的"扩展现有用例"手法补（当前无需求方） |
| `massFix` | 018 时代的修正器开关（031 之后 `massFixGlobal` 为主） | 保持：缺省关；`massFixGlobal`/`massFixRelaxation` 的覆盖状态受"注释也算命中"影响，未在本轮核实 |
| `orientationField` | CrossWlf 的取向场名（缺省 `a`） | 与 **025 的"各向异性黏度/热导率"待做项**一并处理（功能侧未接入，键无用例属一致状态） |

## 3. 结论

- 本轮实际补上两处：`trapAirAlpha`（唯一未赋值的求解器键）与
  `deepMoldTemperature`（043 计划中漏接的一半）；两 case 复跑 PASS；
- 其余 8 个都有明确的"为什么不需要用例"（模型测试覆盖 / 缺省即推荐 /
  与待做项绑定），除 `Q`（可选外部热源，暂无需求方）外不留缺口；
- 更重要的产出是**手法**：058 那个缺口（压力边界 + 网络）不是靠读代码发现
  的，而是靠"改了调用点 → 问一句'谁覆盖它'"。已记入
  `ai-docs/diagnostics.md` 的排查纪律：**改动一条代码路径时顺手确认它有没有
  用例覆盖**，没有就先补（或明确记录不补的理由）。

## 4. 涉及文件

- `tests/cases/moldSteady/0/T`（`deepMoldTemperature`）
- `tests/cases/parallelTrappedAir/constant/moldingDict`（`trapAirAlpha`）
- `ai-docs/diagnostics.md`（纪律条目）

> 相关：043/044（键覆盖补齐）、058（缺口发现手法）、057（诊断类开销口径）。
