// DeepseekClient.h
#pragma once
#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

struct PlantInfo {
    QString encyclopedia; // 百科
    QString cultivation;  // 培养/养护
    QString decor;        // 装饰/搭配
};

struct PlantClassification {
    QString kingdom;      // 界
    QString phylum;       // 门
    QString class_;       // 纲
    QString order;        // 目
    QString family;       // 科
    QString genus;        // 属
    QString species;      // 种
};

struct ChatMessage {
    QString role;    // "user" 或 "assistant"
    QString content; // 消息内容
    QDateTime timestamp; // 时间戳
};

class DeepseekClient : public QObject {
    Q_OBJECT
public:
    explicit DeepseekClient(QObject* parent=nullptr);
    void setApiKey(const QString& key) { apiKey_ = key; }
    void fetchPlantInfo(const QString& latinName,
                        const QString& chineseName);
    
    // 新增：获取植物分类信息
    void fetchPlantClassification(const QString& plantName);
    
    // 新增：自由对话功能
    void sendChatMessage(const QString& message);
    void clearChatHistory();
    QList<ChatMessage> getChatHistory() const { return chatHistory_; }

signals:
    void infoReady(const PlantInfo& info);
    void errorOccured(const QString& err);
    
    // 新增：植物分类信息信号
    void classificationReady(const PlantClassification& classification);
    void classificationErrorOccured(const QString& error);
    
    // 新增：对话相关信号
    void chatResponseReady(const QString& response);
    void chatErrorOccured(const QString& error);

private:
    QNetworkAccessManager nam_;
    QString apiKey_;
    QList<ChatMessage> chatHistory_; // 对话历史
};
