#include "SettingsWindow.h"
#include "./ui_SettingsWindow.h"
#include <QDebug>
#include <QMessageBox>

SettingsWindow::SettingsWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsWindow)
    , m_settings(new QSettings("PlantIdentification", "Settings", this))
    , m_autoSaveEnabled(true)
    , m_currentTheme("默认主题")
    , m_originalAutoSaveEnabled(true)
    , m_originalTheme("默认主题")
{
    ui->setupUi(this);
    setupUI();
    loadSettings();
}

SettingsWindow::~SettingsWindow()
{
    delete ui;
}

void SettingsWindow::setupUI()
{
    // 如果父窗口为 nullptr，设置为独立窗口
    if (parent() == nullptr) {
        setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        setAttribute(Qt::WA_DeleteOnClose, false);
    }
    
    // 设置窗口样式 - 与主界面保持一致
    QString windowStyle = R"(
        QWidget {
            background-color: white;
        }
        QLabel {
            color: rgb(84, 130, 53);
        }
        QGroupBox {
            background-color: white;
            border: 3px solid rgb(84, 130, 53);
            border-radius: 15px;
            margin-top: 10px;
            font-weight: bold;
            color: rgb(84, 130, 53);
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
            color: rgb(84, 130, 53);
            font-weight: bold;
        }
        QCheckBox {
            color: rgb(84, 130, 53);
            font-weight: bold;
        }
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
            border: 2px solid rgb(84, 130, 53);
            border-radius: 4px;
            background-color: white;
        }
        QCheckBox::indicator:checked {
            background-color: rgb(84, 130, 53);
            image: url(data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTIiIGhlaWdodD0iMTIiIHZpZXdCb3g9IjAgMCAxMiAxMiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTEwIDNMNC41IDguNUwyIDYiIHN0cm9rZT0id2hpdGUiIHN0cm9rZS13aWR0aD0iMiIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBzdHJva2UtbGluZWpvaW49InJvdW5kIi8+Cjwvc3ZnPgo=);
        }

        QPushButton {
            background-color: white;
            border: 3px solid rgb(84, 130, 53);
            border-radius: 15px;
            padding: 8px 16px;
            font-weight: bold;
            color: rgb(84, 130, 53);
        }
        QPushButton:hover {
            background-color: rgba(84, 130, 53, 0.1);
            border-color: rgb(84, 130, 53);
            color: rgb(84, 130, 53);
        }
        QPushButton:pressed {
            background-color: rgb(84, 130, 53);
            color: white;
        }
    )";
    
    setStyleSheet(windowStyle);
    
    // 连接信号
    connect(ui->okButton, &QPushButton::clicked, this, &SettingsWindow::onOkClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &SettingsWindow::onCancelClicked);
    connect(ui->applyButton, &QPushButton::clicked, this, &SettingsWindow::onApplyClicked);
    connect(ui->autoSaveCheckBox, &QCheckBox::toggled, this, &SettingsWindow::onAutoSaveToggled);
}

void SettingsWindow::loadSettings()
{
    // 从设置文件加载配置
    m_autoSaveEnabled = m_settings->value("autoSaveEnabled", true).toBool();
    m_currentTheme = m_settings->value("theme", "默认主题").toString();
    
    // 保存原始值
    m_originalAutoSaveEnabled = m_autoSaveEnabled;
    m_originalTheme = m_currentTheme;
    
    // 更新UI
    ui->autoSaveCheckBox->setChecked(m_autoSaveEnabled);
    
    qDebug() << "[Settings] 设置已加载 - 自动保存:" << m_autoSaveEnabled << "主题:" << m_currentTheme;
}

void SettingsWindow::saveSettings()
{
    // 保存到设置文件
    m_settings->setValue("autoSaveEnabled", m_autoSaveEnabled);
    m_settings->setValue("theme", m_currentTheme);
    m_settings->sync();
    
    qDebug() << "[Settings] 设置已保存 - 自动保存:" << m_autoSaveEnabled << "主题:" << m_currentTheme;
}

void SettingsWindow::applySettings()
{
    // 应用设置并发送信号
    emit autoSaveChanged(m_autoSaveEnabled);
    
    qDebug() << "[Settings] 设置已应用 - 自动保存:" << m_autoSaveEnabled;
}

bool SettingsWindow::isAutoSaveEnabled() const
{
    return m_autoSaveEnabled;
}



void SettingsWindow::setAutoSaveEnabled(bool enabled)
{
    m_autoSaveEnabled = enabled;
    ui->autoSaveCheckBox->setChecked(enabled);
}



void SettingsWindow::onOkClicked()
{
    // 保存并应用设置
    saveSettings();
    applySettings();
    
    // 关闭窗口
    close();
    
    qDebug() << "[Settings] 确定按钮点击 - 设置已保存并应用";
}

void SettingsWindow::onCancelClicked()
{
    // 恢复原始设置
    m_autoSaveEnabled = m_originalAutoSaveEnabled;
    m_currentTheme = m_originalTheme;
    
    // 更新UI
    ui->autoSaveCheckBox->setChecked(m_autoSaveEnabled);
    
    // 关闭窗口
    close();
    
    qDebug() << "[Settings] 取消按钮点击 - 设置已恢复";
}

void SettingsWindow::onApplyClicked()
{
    // 保存并应用设置
    saveSettings();
    applySettings();
    
    qDebug() << "[Settings] 应用按钮点击 - 设置已保存并应用";
}

void SettingsWindow::onAutoSaveToggled(bool checked)
{
    m_autoSaveEnabled = checked;
    qDebug() << "[Settings] 自动保存设置已更改:" << checked;
}


