// DeepseekClient.cpp
#include "DeepseekClient.h"
#include <QtNetwork/QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDateTime>

static QString buildUserPrompt(const QString& latin, const QString& zh) {
    // 让模型严格输出 JSON，易于解析
    QString prompt = QString(
                         "你是一名资深园艺与花店陈列顾问。请用简体中文，"
                         "围绕植物【%1】（拉丁名：%2）生成一个**严格符合 JSON 语法**的内容，"
                         "禁止输出任何额外说明、前后缀或解释，仅允许返回合法 JSON：\n"
                         "{\n"
                         "  \"encyclopedia\": \"简明植物百科（包含形态特征、生长习性、原产地与分布；如有毒性/过敏风险需提示）\",\n"
                         "  \"cultivation\": \"家庭与园艺养护要点（光照、温度、土壤、浇水、施肥、修剪、病虫害防治等）\",\n"
                         "  \"decoration\": \"花店/家居装饰与搭配建议（适合的场景、风格、容器选择、配花与色彩方案、注意事项）\"\n"
                         "}\n"
                         "要求：\n"
                         "1. 必须返回合法 JSON，不得包含额外注释或解释。\n"
                         "2. 每个字段内容 80~180 字。\n"
                         "3. 使用简体中文。\n"
                         ).arg(zh.isEmpty() ? latin : zh, latin);

    return prompt;
}

static QString buildClassificationPrompt(const QString& plantName) {
    QString prompt = QString(
        "你是一名植物分类学专家。请为植物【%1】提供准确的分类信息。"
        "请严格按照以下JSON格式返回，不要包含任何额外说明或解释：\n"
        "{\n"
        "  \"kingdom\": \"界\",\n"
        "  \"phylum\": \"门\",\n"
        "  \"class\": \"纲\",\n"
        "  \"order\": \"目\",\n"
        "  \"family\": \"科\",\n"
        "  \"genus\": \"属\",\n"
        "  \"species\": \"种\"\n"
        "}\n"
        "要求：\n"
        "1. 必须返回合法JSON格式\n"
        "2. 使用中文分类名称\n"
        "3. 如果某个分类级别未知，请填写\"未知\"\n"
        "4. 不要包含任何其他内容"
    ).arg(plantName);
    
    return prompt;
}

DeepseekClient::DeepseekClient(QObject* parent) : QObject(parent) {}

void DeepseekClient::fetchPlantInfo(const QString& latinName,
                                    const QString& chineseName)
{
    if (apiKey_.isEmpty()) {
        emit errorOccured("DeepSeek API key 未设置");
        return;
    }

    const QUrl url("https://api.deepseek.com/chat/completions");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QByteArray("Bearer ").append(apiKey_.toUtf8()));

    // 构造 OpenAI/DeepSeek 风格 payload
    QJsonObject sysMsg{{"role","system"},{"content",
                                               "You are a helpful plant expert who writes concise, accurate Chinese answers."
                                           }};
    QJsonObject userMsg{{"role","user"},{"content", buildUserPrompt(latinName, chineseName)}};

    QJsonObject payload;
    payload["model"] = "deepseek-chat";
    payload["temperature"] = 0.3;
    payload["messages"] = QJsonArray{ sysMsg, userMsg };


    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply* rp = nam_.post(req, body);

    // 超时保护（可选）
    QTimer* timer = new QTimer(rp);
    timer->setSingleShot(true);
    timer->start(20000); // 20s
    connect(timer, &QTimer::timeout, rp, [this, rp](){
        rp->abort();
    });

    connect(rp, &QNetworkReply::finished, this, [this, rp](){
        rp->deleteLater();

        if (rp->error()!=QNetworkReply::NoError) {
            emit errorOccured(QString("DeepSeek 请求失败: %1").arg(rp->errorString()));
            return;
        }

        const QByteArray bytes = rp->readAll();
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
        if (perr.error!=QJsonParseError::NoError || !doc.isObject()) {
            emit errorOccured("DeepSeek 响应解析失败（非 JSON 根对象）");
            return;
        }

        const QJsonObject root = doc.object();
        const QJsonArray choices = root.value("choices").toArray();
        if (choices.isEmpty()) {
            emit errorOccured("DeepSeek 响应中无 choices");
            return;
        }

        const QJsonObject msg = choices.first().toObject()
                                    .value("message").toObject();
        const QString content = msg.value("content").toString();

        QJsonParseError innerErr{};
        const QJsonDocument inner = QJsonDocument::fromJson(content.toUtf8(), &innerErr);
        if (innerErr.error!=QJsonParseError::NoError || !inner.isObject()) {
            emit errorOccured("模型未返回合法 JSON（content 解析失败）");
            return;
        }

        const QJsonObject obj = inner.object();
        PlantInfo info;
        info.encyclopedia = obj.value("encyclopedia").toString();
        info.cultivation  = obj.value("cultivation").toString();
        info.decor        = obj.value("decoration").toString();

        emit infoReady(info);
    });
}

