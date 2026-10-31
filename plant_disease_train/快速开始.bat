@echo off
chcp 65001 >nul
echo ================================================
echo 植物病害检测模型训练 - 快速开始
echo ================================================
echo.

cd /d "%~dp0"

echo [步骤 1/3] 准备数据集
echo.
python download_dataset.py
if errorlevel 1 (
    echo [错误] 数据集准备失败
    pause
    exit /b 1
)
echo.

echo [步骤 2/3] 训练模型
echo.
echo 提示：训练大约需要 1-3 小时（CPU）或 30 分钟（GPU）
echo 按 Ctrl+C 可以随时中断
echo.
python train.py
if errorlevel 1 (
    echo [错误] 训练失败
    pause
    exit /b 1
)
echo.

echo [步骤 3/3] 导出 ONNX 模型
echo.
python export_onnx.py
if errorlevel 1 (
    echo [错误] 导出失败
    pause
    exit /b 1
)

echo.
echo ================================================
echo 完成！模型已导出到：
echo   models\plant_disease.onnx
echo   models\plant_disease_labels.txt
echo ================================================
echo.
pause
