#include "InfoWindow.h"
#include "./ui_InfoWindow.h"
#include <QDebug>

InfoWindow::InfoWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::InfoWindow)
{
    ui->setupUi(this);
    setupUI();
}

InfoWindow::~InfoWindow()
{
    delete ui;
}

void InfoWindow::setupUI()
{
    // 如果父窗口为 nullptr，设置为独立窗口
    if (parent() == nullptr) {
        setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        setAttribute(Qt::WA_DeleteOnClose, false);
    }
    
    // 设置窗口样式 - 白色背景
    setStyleSheet("QWidget { background-color: white; }");
    
    // 连接信号
    connect(ui->closeButton, &QPushButton::clicked, this, &InfoWindow::onCloseClicked);
    
    qDebug() << "[Info] 信息窗口已初始化";
}

void InfoWindow::onCloseClicked()
{
    // 关闭窗口
    close();
    
    qDebug() << "[Info] 信息窗口已关闭";
}

