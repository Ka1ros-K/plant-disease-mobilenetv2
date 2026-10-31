"""
植物病害检测模型训练脚本
基于 PlantVillage 数据集，使用 MobileNetV2 迁移学习
"""

import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader
from torchvision import transforms, models
from torchvision.datasets import ImageFolder
import time
import json

# ================== 配置 ==================
DATA_DIR = r"D:\installed\Plants_Camera\plant_disease_train\dataset"  # 数据集目录
SAVE_DIR = r"D:\installed\Plants_Camera\plant_disease_train\models"    # 模型保存目录
BATCH_SIZE = 32
EPOCHS = 15
LEARNING_RATE = 0.001
NUM_WORKERS = 0  # Windows 建议设为 0
DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")

# ================== 数据增强 ==================
train_transforms = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.RandomHorizontalFlip(),
    transforms.RandomRotation(10),
    transforms.ColorJitter(brightness=0.2, contrast=0.2, saturation=0.2),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

val_transforms = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

# ================== 加载数据 ==================
def load_data():
    print(f"[INFO] 正在加载数据集：{DATA_DIR}")

    train_dir = os.path.join(DATA_DIR, "train")
    val_dir = os.path.join(DATA_DIR, "val")

    if not os.path.exists(train_dir):
        print(f"[ERROR] 训练集目录不存在：{train_dir}")
        return None, None, None

    train_dataset = ImageFolder(train_dir, train_transforms)
    val_dataset = ImageFolder(val_dir, val_transforms) if os.path.exists(val_dir) else None

    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True, num_workers=NUM_WORKERS)
    val_loader = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False, num_workers=NUM_WORKERS) if val_dataset else None

    class_names = train_dataset.classes
    print(f"[INFO] 数据集加载完成:")
    print(f"  - 类别数：{len(class_names)}")
    print(f"  - 训练样本：{len(train_dataset)}")
    if val_dataset:
        print(f"  - 验证样本：{len(val_dataset)}")
    print(f"  - 类别：{class_names[:5]}...")  # 显示前 5 个类别

    return train_loader, val_loader, class_names

# ================== 创建模型 ==================
def create_model(num_classes):
    print(f"[INFO] 创建 MobileNetV2 模型，输出类别数：{num_classes}")

    # 使用预训练的 MobileNetV2（如果下载失败则使用随机初始化）
    try:
        model = models.mobilenet_v2(weights=models.MobileNet_V2_Weights.IMAGENET1K_V1)
        print("[INFO] 使用 ImageNet 预训练权重")
    except Exception as e:
        print(f"[WARN] 预训练权重加载失败：{e}，使用随机初始化")
        model = models.mobilenet_v2(weights=None)

    # 替换分类器
    num_features = model.classifier[1].in_features
    model.classifier = nn.Sequential(
        nn.Dropout(0.2),
        nn.Linear(num_features, num_classes)
    )

    model = model.to(DEVICE)
    return model

# ================== 训练函数 ==================
def train_epoch(model, loader, criterion, optimizer):
    model.train()
    running_loss = 0.0
    correct = 0
    total = 0

    for inputs, labels in loader:
        inputs, labels = inputs.to(DEVICE), labels.to(DEVICE)

        optimizer.zero_grad()
        outputs = model(inputs)
        loss = criterion(outputs, labels)
        loss.backward()
        optimizer.step()

        running_loss += loss.item()
        _, predicted = outputs.max(1)
        total += labels.size(0)
        correct += predicted.eq(labels).sum().item()

    return running_loss / len(loader), 100. * correct / total

def validate(model, loader, criterion):
    model.eval()
    running_loss = 0.0
    correct = 0
    total = 0

    with torch.no_grad():
        for inputs, labels in loader:
            inputs, labels = inputs.to(DEVICE), labels.to(DEVICE)
            outputs = model(inputs)
            loss = criterion(outputs, labels)

            running_loss += loss.item()
            _, predicted = outputs.max(1)
            total += labels.size(0)
            correct += predicted.eq(labels).sum().item()

    return running_loss / len(loader), 100. * correct / total

# ================== 主训练循环 ==================
def train():
    print(f"[INFO] 使用设备：{DEVICE}")
    print(f"[INFO] 批次大小：{BATCH_SIZE}")
    print(f"[INFO] 训练轮数：{EPOCHS}")

    # 加载数据
    train_loader, val_loader, class_names = load_data()
    if train_loader is None:
        print("[ERROR] 数据加载失败，请检查数据集路径")
        return

    num_classes = len(class_names)

    # 保存类别信息
    os.makedirs(SAVE_DIR, exist_ok=True)
    class_info = {
        "classes": class_names,
        "num_classes": num_classes,
        "input_size": 224
    }
    with open(os.path.join(SAVE_DIR, "class_names.json"), "w", encoding="utf-8") as f:
        json.dump(class_info, f, indent=2, ensure_ascii=False)
    print(f"[INFO] 类别信息已保存：{os.path.join(SAVE_DIR, 'class_names.json')}")

    # 创建模型
    model = create_model(num_classes)

    # 损失函数和优化器
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)
    scheduler = optim.lr_scheduler.StepLR(optimizer, step_size=7, gamma=0.1)

    # 训练记录
    best_acc = 0.0
    history = {"train_loss": [], "train_acc": [], "val_loss": [], "val_acc": []}

    print("\n" + "="*50)
    print("开始训练")
    print("="*50 + "\n")

    for epoch in range(EPOCHS):
        start_time = time.time()

        # 训练
        train_loss, train_acc = train_epoch(model, train_loader, criterion, optimizer)

        # 验证
        if val_loader:
            val_loss, val_acc = validate(model, val_loader, criterion)
        else:
            val_loss, val_acc = train_loss, train_acc

        # 记录历史
        history["train_loss"].append(train_loss)
        history["train_acc"].append(train_acc)
        history["val_loss"].append(val_loss)
        history["val_acc"].append(val_acc)

        # 保存最佳模型
        if val_acc > best_acc:
            best_acc = val_acc
            torch.save(model.state_dict(), os.path.join(SAVE_DIR, "best.pth"))
            print(f"  [保存] 最佳模型已保存 (准确率：{val_acc:.2f}%)")

        epoch_time = time.time() - start_time
        print(f"Epoch {epoch+1:02d}/{EPOCHS} | "
              f"Loss: {train_loss:.4f} | "
              f"Train Acc: {train_acc:.2f}% | "
              f"Val Acc: {val_acc:.2f}% | "
              f"Time: {epoch_time:.0f}s")

        scheduler.step()

    # 保存最终模型
    torch.save(model.state_dict(), os.path.join(SAVE_DIR, "final.pth"))

    # 保存训练历史
    with open(os.path.join(SAVE_DIR, "training_history.json"), "w", encoding="utf-8") as f:
        json.dump(history, f, indent=2)

    print("\n" + "="*50)
    print(f"训练完成！最佳验证准确率：{best_acc:.2f}%")
    print(f"模型保存位置：{SAVE_DIR}")
    print("="*50)

if __name__ == "__main__":
    train()
