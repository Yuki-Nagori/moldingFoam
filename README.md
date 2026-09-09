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
架构的 Linux + libopenmpi3 运行库。许可与源码指引见包内
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

实测（8 核 ARM64 虚拟机）：串行误差 7.37e-4、并行 7.32e-4，全部通过；
全周期约 3950 步，串行约 12–15 分钟，4 子域并行约 5–8 分钟。

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
| CrossWlf | γ̇→0 时 η→η0(T)；高剪切 log-log 斜率→n−1；6 个手算参考点（含冻结区指数封顶）；`[ηmin,ηmax]` 夹紧 |

参考值取自 openInjMoldSim 附带的 HDPE 牌号数据，由独立脚本计算后固化。

---

## 6. 求解器模型

### moldingFoam（求解器模块）

`Foam::solvers::moldingFoam` 继承 `Foam::solvers::compressibleVoF`，以
`moldingFoam` 注册进 `foamRun` 求解器表。完整成型周期：

- **填充（M1）**：行为等价 `compressibleVoF`；流量控制注入；
- **保压（M2）**：求解器跟踪填充体积分数 `∫α.melt dV / V_腔`，达到
  `packing.switchFraction` 触发 V/P 切换；闸口切换为压力控制，跟随
  `pressure` 曲线（Function1 `table`，相对切换时刻计时）。实现为库内
  两个双模式边界条件 `moldingInletVelocity` 与 `moldingPrghPressure`，
  通过求解器注册在网格上的 `moldingStage` 对象读取阶段——不修改任何
  上游边界条件；
- **冷却（M3）**：模壁（walls patch 的 `T`）保持在模温；保压压力释放
  至大气压后，一旦平均熔体温度降至 `cooling.ejectionTemperature`
  以下，求解器打印并停止运行。

`system/controlDict` 用法契约：

```c++
application     foamRun;
solver          moldingFoam;
libs            ("libmoldingFoam.so");
```

`foamRun` 还会探测 `lib<Solver>Solver.so`，因此构建时会安装
`libmoldingFoamSolver.so -> libmoldingFoam.so` 符号链接，两条加载路径
均可工作。

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

潜热由 `hMelt` 热力学组合提供：在常 Cp 基础上，叠加由 Tait 混合权重对
温度的导数构造的表观 Cp 潜热峰（跨过渡带单位积分），能量方程穿过
`Tt(p)` 时吸放 `latentHeat`（`physicalProperties.melt` 的 `latentHeat`
关键字）。`CpMCv = -T·vT²/vP` 保持解析精确。

> ⚠️ **限制**：`compressibleVoF` 的能量预报器按 T 矩阵求解，压力相关的
> `Tt(p)` 使过渡带内 `Cv = Cp - CpMCv` 变负，非零 `latentHeat` 会令
> T 解发散（已实测，多种稳定化尝试均不足）。因此契约 case 保持
> `latentHeat 0`；`hMelt` 类、能量预报器的半隐式潜热线性化机制与其
> 测试均已就位，完整的 he 型能量预报器是进行中的研发项——方案、
> 实验记录与工作拆解见 `ai-docs/tasks/001-he-energy-predictor.md`。

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

- 全周期（填充+保压+冷却+顶出）约 3950 步 / 12–15 分钟（串行）；
- 保压压力会在闸口/排气口驱动射流，界面库朗数随之上升。契约 case 的
  `maxCo 0.5 / maxAlphaCo 0.1 / nSubCycles 6 / MULESCorr no` 是满足
  1e-3 质量守恒容差的实测组合；放宽控制会使该误差升至 0.5% 左右；
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
| `0/alpha.melt`、`0/U`、`0/p`、`0/p_rgh`、`0/T` | 场；熔体经 `inlet` 注入，`vent` 排气，模壁恒温 |
| `constant/phaseProperties` | `phases (melt air)` + 表面张力 |
| `constant/physicalProperties.melt` | 熔体相：`thermo hMelt`（潜热）、`equationOfState Tait` |
| `constant/physicalProperties.air` | 空气相（perfectGas） |
| `constant/momentumTransport` | laminar `generalisedNewtonian` + `CrossWlf` |
| `constant/moldingDict` | 工艺参数：`injection.meltTemperature`、`packing.switchFraction`、`packing.pressure`（`table`）、`cooling.ejectionTemperature`、`cooling.releasePressure` |

### 契约变更日志

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
│   ├── moldingFoam/boundaryConditions/  双模式浇口边界条件（M2）
│   ├── viscosityModels/CrossWlf/        Cross-WLF 黏度模型
│   ├── equationOfStates/Tait/           Tait 状态方程
│   ├── thermo/hMeltThermo.C             潜热热力学组合 hMelt
│   └── moldingFoamThermos.C             Tait 热物理组合注册
├── case-contract/           契约 case（见第 8 节）
├── tests/                   modelTests（可重复数值测试）
└── scripts/                 vm-sync.sh、run-case.sh、verify-case.py
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
`xmake` 编译 → `xmake run test` 模型测试。

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
