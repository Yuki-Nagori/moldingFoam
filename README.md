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
| 黏性生热（`xmake run couette`） | 解析线性 Couette 剪切层（`γ̇ = 1000 1/s`、绝热、初始稳态剖面）：平均温升与独立积分模型（CrossWlf + Tait）对拍，实测相对误差 **4.1e-4**（阈值 2e-3）；速度剖面对拍线性 |
| CrossWlf | γ̇→0 时 η→η0(T)；高剪切 log-log 斜率→n−1；6 个手算参考点（含冻结区指数封顶）；`[ηmin,ηmax]` 夹紧 |

参考值取自 openInjMoldSim 附带的 HDPE 牌号数据，由独立脚本计算后固化。

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

### 注塑周期状态（moldingStage）与排气/闸口密封

`moldingStage`（regIOobject，注册于网格）除 V/P 阶段外，还承载两个
密封状态并随场持久化（重启续读）：

- `ventSealed`：排气口熔体前沿到达（`max(alpha.melt) ≥ ventSealAlpha`）
  后置位。`moldingVentVelocity` 置零速度、`moldingVentPressure` 转
  零通量，使排气口"只透气、不漏料"；
- `gateSealed`：V/P 切换后经过 `packing.gateSealTime`（或保压目标降到
  `cooling.releasePressure`）置位。`moldingInletVelocity` 置零、
  `moldingPrghPressure` 转零梯度，型腔成为封闭可压缩体，冷却期不再
  排料，平均熔体温度单调。

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
| `0/alpha.melt`、`0/U`、`0/p`、`0/p_rgh`、`0/T` | 场；熔体经 `inlet` 注入；`vent` 用 `moldingVentVelocity` + `moldingVentPressure`（只透气、遇熔体密封）；模壁默认 `fixedValue` 恒温，可选 `moldingMoldTemperature` 集总模温边界（均为本库自注册，见第 6 节） |
| `constant/phaseProperties` | `phases (melt air)` + 表面张力 |
| `constant/physicalProperties.melt` | 熔体相：`thermo hMelt`（潜热）、`equationOfState Tait` |
| `constant/physicalProperties.air` | 空气相（perfectGas） |
| `constant/momentumTransport` | laminar `generalisedNewtonian` + `CrossWlf` |
| `constant/moldingDict` | 工艺参数：`injection.meltTemperature`、`packing.switchFraction`、`packing.switchPressure`、`packing.gateSealTime`、`packing.pressure`（`table`）、`cooling.ejectionTemperature`、`cooling.releasePressure`、`ventSealAlpha`、`viscousDissipation` |

### 契约变更日志

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
├── tests/                   modelTests（可重复数值测试）
└── scripts/                 vm-sync.sh、run-case.sh、verify-case.py
                             run-couette.sh、verify-couette.py
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

### 夜间契约 case 回归（`nightly.yml`）

契约 case 在 PR 级 CI 上太慢（runner 上 20–40 分钟），由
**每夜定时任务**（UTC 22:00）在 amd64 runner 上运行完整验收，失败时
自动开/评论 issue 并上传日志 artifact；也可在 Actions 页手动触发。

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
