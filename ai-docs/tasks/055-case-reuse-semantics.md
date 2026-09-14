# 055 — 测试流程的用例复用语义（`0/` 污染）与 054 防线落地

- 状态：planned
- 优先级：P1
- 依赖：054（防线用例被本项阻塞）、046（防线模式）
- 预估规模：0.5–1 天
- 来源：054 调试中的附带发现——**一次运行会改变 `0/`**，使同一用例目录
  二次运行的初态与首次不同（`ai-docs/tasks/054` §8）

## 1. 背景与现状

1. 求解器在 **startTime 会把所有已注册相场写进 `0/`**：跑过一次的目录会
   多出 `T.air`、`T.melt`（两相热物性注册产生的场）。实测：干净的
   `tests/cases/fountainFlow` 首次运行后 `0/` 从 5 个文件变 7 个；
2. 后果一：**harness 的"串行 pass → 并行 pass"在同一目录里跑**，第二个
   pass 的初态与第一个不同 → 行为/结果不可比（054 的防线用例在"修复后
   仍超时"就是这个叠加造成的）；
3. 后果二：对下游（Kairos e2e）——**复跑对拍必须从干净目录开始**，否则
   差异里混入了初态变化，会伪装成"求解器不稳定/不复现"；
4. 后果三：跨机传递用例时 macOS 的 AppleDouble（`._*`）会一起过去，
   污染 `0/`、`constant/`、`system/`（本轮踩到；对策
   `COPYFILE_DISABLE=1 tar`，或用仓库的 `git archive`/`vm-sync.sh`）。

## 2. 目标

1. **每个 pass 从模板初态开始**：`run-solver-tests.sh` 的每个 case/pass
   在临时目录里跑（`mktemp -d` + 复制），验证器读临时目录；`run-case.sh`
   与 `run-validation.sh` 在开跑前恢复 `0/`（例如 `git checkout -- 0/`，
   非 git 树时按记录的初始文件清单删除新增项）；
2. **054 的防线用例落地**：`tests/cases/parallelTrappedAir`（fountainFlow
   派生 + `system/nProcs 4`，`trapAirInterval` 保持 100）接入
   `system/nProcs` 的并行 pass；必须完成**两端验证**：pre-054 代码上
   超时 FAIL、修复后 PASS；
3. 文档：README 第 5 节与 `ai-docs/README.md` 约定写入"用例目录不可
   无脑复用 + AppleDouble 传递注意"。

## 3. 技术方案

- 临时目录方案（推荐）：把 harness 的清理段改为
  `work=$(mktemp -d) && cp -r "$caseDir"/. "$work"/ && cd "$work"`，
  断言与验证器全部对 `$work` 执行，结束时删除；`expectedPatterns`
  与 `verifyScript` 的路径语义不变（都在 case 根下）；
- 恢复方案（对 `run-case.sh` 这类"用户可见目录"）：先记录
  `git ls-tree -r --name-only HEAD -- 0`（若为 git 树），跑完比对并删除
  新增文件；非 git 树时在首次运行前写一份 `0/.initial-manifest`；
- 防线用例的两端验证命令写进 054 §5（pre-054 用 `git stash` 式回退守卫，
  修复后恢复），并在 case 头注释里注明"该用例依赖干净初态"。

## 4. 验收标准（DoD）

- `xmake run test-solver` 在同一工作树上**连续跑两次结果一致**（含
  `parallelTrappedAir` 的并行 pass PASS）；
- pre-054 代码上 `parallelTrappedAir` 超时 FAIL（harness 的
  `MOLDINGFOAM_PARALLEL_TIMEOUT` 生效）；
- 文档更新完成；契约 case 的验收数字不变。

## 5. 风险与缓解

- 临时目录会让日志/字段留在 `$work` 而非用例目录（调试不便）→ 失败时
  打印 `$work` 路径并保留（成功才删）；
- `run-case.sh` 的 `git checkout -- 0/` 会丢弃用户对 `0/` 的本地改动 →
  仅在"未跟踪文件"层面清理，或在交互式环境下先询问（CI 不需要）。

## 6. 涉及文件

- `scripts/run-solver-tests.sh`、`scripts/run-case.sh`、`scripts/run-validation.sh`
- `tests/cases/parallelTrappedAir/`（新用例）
- `README.md`（第 5 节）、`ai-docs/README.md`（约定）

> 相关：054（本项是其防线的落地前提）。