// 新增：获取植物分类信息
void DeepseekClient::fetchPlantClassification(const QString& plantName)
{
    if (apiKey_.isEmpty()) {
        emit classificationErrorOccured("DeepSeek API key 未设置");
        return;
    }

    const QUrl url("https://api.deepseek.com/chat/completions");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QByteArray("Bearer ").append(apiKey_.toUtf8()));

    // 构造 OpenAI/DeepSeek 风格 payload
    QJsonObject sysMsg{{"role","system"},{"content",
                                               "You are a plant taxonomy expert who provides accurate classification information in Chinese."
                                           }};
    QJsonObject userMsg{{"role","user"},{"content", buildClassificationPrompt(plantName)}};

    QJsonObject payload;
    payload["model"] = "deepseek-chat";
    payload["temperature"] = 0.1; // 降低温度以获得更准确的结果
    payload["messages"] = QJsonArray{ sysMsg, userMsg };

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply* rp = nam_.post(req, body);

    // 超时保护
    QTimer* timer = new QTimer(rp);
    timer->setSingleShot(true);
    timer->start(20000); // 20s
    connect(timer, &QTimer::timeout, rp, [this, rp](){
        rp->abort();
    });

    connect(rp, &QNetworkReply::finished, this, [this, rp](){
        rp->deleteLater();

        if (rp->error() != QNetworkReply::NoError) {
            emit classificationErrorOccured(QString("DeepSeek 请求失败: %1").arg(rp->errorString()));
            return;
        }

        const QByteArray bytes = rp->readAll();
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            emit classificationErrorOccured("DeepSeek 响应解析失败（非 JSON 根对象）");
            return;
        }

        const QJsonObject root = doc.object();
        const QJsonArray choices = root.value("choices").toArray();
        if (choices.isEmpty()) {
            emit classificationErrorOccured("DeepSeek 响应中无 choices");
            return;
        }

        const QJsonObject msg = choices.first().toObject()
                                    .value("message").toObject();
        const QString content = msg.value("content").toString();

        QJsonParseError innerErr{};
        const QJsonDocument inner = QJsonDocument::fromJson(content.toUtf8(), &innerErr);
        if (innerErr.error != QJsonParseError::NoError || !inner.isObject()) {
            emit classificationErrorOccured("模型未返回合法 JSON（content 解析失败）");
            return;
        }

        const QJsonObject obj = inner.object();
        PlantClassification classification;
        classification.kingdom = obj.value("kingdom").toString();
        classification.phylum = obj.value("phylum").toString();
        classification.class_ = obj.value("class").toString();
        classification.order = obj.value("order").toString();
        classification.family = obj.value("family").toString();
        classification.genus = obj.value("genus").toString();
        classification.species = obj.value("species").toString();

        emit classificationReady(classification);
    });
}

// 新增：自由对话功能实现
void DeepseekClient::sendChatMessage(const QString& message)
{
    if (apiKey_.isEmpty()) {
        emit chatErrorOccured("DeepSeek API key 未设置");
        return;
    }

    if (message.trimmed().isEmpty()) {
        emit chatErrorOccured("消息内容不能为空");
        return;
    }

    // 添加用户消息到历史记录
    ChatMessage userMsg;
    userMsg.role = "user";
    userMsg.content = message.trimmed();
    userMsg.timestamp = QDateTime::currentDateTime();
    chatHistory_.append(userMsg);

    const QUrl url("https://api.deepseek.com/chat/completions");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QByteArray("Bearer ").append(apiKey_.toUtf8()));

    // 构建消息历史
    QJsonArray messages;
    
    // 系统消息
    QJsonObject systemMsg;
    systemMsg["role"] = "system";
    systemMsg["content"] = "你是一个友善、有帮助的AI助手。请用简体中文回答用户的问题，回答要准确、有用且简洁。";
    messages.append(systemMsg);

    // 添加对话历史（最多保留最近10轮对话）
    int startIndex = qMax(0, chatHistory_.size() - 20); // 保留最近10轮对话（20条消息）
    for (int i = startIndex; i < chatHistory_.size(); ++i) {
        QJsonObject msg;
        msg["role"] = chatHistory_[i].role;
        msg["content"] = chatHistory_[i].content;
        messages.append(msg);
    }

    // 构造请求
    QJsonObject payload;
    payload["model"] = "deepseek-chat";
    payload["temperature"] = 0.7;
    payload["max_tokens"] = 2000;
    payload["messages"] = messages;

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply* rp = nam_.post(req, body);

    // 超时保护
    QTimer* timer = new QTimer(rp);
    timer->setSingleShot(true);
    timer->start(30000); // 30s
    connect(timer, &QTimer::timeout, rp, [this, rp](){
        rp->abort();
    });

    connect(rp, &QNetworkReply::finished, this, [this, rp](){
        rp->deleteLater();

        if (rp->error() != QNetworkReply::NoError) {
            QString errorMsg = QString("网络请求失败: %1").arg(rp->errorString());
            emit chatErrorOccured(errorMsg);
            return;
        }

        const QByteArray bytes = rp->readAll();
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(bytes, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            emit chatErrorOccured("API响应解析失败");
            return;
        }

        const QJsonObject root = doc.object();
        
        // 检查是否有错误
        if (root.contains("error")) {
            QString errorMsg = root.value("error").toObject().value("message").toString();
            emit chatErrorOccured(QString("API错误: %1").arg(errorMsg));
            return;
        }

        const QJsonArray choices = root.value("choices").toArray();
        if (choices.isEmpty()) {
            emit chatErrorOccured("API响应中无有效回复");
            return;
        }

        const QJsonObject choice = choices.first().toObject();
        const QJsonObject message = choice.value("message").toObject();
        const QString content = message.value("content").toString();

        if (content.isEmpty()) {
            emit chatErrorOccured("AI回复内容为空");
            return;
        }

        // 添加AI回复到历史记录
        ChatMessage aiMsg;
        aiMsg.role = "assistant";
        aiMsg.content = content;
        aiMsg.timestamp = QDateTime::currentDateTime();
        chatHistory_.append(aiMsg);

        emit chatResponseReady(content);
    });
}

void DeepseekClient::clearChatHistory()
{
    chatHistory_.clear();
}
