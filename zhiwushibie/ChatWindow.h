#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "DeepseekClient.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ChatWindow; }
QT_END_NAMESPACE

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();
    
    void setDeepseekClient(DeepseekClient* client);

private slots:
    void onSendClicked();
    void onClearClicked();
    void onChatResponseReady(const QString& response);
    void onChatErrorOccured(const QString& error);

private:
    void setupUI();
    void appendMessage(const QString &message, bool isUser = true);
    void clearChat();
    void setLoadingState(bool loading);
    
    // UI指针
    Ui::ChatWindow *ui;
    
    // DeepSeek客户端
    DeepseekClient* m_deepseekClient;
};

#endif // CHATWINDOW_H
