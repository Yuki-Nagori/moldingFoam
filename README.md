# moldingFoam

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

基于 [OpenFOAM](https://openfoam.org) 基金会 **OpenFOAM-14**（锁定 `version-14` 标签）
研发的注塑成型求解模块套件：

- **`moldingFoam`** — `foamRun` 求解器模块，覆盖热塑性制品的完整注塑成型周期：
  熔体/空气 VoF 型腔填充、V/P 切换保压（压力-时间曲线）、冷却后顶出判据；
- **`CrossWlf`** — Cross-WLF 广义牛顿黏度模型；
- **`Tait`** — 双域 Tait 聚合物 PVT 状态方程；
- **`case-contract/`** — 契约 case，定义外部 case 生成器对接的字典规范。

物理模型参考开源项目
[openInjMoldSim](https://github.com/krebeljk/openInjMoldSim)（OpenFOAM-7）的
材料数据与公式，并按 OpenFOAM-14 API 重新实现。**本项目遵守 GPL-3.0：所有
新增源码文件均带 GPL-3.0 头注；不修改任何 OpenFOAM 上游源码**（详见
[第 10 节 许可证与合规](#10-许可证与合规)）。

---

## 1. 环境要求

OpenFOAM-14 官方只支持 **Linux**，因此编译与验证必须发生在大小写敏感的
Linux 文件系统上。三端指的是三种可用的构建环境（任选其一）：

| 端 | 获取 Ubuntu 环境的方式 | 说明 |
|------|---------|------|
| **Linux** | 原生安装（裸机/云主机）；Docker 可选 | 最简单直接 |
| **macOS** | Multipass Ubuntu 虚拟机（本项目验证环境）；Docker Desktop 需 named volume | 宿主负责编辑，VM 负责构建 |
| **Windows** | WSL2 Ubuntu（推荐）；不支持原生 Windows/Cygwin | 仓库须放在 WSL 的 Linux 文件系统内 |

软件与硬件需求：

| 项目 | 要求 |
|------|------|
| xmake | ≥ 3.0（实测 3.1.x），构建编排工具 |
| OpenFOAM-14 | 官方 apt 二进制 `openfoam14`（`/opt/openfoam14`，见第 2 节）；非 Ubuntu 环境可用 `--of_src` 指向自建环境树 |
| 系统包 | g++、libopenmpi-dev、openmpi-bin、python3、curl、git |
| CPU/内存 | 建议 ≥ 8 核 / ≥ 16 GB |
| 磁盘 | ≥ 20 GB（openfoam14 deb 安装约 2 GB，Ubuntu 镜像约 1 GB） |

> ⚠️ **大小写敏感警告**：OpenFOAM 源码树中存在仅大小写不同的文件对
> （如 `primitives/chars/wchar/wchar.H` 与系统头 `<wchar.h>`；lnInclude
> 拍平目录里的 `Tensor.H` 与 `tensor.H`）。在**大小写不敏感**文件系统
> （macOS 默认 APFS、Windows NTFS）上解压/构建会互相覆盖并污染系统头文件，
> 上游明确不支持。macOS / Windows 用户必须按 2.2 / 2.3 节的说明，把仓库
> 放入大小写敏感的 Linux 文件系统后再构建（macOS 另见 `vm-sync.sh`）。

---

## 2. 三端安装指南（Windows / macOS / Linux）

无论哪一端，本质都是先获得一个**大小写敏感的 Ubuntu 24.04 环境**，差异只在
获取方式。完成安装后，日常使用完全一致，都是进入构建目录执行：

```console
$ xmake                          # 下载（如需）+ 编译 OpenFOAM + libmoldingFoam
$ xmake run test                 # 运行模型数值测试
$ xmake run case-contract        # 运行契约 case 并自动验收
```

### 2.1 Linux 端（原生 Ubuntu 24.04，含裸机/云主机）

最简单的一端。

```console
# 1. 安装系统依赖与 OpenFOAM-14 官方二进制（首选路径，约 2 分钟）
sudo apt update
sudo apt install -y g++ libopenmpi-dev openmpi-bin curl git python3 ca-certificates
curl -fsSL https://dl.openfoam.org/gpg.key | sudo tee /etc/apt/trusted.gpg.d/openfoam.asc >/dev/null
sudo add-apt-repository http://dl.openfoam.org/ubuntu
sudo apt update && sudo apt install -y --no-install-recommends openfoam14

# 2. 安装 xmake（官方脚本，装入 ~/.local/bin）
curl -fsSL https://xmake.io/shget.text | bash
source ~/.bashrc        # 或重新登录，让 PATH 生效

# 3. 获取仓库
git clone <你的仓库地址> moldingFoam
cd moldingFoam

# 4. 构建（默认使用 /opt/openfoam14，仅编译本项目，数分钟）
xmake
```

可选：非 Ubuntu 24.04 等无官方二进制的环境，可按
[openfoam.org 源码构建指南](https://openfoam.org/download/source/)自建
OpenFOAM-14 环境树，然后 `xmake f --of_src=<树根目录>` 使用。

可选：使用 Docker 容器做环境隔离，见 [2.4 节](#24-可选docker-容器化任一端通用)。

### 2.2 macOS 端（Multipass Ubuntu 虚拟机 —— 项目验证环境）

macOS 不在上游支持矩阵内，且默认 APFS 文件系统大小写不敏感，因此通过
Multipass 运行 Ubuntu 虚拟机：宿主机负责编辑代码，虚拟机负责编译与运行。

```console
# 1. 安装并启动 Multipass（安装需管理员密码，请在终端手动执行）
brew install --cask multipass
multipass launch --name of14 --cpus 8 --memory 16G --disk 80G 24.04
```

> **镜像下载失败的后备方案**：若 `launch` 卡在镜像下载（multipassd 的
> 下载器不继承用户网络配置），可手动下载镜像后用 `file://` 启动：
>
> ```console
> $ curl -fL -o ~/Downloads/noble-arm64.img \
>     https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-arm64.img
> $ multipass purge
> $ multipass launch --name of14 --cpus 8 --memory 16G --disk 80G \
>     file://$HOME/Downloads/noble-arm64.img
> ```

> **本地网络权限**：macOS 26+ 可能静默拦截宿主到 VM 的直连。若
> `multipass exec` 报 `No route to host` 而 `multipass list` 显示 VM 正常，
> 请到 *系统设置 → 隐私与安全性 → 本地网络*，为运行命令的 App 打开开关。

```console
# 2. 把仓库挂载进 VM（用于日常编辑同步）
multipass mount ~/eit/moldingFoam of14:/home/ubuntu/moldingFoam

# 3. 进入 VM，安装依赖与 xmake（仅首次）
multipass shell of14
sudo apt update
sudo apt install -y g++ libopenmpi-dev openmpi-bin curl git python3 ca-certificates
curl -fsSL https://dl.openfoam.org/gpg.key | sudo tee /etc/apt/trusted.gpg.d/openfoam.asc >/dev/null
sudo add-apt-repository http://dl.openfoam.org/ubuntu
sudo apt update && sudo apt install -y --no-install-recommends openfoam14
curl -fsSL https://xmake.io/shget.text | bash
exit
```

**构建必须在 VM 原生目录进行**（挂载目录大小写不敏感，见第 1 节警告）。
项目提供了一键同步脚本：

```console
# 在宿主机执行：把仓库镜像到 VM 原生的 ~/moldingFoam-build
multipass exec of14 -- bash ~/moldingFoam/scripts/vm-sync.sh

# 之后所有构建/运行都在 VM 内的 ~/moldingFoam-build 进行
multipass exec of14 -- bash -lc 'cd ~/moldingFoam-build && xmake'
multipass exec of14 -- bash -lc 'cd ~/moldingFoam-build && xmake run test'
multipass exec of14 -- bash -lc 'cd ~/moldingFoam-build && MOLDINGFOAM_PARALLEL=4 xmake run case-contract'
```

宿主机上修改代码后，重新执行 `scripts/vm-sync.sh` 即可增量同步（秒级；
OpenFOAM 环境在 VM 内部——apt 二进制在 `/opt/openfoam14`——不在挂载同步
范围，不受 macOS 文件系统污染）。

> 也可使用 Docker Desktop，但其 bind mount 同样大小写不敏感——必须把
> 源码拷入 named volume（大小写敏感）再构建，故更推荐 Multipass 方案。

### 2.3 Windows 端（WSL2 Ubuntu —— 原生 Windows 不受支持）

OpenFOAM-14 无法在原生 Windows/Cygwin 上编译，推荐用 WSL2 获得真正的
Ubuntu 环境（WSL 内的 `~` 是大小写敏感的 ext4，满足构建要求）。

```powershell
# 1. 管理员 PowerShell：安装 WSL2 与 Ubuntu 24.04（重启后生效）
wsl --install -d Ubuntu-24.04
```

```console
# 2. 进入 WSL，安装依赖与 xmake（同 Linux 端）
wsl -d Ubuntu-24.04
sudo apt update
sudo apt install -y g++ libopenmpi-dev openmpi-bin curl git python3 ca-certificates
curl -fsSL https://dl.openfoam.org/gpg.key | sudo tee /etc/apt/trusted.gpg.d/openfoam.asc >/dev/null
sudo add-apt-repository http://dl.openfoam.org/ubuntu
sudo apt update && sudo apt install -y --no-install-recommends openfoam14
curl -fsSL https://xmake.io/shget.text | bash
source ~/.bashrc

# 3. 仓库放在 WSL 的 Linux 文件系统内，然后构建
git clone <你的仓库地址> moldingFoam
cd moldingFoam
xmake
```

注意事项：

- 仓库**必须**放在 WSL 的 Linux 文件系统内（`~/moldingFoam`）；放在
  `/mnt/c/...`（Windows 盘）会因大小写不敏感损坏源码树，且 I/O 极慢；
- 仅支持 WSL2；WSL1 的系统调用与大小写语义不满足要求；
- MPI 并行在 WSL 内部进行（跨 Windows/WSL 的 MPI 不支持）；
- Docker Desktop on Windows 的 bind mount 同样大小写不敏感，构建请用
  [2.4 节](#24-可选docker-容器化任一端通用)的 named volume 方式，或直接
  在 WSL 内构建（推荐）。

### 2.4 可选：Docker 容器化（任一端通用）

适合需要环境隔离的场景。**关键点：构建必须在容器文件系统（overlayfs，
大小写敏感）里进行**，因此把仓库拷贝（而非挂载）进容器。

```console
# 1. 启动 Ubuntu 24.04 容器（把宿主机仓库目录挂到 /repo，仅作拷贝源）
docker run -it --name mf-build -v "$PWD":/repo ubuntu:24.04 bash

# 2. 容器内安装依赖与 OpenFOAM-14 官方二进制、xmake
apt update
apt install -y g++ libopenmpi-dev openmpi-bin curl git python3 ca-certificates
curl -fsSL https://dl.openfoam.org/gpg.key -o /etc/apt/trusted.gpg.d/openfoam.asc
echo "deb http://dl.openfoam.org/ubuntu noble main" > /etc/apt/sources.list.d/openfoam.list
apt update && apt install -y --no-install-recommends openfoam14
curl -fsSL https://xmake.io/shget.text | bash
. ~/.bashrc

# 3. 把仓库拷贝进容器文件系统（大小写敏感），然后构建
mkdir -p /work && cp -a /repo/. /work/
cd /work
xmake
```

后续进入同一容器继续使用（容器文件系统保留，缓存不丢）：

```console
docker start -ai mf-build          # 进入已有容器
. ~/.bashrc && cd /work            # 恢复环境
xmake run test                     # 缓存命中，秒级
```

---

## 3. 一键构建（三端通用）

在（Linux/macOS 端）的仓库目录或（Windows 端 WSL 内）的仓库目录、或（macOS 端）VM 内的 `~/moldingFoam-build` 目录执行：

```console
$ xmake
```

`xmake` 按以下优先级解析 OpenFOAM-14 环境，随后编译本项目：

1. **`--of_src=<path>`**：直接使用指定的环境树（根目录含 `etc/bashrc`，
   且已构建完成）；
2. **官方二进制（默认）**：使用 apt 安装的 `openfoam14`
   （`/opt/openfoam14`，官方 deb 自带 ThirdParty、全部模块库与
   lnInclude 头文件，见第 2 节安装；amd64 与 arm64 均有官方包）。

环境解析完成后（两种方式相同）：

- `wmake libso src` 生成 `$FOAM_USER_LIBBIN/libmoldingFoam.so`；
- `wmake tests` 生成模型测试 `modelTests`；
- 建立 `libmoldingFoamSolver.so -> libmoldingFoam.so` 符号链接（供
  `foamRun` 的 `lib<Solver>Solver.so` 探测路径使用）。

> 本项目编译始终使用上游 **wmake**；OpenFOAM 本体是上游官方发布的构建
> 产物，flags/ABI 一致性由上游打包保证。

### 缓存与幂等

`openfoam14` 由 apt 管理，`xmake` 每次只编译本项目：wmake 构建是增量式
的，改几个文件后重复执行 `xmake` 通常在数秒内结束，且不会触碰
`/opt/openfoam14`（只写入用户目录 `$FOAM_USER_LIBBIN`/`$FOAM_USER_APPBIN`）。

### 复用已构建的 OpenFOAM 树

```console
$ xmake f --of_src=/path/to/OpenFOAM-14      # 或 export MOLDINGFOAM_OF_SRC=...
$ xmake
```

`of_src` 完全绕过 apt 二进制（不下载、不校验），直接使用该树运行
wmake；它必须是**已经构建完成**的环境树（根目录含 `etc/bashrc`，且
`platforms/<选项>/lib/libOpenFOAM.so` 已存在）。指向已构建过的树是最快的
路径。

### 打包分发（bundle）

```console
$ xmake run bundle
```

产出 `build/moldingFoam-openfoam14-<WM_OPTIONS>-<日期>.tar.xz`（约
120 MB）：完整 OpenFOAM-14 官方环境树（约 700 MB，含 ThirdParty 与全部
模块）加上并入树内平台目录的 `libmoldingFoam.so` 与 `modelTests`。使用
者**无需安装 OpenFOAM**：

```console
$ tar -xJf moldingFoam-openfoam14-*.tar.xz
$ . openfoam14/etc/bashrc
$ modelTests        # 自检；case 中照常 solver moldingFoam + libs ("libmoldingFoam.so")
```

打包时会将 deb 硬编码的 `FOAM_INST_DIR=/opt` 恢复为上游按 bashrc 位置
自推导的逻辑，因此环境树可解压到任意路径使用。运行要求：与打包机同
架构的 Linux + libopenmpi3 运行库。`libmoldingFoam.so`/`modelTests`
以 **基线指令集** 编译（ARMv8-A / x86-64，构建时剥离
`-mcpu=native` 系 flags），任意同架构 CPU 均可运行；bundle 不含
`libmoldingFoamSolver.so` 别名（controlDict 经 `libs()` 显式加载，
避免重复加载告警）。许可与源码指引见包内
`MOLDINGFOAM-BUNDLE.md`（OpenFOAM 与 moldingFoam 均为 GPL-3.0，见
第 10 节）。

---

## 4. 运行契约 case

```console
$ xmake run case-contract                              # 串行 + 自动验收
$ MOLDINGFOAM_PARALLEL=4 xmake run case-contract       # 4 子域并行 + 验收
```

流程：`blockMesh` 建网格 → `foamRun` 求解（加载 `libmoldingFoam.so`，
求解器 `moldingFoam`）→ 自动执行 `scripts/verify-case.py` 验收。验收项：

- **模型生效证据**：日志中出现 `CrossWlf` 广义牛顿模型选择及其系数打印、
  熔体相 `Tait` 状态方程的 `thermoType` 选择回显；
- **阶段证据**：V/P 切换（含触发时的填充分数与时刻）、保压段闸口
  压力/目标压力对照打印、顶出判据满足并停止；
- **质量守恒**：型腔内聚合物质量增量与边界通量积分（浇口流入 + 排气口
  流出）的相对误差 **< 1e-3**（控制字典中的 `inletMassFlow`、
  `ventMassFlow`、`polymerMass` 函数对象）。

实测（8 核 ARM64 虚拟机，热流道保温 1.3 MPa、排气/浇口密封、黏性生热
生效）：4 子域并行质量守恒误差 **9.38e-04**（阈值 1e-3），全周期
7692 步约 6.5 分钟，全部通过。

日志与结果保留在 `case-contract/`（`log.foamRun`、`postProcessing/`、
各时间步目录）。

---

## 5. 模型测试

```console
$ xmake run test
```

运行 `tests/modelTests.C`（可重复执行，失败时以非零码退出）：

| 模型 | 检查项 |
|------|--------|
| Tait | `p=0` 时两个分支均满足 `v̂ = v0(T)`；解析 `psi` 与 `∂ρ/∂T` 与中心差分对拍（rtol 1e-6，覆盖熔体/固体/平滑过渡带）；HDPE 牌号 PVT 数据点复现 |
| hMelt | 潜热 Cp 峰在带宽边缘连续为零；峰中心值解析对拍（rtol 1e-12）；跨带积分恰为 `latentHeat`；`d(hs)/dT = Cp + latentCp` 与中心差分对拍（rtol 1e-8）；`latentHeat 0` 退化到常 Cp |
| moldThermalState | 后向 Euler 离散能量守恒恒等式对拍（rtol 1e-12，含功率源）；稳态 = 导热加权平均；超大时间步落在平衡点；`dt→0` 返回原温；无耦合不变 |
| ventOrifice | 零流恢复环境压力；背压与质量流量二次律（rtol 1e-12）；方向符号；手算点；`CdA=0` 无阻力 |
| 壁面滑移（`xmake run couetteSlip`） | Navier 滑移 Couette（滑移长度 0.2 mm）：速度剖面与 `u = U(y+b)/(h+b)` 对拍，实测 `max|u−u_ana|/U = 3.3e-9`（阈值 1e-4） |
| 凝固（`xmake run stefan`） | 一维 Stefan 问题（常物性，隔离传导+潜热）：凝固前沿与**两相 Neumann 解**全时段对拍，最大相对误差 **0.25%**（阈值 1%），温度剖面 ≤0.34 K（阈值 1 K）；400 cell/0.02 s 与 800 cell/0.01 s 两档一致，已达方法本征精度（1 K 潜热带平滑） |
| 黏性生热（`xmake run couette`） | 解析线性 Couette 剪切层（`γ̇ = 1000 1/s`、绝热、初始稳态剖面）：平均温升与独立积分模型（CrossWlf + Tait）对拍，实测相对误差 **4.1e-4**（阈值 2e-3）；速度剖面对拍线性 |
| CrossWlf | γ̇→0 时 η→η0(T)；高剪切 log-log 斜率→n−1；6 个手算参考点（含冻结区指数封顶）；`[ηmin,ηmax]` 夹紧 |

参考值取自 openInjMoldSim 附带的 HDPE 牌号数据，由独立脚本计算后固化。

### 快速求解器特性用例（`xmake run test-solver`）

`tests/cases/*` 是秒级的小 case，用于在完整契约回归（约 6.5 分钟）
之前快速验证求解器特性：每个 case 运行后按
`system/expectedPatterns` 中的正则逐条检查 `log.foamRun`。现有用例：

| 用例 | 检查 |
|------|------|
| `cycleReset` | 顶出后按 `nCycles` 重置流场并进入第 2 周期（并行/串行均可） |
| `moldCycles` | 模温跨周期保留、无跳变并逐周期升温（`verify-mold-cycles.py` 数值校验） |
| `moldSteady` | 400 周期模温收敛到周期稳态（单周期增量 0.583 → 9.24e-4 K，`verify-mold-steady.py`） |
| `gateFreeze` | 闸口温度型封冻判据：闸口 480 K、阈值 485 K → 首保压步封冻 |
| `coolantChannel` | 1D 冷却水通道：350 K 水把 300 K 模温推高（沿程推进 + 并行一致，`verify-coolant-channel.py`） |
| `runnerNetwork` | 入口流量由 1D 流道网络分流给出，入口质量流与 ρQ 一致（`verify-runner-network.py`） |
| `crystallization` | Nakamura/Avrami 结晶动力学：χ 单调有界增长到 0.99999，潜热耦合（`verify-crystallization.py`） |
| `crystallizationAdvection` | χ 随流输运：新鲜熔体（χ=0）驱替初始 χ=1，熔体加权均值 0.029（`verify-crystallization-advection.py`） |
| `fiberOrientationAdvection` | 取向张量随流输运：初始 a=xx 被 a=I/3 驱替，平均 a_xx=0.072，tr(a) 保持（`verify-fiber-orientation-advection.py`） |
| `fiberOrientation` | Folgar-Tucker 纤维取向：剪切下 max|a12|→0.147，tr(a) 保持 1（`verify-fiber-orientation.py`） |
| `shrinkage` | PVT 一致收缩指标：冷却致密使 max(S) 由 −0.008 增到 0.298（`verify-shrinkage.py`） |
| `weldLine` | 双端充填熔接痕：位于中心面 ±4 cell（实测 2.5 cell），截面填充时间对称 2.5%（`verify-weld-line.py`） |
| `processProfile` | 两段注射流量曲线 + 时间型 V/P 切换：入口质量流与 ρQ 一致（≤2.3%），切换 0.4005 s（`verify-process-profile.py`） |
| `multiGate` | 双浇口共用流道网络：分流比 32.30 vs 解析 32（0.93%）（`verify-multi-gate.py`） |
| `runnerTemperature` | 热流道温度：闸口熔体温度 499.9712 vs 解析 499.9713 K（`verify-runner-temperature.py`） |
| `fountainFlow` | 喷泉流：前沿位置偏差 0.013%，发展剖面 L2 1.64% vs 解析 Poiseuille（`verify-fountain-flow.py`） |
| `moldCHT-cycle` | 多周期多区域 CHT：模温 354→405 K 跨周期保留，界面连续（`verify-moldcht-cycle.py`，`xmake run moldCHT`） |
| 双区域 CHT（`xmake run moldCHT`） | 腔体 `moldingFoam` + 模具 `solid`（`foamMultiRun`）：导热基准 `moldCHT` 界面温度与一维两层参考对拍 0.19%、腔体平均 1.33%；充填基准 `moldCHT-fill` 能量守恒 0.24%、注入/充填体积偏差 0.43%、界面连续误差 0 |

用例可带 `system/verifyScript` 指定数值验证脚本（在
`system/expectedPatterns` 正则检查之后运行）。

新增特性时优先补一个 `tests/cases/<name>` 小 case，把契约 case 留给
集成级回归。

---

## 6. 求解器模型

### moldingFoam（求解器模块）

`Foam::solvers::moldingFoam` 继承 `Foam::solvers::compressibleVoF`，以
`moldingFoam` 注册进 `foamRun` 求解器表。完整成型周期：

- **填充（M1）**：流量控制注入；排气口只透气（`moldingVentVelocity` /
  `moldingVentPressure`），熔体前沿到达后密封（`ventSealAlpha`）；
- **保压（M2）**：填充体积分数达到 `packing.switchFraction`，或闸口压力
  达到 `packing.switchPressure`（保压设定压力）时触发 V/P 切换，闸口
  切换为压力控制、跟随 `pressure` 曲线（Function1 `table`，相对切换
  时刻计时）；切换压力与保压曲线起点一致，无压力阶跃。达到
  `packing.gateSealTime`（或压力降到 `cooling.releasePressure`）后
  **闸口封冻**，型腔停止排料。实现为库内四个边界条件
  （`moldingInletVelocity`、`moldingPrghPressure`、`moldingVentVelocity`、
  `moldingVentPressure`），通过求解器注册在网格上的 `moldingStage`
  对象读取阶段与密封状态——不修改任何上游边界条件；
- **冷却（M3）**：模壁温度由 `fixedValue` 恒温或集总模温边界
  `moldingMoldTemperature`（见下）给出；浇口封冻、压力释放后，一旦
  平均熔体温度降至 `cooling.ejectionTemperature` 以下，求解器打印并
  停止运行。

`system/controlDict` 用法契约：

```c++
application     foamRun;
solver          moldingFoam;
libs            ("libmoldingFoam.so");
```

`foamRun` 还会探测 `lib<Solver>Solver.so`，因此构建时会安装
`libmoldingFoamSolver.so -> libmoldingFoam.so` 符号链接，两条加载路径
均可工作。

### moldingMoldTemperature（集总模温边界，M3）

`Foam::moldingMoldTemperatureFvPatchScalarField`，用于 `0/T` 的模壁
patch，把模具表示为单一热容 `C`：

```
C dT/dt = Σ_f h_f (T_cell,f − T)   ← 铸件跨壁面导热
        + h_A (T_water − T)        ← 冷却水换热（h_A = waterHTC·wettedArea）
        + Q                        ← 可选功率源
```

其中 `h_f = kappaEff·deltaCoeffs·magSf` 与能量方程隐式边界的面导热
系数**完全同源**。边界按**后向 Euler** 更新（与能量方程的时间离散
一致）：

```
T^{n+1} = (C/dt·T^n + Σ h_f T_cell + h_A T_water + Q)/(C/dt + Σ h_f + h_A)
```

- 无条件稳定（`C/dt` 与 `h_A` 都在分母），`dt → ∞` 也不会越过平衡点；
- 每个外迭代重新求值、始终从 `T^n` 推进一步，既精化耦合又不会重复
  推进状态；离散能量守恒 `C(T^{n+1}−T^n) = dt·Q_net` 严格成立；
- 温度状态经 `UniformDimensionedField` 随场写出、重启续读（与上游
  `lumpedMassTemperature` 同模式），比求解器侧显式耦合精度高得多；
- 每个 patch 独立一个集总量；`fixedValue` 模壁（缺省）行为不变。

可选 `coolant` 子字典把 patch 建模为**一维活塞流冷却通道**：

```
coolant
{
    massFlowRate     1e-4;     // [kg/s]
    cp               4182;     // [J/kg/K]
    inletTemperature 350;      // [K]
    direction        (1 0 0);  // 通道轴向
    htc              5000;     // [W/m^2/K]，或
    Nu { C 0.023; m 0.8; n 0.4; Re 1e4; Pr 7; k 0.6; D 0.008; }
}
```

- patch 面按轴向位置分组为截面，组内视为充分混合：截面入口水温
  `Tc` 以离散活塞流推进
  `Tc_out = Tc_in + Σ_f htc·A_f·(T_wall,f − Tc_in)/(ṁ·cp)`，离散通道
  能量守恒精确成立；含 `Nu` 子字典时按幂律关联式
  `h = C·Re^m·Pr^n·k/D` 求 HTC；
- 截面分组只在首次求值时做一次**全局 gather + 排序**（几何与 HTC
  时不变），每个时间步只按截面数推进，开销可忽略；scotch 2 进程与
  串行的模温结果逐位一致；
- 验证：`xmake run test`（`moldingCoolantChannel`：手算 Nusselt/离散
  推进/能量守恒 rtol 1e-12、200 截面与解析指数解 rtol<1e-3）与
  `tests/cases/coolantChannel`（350 K 水加热 300 K 模温）。

用法示例：

```c++
walls
{
    type             moldingMoldTemperature;
    heatCapacity     500;      // [J/K]
    waterHTC         2000;     // [W/m^2/K]
    wettedArea       2.7e-3;   // [m^2]
    waterTemperature 300;      // [K]
    T                353;      // [K] 初始模温
    value            uniform 353;
}
```

### 多区域共轭传热（`foamMultiRun`，任务 008 路线 A）

求解器**无需新增代码**即可作为多区域 CHT 的流体区域求解器运行：
`foamMultiRun` 的 `regionSolvers` 把 `cavity` 映射到 `moldingFoam`、
模具区域映射到上游 `solid` 模块，两侧共享面用
`coupledTemperature`（`Tnbr T`）耦合。为支持该布局，求解器的
`constant/moldingDict` 与 `constant/momentumTransport` 读取路径改为
**区域感知**（经 `IOobject` 的 `dbDir`：多区域读
`constant/<region>/…`，单区域读 `constant/…`），`moldingStage` 因此
在流体区域注册，`moldingInletVelocity` 等边界正常工作。

```c++
regionSolvers
{
    cavity          moldingFoam;
    mold            solid;
}
```

验证（`xmake run moldCHT` 依次运行 `moldCHT` / `moldCHT-fill` /
`moldCHT-cycle`）：

- `validation/moldCHT`：静止熔体 + 钢模具的瞬态导热，界面温度与独立
  一维两层隐式 FD（Tait ρ(T)、Cv(T)）对拍 **0.19%**、腔体平均
  **1.33%**（时间步细化到 dt/2 不变，已时间收敛）；
- `validation/moldCHT-fill`：60×2 mm 通道被熔体充填（VoF）并同时向
  顶部模具传热；模具外壁全绝热使熔体+模具成为仅经浇口/排气口开放的
  封闭系统，全局能量平衡实测 **0.24%**（阈值 2%），注入体积与充填
  体积偏差 **0.43%**（阈值 1%），界面温度连续误差 0；
- `validation/moldCHT-cycle`：多周期集成（cavity 周期重置、模具温度
  保留）：2 个周期完成，模具平均温度 354.1 → 404.5 K，界面连续。

### 喷泉流基准（任务 020 第一阶段）

`tests/cases/fountainFlow`：40×4 mm 通道单浇口充填（绝热壁、远端
排气、常黏度），前沿位置与 `Q t/(h w)` 偏差 **0.013%**；测站
x=5.25 mm 的发展剖面与解析 Poiseuille 抛物线
`u = 1.5·u_mean·(1−(2y/h−1)²)` 的 L2 误差 **1.64%**（阈值 10%）；
细网格（152×16）对照为对称抛物线（L2 6.94%）。前沿形状与文献速度场
对拍为后续阶段。

### 热流道温度（`moldingRunnerTemperature`，任务 026 第二阶段）

`0/T` 的入口可选类型 `moldingRunnerTemperature`（自注册）：闸口熔体
温度取 1D 流道网络的段能量平衡结果
`Tout = Twall + (Tin−Twall)·exp(−htc·π·D·L/(ṁ·cp))`；填充期用网络的
分流流量，保压期用当前 patch 通量。集成用例
`tests/cases/runnerTemperature`（段壁温 500 K、htc 2000、D=4 mm、
L=50 mm）实测入口 499.9712 K vs 解析 499.9713 K（误差 0.0000%）。

### 多浇口分流（任务 026 第一阶段）

`tests/cases/multiGate`：双入口共用同一 1D 流道网络（`totalFlowRate`
相同、`gate` 不同、gate 直径 4/2 mm、幂律 n=0.5）；等压降分流实测
`Q1/Q2 = 32.30` vs 解析 `(D1/D2)^(3+1/n) = 32`（误差 0.93%），总流量
误差 3.4%（闸口密度差异）。热流道温度场耦合与阀浇口时序为后续阶段。

### 多级工艺曲线（`volumetricFlowRateProfile`/`switchTime`，任务 030）

`0/U` 的 `moldingInletVelocity` 新增可选 `volumetricFlowRateProfile`
（`Function1`，时间表曲线）：填充阶段流量 `Q(t)` 由曲线给出，缺省仍用
常数 `volumetricFlowRate`；`constant/moldingDict` 的 `packing` 新增可选
`switchTime`（时间型 V/P 切换准则，与填充分数/压力准则并列）。

- 集成用例 `tests/cases/processProfile`：两段流量（1e-7 → 3e-7 m³/s）
  入口质量流与 ρQ 一致（0.72%/2.33%），时间型切换在 0.4005 s 触发；
- 缺省不写时行为不变；阀浇口时序为后续扩展。

### 熔接痕与气穴（`writeFillTime`，任务 021）

`constant/moldingDict` 可选 `writeFillTime`（bool，缺省 false）：启用后
求解器记录每个单元填充（α≥0.5）的时刻并写出 `fillTime` 场；配合
`trapAirInterval` 同时写出困气单元场 `airTrap`（1 = 困气，复用 003 的
连通域诊断）。熔接痕判据：壁面单元在每个流向截面最后填充，故取**截面
最大填充时间**再沿流向找峰值，峰值截面即熔接痕。

- 集成用例 `tests/cases/weldLine`：40×4 mm 通道双端等流量充填、全顶面
  排气；熔接痕位于中心面 ±4 cell（实测 2.5 cell），截面最大填充时间
  对称误差 2.5%，困气 12/640 cell；
- 小排气口会因困气压缩使双前沿对称性自发破缺（熔接痕偏移 28.75%），
  全顶面排气恢复对称；
- 缺省不写时行为不变。

### 收缩/残余应力指标（`shrinkage`，任务 013a）

`constant/moldingDict` 可选 `shrinkage` 子字典：启用后求解器创建并
写出 `shrinkage` 场，按局部熔体密度给出 PVT 一致的自由体积收缩

```
S = 1 − ρ_ref/ρ
```

并可由 `σ = E/(1−ν)·α·(T_ref−T)` 给出约束板残余热应力指标。参数：
`referenceDensity`（固态参考密度）、`referenceTemperature`、
`elasticModulus`、`poissonRatio`、`thermalExpansion`。结构耦合翘曲
（013b）需结构求解器，超出本模块范围。

- 验证（`xmake run test`）：`S(ρ_ref)=0`、`S(1.05ρ_ref)` 解析值、
  热应力手算点；
- 集成用例 `tests/cases/shrinkage`：开放通道冷却，max(S) 由 −0.008
  增长到 0.298；缺省不写时行为不变。

### 纤维取向（`fiberOrientation`，任务 015）

`constant/moldingDict` 可选 `fiberOrientation` 子字典：启用后求解器
创建并写出二阶取向张量场 `a = <p p>`（缺省各向同性 `I/3`），按
Folgar-Tucker 方程在局部速度梯度下演化

```
Da/Dt = (W·a − a·W) + λ(D·a + a·D − 2A:D) + 2·CI·γ̇·(I − 3a)
```

其中 `λ = (r²−1)/(r²+1)` 为形状因子，`CI` 为纤维相互作用系数，闭合
可选 `quadratic`（`A=aa`，对单纤维精确）或 `hybrid`
（`f=1−27det(a)`）。每步 RK2 推进并做迹归一化，`tr(a)=1` 保持到机器
精度、特征值有界于 `[0,1]`。

- 参数：`aspectRatio`（或直接 `lambda`）、`interactionCoefficient`、
  可选的 `closure`；
- 验证（`xmake run test`）：形状因子、迹不变性、球体各向同性平衡、
  有界性、**Jeffery 轨道**（二次闭合与独立 RK4 的 Jeffery 角方程对拍
  <1e-6）；
- 集成用例 `tests/cases/fiberOrientation`：剪切 Couette 下
  `max|a12|` 增长到 0.147，`tr(a)−1 = 2.2e-16`；
- `a` 随流输运（隐式 upwind + 对称化/迹归一化）：
  `tests/cases/fiberOrientationAdvection` 中初始 `a=xx` 的型腔被
  `a=I/3` 的新鲜熔体驱替，平均 `a_xx=0.072`（阈值 0.5）；启用时需在
  case 的 `fvSchemes` 加 `div(phi,a)`、`fvSolution` 加 `(a|aFinal)`；
- 缺省不写时行为不变；各向异性黏度/热导率为后续扩展。

### 结晶动力学（`crystallization`，任务 014）

`constant/moldingDict` 可选 `crystallization` 子字典：启用后求解器创建
并写出相对结晶度场 `χ`（`0..1`），按 Nakamura 微分形式的 Avrami 方程
在局部温度/压力下演化

```
dχ/dt = n·K(T,p)·(1−χ)·(−ln(1−χ))^((n−1)/n)
K(T,p) = Kmax·exp(−4ln2·(T−Tmax(p))²/W²),  Tmax(p) = Tmax0 + dTdp·p
```

并以等效 Avrami 时间 `ξ = (−ln(1−χ))^(1/n)` 做**精确一步**
`χ_new = 1 − exp(−(ξ+K·dt)^n)`（任意步长无条件有界）。释放的潜热
`α·ρ_melt·L·dχ/dt` 显式加入能量方程，替代 hMelt 的固定潜热平台
（启用时应将 `physicalProperties.melt` 的 `latentHeat` 置 0）。

- 参数：`avramiExponent`、`rateConstant`、`peakTemperature`、
  `windowWidth`（半高全宽）、可选的 `peakTemperaturePressureShift`、
  `latentHeat`、`rho`；
- 验证（`xmake run test`）：窗口峰值/半宽、等温精确解、分段叠加、
  有界性、潜热源 rtol 1e-12/1e-10；
- 集成用例 `tests/cases/crystallization`：χ 单调有界增长到 0.99999，
  潜热使模温略高于无结晶工况；
- χ 随流输运（隐式 upwind + 限幅）：`tests/cases/crystallizationAdvection`
  中初始 χ=1 的型腔被 χ=0 的新鲜熔体驱替，t=1.5 s 熔体加权均值
  0.029（残量为壁面滞流熔体）；启用时需在 case 的 `fvSchemes` 加
  `div(phi,chi)`、`fvSolution` 加 `(chi|chiFinal)`；
- 缺省不写时行为与 001 完全一致；`η(χ)/ρ(χ)` 耦合为后续扩展。

### 1D 流道网络（`runner`，任务 016）

`Foam::moldingRunnerNetwork` 把流道系统表示为主流道（串联）+ 多浇口
（并联）的圆管网络：每段按广义 Hagen–Poiseuille 压降
`dp = 128·η·L·Q/(π·D⁴)`（η 由常数/幂律/CrossWlf 在壁面剪切率
`γ̇ = 32Q/(πD³)` 处求值），并联支路按等压降分流——定黏度退化为阻力比
解析解，同幂律指数时 `Qi/Qj = (Dj/Di)^(3+1/n)`；熔体温度沿段按一维
稳态能量平衡演化（可选 `wallTemperature`/`htc`，即热流道控温）。

- 模型级验证（`xmake run test`）：Hagen–Poiseuille 手算点、串联精确
  复现、并联分流比（定黏度 16、幂律 32）、壁耦合指数温度；
- 求解器耦合：`0/U` 的 `moldingInletVelocity` 可选
  `runner`/`gate`/`totalFlowRate`，入口流量取网络分流（多浇口按阻力
  分配）；`0/p_rgh` 的 `moldingPrghPressure` 可选 `runner`，保压期
  闸口压力 = 保压目标 − 当前流量下的流道压降；
- 集成用例 `tests/cases/runnerNetwork`：单浇口网络入口质量流与
  ρQ 一致（1.6%）；缺省不写 `runner` 时行为不变。

### 黏性生热（`viscousDissipation`，任务 007）

`constant/moldingDict` 可选 `viscousDissipation`（bool，缺省 `false`）。
开启后在能量方程中加入显式体积源

```
Φ = τ : ∇u = 2·η·dev(symm(∇u)) : ∇u
```

- `η` 由与动量方程**同一** CrossWlf 模型、同一应变率定义
  `γ̇ = √2·mag(symm(∇u))` 逐单元求值（走
  `CrossWlf::eta(coeffs, ...)` 重载，避免逐单元重读字典）；
- 应力取对称形式，与动量方程的 `∇u + (∇u)ᵀ − (2/3)tr(∇u)I` 一致。
  注意**不能**直接用 `dev2(T(∇u))` 与 `∇u` 做双点积：该表达式在简单
  剪切下恒为零（曾被解析 Couette 验证捕获）；
- 源恒正，显式加入能量方程；缺省关闭保证既有 case 回归。开启要求
  `simulationType laminar` + `model generalisedNewtonian` +
  `viscosityModel CrossWlf`，否则致命错误；
- 解析验证：`xmake run couette`（`validation/couette/`）——线性
  Couette、绝热、初始即稳态剖面，平均温升与独立积分对拍实测
  **4.1e-4**。

### 困气诊断（`trapAirInterval`，任务 003 选项 B）

`constant/moldingDict` 可选 `trapAirInterval`（时间步间隔，缺省 0 =
关闭）与 `trapAirAlpha`（熔体体积分数阈值，缺省 0.5）。每隔 N 步执行
只读诊断：

- 以 `alpha.melt <= trapAirAlpha` 标记空气单元；
- 从**开放排气口**（未密封的 `moldingVentVelocity` patch）的空气单元
  出发做连通域洪水填充，跨处理器界面交换连通标记直到全局收敛；
- 未与开放排气口连通的空气单元即困气，报告单元数、体积、空气质量、
  质量加权平均压力/温度、最高温度与体积加权质心；
- 排气口密封后，全部残余空气按困气计。

诊断不修改求解、每 N 步一次（BFS 只遍历空气单元），性能开销可忽略。

### 多周期运行（`nCycles`，任务 012）

`constant/moldingDict` 可选 `nCycles`（label，缺省 1）。设为 N > 1 时，
每次满足顶出判据即把流场（`alpha.melt`、`U`、`T`、`p`、`p_rgh` 及其
边界、面通量与动能）重置到初始状态并返回 filling 阶段（清除排气/闸口
密封），**保留模温状态**，自动进入下一周期；最后一个周期按原逻辑停机。
缺省 1 与单周期行为一致。

### 壁面滑移（`moldingSlipVelocity`，任务 011）

`U` 的壁面可选 `moldingSlipVelocity`（自注册，`partialSlip` 派生）：

```
u_w = b · ∂u_t/∂n,   valueFraction = 1/(1 + b·deltaCoeffs)
```

- `slipLength b` [m] 为直接物理输入，`b = 0` 退化为无滑移；
- 只作用于切向分量；验证 `xmake run couetteSlip`
  （`validation/couetteSlip/`）：滑移 Couette 剖面与
  `u = U(y+b)/(h+b)` 对拍 `max|u−u_ana|/U = 3.3e-9`。

### 排气反压（`moldingVentPressure.CdA`，任务 003 选项 A）

`0/p_rgh` 的 `moldingVentPressure` 可选 `CdA`（有效流通面积
`Cd·A_vent` [m²]，缺省 0 = 全开无背压）。受限排气按准稳态孔口关系

```
p_vent = p0 + sign(ṁ)·ṁ²/(2·ρ·CdA²)
```

给出边界压力，其中 `ṁ` 为当前 patch 质量通量（显式耦合，在 PIMPLE
校正中迭代）。受限排气时型腔背压随排出流量上升、熔体前沿到达后照常
密封；缺省不写 `CdA` 与既有 case 完全一致（回归）。可压缩/阻塞孔口
与困气连通域诊断（选项 B）见 `ai-docs/tasks/003`。

### 注塑周期状态（moldingStage）与排气/闸口密封

`moldingStage`（regIOobject，注册于网格）除 V/P 阶段外，还承载两个
密封状态并随场持久化（重启续读）：

- `ventSealed`：排气口熔体前沿到达（`max(alpha.melt) ≥ ventSealAlpha`）
  后置位。`moldingVentVelocity` 置零速度、`moldingVentPressure` 转
  零通量，使排气口"只透气、不漏料"；
- `gateSealed`：V/P 切换后经过 `packing.gateSealTime`（或保压目标降到
  `cooling.releasePressure`，或可选的 `gateFreezeTemperature` 温度
  判据）置位。`moldingInletVelocity` 置零、`moldingPrghPressure` 转
  零梯度，型腔成为封闭可压缩体，冷却期不再排料，平均熔体温度单调。

V/P 切换判据为填充分数 `switchFraction` 或闸口压力 `switchPressure`
先到者；保压曲线起点与 `switchPressure` 一致以避免压力阶跃。

### CrossWlf（黏度）

`Foam::laminarModels::generalisedNewtonianViscosityModels::CrossWlf`，
属于应变率相关的广义牛顿黏度模型家族（OpenFOAM-14 中 `CrossPowerLaw`/
`BirdCarreau` 所在家族），由本库自注册：

```c++
addToRunTimeSelectionTable(generalisedNewtonianViscosityModel, CrossWlf, dictionary);
```

在 `constant/momentumTransport` 中选择（`simulationType laminar;`
`model generalisedNewtonian;`）。公式：

```c++
η = η0 / (1 + (η0·γ̇/τ*)^(1-n)),   η0 = D1·exp(-A1·(T-T*)/(A2+(T-T*))),   T* = D2 + D3·p
```

`T`、`p`、`rho` 从网格注册表读取，缺场即致命错误。数值防护：应变率下限
`gammaDotMin`（默认 1e-6 1/s）、指数封顶（`η0 ≤ ηmax`，防止冻结区
T ≲ T* 时发散）、可配置 `[ηmin, ηmax]` 夹紧。

### Tait（状态方程）

`Foam::Tait<Specie>` 按 `perfectGas`/`rhoConst` 模板实现双域 Tait PVT：

```c++
v̂ = v0(T) · (1 - C·ln(1 + p/B(T))),   C = 0.0894（可配置）
v0(T) = b1m + b2m·(T-Tt)   (T > Tt，熔体；b2m 默认 0)
      = b1s + b2s·(T-Tt)   (T ≤ Tt，固体)
B(T) = b3·exp(-b4·T)       (熔体；可选 b3s/b4s 用于固体域)
Tt   = b5 + b6·p
```

`rho = 1/v̂`、`psi = ∂ρ/∂p|T` 与 `∂ρ/∂T`（通过 `alphav`，压力方程与
能量方程依赖）均为解析导数；在 `Tt` 附近 `smoothBand`（默认 ±0.5 K）
带宽内用三次 C¹ 平滑步混合熔体/固体两支，并计入权重导数项，保证混合
密度及其一阶导数在过渡带内连续，消除保压段收敛抖动。

潜热由 `hMelt` 热力学组合提供：显焓 `hs` 在常 Cp 基础上叠加潜热平台
`latentHeat·(w(T) - w(Tref))`（单位积分穿过 Tait 过渡带），能量方程
穿过 `Tt(p)` 时吸放 `latentHeat`（`physicalProperties.melt` 的
`latentHeat` 关键字，缺省 0）。

能量预报器（`moldingFoam::thermophysicalPredictor()` 覆写）的数值要点：

- **潜热峰只进能量方程、不进物性**：`hMeltThermo` 将表观容量拆分为
  `Cv = Cp + latentCp - CpMCv`（能量矩阵正定对角）与仅含显热的
  `Cp`（物性传输用）。`constTransport` 的导热率 `kappa = Cp·μ/Pr`
  若把 1 K 带宽内的潜热峰计入，κ 会瞬间放大两个数量级并摧毁温度场
  （已实测，见 `ai-docs/tasks/001`）；
- `Tait::CpMCv` 忽略 `b6` 前沿扫掠耦合（量级 0.15 K/MPa），使带内
  `Cv` 保持正定；
- 线性求解后对 `T` 施加 250–3000 K 安全钳制，为 `correctThermo` 的
  Newton 反演提供有限正初值。

契约 case 现以 `latentHeat 2e5` 运行；潜热使冷却曲线在凝固带内出现
平台，制品在顶出判据前完成潜热释放（见 `ai-docs/tasks/001`）。

状态方程是热物理包的编译期模板参数，`src/moldingFoamThermos.C` 在本库
内实例化 `pureMixture + const + hConst + Tait` 组合（`sensibleInternalEnergy`
与 `sensibleEnthalpy`）并注册进 `basicThermo`/`fluidThermo`/
`rhoFluidThermo` 运行时表。相属性写入
`constant/physicalProperties.<相名>`：

```c++
thermoType
{
    type            heRhoThermo;
    mixture         pureMixture;
    transport       const;
    thermo          hConst;
    equationOfState Tait;      // 熔体相；空气相用 perfectGas
    specie          specie;
    energy          sensibleInternalEnergy;
}
```

---

## 7. 性能与数值控制

- 全周期（填充+保压+冷却+顶出）约 7692 步、4 子域并行约 6.5 分钟
  （8 核 ARM64 VM，黏性生热开启）；
- 保压期的可压缩界面输运是质量守恒的主要误差源：契约 case 的
  `maxCo 0.5 / maxAlphaCo 0.03 / nSubCycles 16 / MULESCorr no` 是
  1.3 MPa 保压 + 黏性生热下满足 1e-3 容差的实测组合；放宽到
  `maxAlphaCo 0.1 / nSubCycles 6` 会使误差升至 1.5–2.6e-3；
- 更高的保压压力（如 40–100 MPa）物理上完全支持，但时间步会按库朗数
  成比例缩小，请相应评估时长预算；
- `CrossWlf::nu` 逐单元求值（内部场+边界场单一代码路径），与
  `tests/modelTests` 中的手算点完全同源。

---

## 8. 契约 case（对接规范）

`case-contract/` 的字典布局**就是外部 case 生成器的对接接口**。所有
字典名与关键字自 v1 起冻结：

| 文件 | 职责 |
|------|------|
| `system/controlDict` | `application foamRun`、`solver moldingFoam`、`libs ("libmoldingFoam.so")`、时间控制、验收函数对象 |
| `system/blockMeshDict` | 矩形板腔 + 底面浇口（`inlet`）+ 顶部排气（`vent`），2 mm 厚 |
| `system/fvSchemes`、`system/fvSolution`、`system/decomposeParDict` | 数值格式与分解 |
| `0/alpha.melt`、`0/U`、`0/p`、`0/p_rgh`、`0/T` | 场；熔体经 `inlet` 注入；`vent` 用 `moldingVentVelocity` + `moldingVentPressure`（只透气、遇熔体密封，可选 `CdA` 排气反压）；模壁默认 `fixedValue` 恒温，可选 `moldingMoldTemperature` 集总模温边界（均为本库自注册，见第 6 节） |
| `constant/phaseProperties` | `phases (melt air)` + 表面张力 |
| `constant/physicalProperties.melt` | 熔体相：`thermo hMelt`（潜热）、`equationOfState Tait` |
| `constant/physicalProperties.air` | 空气相（perfectGas） |
| `constant/momentumTransport` | laminar `generalisedNewtonian` + `CrossWlf` |
| `constant/moldingDict` | 工艺参数：`injection.meltTemperature`、`packing.switchFraction`、`packing.switchPressure`、`packing.gateSealTime`、`packing.pressure`（`table`）、`cooling.ejectionTemperature`、`cooling.releasePressure`、`ventSealAlpha`、`viscousDissipation`、`nCycles`、`trapAirInterval`、`trapAirAlpha` |

### 契约变更日志

**v1.20**（热流道温度边界，任务 026）：

- `0/T` 的入口新增可选类型 `moldingRunnerTemperature`：闸口熔体温度
  取流道网络段能量平衡结果。不写该类型的旧 case 行为与 v1.19 一致。

**v1.19**（结晶度黏度修正，任务 024）：

- `constant/momentumTransport` 的 `CrossWlfCoeffs` 新增可选
  `crystallinity { chiInfinity; exponent; }`：动量黏度按
  `(1−χ/χ∞)^(−a)` 放大。缺省不写时与 v1.18 一致。

**v1.18**（多级工艺曲线，任务 030）：

- `0/U` 的 `moldingInletVelocity` 新增可选 `volumetricFlowRateProfile`
  （Function1 时间曲线）；`packing` 新增可选 `switchTime`（时间型 V/P
  切换）。缺省不写时行为与 v1.17 一致。

**v1.17**（熔接痕/气穴，任务 021）：

- `constant/moldingDict` 新增可选 `writeFillTime`（bool，缺省 false）：
  写出单元填充时间场 `fillTime`；`trapAirInterval` 启用时同时写出困气
  场 `airTrap`。缺省不写时行为与 v1.16 一致。

**v1.16**（收缩/残余应力指标，任务 013a）：

- `constant/moldingDict` 新增可选 `shrinkage` 子字典：求解器创建并
  写出 PVT 一致的体积收缩场。缺省不写时行为与 v1.15 一致。

**v1.15**（纤维取向，任务 015）：

- `constant/moldingDict` 新增可选 `fiberOrientation` 子字典
  （Folgar-Tucker + 二次/Hybrid 闭合）：求解器创建并写出取向张量场
  `a`。缺省不写时行为与 v1.14 一致。

**v1.14**（结晶动力学，任务 014）：

- `constant/moldingDict` 新增可选 `crystallization` 子字典（Nakamura/
  Avrami 动力学 + 潜热释放）：求解器创建并写出 χ 场，潜热源加入能量
  方程。缺省不写时行为与 v1.13 一致。

**v1.13**（1D 流道网络，任务 016）：

- `0/U` 的 `moldingInletVelocity` 新增可选 `runner` 子字典与
  `gate`/`totalFlowRate`：入口流量由流道网络的等压降分流给出（多浇口
  按阻力分配）；缺省不写时仍用 `volumetricFlowRate`，行为与 v1.12
  一致；
- `0/p_rgh` 的 `moldingPrghPressure` 新增可选 `runner` 子字典：保压
  期闸口压力 = 保压目标 − 当前闸口流量下的流道压降。

**v1.12**（冷却水 1D 通道，任务 008）：

- `0/T` 的 `moldingMoldTemperature` 新增可选 `coolant` 子字典
  （`massFlowRate`/`cp`/`inletTemperature`/`direction` 及 `htc` 或
  `Nu` 幂律关联式）：把 patch 视为一维活塞流冷却通道，沿程推进水温并
  参与模体热平衡。缺省不写时行为与 v1.11 完全一致（均匀
  `waterHTC`/`wettedArea`/`waterTemperature` 路径）。

**v1.11**（闸口温度型封冻，任务 010）：

- `constant/moldingDict`：新增可选 `gateFreezeTemperature` [K]：保压
  阶段闸口单元质量加权平均温度降到该值即封冻（与 `gateSealTime`、
  releasePressure 兜底并存）。缺省不写行为与 v1.10 一致；契约 case
  的固定 480 K 入口不会触发该判据。

**v1.10**（模壁深层热阻，任务 008 第一阶段）：

- `0/T` 的 `moldingMoldTemperature` 新增可选 `wallResistance`
  [m²K/W] 与 `deepMoldTemperature` [K]：模壁经热阻与深层模体换热，
  在单区域框架内近似模具内温度梯度；缺省不写行为与 v1.9 一致。

**v1.9**（困气诊断，任务 003 选项 B）：

- `constant/moldingDict`：新增可选 `trapAirInterval`（label，缺省 0）
  与 `trapAirAlpha`（scalar，缺省 0.5）。开启后每 N 步输出困气区
  规模/压力/温度/位置；不写这两个键的旧 case 行为不变。

**v1.8**（多周期运行，任务 012 周期循环部分）：

- `constant/moldingDict`：新增可选 `nCycles`（label，缺省 1）。设为
  N > 1 时自动运行 N 个注塑周期，周期之间重置流场、保留模温状态；
  缺省 1 与单周期行为一致。

**v1.7**（壁面滑移，任务 011）：

- `0/U` 壁面新增可选类型 `moldingSlipVelocity`（自注册，`slipLength`
  [m]，缺省不写 = 无滑移）。不写该类型的旧 case 行为与 v1.6 一致。

**v1.6**（排气反压，任务 003 选项 A）：

- `0/p_rgh` 的 `moldingVentPressure` 新增可选 `CdA`（有效流通面积
  [m²]，缺省 0 = 全开）。受限排气按孔口关系给边界背压；不写该键的
  旧 case 行为与 v1.5 一致。

**v1.5**（黏性生热）：

- `constant/moldingDict`：新增可选 `viscousDissipation`（bool，缺省
  `false`；契约 case 开启）。开启后能量方程加入与动量方程同源的剪切
  耗散源（见第 6 节）；
- `system/controlDict` / `system/fvSolution`：黏性生热改变充填热历史
  后，为守住 1e-3 质量守恒，界面控制收紧为 `maxAlphaCo 0.03` /
  `nSubCycles 16`。

未改名、未删除任何关键字；不开启时行为与 v1.4 一致。

**v1.4**（注塑周期物理完备化：排气封堵 / 压力切换 / 闸口封冻）：

- `0/U` 的 `vent`：`pressureInletOutletVelocity` → `moldingVentVelocity`
  （自注册；未密封时按压力出/入流，熔体到达后置零速度）；
- `0/p_rgh` 的 `vent`：`prghTotalPressure` → `moldingVentPressure`
  （自注册；未密封时定压 `p0`，熔体到达后零通量）；
- `constant/moldingDict`：
  - `packing.switchPressure`：闸口压力达到该值即触发 V/P 切换（保压
    设定压力），避免熔体封死排气口后继续按流量充注造成过压；
  - `packing.gateSealTime`：V/P 切换后该时刻闸口封冻，型腔在压力释放
    后不再排料；
  - 顶层 `ventSealAlpha`：排气口熔体体积分数密封阈值（缺省 0.5）；
  - 保压曲线调整为 1.3 MPa 平台 0.15 s 后降至 1e5（起点与切换压力
    一致，无压力阶跃）；
- `system/fvSolution`：`nSubCycles 6 → 12`；`system/controlDict`：
  `maxAlphaCo 0.10 → 0.05`（高压保压下把可压缩界面输运的质量误差
  压回 1e-3 以内）。

未改名、未删除任何既有必需关键字；不写新键、vent 保持上游类型的
旧 case 行为与 v1.3 一致。

**v1.3**（潜热启用与集总模温边界）：

- `constant/physicalProperties.melt`：`latentHeat` 关键字正式生效
  （契约 case 取 HDPE 量级 `2e5`；v1.2 时期因数值限制实际按 0 使用）；
- `constant/moldingDict`：`cooling.ejectionTemperature` 由 393.15 K
  调整为 383.15 K（低于 Tait 凝固温度 `Tt = 390.65 K`），使顶出前
  制品确实释放潜热；
- `0/T` 模壁边界：新增可选类型 `moldingMoldTemperature`（本库
  自注册的集总模温边界，缺省仍为 `fixedValue` 恒温，向后兼容）：
  `heatCapacity` [J/K]、`waterHTC` [W/m²/K]（可选）、`wettedArea`
  [m²]（可选）、`waterTemperature` [K]（可选）、`Q` [W]（可选源）、
  `T` [K] 初始模温；
- `system/fvConstraints`：移除 `limitTemperature` 约束。熔体与空气
  共享单一 `T` 场，`phase melt` 绑定的是不存在的 `T.melt`，约束
  实际从不生效（上游会告警）；温度安全钳制改由求解器内部完成。

未改名、未删除任何既有必需关键字；模壁保持 `fixedValue` 的旧 case
行为与 v1.2 一致。

**v1.2**（潜热与顶出判据）：

- `constant/physicalProperties.melt`：`thermo hConst` 改为
  `thermo hMelt`（本库新注册的组合类，带表观 Cp 潜热峰），
  `thermodynamics` 新增可选 `latentHeat` 关键字（缺省 0，等价常 Cp）；
- `constant/moldingDict`：`cooling` 组新增可选 `releasePressure`
  （缺省 1e5），顶出检查在保压目标压力降至该值后才启动，替代原先
  硬编码的 1 bar。

未改名、未删除任何关键字；旧 case 不添加新键即保持原行为。

**v1.1**（M2/M3 交付）：

- `0/U` 浇口边界由 `flowRateInletVelocity` 更名为 `moldingInletVelocity`
  （`volumetricFlowRate` 关键字不变）；
- `0/p_rgh` 浇口边界由 `fixedFluxPressure` 更名为 `moldingPrghPressure`
  （新增标准 `mixed` 条目 `refValue`/`refGradient`/`valueFraction`）；
- `0/T` 壁面边界由 `zeroGradient` 改为 `fixedValue uniform 353`
  （模温自注入起生效）；
- `constant/moldingDict`：`packing.pressure` 数值调整为 0.35 s 保压窗口、
  `switchFraction` 定为 0.9（受控演示周期）；
- `system/controlDict`：`endTime` 3、`writeInterval` 0.1、`maxCo` 0.5、
  `maxAlphaCo` 0.1；`system/fvSolution`：`nSubCycles` 6、`MULESCorr no`
  （满足 1e-3 质量守恒容差的实测组合）；
- `system/blockMeshDict`：板厚增至 2 mm，使保压/冷却窗口可解析。

未改名、未删除任何关键字；下游生成器仅需按上述清单增补。

---

## 9. 仓库结构

```
moldingFoam/
├── xmake.lua                构建编排（OpenFOAM 环境解析/wmake 调用）
├── .github/workflows/       GitHub Actions（CI 自动验证 / CD 手动发版）
├── src/
│   ├── Make/{files,options} libmoldingFoam.so 的 wmake 工程
│   ├── moldingFoam/         foamRun 求解器模块 "moldingFoam" + moldingStage
│   │                        + moldThermalState（集总模温更新律，M3）
│   ├── moldingFoam/boundaryConditions/  双模式浇口边界（M2）
│   │                        + 相感知排气边界 moldingVent{Velocity,Pressure}
│   │                        + 集总模温边界 moldingMoldTemperature（M3）
│   ├── viscosityModels/CrossWlf/        Cross-WLF 黏度模型
│   ├── equationOfStates/Tait/           Tait 状态方程
│   ├── thermo/hMeltThermo.C             潜热热力学组合 hMelt
│   └── moldingFoamThermos.C             Tait 热物理组合注册
├── case-contract/           契约 case（见第 8 节）
├── validation/couette/      解析验证 case（黏性生热，`xmake run couette`）
├── validation/couetteSlip/  解析验证 case（壁面滑移，`xmake run couetteSlip`）
├── validation/stefan/       解析验证 case（凝固/潜热，`xmake run stefan`）
├── validation/moldCHT/      双区域共轭传热导热基准（`xmake run moldCHT`）
├── validation/moldCHT-fill/ 双区域共轭传热充填基准（同上目标）
├── tests/                   modelTests + cases/（快速求解器特性用例）
└── scripts/                 vm-sync.sh、run-case.sh、verify-case.py
                             run-validation.sh、run-solver-tests.sh
                             run-moldcht.sh、verify-couette.py
                             verify-stefan.py、verify-moldcht.py
                             verify-coolant-channel.py
                             verify-runner-network.py
                             verify-crystallization.py
                             verify-fiber-orientation.py
                             verify-shrinkage.py
                             verify-weld-line.py
                             verify-process-profile.py
                             verify-multi-gate.py
                             verify-runner-temperature.py
```

---

## 10. 许可证与合规

moldingFoam 以 **GNU GPL-3.0** 授权（见 [LICENSE](LICENSE)），所有源码文件
均带 GPL-3.0 头注。本项目基于 OpenFOAM（GPL-3.0）研发并选择 GPL-3.0
授权，正是该许可证设计的正确使用方式。合规要点：

1. 本仓库不包含、也不再分发任何 OpenFOAM 源码或二进制：构建时使用上游
   官方发布的 `openfoam14` 二进制包（或从上游官方地址获取**未修改**的
   version-14 源码自行编译），对上游的许可义务（保留声明、提供源码）由
   上游自身满足；
2. 本项目动态链接 OpenFOAM 库并继承其类，属衍生作品，因此**必须且已经**
   以 GPL-3.0 授权；再分发本仓库源码时，保留 LICENSE 与各文件头注即可；
3. 若将来分发包含 OpenFOAM 的二进制组合产物，需同时提供其完整对应源码
   （指向上游 version-14 源码即可满足），并注明本项目对上游零修改；
4. 红线：不得以非 GPL 兼容协议再授权；不得移除版权/许可声明；不得对
   上游源码打补丁后在非支持平台编译分发；
5. 商标："OpenFOAM" 是注册商标。请保持描述性使用（"基于 OpenFOAM-14 的
   求解模块"），不要暗示与 OpenFOAM 基金会或 ESI 有官方关联；本项目名
   moldingFoam 未包含 "OpenFOAM" 字样；
6. [openInjMoldSim](https://github.com/krebeljk/openInjMoldSim)（同为 GPL）
   仅作为公开材料系数与文献公式的参考，未复制其代码，不构成侵权，
   文中已致谢。

---

## 11. CI / CD（GitHub Actions）

配置在 `.github/workflows/`。两个流水线都在 **amd64 与 arm64 原生
runner**（`ubuntu-24.04` / `ubuntu-24.04-arm`，后者对公共仓库免费）上
并行运行。

### CI（自动触发）

`ci.yml` 在 **push 到 `main`** 和所有 **Pull Request** 上自动运行，
双架构各一遍：安装系统依赖与官方 `openfoam14` 二进制（apt）→
`xmake` 编译 → `xmake run test` 模型测试。求解器特性用例与解析验证
（每个都要重建并跑 case）耗时更长，统一放在夜间任务。

### 夜间契约 case 回归（`nightly.yml`）

求解器特性用例、解析验证与契约 case 在 PR 级 CI 上太慢（每个
`xmake run <case>` 都要重建库并跑 case），由**每夜定时任务**
（UTC 22:00）在 amd64 runner 上运行：`test-solver`、`couette`、
`couetteSlip`、`stefan`、`moldCHT`（双区域共轭传热）与完整契约
验收，失败时自动开/评论 issue 并上传日志 artifact；也可在 Actions
页手动触发。

### CD（手动触发，写入 tag）

`cd.yml` 由 **Actions 页 → CD → Run workflow** 手动触发，触发时填写：

| 输入 | 说明 |
|------|------|
| `tag` | 必填，形如 `v0.1.0`（`v` 开头的版本号，不得与已有 Release 重复） |
| `prerelease` | 可选，勾选则标记为 pre-release |

流程：tag 合法性校验 → 双架构并行编译并运行模型测试、各自
`xmake run bundle` → **在当前 commit 上写入该 tag**，创建一个 GitHub
Release 同时挂上 amd64 与 arm64 两个自包含包（另存为 workflow artifact
保留 14 天）。

> 注意：原生 arm64 runner 仅对**公共仓库**免费；私有仓库需付费 larger
> runner，此时 arm64 腿会一直排队，可在本地 arm 机器（如 Multipass
> VM）用 `xmake run bundle` 产出后 `gh release upload <tag> <文件>` 补挂。
> 若需 CD 写 tag / 发 Release，在仓库 Settings → Actions → General →
> Workflow permissions 中允许 "Read and write permissions"。
