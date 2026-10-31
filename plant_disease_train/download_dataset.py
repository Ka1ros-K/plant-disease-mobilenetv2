"""
PlantVillage 数据集下载脚本
自动下载并划分训练集/验证集
"""

import os
import tarfile
import shutil
import random
from pathlib import Path

# 数据集下载链接（使用 Kaggle 镜像或备用源）
DATASET_URLS = [
    # 主要来源：Kaggle PlantVillage
    "https://storage.googleapis.com/kagglesdsdata/datasets/116907/273127/plantvillage-dataset.zip",
    # 备用：GitHub
    "https://github.com/spMohanty/PlantVillage-Dataset/raw/master/raw/color/",
]

DATASET_DIR = Path(r"D:\installed\Plants_Camera\plant_disease_train\dataset")
RAW_DIR = DATASET_DIR / "raw"
TRAIN_DIR = DATASET_DIR / "train"
VAL_DIR = DATASET_DIR / "val"

VAL_RATIO = 0.2  # 20% 作为验证集

def download_dataset():
    """下载数据集"""
    print("[INFO] 开始检测 PlantVillage 数据集...")

    # 检查是否已存在
    if (DATASET_DIR / "train").exists():
        print("[INFO] 数据集已存在，跳过下载")
        return True

    # 创建目录
    DATASET_DIR.mkdir(parents=True, exist_ok=True)
    RAW_DIR.mkdir(parents=True, exist_ok=True)

    # 自动检测数据集位置
    # 情况 1: 数据直接在 raw 目录下
    raw_subdirs = [d for d in RAW_DIR.iterdir() if d.is_dir() and d.name != "plantvillage dataset"]

    # 情况 2: 数据在 raw/plantvillage dataset/color 下（Kaggle 标准结构）
    if not raw_subdirs:
        possible_paths = [
            RAW_DIR / "plantvillage dataset" / "color",
            RAW_DIR / "plantvillage-dataset" / "color",
            RAW_DIR / "color",
        ]
        for p in possible_paths:
            if p.exists():
                print(f"[INFO] 检测到数据集在：{p}")
                # 移动类别文件夹到 raw 目录
                for cat_dir in p.iterdir():
                    if cat_dir.is_dir():
                        shutil.move(str(cat_dir), str(RAW_DIR / cat_dir.name))
                print(f"[INFO] 已移动类别文件夹到 raw 目录")
                raw_subdirs = [d for d in RAW_DIR.iterdir() if d.is_dir()]
                break

    if not raw_subdirs:
        print("[INFO] 未找到数据集，请手动下载:")
        print("")
        print("  方式 1 - Kaggle (推荐):")
        print("    https://www.kaggle.com/datasets/abdallahalidev/plantvillage")
        print("    下载后解压到：D:\\installed\\Plants_Camera\\plant_disease_train\\dataset\\raw\\")
        print("")
        print("  方式 2 - GitHub:")
        print("    https://github.com/spMohanty/PlantVillage-Dataset")
        print("")

        input("数据集下载/解压完成后，按回车继续...")

        # 再次检测
        raw_subdirs = [d for d in RAW_DIR.iterdir() if d.is_dir()]
        if not raw_subdirs:
            print("[ERROR] 仍未找到数据文件夹")
            return False

    print(f"[INFO] 找到 {len(raw_subdirs)} 个类别文件夹")
    return True

def split_dataset():
    """划分训练集和验证集"""
    print("\n[INFO] 开始划分数据集...")

    # 清理旧目录
    if TRAIN_DIR.exists():
        shutil.rmtree(TRAIN_DIR)
    if VAL_DIR.exists():
        shutil.rmtree(VAL_DIR)

    TRAIN_DIR.mkdir(parents=True, exist_ok=True)
    VAL_DIR.mkdir(parents=True, exist_ok=True)

    # 遍历每个类别
    categories = [d for d in RAW_DIR.iterdir() if d.is_dir()]

    total_train = 0
    total_val = 0

    for cat_dir in categories:
        cat_name = cat_dir.name
        images = list(cat_dir.glob("*.jpg")) + list(cat_dir.glob("*.png"))

        if not images:
            print(f"  [跳过] {cat_name}: 未找到图片")
            continue

        # 随机打乱
        random.shuffle(images)

        # 划分
        split_idx = int(len(images) * (1 - VAL_RATIO))
        train_imgs = images[:split_idx]
        val_imgs = images[split_idx:]

        # 创建类别目录
        train_cat_dir = TRAIN_DIR / cat_name
        val_cat_dir = VAL_DIR / cat_name
        train_cat_dir.mkdir(parents=True, exist_ok=True)
        val_cat_dir.mkdir(parents=True, exist_ok=True)

        # 复制文件
        for img in train_imgs:
            shutil.copy2(img, train_cat_dir / img.name)
            total_train += 1

        for img in val_imgs:
            shutil.copy2(img, val_cat_dir / img.name)
            total_val += 1

        print(f"  {cat_name}: 训练={len(train_imgs)}, 验证={len(val_imgs)}")

    print(f"\n[INFO] 数据集划分完成!")
    print(f"  训练集：{total_train} 张")
    print(f"  验证集：{total_val} 张")
    print(f"  总计：{total_train + total_val} 张")

    return True

def main():
    print("="*50)
    print("PlantVillage 数据集准备工具")
    print("="*50)
    print()

    if not download_dataset():
        print("[ERROR] 数据集准备失败")
        return

    if not split_dataset():
        print("[ERROR] 数据集划分失败")
        return

    print("\n" + "="*50)
    print("数据集准备完成!")
    print(f"位置：{DATASET_DIR}")
    print("="*50)
    print("\n现在可以运行 train.py 开始训练了")
    print("命令：python train.py")

if __name__ == "__main__":
    random.seed(42)
    main()
