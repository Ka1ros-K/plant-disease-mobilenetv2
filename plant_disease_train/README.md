# 植物病害检测模型训练指南

## 目录结构

```
plant_disease_train/
├── dataset/              # 数据集目录
│   ├── raw/              # 原始数据（下载后放在这里）
│   ├── train/            # 训练集（自动生成）
│   └── val/              # 验证集（自动生成）
├── models/               # 训练好的模型
│   ├── best.pth          # 最佳模型权重
│   ├── plant_disease.onnx    # ONNX 格式模型
│   └── plant_disease_labels.txt # 标签文件
├── train.py              # 训练脚本
├── download_dataset.py   # 数据集下载脚本
├── export_onnx.py        # ONNX 导出脚本
└── README.md             # 说明文档
```

## 步骤 1：下载数据集

运行下载脚本：
```bash
cd D:\installed\Plants_Camera\plant_disease_train
python download_dataset.py
```

脚本会提示你手动下载数据集。推荐从以下来源下载：

### 方式 1：Kaggle（推荐）
- 网址：https://www.kaggle.com/datasets/abdallahalidev/plantvillage
- 下载后解压到：`dataset/raw/` 目录

### 方式 2：GitHub
- 网址：https://github.com/spMohanty/PlantVillage-Dataset
- 下载 ZIP，解压图片到 `dataset/raw/`

### 方式 3：Mendeley Data
- 网址：https://data.mendeley.com/datasets/tywbtsjrjz/1
- 完整的数据集，包含标注信息

### 数据集结构要求
```
dataset/raw/
├── Apple___Apple_scab/
│   ├── image001.jpg
│   └── ...
├── Apple___Black_rot/
│   ├── image001.jpg
│   └── ...
├── Corn___Cercospora_leaf_spot/
│   └── ...
...
```

**注意**：文件夹名称格式为 `植物___病害名`

## 步骤 2：训练模型

```bash
python train.py
```

### 训练参数（可在 train.py 中修改）
- `EPOCHS = 15` - 训练轮数
- `BATCH_SIZE = 32` - 批次大小
- `LEARNING_RATE = 0.001` - 学习率

### 预计训练时间
| 设备 | 时间 |
|------|------|
| CPU (i7) | 约 2-3 小时 |
| GPU (GTX 1060) | 约 30-45 分钟 |
| GPU (RTX 3060) | 约 15-25 分钟 |

### 训练输出
- `models/best.pth` - 最佳模型权重
- `models/class_names.json` - 类别名称映射
- `models/training_history.json` - 训练历史记录

## 步骤 3：导出 ONNX 模型

```bash
python export_onnx.py
```

生成文件：
- `plant_disease.onnx` - ONNX 格式模型
- `plant_disease_labels.txt` - 标签文件

## 步骤 4：集成到 Qt 项目

将生成的两个文件复制到：
```
D:\installed\Plants_Camera\zhiwushibie\build\unknown_ed1ab7-Debug\assets\models\
```

## 常见问题

### Q: 训练速度太慢
A: 可以尝试：
1. 减少 `EPOCHS` 到 10
2. 减少 `BATCH_SIZE` 到 16
3. 使用 GPU（如有）

### Q: 内存不足
A: 减少 `BATCH_SIZE` 到 16 或 8

### Q: 准确率太低
A:
1. 增加训练轮数到 20-30
2. 确保数据集质量良好
3. 检查图片是否损坏

### Q: ONNX 导出失败
A: 确保 `models/best.pth` 文件存在，且训练成功完成

## 植物病害类别示例

PlantVillage 数据集包含以下类别（部分）：

| 植物 | 病害 |
|------|------|
| Apple | Apple scab, Black rot, Cedar apple rust |
| Blueberry | Healthy |
| Cherry | Powdery mildew, Healthy |
| Corn | Cercospora leaf spot, Gray leaf spot |
| Grape | Black rot, Esca, Leaf blight |
| Orange | Haunglongbing (Citrus greening) |
| Peach | Bacterial spot |
| Pepper | Bacterial spot |
| Potato | Early blight, Late blight |
| Soybean | Healthy |
| Squash | Powdery mildew |
| Strawberry | Leaf scorch, Powdery mildew |
| Tomato | Early blight, Late blight, Septoria leaf spot, etc. |

## 下一步：在 Qt 项目中添加病害检测功能

创建以下文件到你的 Qt 项目：
1. `PlantDisease.h` - 病害检测类头文件
2. `PlantDisease.cpp` - 病害检测类实现

（可以参考现有的 `PlantId.h` 和 `PlantId.cpp`）
