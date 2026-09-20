# MeasureExpert

基于 **Qt Widgets + HALCON** 的工业通用测量专家（工具箱 + 配方流水线）。

详细规格见 [docs/实施文档.md](docs/实施文档.md)。  
工具清单与 V1.5 范围见 [docs/工具地图.md](docs/工具地图.md)。  
第三方库放置说明见 [docs/第三方依赖.md](docs/第三方依赖.md)。

## 功能范围（V1.5）

- 配方驱动：文件夹采图 → 形状匹配定位 → 测量工具 → 公差判定
- 工具箱：找线 / 找圆 / 卡尺宽度·直径 / 点线距·交点·夹角·点距
- 编辑 / 运行模式；单次试跑与连续运行；运行模式写 CSV
- 相机 / 通信接口已预留空实现（V2+）

## 快速上手

1. 构建并运行 `../MeasureExpert_runtime/MeasureExpert.exe`（或 `MeasureExpertD.exe`）
2. **文件 → 新建工程** 并保存 `*.mexp.json`
3. **加载文件夹**（如 HALCON `examples/images/board/`）
4. **创建定位模板**：主图画 ROI → 右键确认 → 再点「确认训练模板」（角度建议 360°）
5. **添加测量项**（宽度/直径/找线/找圆/派生）→ ROI 类工具画测量 ROI；派生项在编辑里设引用 A/B
6. **单次试跑**看主图叠加与右侧 OK/NG
7. 切到 **运行模式** 后再试跑/连续运行，结果写入 `results/yyyy-MM-dd.csv`

### 编辑模式 vs 运行模式

| | 编辑 | 运行 |
|--|------|------|
| 改配方 / 训练模板 / 画卡尺 | 可以 | 锁定 |
| 单次 / 连续试跑 | 可以 | 可以 |
| 写 CSV | 否 | 是 |

### 连续运行

在已加载文件夹的前提下点 **连续运行**：按缩略图顺序循环测图，点 **停止** 结束（当前周期会跑完）。

CSV 路径：`MeasureExpert_runtime/results/yyyy-MM-dd.csv`  
列：时间戳、配方、图像、总判定、耗时、原因、测量项名/值/单位/单项判定/消息。

## 环境依赖

| 依赖 | 说明 |
|------|------|
| Qt 6.5+ | MSVC 64-bit Kit（与 HALCON 一致） |
| MSVC | Visual Studio C++ 工具链 |
| CMake | ≥ 3.19 |
| HALCON | 头文件/库在 `3rdParty/Halcon`；运行需本机授权版 DLL（`bin` 加入 PATH） |
| OpenCV | `find_package(OpenCV)`；优先 `3rdParty/OpenCV`（见 [docs/第三方依赖.md](docs/第三方依赖.md)） |

### 构建相关环境变量

```bat
set CMAKE_PREFIX_PATH=<你的 Qt>\6.x.x\msvc2022_64
```

运行程序时，将本机 HALCON 的 `bin\<arch>` 与 Qt 的 `bin` 加入 `PATH`。

可选：`VCVARS`（指向 `vcvars64.bat`）、`CMAKE`、`ninja`/`jom`。

## 仓库布局

```text
.
├── 3rdParty/          # 本地自备，已 gitignore（Halcon / OpenCV）
├── CMakeLists.txt
├── docs/
├── scripts/build_msvc.bat
└── src/
    ├── Core/          # 工程 JSON、CSV、预留 ImageSource/CommIO
    ├── Alg/           # 定位 / 测量 / 判定 / RunEngine
    └── UI/            # 主界面
```

源码旁的运行目录（不在仓库内）：

```text
../MeasureExpert_runtime/
  MeasureExpert.exe / MeasureExpertD.exe
  results/             # CSV
  log/                 # 按日日志
  .cmake/Debug|Release/
```

可用 `-DME_RUNTIME_DIR=<path>` 覆盖默认输出目录。

## Qt Creator

1. 打开仓库根目录的 `CMakeLists.txt`
2. 选择本机 **Desktop Qt … MSVC … 64bit** Kit
3. 构建目录（相对工程）：
   - Debug → `../MeasureExpert_runtime/.cmake/Debug`
   - Release → `../MeasureExpert_runtime/.cmake/Release`
4. 运行工作目录：`../MeasureExpert_runtime`
5. Configure → Build → Run

本机 `.qtcreator/` 已 gitignore，勿提交个人绝对路径。

## 命令行构建

```bat
scripts\build_msvc.bat
```

成功后在 `..\MeasureExpert_runtime\` 下可见 `MeasureExpertD.exe` 与 `MeasureExpert.exe`。

## 推荐演示例程图

| HALCON 例程 | 图目录 | 用途 |
|-------------|--------|------|
| `pm_measure_board` | `examples/images/board/` | 主演示：定位 + 双卡尺 |
| `align_measurements` | `razors*` | 第二套定位+卡尺对照 |

## 许可证与 HALCON

本仓库业务代码按项目 LICENSE 开源；**HALCON 为商业库**，使用者需自行具备合法授权，本项目不附带 HALCON 运行时。
