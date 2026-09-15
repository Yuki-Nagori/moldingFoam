# Dual Domain 联调 fixture

`dual-domain-v1.sample.json` 是公开 CI 使用的合成 10 mm 立方体外表面
（8 节点、12 三角形、逐面厚度 10 mm）。真实 Mug STL 导出在本地验证为
4,417 节点、8,834 三角形、厚度约 0.034–101.785 mm，但不提交仓库。
不包含 Mug 几何或专有材料。

从仓库根目录运行：

```bash
python3 scripts/test-dual-domain-input.py
python3 scripts/dual_domain_input.py tests/fixtures/dual-domain-v1.sample.json
```

读取器保留原始 mm 数值，只验证受控 JSON，不生成 OpenFOAM 字典、转换体网格
或运行 solver。摘要 `input_valid` 与 `solverReady: false` 同时输出；
失败返回非零，stderr 包含输入路径和字段/实体信息，可重定向保留日志。
sourceDataDigest 以 Python JSON 排序键、紧凑分隔符的序列化计算 SHA-256；
尚不是跨语言规范化网格摘要，整数和浮点表示可能产生不同摘要。

当前检查几何零面积、边流形性和相邻面的绕序一致性；未验证绝对内外法向、
双面配对、中面语义、完整边界、梁耦合距离的几何一致性或积分规则。
两面整体同时翻转不能靠相邻绕序检查辨别。
该 fixture 仍不用于薄壁解析解或 Mug 完整精度验收：它是 STL 外表面导出，
没有双面匹配、内外法向、边界和积分规则；参考规模 33,418/66,830 也未达到。
不能把 `area × thickness` 直接当成实体体积，除非中面语义先通过 073 门禁。
完整契约与后续测试见任务 073。

Mug STL、导出 JSON 和包含完整 Mug 几何的 case 仅保留本地，
不提交或上传普通 CI artifact。CI 只消费本合成 fixture。

## 完整实验 manifest（T102）

`dual-domain-v1-experiment-manifest.json` 是用户提供的非专有实验组装 fixture，
通过相对路径引用上述网格、PP-REF-01 材料标识及冻结 Mug 工艺数值。

```bash
python3 scripts/dual_domain_input.py --experiment tests/fixtures/dual-domain-v1-experiment-manifest.json
```

当前支持 manifest/网格版本、显式单位、相对路径回读、材料引用、阶段、
温度/时长和严格递增保压曲线校验，16 个测试纳入双架构 PR CI。
路径相对于 manifest 所在目录解析；禁止绝对路径及向父目录逃逸。
将来本地 Mug 实验可把 manifest 放在本地 JSON 同目录并使用其相对文件名。

manifest 的 C/MPa/s 原样保留，尚未进行求解字典 SI 换算。
`materialResolved: false` 明确表示没有根据 ID 自动猜测或注入材料系数。
Kairos 把 PP-REF-01 标为 Generic PP 拟合模板，不能等同参考软件的默认 PP。
后续还需核对材料键映射/摘要、表压或绝压、保压起点、两面模温绑定、
冷却阶段及顶出判据；本 fixture 通过不代表 fill-pack-cool solver 已通过。
