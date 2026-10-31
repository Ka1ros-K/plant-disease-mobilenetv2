#include "ChatWindow.h"
#include "./ui_ChatWindow.h"
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QColor>
#include <QFont>

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatWindow)
    , m_deepseekClient(nullptr)
{
    ui->setupUi(this);
    setupUI();
}

ChatWindow::~ChatWindow()
{
    delete ui;
}

void ChatWindow::setupUI()
{
    // 如果父窗口为 nullptr，设置为独立窗口
    if (parent() == nullptr) {
        setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        setAttribute(Qt::WA_DeleteOnClose, false);
    }
    
    // 设置窗口样式 - 白色背景
    setStyleSheet("QWidget { background-color: white; }");
    
    // 设置状态标签为透明背景
    ui->statusLabel->setStyleSheet("QLabel { background-color: transparent; color: rgb(100, 100, 100); }");
    
    // 连接信号
    connect(ui->sendButton, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &ChatWindow::onClearClicked);
    
    // 设置输入框的回车键发送
    connect(ui->inputEdit, &QTextEdit::textChanged, this, [this]() {
        // 简单的输入验证
        bool hasText = !ui->inputEdit->toPlainText().trimmed().isEmpty();
        ui->sendButton->setEnabled(hasText);
    });
    
    // 初始化发送按钮状态
    ui->sendButton->setEnabled(false);
    
    qDebug() << "[Chat] 对话窗口已初始化";
}

void ChatWindow::onSendClicked()
{
    QString message = ui->inputEdit->toPlainText().trimmed();
    if (message.isEmpty()) {
        return;
    }
    
    // 检查是否有DeepSeek客户端
    if (!m_deepseekClient) {
        ui->statusLabel->setText("错误：未配置AI客户端");
        appendMessage("错误：未配置AI客户端，无法进行对话。", false);
        return;
    }
    
    // 添加用户消息到对话显示区域
    appendMessage(message, true);
    
    // 清空输入框
    ui->inputEdit->clear();
    ui->sendButton->setEnabled(false);
    
    // 设置加载状态
    setLoadingState(true);
    ui->statusLabel->setText("AI正在思考中...");
    
    // 发送消息到DeepSeek API
    m_deepseekClient->sendChatMessage(message);
    
    qDebug() << "[Chat] 用户发送消息:" << message;
}

void ChatWindow::onClearClicked()
{
    clearChat();
    
    // 清空DeepSeek客户端的对话历史
    if (m_deepseekClient) {
        m_deepseekClient->clearChatHistory();
    }
    
    qDebug() << "[Chat] 对话已清空";
}

void ChatWindow::appendMessage(const QString &message, bool isUser)
{
    // 移动到文本末尾
    ui->chatDisplay->moveCursor(QTextCursor::End);
    
    // 设置文本格式
    QTextCharFormat format;
    if (isUser) {
        // 用户消息：黑色黑体
        format.setForeground(QColor(0, 0, 0)); // 黑色
        format.setFontWeight(QFont::Bold);     // 黑体
    } else {
        // AI消息：绿色黑体
        format.setForeground(QColor(84, 130, 53)); // 绿色 (与主题色一致)
        format.setFontWeight(QFont::Bold);         // 黑体
    }
    
    // 应用格式并插入消息
    ui->chatDisplay->setCurrentCharFormat(format);
    ui->chatDisplay->insertPlainText(message + "\n\n");
    
    // 滚动到底部
    ui->chatDisplay->moveCursor(QTextCursor::End);
}

void ChatWindow::clearChat()
{
    ui->chatDisplay->clear();
    ui->chatDisplay->setPlainText("欢迎使用AI智能对话功能！\n\n您可以在这里与AI进行自由对话，询问任何问题。\n\n请在下方的输入框中输入您的问题，然后点击发送按钮。");
    ui->statusLabel->setText("对话已清空");
}

void ChatWindow::setDeepseekClient(DeepseekClient* client)
{
    m_deepseekClient = client;
    
    if (m_deepseekClient) {
        // 连接信号
        connect(m_deepseekClient, &DeepseekClient::chatResponseReady,
                this, &ChatWindow::onChatResponseReady);
        connect(m_deepseekClient, &DeepseekClient::chatErrorOccured,
                this, &ChatWindow::onChatErrorOccured);
        
        qDebug() << "[Chat] DeepSeek客户端已设置";
    }
}

void ChatWindow::onChatResponseReady(const QString& response)
{
    // 取消加载状态
    setLoadingState(false);
    
    // 添加AI回复到对话显示区域
    appendMessage(response, false);
    
    // 更新状态
    ui->statusLabel->setText("准备就绪");
    
    qDebug() << "[Chat] 收到AI回复:" << response.left(100) << "...";
}

void ChatWindow::onChatErrorOccured(const QString& error)
{
    // 取消加载状态
    setLoadingState(false);
    
    // 显示错误消息
    appendMessage(QString("错误：%1").arg(error), false);
    
    // 更新状态
    ui->statusLabel->setText("发生错误");
    
    qDebug() << "[Chat] AI对话错误:" << error;
}

void ChatWindow::setLoadingState(bool loading)
{
    ui->sendButton->setEnabled(!loading);
    ui->inputEdit->setEnabled(!loading);
    
    if (loading) {
        ui->sendButton->setText("发送中...");
    } else {
        ui->sendButton->setText("发送");
    }
}
