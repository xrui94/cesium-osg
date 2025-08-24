# cesium-osg

将 Cesium 特有的 3D 地理空间技术，尤其是3DTiles 带到 OSG（OpenSceneGraph），有点类似 [cesium-unreal](https://github.com/CesiumGS/cesium-unreal) 和 [cesium-unity](https://github.com/CesiumGS/cesium-unreal)两个项目。

**注**：cesium-unreal 和 cesium-unity分别是 cesium for ue 和 cesium for unity两个插件的官方源码

## 🚀 特性

- 目前支持：
  - HTTP协议的 3D Tiles 1.0 / 1.1模型加载和渲染
  - 异步加载与层级细节（LOD）
  - 与 OpenSceneGraph 渲染管线无缝集成

- TODO:
  - 支持本地（包括 file:/// 协议） 3DTiles 1.0 / 1.1 模型加载和渲染
  - 支持 Cesium quantized-mesh 地形数据的加载和渲染
  - 基于 WGS84 的精确地理投影
  - 支持 WMTS 影像服务数据的加载和渲染

---

### 1. 前提条件

- [CMake](https://cmake.org/) >= 3.20
- [Visual Studio 2022](https://visualstudio.microsoft.com/) 或更高版本
- [vcpkg](https://vcpkg.io/)（推荐用于依赖管理）
- 已安装 OpenSceneGraph 开发库（可通过 vcpkg 安装：`vcpkg install openscenegraph`）

### 1.1 克隆项目（含子模块）

```powershell
git clone --recurse-submodules https://github.com/xrui94/cesium-osg.git
```

## 1. 前提条件

开发环境需要如下依赖，须要事先准备好（目前仅支持 Windows）：

- [CMake](https://cmake.org/) >= 3.20
- [Visual Studio 2022](https://visualstudio.microsoft.com/) 或更高版本
- [OSG-version3.6.5]
- [libcurl]

须将 OSG 和 libcurl 两个编译好的库放大1个目录下，然后，在CMake生成C++项目时，指定改路径

## 2. 入门

### 2.1 克隆镜像

使用如下命令，克隆 cesium-osg 项目及其子项目模块

```powershell
git clone --recurse-submodules https://github.com/xrui94/cesium-osg.git
```

如果忘记了“--recurse-submodules”参数，以克隆子项目，cesium-osg 项目，可以在项目的根目录下重新执行如下命令：

```powershell
git submodule update --init --recursive
```

### 🛠 2.2 编译和构建

```ps
mkdir build
cd  build
cmake .. -G"Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="./build/install" -DTHIRD_PARTY_DIR=C:/env/libc++
cmake --build . --config Debug
```

## 致谢

cesium-osg项目收到了如下三个项目的启发：
- [osgEarth](https://github.com/gwaldron/osgearth): 3D Maps & Terrain SDK / C++17
- [godot-3dtiles](https://github.com/wxzen/godot-3dtiles): Godot Cesium 3D Tiles plugin
- [cesium-unreal](https://github.com/CesiumGS/cesium-unreal): Bringing the 3D geospatial ecosystem to Unreal Engine