#include "CameraWindow.h"
#include <QApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>

CameraWindow::CameraWindow(QWidget *parent)
    : QWidget(parent)
    , m_camera(nullptr)
    , m_previewTimer(nullptr)
    , m_cameraActive(false)
{
    setupUI();
    setupCamera();
}

CameraWindow::~CameraWindow()
{
    if (m_camera && m_camera->isOpened()) {
        m_camera->release();
    }
    if (m_previewTimer) {
        m_previewTimer->stop();
    }
}

void CameraWindow::setupUI()
{
    setWindowTitle("摄像头拍照");
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    
    // 设置窗口大小
    resize(800, 600);
    setMinimumSize(640, 480);
    
    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // 创建摄像头预览区域
    m_viewfinder = new QLabel(this);
    m_viewfinder->setMinimumSize(640, 480);
    m_viewfinder->setAlignment(Qt::AlignCenter);
    m_viewfinder->setStyleSheet(R"(
        QLabel {
            border: 3px solid rgb(84, 130, 53);
            border-radius: 15px;
            background-color: black;
            color: white;
        }
    )");
    m_viewfinder->setText("正在初始化摄像头...");
    m_mainLayout->addWidget(m_viewfinder);
    
    // 创建状态标签
    m_statusLabel = new QLabel("正在初始化摄像头...", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(R"(
        QLabel {
            color: rgb(84, 130, 53);
            font-weight: bold;
            font-size: 14px;
            padding: 10px;
        }
    )");
    m_mainLayout->addWidget(m_statusLabel);
    
    // 创建按钮布局
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(20);
    
    // 拍照按钮
    m_captureButton = new QPushButton("📷 拍照", this);
    m_captureButton->setMinimumSize(120, 40);
    m_captureButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgb(84, 130, 53);
            color: white;
            border: none;
            border-radius: 20px;
            font-size: 14px;
            font-weight: bold;
            padding: 10px 20px;
        }
        QPushButton:hover {
            background-color: rgb(100, 150, 70);
        }
        QPushButton:pressed {
            background-color: rgb(70, 110, 40);
        }
        QPushButton:disabled {
            background-color: rgb(200, 200, 200);
            color: rgb(100, 100, 100);
        }
    )");
    m_captureButton->setEnabled(false);
    connect(m_captureButton, &QPushButton::clicked, this, &CameraWindow::onCaptureClicked);
    
    // 取消按钮
    m_cancelButton = new QPushButton("❌ 取消", this);
    m_cancelButton->setMinimumSize(120, 40);
    m_cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: rgb(200, 100, 100);
            color: white;
            border: none;
            border-radius: 20px;
            font-size: 14px;
            font-weight: bold;
            padding: 10px 20px;
        }
        QPushButton:hover {
            background-color: rgb(220, 120, 120);
        }
        QPushButton:pressed {
            background-color: rgb(180, 80, 80);
        }
    )");
    connect(m_cancelButton, &QPushButton::clicked, this, &CameraWindow::onCancelClicked);
    
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_captureButton);
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addStretch();
    
    m_mainLayout->addLayout(m_buttonLayout);
    
    // 设置窗口样式
    setStyleSheet(R"(
        QWidget {
            background-color: white;
        }
    )");
}

void CameraWindow::setupCamera()
{
    // 创建OpenCV摄像头实例
    m_camera = new cv::VideoCapture(0); // 使用默认摄像头
    
    if (!m_camera->isOpened()) {
        showError("无法打开摄像头！\n请检查摄像头连接或权限设置。");
        return;
    }
    
    // 设置摄像头参数
    m_camera->set(cv::CAP_PROP_FRAME_WIDTH, 640);
    m_camera->set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    m_camera->set(cv::CAP_PROP_FPS, 30);
    
    // 创建预览定时器
    m_previewTimer = new QTimer(this);
    connect(m_previewTimer, &QTimer::timeout, this, &CameraWindow::onPreviewTimer);
    
    // 启动预览
    m_previewTimer->start(33); // 约30fps
    m_cameraActive = true;
    
    m_viewfinder->setText("摄像头已启动\n实时预览中...");
    m_statusLabel->setText("摄像头已启动，请调整拍摄角度后点击拍照");
    m_captureButton->setEnabled(true);
    
    qDebug() << "[Camera] OpenCV摄像头启动成功";
}

void CameraWindow::onPreviewTimer()
{
    if (!m_camera || !m_camera->isOpened() || !m_cameraActive) {
        return;
    }
    
    cv::Mat frame;
    if (m_camera->read(frame)) {
        // 将OpenCV Mat转换为QPixmap并显示
        QPixmap pixmap = matToQPixmap(frame);
        
        // 缩放图像以适应预览区域
        QSize labelSize = m_viewfinder->size();
        QPixmap scaledPixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        m_viewfinder->setPixmap(scaledPixmap);
    }
}

void CameraWindow::onCaptureClicked()
{
    if (!m_camera || !m_camera->isOpened() || !m_cameraActive) {
        showError("摄像头未就绪，无法拍照！");
        return;
    }
    
    m_captureButton->setEnabled(false);
    m_statusLabel->setText("正在拍照...");
    
    // 停止预览定时器
    if (m_previewTimer) {
        m_previewTimer->stop();
    }
    
    // 捕获当前帧
    cv::Mat frame;
    if (m_camera->read(frame)) {
        qDebug() << "[Camera] 图像捕获成功，尺寸:" << frame.cols << "x" << frame.rows;
        
        // 将OpenCV Mat转换为QPixmap
        QPixmap pixmap = matToQPixmap(frame);
        
        // 发送图像信号
        emit imageCaptured(pixmap);
        
        // 显示成功消息
        m_statusLabel->setText("拍照成功！正在返回主界面...");
        
        // 延迟关闭窗口
        QTimer::singleShot(1000, this, [this]() {
            close();
        });
    } else {
        showError("拍照失败！请重试。");
        m_captureButton->setEnabled(true);
        m_statusLabel->setText("摄像头已启动，请调整拍摄角度后点击拍照");
        
        // 重新启动预览
        if (m_previewTimer) {
            m_previewTimer->start(33);
        }
    }
}

void CameraWindow::onCancelClicked()
{
    qDebug() << "[Camera] 用户取消拍照";
    close();
}

QPixmap CameraWindow::matToQPixmap(const cv::Mat& mat)
{
    if (mat.empty()) {
        return QPixmap();
    }
    
    cv::Mat rgbMat;
    
    // 根据输入图像类型进行转换
    if (mat.channels() == 3) {
        // BGR转RGB
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
    } else if (mat.channels() == 1) {
        // 灰度图转RGB
        cv::cvtColor(mat, rgbMat, cv::COLOR_GRAY2RGB);
    } else {
        // 其他情况直接复制
        rgbMat = mat.clone();
    }
    
    // 创建QImage
    QImage qimg(rgbMat.data, rgbMat.cols, rgbMat.rows, 
                rgbMat.step, QImage::Format_RGB888);
    
    // 转换为QPixmap
    return QPixmap::fromImage(qimg);
}

void CameraWindow::showError(const QString& message)
{
    qWarning() << "[Camera] 错误:" << message;
    QMessageBox::warning(this, "摄像头错误", message);
}