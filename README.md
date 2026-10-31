# 植物叶片病害识别系统 —— MobileNetV2 + ONNX 桌面应用

《计算机视觉》课程设计大作业 · 第七组完整代码仓库。

> **一句话**：PyTorch 训练 MobileNetV2 细粒度植物叶片病害分类模型 → 导出 ONNX → Qt6/C++ 桌面应用（ONNX Runtime 推理），附植物识别、AI 问答、识别历史等功能。测试集 Top-1 准确率 **99.6874%**，CPU 单图推理 **13.9 ms（71.8 FPS）**。

---

## 目录结构

```
plant-disease-mobilenetv2/
├── 核心代码.py                  # 训练/评估/Grad-CAM/导出/预测 一体化脚本
├── plant_disease_train/         # PyTorch 训练工程
│   ├── train.py                 # 训练主脚本（数据增强、余弦退火、早停）
│   ├── download_dataset.py      # Kaggle PlantVillage 数据集下载
│   ├── export_onnx.py           # ONNX 导出（opset 17, 动态 batch）
│   ├── 快速开始.bat             # 一键安装依赖 + 下载数据 + 训练
│   ├── models/                  # 训练输出目录（仓库含标签/类别映射/训练历史；权重需训练导出获得）
│   └── 完成报告.md              # 训练实验记录
└── zhiwushibie/                 # Qt6 C++ 桌面应用（识别 + 相机 + 问答 + 历史）
    ├── CMakeLists.txt
    ├── main.cpp / mainwindow.*  # 主窗口（病害识别、Top-5 置信度条）
    ├── PlantDisease.*           # ONNX Runtime 病害推理封装
    ├── PlantId.*                # 植物种类识别（quarrying 模型）
    ├── CameraWindow.*           # 摄像头实时识别
    ├── ChatWindow.* / DeepseekClient.*   # DeepSeek AI 问答
    ├── HistoryWindow.* / DatabaseManager.*  # 识别历史（SQLite）
    ├── InfoWindow.* / PlantComparisonWindow.* / SettingsWindow.*
    ├── resources/               # 图标、文案、标签映射（模型 *.onnx 不随仓库提供，见下）
    └── 启动植物识别.bat         # Windows 启动脚本（路径需按本机调整）
```

## 环境要求

| 组件 | 版本 | 说明 |
|------|------|------|
| Python | 3.10+ | 训练侧 |
| PyTorch | 2.x | CPU/GPU 均可（GPU 训练更快） |
| Qt | 6.10 (msvc2022_64) | 桌面应用 |
| CMake | 3.21+ | 构建 |
| Visual Studio | 2022 | MSVC 编译器 |
| OpenCV | 4.12 (vc16) | 图像/相机 |
| ONNX Runtime | 1.22.1 win-x64 | 推理，[官方下载](https://onnxruntime.ai/downloads) |

## 一键复现

### 1. 训练（PyTorch）

```bash
cd plant_disease_train
pip install torch torchvision numpy opencv-python pillow

# 下载 Kaggle PlantVillage 数据集（需 kaggle.json 或按脚本提示操作）
python download_dataset.py

# 训练（15 epoch，自动保存 best.pth）
python train.py

# 导出 ONNX
python export_onnx.py
```

或直接双击 `plant_disease_train/快速开始.bat`（Windows 一键：装依赖 → 下数据 → 训练）。

> **关于模型权重**：为控制仓库体积，`.pth` / `.onnx` 等二进制权重**不随仓库提供**——病害识别模型按上述步骤训练导出即可获得；`zhiwushibie/resources/assets/models/` 下已随附 `plant_disease_labels.txt`、`plant_disease_info.json`、`quarrying_plantid_label_map.txt` 等标签映射文件。

### 2. 构建桌面应用（Qt6 C++）

```bash
cd zhiwushibie
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建前请确认：

1. **ONNX Runtime**：下载 `onnxruntime-win-x64-1.22.1`，解压到与 `zhiwushibie/` 同级（或按 `CMakeLists.txt` 中路径调整），使 `lib/`、`include/` 可被找到；
2. **模型文件**：将 `plant_disease_train/export_onnx.py` 导出的 `plant_disease.onnx`（及同名 `.data` 外部权重，如有）放入 `zhiwushibie/resources/assets/models/`（该目录已随附标签映射与 `plant_disease_info.json`）。如需「植物识别」功能，另需第三方模型 `quarrying_plantid_model.onnx` 放入同目录——**该模型不随仓库分发，缺失时仅植物识别功能不可用，病害识别主功能不受影响**；
3. 运行入口：Windows 可用 `启动植物识别.bat`，其中 `PATH` 里 Qt / OpenCV 的路径写的是开发机路径，**请按本机 Qt 6.10 与 OpenCV 4.12 的安装位置修改后再运行**。

### 3. 配置（可选）：AI 问答

代码通过环境变量 `DEEPSEEK_API_KEY` 读取 DeepSeek 密钥（**切勿把真实密钥写进代码**）：

```powershell
# PowerShell
$env:DEEPSEEK_API_KEY = "sk-你的密钥"
```

不配置密钥时程序正常启动，仅 AI 问答功能不可用，识别功能不受影响。

## 数据集

- 来源：Kaggle [PlantVillage](https://www.kaggle.com/datasets/abdallahalidev/plantvillage-dataset)（abdallahalidev/plantvillage）
- 规模：38 类植物叶片（健康 + 病害）图像；本项目按 **8:1:1** 划分训练/验证/测试
- 预处理：Resize 224×224、归一化；训练增强（随机翻转/旋转/色彩抖动等，详见 `train.py`）

## 模型与指标

| 指标 | 数值 |
|------|------|
| 骨干 | MobileNetV2（ImageNet 预训练，最后全连接改为 38 类） |
| 测试集 Top-1 | **99.6874%**（10842/10876，34 例误检） |
| CPU 推理 | **13.9 ms / 71.8 FPS**（单图，ONNX Runtime） |
| 导出 | ONNX opset 17，动态 batch |

## 使用说明

1. 打开应用 →「病害识别」选图或拖入叶片照片 → 显示 Top-5 标签与**两位小数置信度**（如 `99.60%`，不做取整，避免 99.98%→100% 的失真显示）；
2. 「植物识别」由 `quarrying_plantid_model.onnx` 输出植物种类并附带植物知识卡片；
3. 「相机」实时识别摄像头画面；「历史」查看识别记录（SQLite）；「AI 问答」调用 DeepSeek 解答植物养护问题。

## 常见问题

- **启动报缺 DLL**：把 Qt 的 `bin`、OpenCV 的 `vc16\bin`、ONNX Runtime 的 `lib` 加入 `PATH`，或拷贝对应 DLL 到 exe 同目录；
- **模型加载失败**：确认 exe 同级 `resources/assets/models/` 模型文件完整（`.onnx` 约 9–31 MB）；
- **AI 问答无响应**：检查 `DEEPSEEK_API_KEY` 是否配置、网络是否可达。

## 作者

- 宋世恺（2310301322）
- 罗志康（2310301313）
- 杨振宇（2310301323）

**湖北工业大学** ·《计算机视觉》课程设计 · 指导教师：韦超
