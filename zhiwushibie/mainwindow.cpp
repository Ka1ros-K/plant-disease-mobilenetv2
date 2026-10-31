#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "PlantId.h"
#include "PlantDisease.h"

// >>> 新增：DeepSeek 客户端头文件
#include "DeepseekClient.h"
#include "SettingsWindow.h"
#include "InfoWindow.h"
#include "ChatWindow.h"
#include "PlantComparisonWindow.h"

#include <QFileDialog>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QMessageBox>
#include <QPixmap>
#include <QFile>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QCryptographicHash>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <type_traits>
#include <QStandardPaths>
#include <QDateTime>
#include <QMap>

// ---------------------- 工具函数：模型/资源/图片 ----------------------
static bool checkModelFile(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        qWarning() << "[MODEL] 文件不存在:" << path;
        return false;
    }
    qDebug() << "[MODEL] 绝对路径:" << fi.absoluteFilePath();
    qDebug() << "[MODEL] 大小 (bytes):" << fi.size()
             << " 修改时间:" << fi.lastModified().toString(Qt::ISODate);

    QFile f(fi.absoluteFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[MODEL] 无法打开 (权限/占用):" << path;
        return false;
    }
    // 为了速度，只取前 4 MiB 计算 SHA1
    QByteArray chunk = f.read(4 * 1024 * 1024);
    QByteArray sha1 = QCryptographicHash::hash(chunk, QCryptographicHash::Sha1).toHex();
    qDebug() << "[MODEL] 前 4MiB-SHA1:" << sha1;
    if (fi.size() < 1024 * 100) { // <100KB 基本不可能是正确的分类模型
        qWarning() << "[MODEL] 文件体积异常过小，疑似损坏/外部权重缺失";
        return false;
    }
    return true;
}

static QString resolveAsset(const QString& relPath) {
    // 打印当前工作目录与程序目录，帮助定位
    qDebug() << "[ASSET] QDir::currentPath()=" << QDir::currentPath();
    qDebug() << "[ASSET] appDir           =" << QCoreApplication::applicationDirPath();

    QStringList candidates;
    const QString appDir = QCoreApplication::applicationDirPath();

    // 常见的几种运行布局
    candidates << relPath;
    candidates << appDir + "/" + relPath;
    candidates << appDir + "/resources/" + relPath;      // 添加 resources 前缀
    candidates << appDir + "/../" + relPath;
    candidates << appDir + "/../../" + relPath;
    candidates << appDir + "/../../../" + relPath;

    for (const QString& c : candidates) {
        QFileInfo fi(c);
        qDebug() << "[ASSET] try:" << QDir::toNativeSeparators(fi.absoluteFilePath());
        if (fi.exists() && fi.isFile()) {
            qDebug() << "[ASSET] hit:" << QDir::toNativeSeparators(fi.absoluteFilePath());
            return fi.absoluteFilePath();
        }
    }
    return QString(); // 失败
}

// OpenCV
#include <opencv2/opencv.hpp>

// 将 QString 路径安全读成 cv::Mat（支持中文/特殊字符路径）
static cv::Mat imreadQString(const QString& path, int flags = cv::IMREAD_COLOR) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray data = f.readAll();
    std::vector<uchar> buf(data.begin(), data.end());
    return cv::imdecode(buf, flags);
}

// 创建圆角图片
static QPixmap createRoundedPixmap(const QPixmap& source, int radius) {
    if (source.isNull()) return QPixmap();

    QPixmap rounded(source.size());
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(rounded.rect(), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, source);

    return rounded;
}

// 动态调整标签宽度
static void adjustLabelWidth(QLabel* label, const QString& text, int minWidth = 80, int maxWidth = 500) {
    if (!label) return;

    QFont font = label->font();
    QFontMetrics fm(font);

    int textWidth = fm.horizontalAdvance(text) + 20;
    int newWidth = qMax(minWidth, qMin(maxWidth, textWidth));

    QRect currentRect = label->geometry();
    label->setGeometry(currentRect.x(), currentRect.y(), newWidth, currentRect.height());

    qDebug() << "[LABEL] 调整" << label->objectName() << "宽度：文本=" << text
             << " 计算宽度=" << textWidth << " 最终宽度=" << newWidth;
}

// ---------------------- DeepSeek 客户端全局指针 ----------------------
static DeepseekClient* g_deepseek = nullptr;

// ---------------------- MainWindow ----------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_loadingOverlay(nullptr)
    , m_loadingLabel(nullptr)
    , m_thumbnailLabel(nullptr)
{
    ui->setupUi(this);

    ui->label->setScaledContents(true);
    // 默认使用植物识别背景
    this->setStyleSheet("QMainWindow { background-image: url(:/images/background_plant.jpg); background-repeat: no-repeat; background-position: center; }");

    // 创建缩略图预览标签
    m_thumbnailLabel = new QLabel(this);
    m_thumbnailLabel->setGeometry(380, 420, 100, 75);
    m_thumbnailLabel->setStyleSheet(R"(
        QLabel {
            background-color: rgba(255, 255, 255, 200);
            border: 3px solid rgb(84, 130, 53);
            border-radius: 8px;
        }
    )");
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setVisible(false);
    m_thumbnailLabel->raise();

    // 设置按钮图标
    QPixmap openIcon(":/images/openlogo.png");
    if (!openIcon.isNull()) {
        ui->pushButton->setIcon(QIcon(openIcon));
        ui->pushButton->setIconSize(QSize(24, 24));
    }

    // 初始化植物识别按钮为禁用状态
    updatePlantIdentifyButton();

    // 初始化 label_6 到 label_10
    QLabel* initLabels[5] = { ui->label_6, ui->label_7, ui->label_8, ui->label_9, ui->label_10 };
    for (int i = 0; i < 5; ++i) {
        if (initLabels[i]) {
            initLabels[i]->clear();
            initLabels[i]->setText("");
            QRect currentRect = initLabels[i]->geometry();
            initLabels[i]->setGeometry(currentRect.x(), currentRect.y(), 80, currentRect.height());
        }
    }

    // 初始化进度条
    QProgressBar* initBars[5] = { ui->progressBar_6, ui->progressBar_7, ui->progressBar_8, ui->progressBar_9, ui->progressBar_10 };
    for (int i = 0; i < 5; ++i) {
        if (initBars[i]) {
            initBars[i]->setValue(0);
            initBars[i]->setFormat("");
            initBars[i]->setVisible(false);
        }
    }

    if (ui->label_26) {
        ui->label_26->setVisible(true);
    }

    // 初始化数据库管理器
    m_dbManager = new DatabaseManager(this);
    qDebug() << "[DEBUG] 开始初始化数据库...";
    if (!m_dbManager->initializeDatabase()) {
        qWarning() << "[Database] 数据库初始化失败";
    } else {
        qDebug() << "[Database] 数据库初始化成功";
    }
    qDebug() << "[DEBUG] m_dbManager 指针:" << m_dbManager;

    // 初始化各窗口
    m_historyWindow = nullptr;
    m_settingsWindow = nullptr;
    m_infoWindow = nullptr;
    m_chatWindow = nullptr;
    m_cameraWindow = nullptr;
    m_comparisonWindow = nullptr;

    m_autoSaveEnabled = true;
    m_currentRecordId = -1;
    m_tempPlantName.clear();

    // 初始化病害检测模式
    isDiseaseMode_ = false;

    // 让百科/养护/搭配三个 Label 更易读
    for (QTextEdit* lb : {ui->label_2, ui->label_3, ui->label_4}) {
        if (!lb) continue;
        lb->setReadOnly(true);
        lb->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        lb->setAcceptRichText(true);
        lb->clear();
    }

    // -------- DeepSeek 客户端 --------
    if (!g_deepseek) {
        g_deepseek = new DeepseekClient(this);
        // 从环境变量 DEEPSEEK_API_KEY 读取密钥（勿将真实密钥写入代码）
        const QString deepseekKey = QString::fromUtf8(qgetenv("DEEPSEEK_API_KEY"));
        g_deepseek->setApiKey(deepseekKey);
        deepseek_.setApiKey(deepseekKey);
        qDebug() << "[API] DeepSeek API 密钥已设置:" << (deepseekKey.isEmpty() ? "未配置(仅影响AI问答)" : "已加载");

        connect(g_deepseek, &DeepseekClient::infoReady, this, [this](const PlantInfo& info){
            if (ui->label_2) ui->label_2->setText(info.encyclopedia.toHtmlEscaped());
            if (ui->label_3) ui->label_3->setText(info.cultivation.toHtmlEscaped());
            if (ui->label_4) ui->label_4->setText(info.decor.toHtmlEscaped());
        });

        connect(g_deepseek, &DeepseekClient::errorOccured, this, [this](const QString& err){
            if (ui->label_2) ui->label_2->setText(err.toHtmlEscaped());
        });
    }
}

