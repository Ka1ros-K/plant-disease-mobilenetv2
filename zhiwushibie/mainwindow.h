#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <vector>
#include <utility>
#include <string>


#include "DeepseekClient.h"   // 封装的 DeepSeek API 客户端（含 PlantInfo 结构体）
#include "PlantId.h"          // 含 loadLabels 与 displayNameFromLatin 映射
#include "PlantDisease.h"     // 植物病害检测类
#include "DatabaseManager.h"  // 数据库管理器
#include "HistoryWindow.h"    // 历史记录窗口
#include "SettingsWindow.h"   // 设置窗口
#include "InfoWindow.h"       // 信息窗口
#include "ChatWindow.h"       // 对话窗口
#include "CameraWindow.h"     // 摄像头拍照窗口
#include "PlantComparisonWindow.h"  // 植物对比窗口

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 推理完成后调用：传入 Top-K 结果（拉丁名 + 置信度），内部会取 Top-1 去请求 DeepSeek
    void onInferenceDone(const std::vector<std::pair<std::string,double>>& preds);


    void setDeepseekApiKey(const QString& key) { deepseek_.setApiKey(key); }

private slots:
    void on_pushButton_clicked();   // 打开图片
    void on_pushButton_2_clicked(); // 识别
    void on_pushButton_3_clicked(); // 打开信息窗口
    void on_pushButton_4_clicked(); // 打开设置窗口
    void on_pushButton_5_clicked(); // 打开历史记录窗口
    void on_pushButton_6_clicked(); // 收藏当前识别结果
    void on_pushButton_7_clicked(); // 打开对话窗口
    void on_pushButton_8_clicked(); // 摄像头拍照
    void on_pushButton_9_clicked(); // 植物对比功能
    void on_pushButton_disease_clicked(); // 病害识别
    void on_modeSwitchButton_clicked(); // 切换模式（植物/病害）

    // DeepSeek 回调槽：info 就绪/出错（你也可以在 cpp 里用 lambda 连接）


private:
    // 私有方法
    void handleCameraImage(const QPixmap& image);
    void displayPlantResults(const std::vector<PlantResult>& results, double maxConfidence);
    void displayDiseaseResults(const std::vector<PlantDiseaseResult>& results);
    QString getDiseaseAdvice(const QString& diseaseName);  // 获取病害防治建议

    // UI 指针
    Ui::MainWindow *ui;

    // 最近选择的图片路径（你原来的新增成员）
    QString lastImagePath_;

    // 业务成员：DeepSeek 客户端与标签/映射提供者
    DeepseekClient deepseek_;
    std::unique_ptr<PlantId> plantId_;
    
    // 数据库和历史记录管理
    DatabaseManager* m_dbManager;
    HistoryWindow* m_historyWindow;
    SettingsWindow* m_settingsWindow;
    InfoWindow* m_infoWindow;
    ChatWindow* m_chatWindow;
    CameraWindow* m_cameraWindow;
    PlantComparisonWindow* m_comparisonWindow;
    
    // 设置状态
    bool m_autoSaveEnabled;
    
    // 当前识别结果的记录ID（用于收藏功能）
    int m_currentRecordId;
    
    // 植物对比功能相关
    QString m_tempPlantName;  // 临时存储的植物名称

    // 病害检测相关
    std::unique_ptr<PlantDisease> diseaseDetector_;
    bool isDiseaseMode_ = false;  // 当前模式：false=植物识别，true=病害检测

    // 加载覆盖层相关
    QWidget* m_loadingOverlay;        // 加载覆盖层
    QLabel* m_loadingLabel;           // 加载提示标签

    // 图片预览相关
    QLabel* m_thumbnailLabel;         // 缩略图预览标签

    // 初始化/连接信号的内部辅助函数（在 cpp 里实现）
    void initUiLabels();              // 设置 Lable_2 / Lable_3 / Lable_4 的显示属性
    void connectDeepseekSignals();    // 连接 deepseek_ 的信号到本类槽
    void showLoadingForTop1(const QString& zh, const QString& latin, double score); // "正在生成…" 提示
    void updatePlantIdentifyButton(); // 根据图片导入状态更新植物识别按钮

    // 用户体验优化
    void showImagePreview(const QString& imagePath);  // 显示图片预览
    void showLoadingOverlay(const QString& message);  // 显示加载覆盖层
    void hideLoadingOverlay();                        // 隐藏加载覆盖层
};

#endif // MAINWINDOW_H
