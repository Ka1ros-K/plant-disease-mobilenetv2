"""
将训练好的 PyTorch 模型导出为 ONNX 格式
用于在 Qt/C++ 项目中部署
"""

import os
os.environ['PYTHONIOENCODING'] = 'utf-8'

import torch
import torch.onnx as onnx
import torch.nn as nn
from torchvision import models
import json
import os

# ================== 配置 ==================
MODEL_PATH = r"D:\installed\Plants_Camera\plant_disease_train\models\best.pth"
CLASS_INFO_PATH = r"D:\installed\Plants_Camera\plant_disease_train\models\class_names.json"
OUTPUT_ONNX = r"D:\installed\Plants_Camera\plant_disease_train\models\plant_disease.onnx"
OUTPUT_LABELS = r"D:\installed\Plants_Camera\plant_disease_train\models\plant_disease_labels.txt"

# ================== 定义模型结构 ==================
class PlantDiseaseModel(nn.Module):
    def __init__(self, num_classes):
        super().__init__()
        # 使用 MobileNetV2 作为基础
        base = models.mobilenet_v2(weights=None)
        self.features = base.features
        self.classifier = nn.Sequential(
            nn.Dropout(0.2),
            nn.Linear(base.classifier[1].in_features, num_classes)
        )

    def forward(self, x):
        x = self.features(x)
        x = torch.nn.functional.adaptive_avg_pool2d(x, (1, 1))
        x = torch.flatten(x, 1)
        x = self.classifier(x)
        return x

def export_to_onnx():
    """导出模型为 ONNX 格式"""
    print("[INFO] 加载类别信息...")

    # 加载类别信息
    with open(CLASS_INFO_PATH, "r", encoding="utf-8") as f:
        class_info = json.load(f)

    num_classes = class_info["num_classes"]
    input_size = class_info["input_size"]
    class_names = class_info["classes"]

    print(f"  类别数：{num_classes}")
    print(f"  输入尺寸：{input_size}x{input_size}")

    print("\n[INFO] 创建模型并加载权重...")

    # 创建模型
    model = PlantDiseaseModel(num_classes)

    # 加载训练好的权重
    if not os.path.exists(MODEL_PATH):
        print(f"[ERROR] 模型文件不存在：{MODEL_PATH}")
        print("请先运行 train.py 训练模型")
        return False

    model.load_state_dict(torch.load(MODEL_PATH, map_location="cpu", weights_only=True))
    model.eval()

    print("  权重加载成功")

    # 创建示例输入
    dummy_input = torch.randn(1, 3, input_size, input_size)

    print(f"\n[INFO] 导出 ONNX 模型...")
    print(f"  输出路径：{OUTPUT_ONNX}")

    # 导出为 ONNX (使用 export_params=True 和 do_constant_folding=True)
    torch.onnx.export(
        model,
        dummy_input,
        OUTPUT_ONNX,
        export_params=True,
        opset_version=11,
        do_constant_folding=True,
        input_names=["input"],
        output_names=["output"],
        dynamic_axes={
            "input": {0: "batch_size"},
            "output": {0: "batch_size"}
        },
        dynamo=False
    )

    print("  ONNX 模型导出成功")

    # 保存标签文件
    print(f"\n[INFO] 保存标签文件...")
    print(f"  输出路径：{OUTPUT_LABELS}")

    labels_data = {"species_taxons": {}}
    for i, name in enumerate(class_names):
        labels_data["species_taxons"][str(i)] = {
            "chinese_name": name,
            "latin_name": name
        }

    with open(OUTPUT_LABELS, "w", encoding="utf-8") as f:
        json.dump(labels_data, f, indent=2, ensure_ascii=False)

    print("  标签文件保存成功")

    print("\n" + "="*50)
    print("导出完成!")
    print("="*50)
    print(f"\n生成的文件:")
    print(f"  1. {OUTPUT_ONNX} - ONNX 模型文件")
    print(f"  2. {OUTPUT_LABELS} - 标签文件")
    print(f"\n将这两个文件复制到你的 Qt 项目:")
    print(f"  D:\\installed\\Plants_Camera\\zhiwushibie\\build\\unknown_ed1ab7-Debug\\assets\\models\\")

    return True

if __name__ == "__main__":
    export_to_onnx()
