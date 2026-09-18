# MeasureExpert

基于 **Qt Widgets + HALCON** 的工业通用测量专家（MVP）。

详细规格见 [docs/实施文档.md](docs/实施文档.md)。  
第三方库放置说明见 [docs/第三方依赖.md](docs/第三方依赖.md)。

## 功能范围（MVP）

- 配方驱动：采图 → 形状匹配定位 → 卡尺测量 → 公差判定 → CSV/日志
- 编辑 / 运行模式；单工位；文件夹图像源优先（相机接口预留）

## 环境依赖

| 依赖 | 说明 |
|------|------|
| Qt 6.5+ | MSVC 64-bit Kit（与 HALCON 一致） |
| MSVC | Visual Studio C++ 工具链 |
| CMake | ≥ 3.19 |
| HALCON | 头文件/库在 `3rdParty/Halcon`；运行需本机授权版 DLL（`bin` 加入 PATH） |
| OpenCV | 与烛照相同：`find_package(OpenCV)`；优先 `3rdParty/OpenCV`（见 [docs/第三方依赖.md](docs/第三方依赖.md)） |

### 构建相关环境变量

```bat
set CMAKE_PREFIX_PATH=<你的 Qt>\6.x.x\msvc2022_64
```

运行程序时，将本机 HALCON 的 `bin\<arch>` 与 Qt 的 `bin` 加入 `PATH`。

可选：`VCVARS`（指向 `vcvars64.bat`）、`CMAKE`、`ninja`/`jom`。

## 仓库布局

```text
.
├── 3rdParty/                       # 本地自备，已 gitignore（Halcon / OpenCV）
├── CMakeLists.txt
├── CMakeUserPresets.json.example
├── docs/实施文档.md
├── scripts/build_msvc.bat
├── src/
│   ├── main.cpp
│   ├── UI/          # 界面
│   └── Alg/         # 后期算法
└── 3rdParty/Halcon/
```

源码旁的运行目录（不在仓库内）：

```text
../MeasureExpert_runtime/
  MeasureExpert.exe      # Release
  MeasureExpertD.exe     # Debug
  .cmake/Debug|Release/  # CMake 中间文件
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

## 许可证与 HALCON

本仓库业务代码按项目 LICENSE 开源；**HALCON 为商业库**，使用者需自行具备合法授权，本项目不附带 HALCON 运行时。
