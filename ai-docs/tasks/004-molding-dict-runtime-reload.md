# 004 — moldingDict 运行时重载

- 状态：done（2026-09-10。验收：运行中修改
  `cooling.ejectionTemperature`，下一时间步日志输出参数更新，
  契约 case 全项验收通过，质量守恒 7.400e-04）
- 优先级：P1（小改动，交互体验）
- 依赖：无
- 预估规模：半天

## 1. 背景与现状

契约 case 的 `controlDict` 设置了 `runTimeModifiable yes`，运行中
修改字典会触发 `read()`。但：

- `moldingFoam::read()` 仅转发 `compressibleVoF::read()`；
- `constant/moldingDict` 只在构造函数中读取一次：
  `switchFraction`、`pressure`（Function1 曲线）、
  `ejectionTemperature`、`releasePressure` 运行中修改被静默忽略；
- `moldingStage` 的 `switchFraction`/`pressure` 已通过网格注册的
  regIOobject 与 BC 解耦，但对象本身不响应重读。

评审（P2）指出：求解器侧缺少对该限制的声明与实现。

## 2. 目标与非目标

### 目标
1. `moldingFoam::read()` 检测 `constant/moldingDict` 变化并热更新：
   `switchFraction`、`pressure` 曲线、`ejectionTemperature`、
   `releasePressure`；
2. 更新时打印新旧值对照（可追溯）；
3. `moldingStage` 提供参数更新接口（不重建对象，保留阶段状态与
   switchTime）。

### 非目标
- `T` 边界条件、注入流量等 case 层参数的运行时修改（由 OpenFOAM
  自身的 `runTimeModifiable` 对场文件/BC 字典生效，非本任务）；
- 修改 `switchFraction` 的回退语义（只对新时刻生效）。

## 3. 技术方案

1. `moldingStage` 新增 `read(const dictionary&)`（或
   `updateFrom(const dictionary&)`）：
   - 重读 `switchFraction` 与 `pressure`（`Function1::New` 替换
     `pressure_`）；
   - 保持 `stage_`/`switchTime_` 不变（已进入保压则不回退）；
2. `moldingFoam::read()` 覆写逻辑：
   ```cpp
   if (compressibleVoF::read() && mesh.foundObject<moldingStage>(...))
   {
       IFstream is(moldingDictPath);
       dictionary moldingDict(is);
       // 新值与现值 diff，打印 "moldingFoam: moldingDict updated:
       // key old -> new"，调用 stage 更新并刷新
       // ejectionTemperature_/releasePressure_
   }
   ```
3. 持久化一致性：`moldingStage` 的 `writeData` 已写
   `packingFlag/switchTime`，参数不持久化（重启后从当前 moldingDict
   读取）——行为可接受，文档注明。

## 4. 工作拆解

1. moldingStage.H/.C：新增 `read(const dictionary&)` 与打印；
2. moldingFoam.C：`read()` 中加载并 diff 字典（对照
   `ejectionTemperature_`/`releasePressure_`/stage 的
   `switchFraction()`/`pressure()` 现值），变化才更新；
3. modelTests：`moldingStage` 的参数更新单测（改字典 → switchFraction
   /pressure 曲线更新、阶段状态保留）——需要把 moldingStage 从
   fvMesh 依赖中解出（当前 ctor 需要 mesh+runTime；测试可用最小
   构造或加一个测试用轻量构造路径，若成本过高则降级为契约 case
   手工验证并记录）；
4. README：第 6 节解除"运行中修改不生效"的说明，变更日志补记。

## 5. 验收标准（DoD）

- 运行中修改 `ejectionTemperature` 后下一次判定即用新值（日志可见
  新旧对照）；
- 修改 `pressure` 曲线后保压目标立即跟随；
- 修改 `switchFraction` 仅对未来切换生效（已切换则无回退）；
- `xmake run test` / 契约 case 回归绿。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| moldingStage 测试需要 mesh | modelTests 已链接 finiteVolume；可用 `createMesh` 式最小 Time+fvMesh 构造（OpenFOAM 测试惯例），或仅做契约 case 手工验证 |
| 半途修改 switchFraction 造成逻辑歧义 | 明确语义：只影响"尚未切换"的判定；文档写明 |
