# Janus Editor Icons

40 枚为 Janus 绘制的矢量图标，参考用户提供截图的功能语义与深色界面配色。
24×24 坐标网格、2 单位描边、圆角端点；以 16–24 px 为主要 UI 展示尺寸。
文字标签始终保留，颜色是辅助提示，不作为唯一辨识依据。

## 素材与实现

- `svg/`：40 个独立 SVG，使用 `currentColor`，默认配色写在根元素 style 中。
- `preview.html`：16 / 24 / 40 px 图标总览，可用本机浏览器打开。
- `EditorIconCatalog.h` / `EditorIconGeometry.inl`：编译进 Editor 的同源矢量几何。
- `../EditorIcons.cpp`：ImGui 图标绘制及按钮、标签、组件标题辅助函数。
- 唯一几何定义与生成器：[generate_editor_icons.py](../../tools/generate_editor_icons.py)。

运行时无需 SVG 解析器、图标字体、外部纹理或额外资源复制；几何随 DPI/font size 缩放。
禁用状态沿用 ImGui 的透明度，点击行为和 ID 继续使用原有按钮及共享命令入口。

## 分类

| 分类 | 图标 |
| --- | --- |
| 常用操作 | Save / Undo / Redo / Add / Delete / Search / Settings |
| 运行 | Play / Pause / Step / Stop |
| 视图 | Select / Move / Rotate / Scale / Frame / Focus / Grid / List |
| 栏目及对象 | Hierarchy / Inspector / Entity / Folder / Camera / Canvas / Prefab |
| 组件及资源 | Transform / Texture / Script / Animation / Audio / Font / Collider / Physics |
| 诊断 | Console / Info / Warning / Error / Agent / Profiler |

Move / Rotate / Scale 等图标仅为素材储备；生成图标不表示对应编辑器操纵器已实现。
蓝色用于编辑，绿色用于运行/脚本，紫色用于动画/Agent，金色用于目录/音频，黄红表示警告/错误。

## 更新

在仓库根目录执行，只需 Python 标准库：

```powershell
python tools/generate_editor_icons.py
python tools/generate_editor_icons.py --check
```

修改生成器中的 `ICONS` 后重新生成；不要手动修改生成的 C++ / SVG，否则 `--check` 会报错。
生成物作为项目素材和编译输入保留在版本控制中；普通 CMake 构建不要求安装 Python。