MainWindow::~MainWindow()
{
    if (m_historyWindow) delete m_historyWindow;
    if (m_settingsWindow) delete m_settingsWindow;
    if (m_infoWindow) delete m_infoWindow;
    if (m_chatWindow) delete m_chatWindow;
    if (m_comparisonWindow) delete m_comparisonWindow;
    if (m_loadingOverlay) delete m_loadingOverlay;
    if (m_thumbnailLabel) delete m_thumbnailLabel;
    delete ui;
}

// ---------------------- 用户体验优化功能 ----------------------

// 显示图片预览（缩略图）
void MainWindow::showImagePreview(const QString& imagePath)
{
    if (!m_thumbnailLabel) return;

    QPixmap pix(imagePath);
    if (pix.isNull()) {
        m_thumbnailLabel->setVisible(false);
        return;
    }

    // 缩放为缩略图尺寸
    QPixmap scaledPix = pix.scaled(120, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_thumbnailLabel->setPixmap(scaledPix);
    m_thumbnailLabel->setVisible(true);
    m_thumbnailLabel->setToolTip("已加载图片：" + imagePath);
}

// 显示加载覆盖层
void MainWindow::showLoadingOverlay(const QString& message)
{
    if (!m_loadingOverlay) {
        // 创建加载覆盖层
        m_loadingOverlay = new QWidget(this);
        m_loadingOverlay->setGeometry(this->rect());
        m_loadingOverlay->setStyleSheet(R"(
            QWidget {
                background-color: rgba(0, 0, 0, 150);
            }
        )");

        QVBoxLayout* layout = new QVBoxLayout(m_loadingOverlay);
        layout->setAlignment(Qt::AlignCenter);

        // 创建加载标签
        m_loadingLabel = new QLabel(message);
        m_loadingLabel->setAlignment(Qt::AlignCenter);
        m_loadingLabel->setStyleSheet(R"(
            QLabel {
                color: white;
                font-size: 18px;
                font-weight: bold;
                padding: 20px;
                background-color: rgba(50, 50, 50, 200);
                border-radius: 15px;
            }
        )");

        layout->addWidget(m_loadingLabel);

        // 创建加载动画标签
        QLabel* loadingIcon = new QLabel("⏳ 识别处理中...");
        loadingIcon->setAlignment(Qt::AlignCenter);
        loadingIcon->setStyleSheet(R"(
            QLabel {
                color: rgb(255, 200, 100);
                font-size: 16px;
                padding: 10px;
            }
        )");
        layout->addWidget(loadingIcon);
    } else {
        if (m_loadingLabel) {
            m_loadingLabel->setText(message);
        }
    }

    m_loadingOverlay->raise();
    m_loadingOverlay->show();
    qApp->processEvents(); // 处理事件，让 UI 立即更新
}

// 隐藏加载覆盖层
void MainWindow::hideLoadingOverlay()
{
    if (m_loadingOverlay) {
        m_loadingOverlay->hide();
    }
}

void MainWindow::updatePlantIdentifyButton()
{
    if (!ui->pushButton_2) return;

    if (lastImagePath_.isEmpty()) {
        ui->pushButton_2->setEnabled(false);
        ui->pushButton_2->setStyleSheet(
            "QPushButton {"
            "    background-color: rgb(200, 200, 200);"
            "    color: rgb(128, 128, 128);"
            "    border: none; border-radius: 8px; padding: 8px;"
            "}"
        );
    } else {
        ui->pushButton_2->setEnabled(true);
        ui->pushButton_2->setStyleSheet(
            "QPushButton {"
            "    background-color: rgb(204, 204, 0);"
            "    color: rgb(0, 0, 0);"
            "    border: none; border-radius: 8px; padding: 8px;"
            "}"
            "QPushButton:hover { background-color: rgb(224, 224, 0); }"
            "QPushButton:pressed { background-color: rgb(184, 184, 0); }"
        );
    }
}

// 显示植物识别结果
void MainWindow::displayPlantResults(const std::vector<PlantResult>& results, double maxConfidence)
{
    QLabel* labels[5] = { ui->label_6, ui->label_7, ui->label_8, ui->label_9, ui->label_10 };
    QProgressBar* bars[5] = {
        findChild<QProgressBar*>("progressBar_6"),
        findChild<QProgressBar*>("progressBar_7"),
        findChild<QProgressBar*>("progressBar_8"),
        findChild<QProgressBar*>("progressBar_9"),
        findChild<QProgressBar*>("progressBar_10")
    };

    for (int k = 0; k < 5; ++k) {
        if (bars[k]) {
            bars[k]->setRange(0, 100);
            bars[k]->setFormat("%p%");
            bars[k]->setVisible(true);
        }
    }

    int i = 0;
    for (const auto& r : results) {
        if (i >= 5) break;

        const QString name = QString::fromStdString(r.name);
        const double clamped = std::max(0.0, std::min(1.0, static_cast<double>(r.score)));
        const int percent = static_cast<int>(std::lround(clamped * 100.0));

        if (labels[i]) {
            labels[i]->setText(name);
            labels[i]->setToolTip(QString("%1\n置信度：%2").arg(name).arg(QString::number(r.score, 'f', 4)));
            adjustLabelWidth(labels[i], name, 80, 500);
        }
        if (bars[i]) {
            bars[i]->setValue(percent);
            // 两位小数置信度：避免 99.98% 被取整成 100%、0.02% 取整成 0% 的失真显示
            bars[i]->setFormat(QString::number(clamped * 100.0, 'f', 2) + "%");
            bars[i]->setToolTip(QString("置信度：%1").arg(QString::number(r.score, 'f', 4)));
        }
        ++i;
    }

    // 根据最高置信度调整样式
    if (maxConfidence >= 0.3 && labels[0]) {
        labels[0]->setStyleSheet(
            "QLabel { background-color: rgb(224, 224, 0); color: rgb(0, 0, 0); border: none; border-radius: 8px; padding: 5px; }"
        );
        if (bars[0]) {
            bars[0]->setStyleSheet(
                "QProgressBar { border: none; border-radius: 8px; text-align: left; background-color: rgb(240, 240, 240); }"
                "QProgressBar::chunk { border-radius: 8px; background-color: rgb(224, 224, 0); }"
            );
        }
    } else if (maxConfidence < 0.3) {
        QMessageBox::warning(this, tr("警告"), tr("识别置信度均低于 30%，识别可能有误或图片异常！"));
    }

    // 清空剩余槽位
    for (; i < 5; ++i) {
        if (labels[i]) {
            labels[i]->clear();
            labels[i]->setStyleSheet(
                "QLabel { background-color: rgb(84, 130, 53); color: rgb(255, 255, 255); border: none; border-radius: 8px; padding: 5px; }"
            );
        }
        if (bars[i]) {
            bars[i]->setValue(0);
            bars[i]->setVisible(false);
            bars[i]->setStyleSheet(
                "QProgressBar { border: none; border-radius: 8px; text-align: left; background-color: rgb(240, 240, 240); }"
                "QProgressBar::chunk { border-radius: 8px; background-color: rgb(84, 130, 53); }"
            );
        }
    }
}

// 显示病害检测结果
void MainWindow::displayDiseaseResults(const std::vector<PlantDiseaseResult>& results)
{
    qDebug() << "[DEBUG] displayDiseaseResults 被调用，结果数量:" << results.size();
    qDebug() << "[DEBUG] diseaseDetector_ 指针:" << diseaseDetector_.get();

    if (results.empty()) {
        if (ui->label_2) ui->label_2->setText("无病害信息");
        if (ui->label_3) ui->label_3->setText("无");
        if (ui->label_4) ui->label_4->setText("未知");
    } else {
        // 获取 Top-1 结果
        const auto& topResult = results.front();
        const QString diseaseName = QString::fromStdString(topResult.name);
        const double confidence = topResult.score;

        qDebug() << "[DEBUG] Top-1 结果:" << diseaseName << ", confidence:" << confidence;

        // 获取病害详细信息（从 JSON 数据集加载）
        QString diagnosis, severityIntro, treatment;

        if (diseaseDetector_) {
            qDebug() << "[DEBUG] 调用 getDiseaseInfoByName...";
            try {
                DiseaseInfo info = diseaseDetector_->getDiseaseInfoByName(topResult.name);
                if (!info.diagnosis.empty()) {
                    diagnosis = QString::fromUtf8(info.diagnosis.c_str());
                    severityIntro = QString::fromUtf8(info.severity_intro.c_str());
                    treatment = QString::fromUtf8(info.treatment.c_str());
                    qDebug() << "[DEBUG] 病害信息加载成功:" << diagnosis.left(50);
                } else {
                    qDebug() << "[DEBUG] 病害信息为空，使用默认信息";
                    diagnosis = "暂无详细病害信息";
                    severityIntro = "根据置信度判断：≥80% 为重度，50-80% 为中度，<50% 为轻度";
                    treatment = "请咨询当地农业技术部门获取专业防治建议";
                }
            } catch (const std::exception& e) {
                qWarning() << "[DEBUG] 获取病害信息异常:" << e.what();
                diagnosis = "病害诊断信息加载中...";
                severityIntro = "程度说明加载中...";
                treatment = "防治建议加载中...";
            } catch (...) {
                qWarning() << "[DEBUG] 获取病害信息未知异常";
                diagnosis = "病害诊断信息加载中...";
                severityIntro = "程度说明加载中...";
                treatment = "防治建议加载中...";
            }
        } else {
            qWarning() << "[DEBUG] diseaseDetector_ 为空指针";
            diagnosis = "病害诊断信息加载中...";
            severityIntro = "程度说明加载中...";
            treatment = "防治建议加载中...";
        }

        // 病害程度分级（基于置信度；健康类别不参与"发生程度"分级）
        QString severityLevel;
        const bool topIsHealthy = diseaseDetector_ && diseaseDetector_->isHealthy(topResult.index);
        if (topIsHealthy) {
            severityLevel = "健康植株";
        } else if (confidence >= 0.8) {
            severityLevel = "重度发生";
        } else if (confidence >= 0.5) {
            severityLevel = "中度发生";
        } else {
            severityLevel = "轻度发生";
        }

        qDebug() << "[DEBUG] 设置 UI 内容...";
        // label_2: 病害诊断（病情介绍）
        if (ui->label_2) {
            ui->label_2->setText(diagnosis);
        }

        // label_3: 病害程度
        if (ui->label_3) {
            QString severityText = QString("%1\n%2").arg(severityLevel).arg(severityIntro);
            ui->label_3->setText(severityText);
        }

        // label_4: 防治建议
        if (ui->label_4) {
            ui->label_4->setText(treatment);
        }
        qDebug() << "[DEBUG] UI 内容设置完成";

        // 保存历史记录
        qDebug() << "[DEBUG] 病害检测：准备保存历史记录，病害名称:" << diseaseName << ", 置信度:" << topResult.score;
        qDebug() << "[DEBUG] m_autoSaveEnabled:" << m_autoSaveEnabled << ", m_dbManager:" << (m_dbManager != nullptr) << ", lastImagePath_:" << lastImagePath_;

        if (m_autoSaveEnabled && m_dbManager && !lastImagePath_.isEmpty()) {
            QString imageFilename = m_dbManager->saveImageFile(lastImagePath_);
            qDebug() << "[DEBUG] 图片保存结果:" << imageFilename;
            if (!imageFilename.isEmpty()) {
                HistoryRecord record(imageFilename, diseaseName, topResult.score);
                m_currentRecordId = m_dbManager->addHistoryRecord(record);
                qDebug() << "[DEBUG] 历史记录 ID:" << m_currentRecordId;
            }
        } else {
            qDebug() << "[DEBUG] 未满足保存条件";
        }
    }

    QLabel* labels[5] = { ui->label_6, ui->label_7, ui->label_8, ui->label_9, ui->label_10 };
    QProgressBar* bars[5] = {
        findChild<QProgressBar*>("progressBar_6"),
        findChild<QProgressBar*>("progressBar_7"),
        findChild<QProgressBar*>("progressBar_8"),
        findChild<QProgressBar*>("progressBar_9"),
        findChild<QProgressBar*>("progressBar_10")
    };

    for (int k = 0; k < 5; ++k) {
        if (bars[k]) {
            bars[k]->setRange(0, 100);
            bars[k]->setFormat("%p%");
            bars[k]->setVisible(true);
        }
    }

    int i = 0;
    double maxConfidence = 0.0;
    for (const auto& r : results) {
        if (i >= 5) break;

        const QString name = QString::fromStdString(r.name);
        const double clamped = std::max(0.0, std::min(1.0, static_cast<double>(r.score)));
        const int percent = static_cast<int>(std::lround(clamped * 100.0));

        if (i == 0) maxConfidence = r.score;

        if (labels[i]) {
            labels[i]->setText(name);
            labels[i]->setToolTip(QString("%1\n置信度：%2").arg(name).arg(QString::number(r.score, 'f', 4)));
            adjustLabelWidth(labels[i], name, 80, 500);
        }
        if (bars[i]) {
            bars[i]->setValue(percent);
            // 两位小数置信度：避免取整失真（100%/0% 假象）
            bars[i]->setFormat(QString::number(clamped * 100.0, 'f', 2) + "%");
        }
        ++i;
    }

    // 根据最高置信度调整样式（病害检测模式使用橙红色主题）
    if (maxConfidence >= 0.3 && labels[0]) {
        labels[0]->setStyleSheet(
            "QLabel { background-color: rgb(255, 140, 0); color: rgb(255, 255, 255); border: none; border-radius: 8px; padding: 5px; }"
        );
        if (bars[0]) {
            bars[0]->setStyleSheet(
                "QProgressBar { border: none; border-radius: 8px; text-align: left; background-color: rgb(240, 240, 240); }"
                "QProgressBar::chunk { border-radius: 8px; background-color: rgb(255, 140, 0); }"
            );
        }
    } else if (maxConfidence < 0.3) {
        QMessageBox::warning(this, tr("警告"), tr("识别置信度均低于 30%，识别可能有误或图片异常！"));
    }

    // 清空剩余槽位
    for (; i < 5; ++i) {
        if (labels[i]) {
            labels[i]->clear();
            labels[i]->setStyleSheet(
                "QLabel { background-color: rgb(180, 50, 50); color: rgb(255, 255, 255); border: none; border-radius: 8px; padding: 5px; }"
            );
        }
        if (bars[i]) {
            bars[i]->setValue(0);
            bars[i]->setVisible(false);
        }
    }
}

// 获取病害防治建议
QString MainWindow::getDiseaseAdvice(const QString& diseaseName)
{
    QString advice;

    // 按病害类型提供防治建议
    if (diseaseName.contains("healthy", Qt::CaseInsensitive) || diseaseName.contains("健康", Qt::CaseInsensitive)) {
        advice = "植物生长健康，无需特殊防治。\n\n建议：\n• 定期检查叶片状态\n• 保持适当浇水和施肥\n• 注意通风透光\n• 及时清理枯叶";
    }
    else if (diseaseName.contains("scab", Qt::CaseInsensitive) || diseaseName.contains("疮痂", Qt::CaseInsensitive)) {
        advice = "【苹果疮痂病防治】\n\n农业防治：\n• 清除落叶落果，减少菌源\n• 合理修剪，改善通风\n• 选用抗病品种\n\n化学防治：\n• 萌芽期喷施石硫合剂\n• 花后定期喷施杀菌剂\n• 交替使用不同药剂";
    }
    else if (diseaseName.contains("black rot", Qt::CaseInsensitive) || diseaseName.contains("黑腐", Qt::CaseInsensitive)) {
        advice = "【黑腐病防治】\n\n农业防治：\n• 剪除病枝病果\n• 改善通风透光\n• 避免伤口产生\n\n化学防治：\n• 发芽前喷石硫合剂\n• 生长期喷多菌灵\n• 雨后及时补喷";
    }
    else if (diseaseName.contains("rust", Qt::CaseInsensitive) || diseaseName.contains("锈病", Qt::CaseInsensitive)) {
        advice = "【锈病防治】\n\n农业防治：\n• 清除转主寄主\n• 合理施肥增强树势\n• 及时排水降湿\n\n化学防治：\n• 发病初期喷粉锈宁\n• 喷施三唑酮\n• 间隔 10-15 天连喷 2-3 次";
    }
    else if (diseaseName.contains("powdery mildew", Qt::CaseInsensitive) || diseaseName.contains("白粉", Qt::CaseInsensitive)) {
        advice = "【白粉病防治】\n\n农业防治：\n• 加强通风透光\n• 控制氮肥，增施磷钾肥\n• 及时摘除病叶\n\n化学防治：\n• 发病初期喷三唑酮\n• 喷施多菌灵或甲基托布津\n• 间隔 7-10 天，连喷 2-3 次";
    }
    else if (diseaseName.contains("leaf spot", Qt::CaseInsensitive) || diseaseName.contains("叶斑", Qt::CaseInsensitive) ||
             diseaseName.contains("Cercospora", Qt::CaseInsensitive) || diseaseName.contains("尾孢", Qt::CaseInsensitive)) {
        advice = "【叶斑病防治】\n\n农业防治：\n• 清除病残体\n• 合理密植，改善通风\n• 轮作倒茬\n\n化学防治：\n• 发病初期喷代森锰锌\n• 喷施百菌清或多菌灵\n• 间隔 7-10 天连喷";
    }
    else if (diseaseName.contains("blight", Qt::CaseInsensitive) || diseaseName.contains("疫病", Qt::CaseInsensitive) ||
             diseaseName.contains("Late blight", Qt::CaseInsensitive) || diseaseName.contains("早疫", Qt::CaseInsensitive)) {
        advice = "【疫病防治】\n\n农业防治：\n• 选用抗病品种\n• 高畦栽培，注意排水\n• 避免偏施氮肥\n\n化学防治：\n• 发病初期喷甲霜灵\n• 喷施瑞毒霉或杀毒矾\n• 间隔 5-7 天连喷 2-3 次";
    }
    else if (diseaseName.contains("bacterial spot", Qt::CaseInsensitive) || diseaseName.contains("细菌性斑点", Qt::CaseInsensitive)) {
        advice = "【细菌性斑点病防治】\n\n农业防治：\n• 选用无病种子\n• 轮作栽培\n• 避免高温高湿\n\n化学防治：\n• 喷施农用链霉素\n• 喷施氢氧化铜\n• 间隔 7-10 天连喷";
    }
    else if (diseaseName.contains("mosaic", Qt::CaseInsensitive) || diseaseName.contains("花叶病毒", Qt::CaseInsensitive)) {
        advice = "【病毒病防治】\n\n农业防治：\n• 选用抗病品种\n• 及时拔除病株\n• 防治蚜虫传播\n\n化学防治：\n• 喷施病毒 A\n• 喷施菌毒清\n• 加强水肥管理";
    }
    else if (diseaseName.contains("Leaf Mold", Qt::CaseInsensitive) || diseaseName.contains("叶霉", Qt::CaseInsensitive)) {
        advice = "【叶霉病防治】\n\n农业防治：\n• 加强通风排湿\n• 控制浇水\n• 及时摘除老叶\n\n化学防治：\n• 喷施多菌灵\n• 喷施甲基托布津\n• 间隔 7 天连喷 2-3 次";
    }
    else if (diseaseName.contains("Septoria", Qt::CaseInsensitive) || diseaseName.contains("斑枯", Qt::CaseInsensitive)) {
        advice = "【斑枯病防治】\n\n农业防治：\n• 清除病残体\n• 轮作栽培\n• 合理密植\n\n化学防治：\n• 喷施代森锰锌\n• 喷施百菌清\n• 间隔 7-10 天连喷";
    }
    else if (diseaseName.contains("Target Spot", Qt::CaseInsensitive) || diseaseName.contains("靶斑", Qt::CaseInsensitive)) {
        advice = "【靶斑病防治】\n\n农业防治：\n• 选用抗病品种\n• 高畦栽培\n• 避免连作\n\n化学防治：\n• 喷施阿米西达\n• 喷施世高或福戈\n• 间隔 7 天连喷";
    }
    else if (diseaseName.contains("Yellow Leaf Curl", Qt::CaseInsensitive) || diseaseName.contains("黄化曲叶", Qt::CaseInsensitive)) {
        advice = "【黄化曲叶病毒病防治】\n\n农业防治：\n• 选用抗病品种\n• 防治烟粉虱\n• 及时拔除病株\n\n化学防治：\n• 喷施吡虫啉防治传毒介体\n• 喷施病毒 A\n• 加强水肥管理";
    }
    else if (diseaseName.contains("Spider mites", Qt::CaseInsensitive) || diseaseName.contains("红蜘蛛", Qt::CaseInsensitive)) {
        advice = "【红蜘蛛防治】\n\n农业防治：\n• 及时清除杂草\n• 合理浇水增加湿度\n• 人工捕杀\n\n化学防治：\n• 喷施阿维菌素\n• 喷施哒螨灵\n• 间隔 5-7 天连喷 2-3 次";
    }
    else if (diseaseName.contains("Haunglongbing", Qt::CaseInsensitive) || diseaseName.contains("黄龙病", Qt::CaseInsensitive) ||
             diseaseName.contains("Citrus greening", Qt::CaseInsensitive)) {
        advice = "【黄龙病防治】⚠️\n\n⚠️ 柑橘癌症，目前无特效药！\n\n防治措施：\n• 严格检疫，防止扩散\n• 及时挖除病树\n• 防治木虱传播\n• 种植无病苗木";
    }
    else if (diseaseName.contains("Esca", Qt::CaseInsensitive) || diseaseName.contains("黑麻疹", Qt::CaseInsensitive)) {
        advice = "【葡萄黑麻疹病防治】\n\n农业防治：\n• 修剪时减少伤口\n• 伤口涂抹保护剂\n• 清除病株\n\n化学防治：\n• 休眠期喷石硫合剂\n• 生长期喷代森锰锌";
    }
    else if (diseaseName.contains("Northern Leaf Blight", Qt::CaseInsensitive) || diseaseName.contains("北方叶枯", Qt::CaseInsensitive)) {
        advice = "【玉米北方叶枯病防治】\n\n农业防治：\n• 选用抗病品种\n• 轮作倒茬\n• 清除病残体\n\n化学防治：\n• 喷施多菌灵\n• 喷施甲基托布津\n• 间隔 7 天连喷 2-3 次";
    }
    else if (diseaseName.contains("Common Rust", Qt::CaseInsensitive) || diseaseName.contains("普通锈病", Qt::CaseInsensitive)) {
        advice = "【玉米普通锈病防治】\n\n农业防治：\n• 选用抗病品种\n• 合理密植\n• 及时排水\n\n化学防治：\n• 喷施粉锈宁\n• 喷施三唑酮\n• 间隔 10 天连喷 2 次";
    }
    else if (diseaseName.contains("scorch", Qt::CaseInsensitive) || diseaseName.contains("叶焦", Qt::CaseInsensitive)) {
        advice = "【叶焦病防治】\n\n农业防治：\n• 合理施肥浇水\n• 改善通风透光\n• 清除病叶\n\n化学防治：\n• 喷施多菌灵\n• 喷施百菌清\n• 间隔 7-10 天连喷";
    }
    else {
        advice = "【一般防治建议】\n\n• 加强田间管理\n• 改善通风透光\n• 合理施肥浇水\n• 及时清除病残体\n• 必要时喷施广谱杀菌剂";
    }

    return advice;
}

void MainWindow::on_pushButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("打开图片"), "", tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"));
    if (fileName.isEmpty()) return;

    lastImagePath_ = fileName;

    QPixmap pix(fileName);
    if (pix.isNull()) {
        QMessageBox::warning(this, tr("错误"), tr("无法加载图片！"));
        return;
    }

    QPixmap scaledPix = pix.scaled(ui->label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap roundedPix = createRoundedPixmap(scaledPix, 20);
    ui->label->setPixmap(roundedPix);

    // 显示缩略图预览
    showImagePreview(fileName);

    if (ui->label_26) ui->label_26->setVisible(false);
    updatePlantIdentifyButton();

    // 重置识别结果
    QLabel* resetLabels[5] = { ui->label_6, ui->label_7, ui->label_8, ui->label_9, ui->label_10 };
    QProgressBar* resetBars[5] = { ui->progressBar_6, ui->progressBar_7, ui->progressBar_8, ui->progressBar_9, ui->progressBar_10 };
    for (int i = 0; i < 5; ++i) {
        if (resetLabels[i]) { resetLabels[i]->clear(); resetLabels[i]->setText(""); }
        if (resetBars[i]) { resetBars[i]->setValue(0); resetBars[i]->setFormat(""); resetBars[i]->setVisible(false); }
    }
}

void MainWindow::on_pushButton_2_clicked()
{
    if (!ui->pushButton_2) return;

    // 检查图片是否已选择
    if (lastImagePath_.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择要识别的图片！\n\n点击'打开图片'按钮导入图片文件。"));
        return;
    }

    // 重置样式
    if (ui->label_6) {
        ui->label_6->setStyleSheet("QLabel { background-color: rgb(84, 130, 53); color: rgb(255, 255, 255); border: none; border-radius: 8px; padding: 5px; }");
    }
    if (ui->progressBar_6) {
        ui->progressBar_6->setStyleSheet("QProgressBar { border: none; border-radius: 8px; text-align: left; background-color: rgb(240, 240, 240); } QProgressBar::chunk { border-radius: 8px; background-color: rgb(84, 130, 53); }");
    }

    // 显示加载动画
    QString modeText = isDiseaseMode_ ? "病害" : "植物";
    showLoadingOverlay(QString("正在进行%1识别...\n请稍候").arg(modeText));

    qDebug() << "[DEBUG] 打开图片路径:" << lastImagePath_;
    cv::Mat img = imreadQString(lastImagePath_, cv::IMREAD_COLOR);
    if (img.empty()) {
        hideLoadingOverlay();
        QMessageBox::warning(this, tr("错误"), tr("读取图片失败！\n\n请确认图片文件未损坏，格式正确。"));
        return;
    }
    qDebug() << "[DEBUG] 图像读取成功，尺寸:" << img.cols << "x" << img.rows;

    try {
        if (isDiseaseMode_) {
            // 病害检测模式 - 使用病害模型
            const QString diseaseModelAbs = resolveAsset("assets/models/plant_disease.onnx");
            const QString diseaseLabelAbs = resolveAsset("assets/models/plant_disease_labels.txt");

            if (!checkModelFile(diseaseModelAbs)) {
                QMessageBox::critical(this, tr("错误"), tr("病害模型文件异常：%1").arg(diseaseModelAbs));
                return;
            }
            if (diseaseModelAbs.isEmpty() || diseaseLabelAbs.isEmpty()) {
                QMessageBox::critical(this, tr("错误"), tr("病害模型或标签文件未找到。"));
                return;
            }

            // 初始化病害检测器
            if (!diseaseDetector_) {
                qDebug() << "[DEBUG] 创建 diseaseDetector_ 实例...";
                diseaseDetector_ = std::make_unique<PlantDisease>(
                    diseaseModelAbs.toUtf8().constData(),
                    diseaseLabelAbs.toUtf8().constData()
                );
                qDebug() << "[DEBUG] diseaseDetector_ 创建完成，指针:" << diseaseDetector_.get();
            }

            // 步骤 1: 先用植物识别模型判断植物种类
            const QString plantModelAbs = resolveAsset("assets/models/quarrying_plantid_model.onnx");
            const QString plantLabelAbs = resolveAsset("assets/models/quarrying_plantid_label_map.txt");

            static std::unique_ptr<PlantId> pid;
            if (!pid) {
                pid = std::make_unique<PlantId>(
                    plantModelAbs.toUtf8().constData(),
                    plantLabelAbs.toUtf8().constData()
                );
            }

            auto plantResults = pid->infer(img, 3);
            QString detectedPlant;
            double plantConfidence = plantResults.empty() ? 0 : plantResults.front().score;

            if (!plantResults.empty()) {
                detectedPlant = QString::fromStdString(plantResults.front().name);
                qDebug() << "[DEBUG] 植物识别结果:" << detectedPlant << "置信度:" << plantConfidence;
            }

            // 步骤 2: 进行病害检测
            auto diseaseResults = diseaseDetector_->infer(img, 5);
            qDebug() << "[DEBUG] 病害推理完成，返回结果数量:" << diseaseResults.size();

            if (diseaseResults.empty()) {
                hideLoadingOverlay();
                QMessageBox::warning(this, tr("无法识别病害"), tr("模型未返回任何识别结果。"));
                if (ui->label_2) ui->label_2->setText("无病害信息");
                if (ui->label_3) ui->label_3->setText("无");
                if (ui->label_4) ui->label_4->setText("未知");
                return;
            }

            // 获取 Top 结果
            const auto& topResult = diseaseResults.front();
            double diseaseConfidence = topResult.score;

            // 步骤 3: 提取植物识别结果中的植物属/种信息
            // 格式："蔷薇科_苹果属_苹果" → 提取"苹果"或"苹果属"
            QString plantSpecies;
            if (detectedPlant.contains("_")) {
                QStringList parts = detectedPlant.split("_");
                if (parts.size() >= 3) {
                    plantSpecies = parts[2]; // 种名
                } else if (parts.size() == 2) {
                    plantSpecies = parts[1]; // 属名
                }
            }
            qDebug() << "[DEBUG] 识别到的植物种/属:" << plantSpecies;

            // 步骤 4: 提取病害结果中的植物名
            // 格式："Apple___Apple_scab" → "Apple"
            QString diseasePlantLatin = QString::fromStdString(diseaseDetector_->getPlantName(topResult.index));
            qDebug() << "[DEBUG] 病害模型中的植物 (拉丁):" << diseasePlantLatin;

            // 步骤 5: 植物种类匹配检查
            // 如果植物识别结果明确（置信度>30%），但与病害模型的植物不匹配，则拒绝
            bool plantMismatch = false;
            if (plantConfidence > 0.30 && !plantSpecies.isEmpty() && !diseasePlantLatin.isEmpty()) {
                // 检查植物名是否包含关系（中英文都检查）
                bool isMatch = false;

                // 常见植物中英文名映射
                QMap<QString, QStringList> plantNameMap = {
                    {"Apple", {"苹果", "苹果属"}},
                    {"Tomato", {"番茄", "番茄属"}},
                    {"Potato", {"马铃薯", "土豆"}},
                    {"Pepper", {"辣椒", "甜椒", "辣椒属"}},
                    {"Grape", {"葡萄", "葡萄属"}},
                    {"Corn", {"玉米", "玉蜀黍"}},
                    {"Cherry", {"樱桃", "樱桃属"}},
                    {"Peach", {"桃", "桃属"}},
                    {"Strawberry", {"草莓", "草莓属"}},
                    {"Blueberry", {"蓝莓", "蓝莓属"}},
                    {"Orange", {"柑橘", "柑橘属"}},
                    {"Soybean", {"大豆", "黄豆"}},
                    {"Squash", {"南瓜", "西葫芦"}},
                    {"Cucumber", {"黄瓜", "黄瓜属"}},
                    {"Raspberry", {"树莓", "悬钩子"}},
                    {"Pumpkin", {"南瓜"}},
                };

                // 检查是否匹配
                if (plantNameMap.contains(diseasePlantLatin)) {
                    QStringList chineseNames = plantNameMap[diseasePlantLatin];
                    for (const QString& cn : chineseNames) {
                        if (plantSpecies.contains(cn) || cn.contains(plantSpecies)) {
                            isMatch = true;
                            break;
                        }
                    }
                }

                // 也检查拉丁名
                if (!isMatch && !diseasePlantLatin.isEmpty()) {
                    if (detectedPlant.contains(diseasePlantLatin, Qt::CaseInsensitive)) {
                        isMatch = true;
                    }
                }

                if (!isMatch) {
                    plantMismatch = true;
                }

                qDebug() << "[DEBUG] 植物匹配检查:" << (isMatch ? "匹配" : "不匹配");
            }

            hideLoadingOverlay();

            // 如果植物不匹配，拒绝显示
            if (plantMismatch) {
                QMessageBox::warning(this, tr("识别结果存疑"),
                    QString("检测到的植物与病害模型不匹配。\n\n"
                           "植物识别：%1 (置信度%2)\n"
                           "病害模型识别：%3\n\n"
                           "病害模型仅支持以下植物:\n"
                           "苹果、番茄、马铃薯、辣椒、葡萄、玉米、\n"
                           "樱桃、桃、草莓、蓝莓、柑橘、大豆、\n"
                           "南瓜、树莓、蓝莓、荞麦")
                        .arg(detectedPlant)
                        .arg(QString::number(plantConfidence * 100, 'f', 1))
                        .arg(diseasePlantLatin));
                if (ui->label_2) ui->label_2->setText("无病害信息");
                if (ui->label_3) ui->label_3->setText("无");
                if (ui->label_4) ui->label_4->setText("未知");
                return;
            }

            // 步骤 6: 检查是否是健康状态
            bool isHealthy = diseaseDetector_->isHealthy(topResult.index);
            qDebug() << "[DEBUG] Top 结果:" << QString::fromStdString(topResult.name)
                     << "置信度:" << diseaseConfidence << "健康状态:" << isHealthy;

            // 如果是健康状态且置信度不高，提示健康
            if (isHealthy && diseaseConfidence < 0.5) {
                QMessageBox::information(this, tr("检测结果"),
                    tr("植株健康状况良好，未检测到明显病害。"));
                displayDiseaseResults(diseaseResults);
                return;
            }

            // 如果是病害但置信度不够高，提示不确定
            if (!isHealthy && diseaseConfidence < 0.75) {
                QMessageBox::warning(this, tr("无法确定"),
                    tr("检测到疑似病害，但置信度较低 (%1%)。\n\n"
                       "可能原因：\n"
                       "1. 病害症状不典型\n"
                       "2. 图片质量不佳\n"
                       "3. 该病害不在识别范围内")
                    .arg(QString::number(diseaseConfidence * 100, 'f', 1)));
                if (ui->label_2) ui->label_2->setText("无病害信息");
                if (ui->label_3) ui->label_3->setText("无");
                if (ui->label_4) ui->label_4->setText("未知");
                return;
            }

            // 置信度足够高，显示结果
            displayDiseaseResults(diseaseResults);

            // 如果是健康状态且置信度不高，提示健康
            if (isHealthy && diseaseConfidence < 0.7) {
                QMessageBox::information(this, tr("检测结果"),
                    tr("植株健康状况良好，未检测到明显病害。"));
                displayDiseaseResults(diseaseResults);
                return;
            }

            // 如果是病害但置信度不够高，提示不确定
            if (!isHealthy && diseaseConfidence < 0.85) {
                QMessageBox::warning(this, tr("无法确定"),
                    tr("检测到疑似病害，但置信度较低 (%1%)。\n\n"
                       "可能原因：\n"
                       "1. 病害症状不典型\n"
                       "2. 图片质量不佳\n"
                       "3. 该病害不在识别范围内")
                    .arg(QString::number(diseaseConfidence * 100, 'f', 1)));
                if (ui->label_2) ui->label_2->setText("无病害信息");
                if (ui->label_3) ui->label_3->setText("无");
                if (ui->label_4) ui->label_4->setText("未知");
                return;
            }

            // 置信度足够高，显示结果
            displayDiseaseResults(diseaseResults);

        } else {
            // 植物识别模式
            const QString modelAbs = resolveAsset("assets/models/quarrying_plantid_model.onnx");
            const QString labelAbs = resolveAsset("assets/models/quarrying_plantid_label_map.txt");

            if (!checkModelFile(modelAbs)) {
                QMessageBox::critical(this, tr("错误"), tr("植物模型文件异常：%1").arg(modelAbs));
                return;
            }
            if (modelAbs.isEmpty() || labelAbs.isEmpty()) {
                QMessageBox::critical(this, tr("错误"), tr("植物模型或标签文件未找到。"));
                return;
            }

            static std::unique_ptr<PlantId> pid;
            if (!pid) {
                pid = std::make_unique<PlantId>(
                    modelAbs.toUtf8().constData(),
                    labelAbs.toUtf8().constData()
                );
            }

            auto results = pid->infer(img, 5);
            qDebug() << "[DEBUG] 植物推理完成，返回结果数量:" << results.size();

            // 计算最高置信度
            double maxConfidence = results.empty() ? 0.0 : results.front().score;
            hideLoadingOverlay();
            displayPlantResults(results, maxConfidence);

            // 保存历史记录和 DeepSeek 请求
            if (!results.empty() && maxConfidence >= 0.3) {
                const auto& top = results.front();
                const QString zhName = QString::fromStdString(top.name);

                qDebug() << "[DEBUG] 准备保存历史记录，植物名称:" << zhName << ", 置信度:" << maxConfidence;
                qDebug() << "[DEBUG] m_autoSaveEnabled:" << m_autoSaveEnabled << ", m_dbManager:" << (m_dbManager != nullptr) << ", lastImagePath_:" << lastImagePath_;

                if (m_autoSaveEnabled && m_dbManager && !lastImagePath_.isEmpty()) {
                    QString imageFilename = m_dbManager->saveImageFile(lastImagePath_);
                    qDebug() << "[DEBUG] 图片保存结果:" << imageFilename;
                    if (!imageFilename.isEmpty()) {
                        HistoryRecord record(imageFilename, zhName, top.score);
                        m_currentRecordId = m_dbManager->addHistoryRecord(record);
                        qDebug() << "[DEBUG] 历史记录 ID:" << m_currentRecordId;
                    }
                } else {
                    qDebug() << "[DEBUG] 未满足保存条件";
                }

                if (ui->label_2) ui->label_2->setText(QString("正在生成：%1（置信度 %2）…").arg(zhName.toHtmlEscaped()).arg(QString::number(top.score, 'f', 4)));
                if (ui->label_3) ui->label_3->setText("正在生成…");
                if (ui->label_4) ui->label_4->setText("正在生成…");

                if (g_deepseek) {
                    g_deepseek->fetchPlantInfo(zhName, zhName);
                }
            } else if (!results.empty() && maxConfidence < 0.3) {
                if (ui->label_2) ui->label_2->clear();
                if (ui->label_3) ui->label_3->clear();
                if (ui->label_4) ui->label_4->clear();
            }
        }

    } catch (const std::exception& e) {
        qWarning() << "[ERROR] 推理阶段异常:" << e.what();
        QMessageBox::critical(this, tr("错误"), tr("推理失败：%1").arg(QString::fromLocal8Bit(e.what())));
    }
}

void MainWindow::on_pushButton_3_clicked()
{
    if (!m_infoWindow) m_infoWindow = new InfoWindow(nullptr);
    m_infoWindow->show();
    m_infoWindow->raise();
    m_infoWindow->activateWindow();
}

void MainWindow::on_pushButton_4_clicked()
{
    if (!m_settingsWindow) {
        m_settingsWindow = new SettingsWindow(nullptr);
        connect(m_settingsWindow, &SettingsWindow::autoSaveChanged, this, [this](bool enabled) {
            m_autoSaveEnabled = enabled;
        });
    }
    m_settingsWindow->show();
    m_settingsWindow->raise();
    m_settingsWindow->activateWindow();
}

void MainWindow::on_pushButton_5_clicked()
{
    qDebug() << "[DEBUG] ========== 历史记录按钮调试信息 ==========";
    qDebug() << "[DEBUG] m_dbManager 指针:" << m_dbManager;
    qDebug() << "[DEBUG] m_historyWindow 指针:" << m_historyWindow;
    qDebug() << "[DEBUG] isDiseaseMode_:" << isDiseaseMode_;

    if (!m_historyWindow) {
        qDebug() << "[DEBUG] 创建新的 HistoryWindow";
        m_historyWindow = new HistoryWindow(m_dbManager, nullptr);
        qDebug() << "[DEBUG] HistoryWindow 创建完成，新指针:" << m_historyWindow;
    } else {
        qDebug() << "[DEBUG] 刷新现有 HistoryWindow";
        m_historyWindow->refreshHistory();
    }

    if (m_historyWindow && m_historyWindow->isVisible()) {
        qDebug() << "[DEBUG] HistoryWindow 已可见，调用 raise/activate";
    }

    m_historyWindow->show();
    m_historyWindow->raise();
    m_historyWindow->activateWindow();

    qDebug() << "[DEBUG] ========== 调试信息结束 ==========";
}

void MainWindow::on_pushButton_6_clicked()
{
    qDebug() << "[DEBUG] 收藏按钮被点击，m_currentRecordId =" << m_currentRecordId;

    if (m_currentRecordId <= 0) {
        qDebug() << "[DEBUG] 收藏失败：m_currentRecordId <= 0";
        QMessageBox::warning(this, "提示", "没有可收藏的识别结果！\n请先进行植物识别。");
        return;
    }
    if (!m_dbManager) {
        qDebug() << "[DEBUG] 收藏失败：m_dbManager 为空";
        QMessageBox::warning(this, "错误", "数据库管理器未初始化！");
        return;
    }

    qDebug() << "[DEBUG] 准备收藏记录 ID:" << m_currentRecordId;
    if (m_dbManager->setRecordFavorite(m_currentRecordId, true)) {
        QMessageBox::information(this, "收藏成功", "图片收藏成功！");
    } else {
        QMessageBox::warning(this, "收藏失败", "收藏操作失败，请重试。");
    }
}

void MainWindow::on_pushButton_7_clicked()
{
    if (!m_chatWindow) {
        m_chatWindow = new ChatWindow(nullptr);
        m_chatWindow->setDeepseekClient(&deepseek_);
    }
    m_chatWindow->show();
    m_chatWindow->raise();
    m_chatWindow->activateWindow();
}

void MainWindow::on_pushButton_8_clicked()
{
    if (!m_cameraWindow) {
        m_cameraWindow = new CameraWindow(nullptr);
        connect(m_cameraWindow, &CameraWindow::imageCaptured, this, [this](const QPixmap& image) {
            handleCameraImage(image);
        });
    }
    m_cameraWindow->show();
    m_cameraWindow->raise();
    m_cameraWindow->activateWindow();
}

void MainWindow::handleCameraImage(const QPixmap& image)
{
    if (image.isNull()) {
        QMessageBox::warning(this, "拍照错误", "拍摄的图像无效！");
        return;
    }

    QPixmap roundedImage = createRoundedPixmap(image, 20);
    ui->label->setPixmap(roundedImage);
    ui->label_26->setVisible(false);

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString tempFileName = QString("camera_image_%1.jpg").arg(QDateTime::currentMSecsSinceEpoch());
    QString tempFilePath = QDir(tempDir).absoluteFilePath(tempFileName);

    if (image.save(tempFilePath, "JPEG", 90)) {
        lastImagePath_ = tempFilePath;
    }

    updatePlantIdentifyButton();

    ui->label_2->clear();
    ui->label_3->clear();
    ui->label_4->clear();

    QLabel* labels[5] = { ui->label_6, ui->label_7, ui->label_8, ui->label_9, ui->label_10 };
    QProgressBar* progressBars[5] = { ui->progressBar_6, ui->progressBar_7, ui->progressBar_8, ui->progressBar_9, ui->progressBar_10 };

    for (int i = 0; i < 5; ++i) {
        if (labels[i]) labels[i]->clear();
        if (progressBars[i]) { progressBars[i]->setValue(0); progressBars[i]->setVisible(false); }
    }

    m_currentRecordId = -1;
}

void MainWindow::on_pushButton_9_clicked()
{
    if (!ui->label_6 || ui->label_6->text().isEmpty() || ui->label_6->text() == "未知") {
        QMessageBox::warning(this, "植物对比", "请先进行植物识别！");
        return;
    }

    QString currentPlantName = ui->label_6->text();

    if (m_tempPlantName.isEmpty()) {
        m_tempPlantName = currentPlantName;
        QMessageBox::information(this, "植物对比", QString("已记录植物：%1\n\n请识别另一种植物，然后再次点击对比按钮。").arg(currentPlantName));
    } else {
        if (m_tempPlantName == currentPlantName) {
            QMessageBox::warning(this, "植物对比", "请识别不同的植物进行对比！");
            return;
        }

        if (!m_comparisonWindow) {
            m_comparisonWindow = new PlantComparisonWindow(nullptr);
            m_comparisonWindow->setDeepseekClient(&deepseek_);
        }

        m_comparisonWindow->setPlantNames(m_tempPlantName, currentPlantName);
        m_comparisonWindow->show();
        m_comparisonWindow->raise();
        m_comparisonWindow->activateWindow();

        m_tempPlantName.clear();
    }
}

void MainWindow::on_modeSwitchButton_clicked()
{
    isDiseaseMode_ = !isDiseaseMode_;

    // 植物识别模式样式（绿色主题）
    const QString plantPrimaryStyle = "rgb(84, 130, 53)";
    const QString plantHighlightStyle = "rgb(224, 224, 0)";

    // 病害检测模式样式（橙红色主题）
    const QString diseasePrimaryStyle = "rgb(180, 50, 50)";
    const QString diseaseHighlightStyle = "rgb(255, 140, 0)";

    if (isDiseaseMode_) {
        // 切换到病害检测模式
        if (ui->pushButton_2) {
            ui->pushButton_2->setText("   病害识别");
        }
        // 修改切换按钮样式为病害风格（橙红色）
        if (ui->modeSwitchButton) {
            ui->modeSwitchButton->setText("   植物识别");
            ui->modeSwitchButton->setStyleSheet(R"(
                QPushButton {
                    background-color: rgb(180, 50, 50);
                    color: rgb(255, 255, 255);
                    border: none;
                    border-radius: 8px;
                    padding: 8px;
                }
                QPushButton:hover {
                    background-color: rgb(200, 70, 70);
                }
                QPushButton:pressed {
                    background-color: rgb(160, 40, 40);
                }
            )");
        }
        // 修改标签标题 - 病害检测专用
        if (ui->label_19) ui->label_19->setText("病害诊断");
        if (ui->label_20) ui->label_20->setText("病害程度");
        if (ui->label_21) ui->label_21->setText("防治建议");

        // 更新标签颜色为病害风格（橙红色）
        if (ui->label_19) {
            ui->label_19->setStyleSheet(R"(
                QLabel {
                    background-color: rgb(180, 50, 50);
                    border-top-left-radius: 10px;
                    border-top-right-radius: 10px;
                    border-bottom-left-radius: 0px;
                    border-bottom-right-radius: 0px;
                    color: white;
                }
            )");
        }
        if (ui->label_20) {
            ui->label_20->setStyleSheet(R"(
                QLabel {
                    background-color: rgb(255, 140, 0);
                    border-top-left-radius: 10px;
                    border-top-right-radius: 10px;
                    border-bottom-left-radius: 0px;
                    border-bottom-right-radius: 0px;
                    color: white;
                }
            )");
        }
        if (ui->label_21) {
            ui->label_21->setStyleSheet(R"(
                QLabel {
                    background-color: rgb(180, 50, 50);
                    border-top-left-radius: 10px;
                    border-top-right-radius: 10px;
                    border-bottom-left-radius: 0px;
                    border-bottom-right-radius: 0px;
                    color: white;
                }
            )");
        }

        // 切换背景图为病害风格（红色调）
        this->setStyleSheet("QMainWindow { background-image: url(:/images/background_disease.jpg); background-repeat: no-repeat; background-position: center; }");

        QMessageBox::information(this, "模式切换", "已切换到病害检测模式\n\n请上传植物叶片图片进行病害检测。");
    } else {
        // 切换到植物识别模式
        if (ui->pushButton_2) {
            ui->pushButton_2->setText("   植物识别");
        }
        // 恢复切换按钮样式为植物风格（绿色）
        if (ui->modeSwitchButton) {
            ui->modeSwitchButton->setText("   病害检测");
            ui->modeSwitchButton->setStyleSheet(R"(
                QPushButton {
                    background-color: rgb(0, 150, 0);
                    color: rgb(255, 255, 255);
                    border: none;
                    border-radius: 8px;
                    padding: 8px;
                }
                QPushButton:hover {
                    background-color: rgb(0, 170, 0);
                }
                QPushButton:pressed {
                    background-color: rgb(0, 130, 0);
                }
            )");
        }
        // 恢复标签标题
        if (ui->label_19) ui->label_19->setText("植物百科");
        if (ui->label_20) ui->label_20->setText("培育指南");
        if (ui->label_21) ui->label_21->setText("装饰技巧");

        // 恢复标签颜色为植物风格（绿色）
        if (ui->label_19) {
            ui->label_19->setStyleSheet(R"(
                QLabel {
                    background-color: rgb(0, 100, 200);
                    border-top-left-radius: 10px;
                    border-top-right-radius: 10px;
                    border-bottom-left-radius: 0px;
                    border-bottom-right-radius: 0px;
                    color: white;
                }
            )");
        }
        if (ui->label_20) {
            ui->label_20->setStyleSheet(R"(
                QLabel {
                    background-color: rgb(0, 150, 0);
                    border-top-left-radius: 10px;
                    border-top-right-radius: 10px;
                    border-bottom-left-radius: 0px;
                    border-bottom-right-radius: 0px;
                    color: white;
                }
            )");
        }
        if (ui->label_21) {
            ui->label_21->setStyleSheet(R"(
                QLabel {
                    background-color: rgb(255, 100, 150);
                    border-top-left-radius: 10px;
                    border-top-right-radius: 10px;
                    border-bottom-left-radius: 0px;
                    border-bottom-right-radius: 0px;
                    color: white;
                }
            )");
        }

        // 切换背景图为植物风格（绿色调）
        this->setStyleSheet("QMainWindow { background-image: url(:/images/background_plant.jpg); background-repeat: no-repeat; background-position: center; }");

        QMessageBox::information(this, "模式切换", "已切换到植物识别模式\n\n请上传植物图片进行种类识别。");
    }

    // 重置识别结果
    QLabel* labels[5] = { ui->label_6, ui->label_7, ui->label_8, ui->label_9, ui->label_10 };
    QProgressBar* bars[5] = { ui->progressBar_6, ui->progressBar_7, ui->progressBar_8, ui->progressBar_9, ui->progressBar_10 };
    for (int i = 0; i < 5; ++i) {
        if (labels[i]) labels[i]->clear();
        if (bars[i]) { bars[i]->setValue(0); bars[i]->setVisible(false); }
    }

    // 清空内容框
    if (ui->label_2) ui->label_2->clear();
    if (ui->label_3) ui->label_3->clear();
    if (ui->label_4) ui->label_4->clear();
}

void MainWindow::on_pushButton_disease_clicked()
{
    on_modeSwitchButton_clicked();
}
