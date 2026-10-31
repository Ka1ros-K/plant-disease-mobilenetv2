#ifndef PLANTCOMPARISONWINDOW_H
#define PLANTCOMPARISONWINDOW_H

#include <QWidget>
#include <QString>
#include "DeepseekClient.h"

QT_BEGIN_NAMESPACE
namespace Ui { class PlantComparisonWindow; }
QT_END_NAMESPACE

class PlantComparisonWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PlantComparisonWindow(QWidget *parent = nullptr);
    ~PlantComparisonWindow();
    
    // 设置要对比的两个植物名称
    void setPlantNames(const QString& plant1, const QString& plant2);
    
    // 设置DeepSeek客户端
    void setDeepseekClient(DeepseekClient* client);

private slots:
    // 处理分类信息获取完成
    void onClassificationReady(const PlantClassification& classification);
    // 处理分类信息获取错误
    void onClassificationError(const QString& error);

private:
    Ui::PlantComparisonWindow *ui;
    DeepseekClient* m_deepseekClient;
    
    // 存储两个植物的分类信息
    PlantClassification m_plant1Classification;
    PlantClassification m_plant2Classification;
    
    // 跟踪当前正在获取哪个植物的分类信息
    int m_currentPlantIndex; // 0: plant1, 1: plant2
    
    // 存储植物名称
    QString m_plant1Name;
    QString m_plant2Name;
    
    // 更新分类信息显示
    void updateClassificationDisplay();
    
    // 初始化表格
    void initializeTable();
};

#endif // PLANTCOMPARISONWINDOW_H
