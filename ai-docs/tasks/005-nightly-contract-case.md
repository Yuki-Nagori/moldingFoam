# 005 — CI 夜间契约 case 回归

- 状态：done（2026-09-10。`.github/workflows/nightly.yml`：schedule
  UTC 22:00 + workflow_dispatch；apt 安装带 actions/cache 缓存；
  失败自动开/评论 issue 并上传 log artifact。运行时验证需推送后
  手动触发一次全链路）
- 优先级：P2
- 依赖：无
- 预估规模：半天

## 1. 背景与现状

CI（`.github/workflows/ci.yml`，push/PR 触发）只跑编译 + 模型测试；
契约 case（约 4000 步、并行 4 子域、runner 上 20–40 分钟）已从 CI
移除。后果：能量方程、潜热、顶出判据、验收脚本等**集成级行为没有
自动回归**，只能在本地 VM 手工跑。

潜热（001）与模温（002）等能量侧功能落地后，契约 case 是唯一能
暴露"编译通过但物理不对"问题的测试，需要一条不阻塞 PR 的回归通道。

## 2. 目标与非目标

### 目标
1. 新增 `nightly.yml`：GitHub Actions `schedule`（cron 每夜）+
   `workflow_dispatch` 手动触发；
2. 单架构（amd64）跑完整链路：apt 装 `openfoam14` → 编译 →
   modelTests → `MOLDINGFOAM_PARALLEL=4 xmake run case-contract`；
3. 失败时自动开 issue（含 log.foamRun 尾部与验收输出）；
4. 产物（log、postProcessing 摘要、验收输出）作为 workflow artifact
   保留。

### 非目标
- arm64 夜间跑（arm runner 配额留给 PR/CD；如需可复制 job）；
- 契约 case 参数扫描（参数化研究是本地/CD 任务）。

## 3. 技术要点

1. **openfoam14 安装缓存**：runner 每次重装 deb（约 1–2 分钟解包 +
   下载 116MB）。可先用 `actions/cache` 缓存 `/opt/openfoam14`？——
   apt 装到系统路径，缓存 `/opt/openfoam14` 目录即可（key 含
   deb 版本号 20260724），命中则跳过 apt；
2. **失败开 issue**：`actions/github-script` 或 `gh issue create
   --title "nightly contract-case FAILED $(date)"`，需
   `permissions: issues: write`；重复失败避免刷屏：先
   `gh issue list --search` 查重；
3. **超时**：case 在 2 核超订下 20–40 分钟，job 超时 120 分钟；
4. **触发分离**：`schedule` 分支限定 `main`；cron 用 UTC 时间
   （GitHub Actions cron 均为 UTC，选北京时间白天低峰对冲，如
   `0 22 * * *` UTC = 北京 6:00）。

## 4. 工作拆解

1. `.github/workflows/nightly.yml`：schedule + dispatch；沿用 ci.yml
   的安装步骤；增加 cache 步骤；
2. 契约 case 步骤 + 验收输出保存（`verify-case.py` 的输出 tee 到
   `acceptance.txt`）；
3. 失败开 issue 步骤（含日志摘要）；
4. 成功/失败的 badge 或总结输出（可选）；
5. 手动触发验证一次全链路；
6. README 第 11 节补记。

## 5. 验收标准（DoD）

- 手动触发一次全链路绿；
- 人为制造失败（如临时改坏一个字典值）能开出 issue 并附日志；
- 次日定时触发自动执行。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| dl.openfoam.org 偶发不可达导致误报 | apt 步骤加 retry；缓存命中后不再依赖网络 |
| 每夜 runner 时长消耗 | 公共仓库免费额度充足；job 内先跑快速失败路径 |
| 潜热启用后 case 变长 | 超时 120 分钟；必要时并行数提升（runner 4 vCPU 对 4 子域已饱和） |
