#include "PlantComparisonWindow.h"
#include "./ui_PlantComparisonWindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QTableWidgetItem>

PlantComparisonWindow::PlantComparisonWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlantComparisonWindow)
    , m_deepseekClient(nullptr)
    , m_currentPlantIndex(0)
{
    ui->setupUi(this);
    
    // 设置窗口属性
    setWindowTitle("植物图谱");
    resize(600, 282);
    setMinimumSize(500, 250);
    setMaximumSize(1200, 800);
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    
    // 初始化表格
    initializeTable();
    
    // 设置窗口样式
    setStyleSheet(
        "QWidget {"
        "    background-color: rgb(240, 248, 255);"
        "    font-family: 'Microsoft YaHei';"
        "}"
        "QLabel {"
        "    color: rgb(84, 130, 53);"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 2px solid rgb(84, 130, 53);"
        "    border-radius: 8px;"
        "    margin-top: 10px;"
        "    padding-top: 10px;"
        "    background-color: rgb(255, 255, 255);"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "    color: rgb(84, 130, 53);"
        "}"
    );
}

PlantComparisonWindow::~PlantComparisonWindow()
{
    delete ui;
}

void PlantComparisonWindow::setPlantNames(const QString& plant1, const QString& plant2)
{
    // 存储植物名称
    m_plant1Name = plant1;
    m_plant2Name = plant2;
    
    // 更新表格标题 - 只显示植物的"种"名
    if (ui->tableWidget) {
        // 植物名称已经是"种"名（如"香蕉"），直接使用
        ui->tableWidget->setHorizontalHeaderItem(0, new QTableWidgetItem(plant1));
        ui->tableWidget->setHorizontalHeaderItem(1, new QTableWidgetItem(plant2));
    }
    
    // 开始获取分类信息
    if (m_deepseekClient) {
        // 先获取第一个植物的分类信息
        m_currentPlantIndex = 0;
        m_deepseekClient->fetchPlantClassification(plant1);
        qDebug() << "[Comparison] 开始获取植物1分类信息:" << plant1;
    } else {
        qWarning() << "[Comparison] DeepSeek客户端未设置";
    }
}

void PlantComparisonWindow::setDeepseekClient(DeepseekClient* client)
{
    m_deepseekClient = client;
    
    if (m_deepseekClient) {
        // 连接信号
        connect(m_deepseekClient, &DeepseekClient::classificationReady,
                this, &PlantComparisonWindow::onClassificationReady);
        connect(m_deepseekClient, &DeepseekClient::classificationErrorOccured,
                this, &PlantComparisonWindow::onClassificationError);
    }
}

void PlantComparisonWindow::onClassificationReady(const PlantClassification& classification)
{
    qDebug() << "[Comparison] 收到分类信息，当前植物索引:" << m_currentPlantIndex;
    
    if (m_currentPlantIndex == 0) {
        // 第一个植物的分类信息
        m_plant1Classification = classification;
        m_currentPlantIndex = 1;
        
        // 获取第二个植物的分类信息
        if (!m_plant2Name.isEmpty()) {
            m_deepseekClient->fetchPlantClassification(m_plant2Name);
            qDebug() << "[Comparison] 开始获取植物2分类信息:" << m_plant2Name;
        }
    } else if (m_currentPlantIndex == 1) {
        // 第二个植物的分类信息
        m_plant2Classification = classification;
        
        // 更新显示
        updateClassificationDisplay();
        qDebug() << "[Comparison] 分类信息获取完成，更新显示";
    }
}

void PlantComparisonWindow::onClassificationError(const QString& error)
{
    qWarning() << "[Comparison] 获取分类信息失败:" << error;
    QMessageBox::warning(this, "获取分类信息失败", error);
}

void PlantComparisonWindow::initializeTable()
{
    if (!ui->tableWidget) return;
    
    // 设置表格行数和列数
    ui->tableWidget->setRowCount(7); // 界门纲目科属种
    ui->tableWidget->setColumnCount(2); // 植物一 + 植物二
    
    // 设置表头
    QStringList headers;
    headers << "植物一" << "植物二";
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    
    // 设置行标签
    QStringList rowLabels;
    rowLabels << "界" << "门" << "纲" << "目" << "科" << "属" << "种";
    ui->tableWidget->setVerticalHeaderLabels(rowLabels);
    
    // 设置表格属性
    ui->tableWidget->setAlternatingRowColors(true);
    ui->tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    ui->tableWidget->setSortingEnabled(false);
    
    // 设置列宽 - 两列布局，支持窗口大小调节
    ui->tableWidget->setColumnWidth(0, 250);  // 植物一列
    ui->tableWidget->setColumnWidth(1, 250);  // 植物二列
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);  // 允许最后一列拉伸
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);  // 允许用户调节列宽
    
    // 初始化所有单元格
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 2; ++col) {
            QTableWidgetItem* item = new QTableWidgetItem("");
            item->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, col, item);
        }
    }
}

void PlantComparisonWindow::updateClassificationDisplay()
{
    if (!ui->tableWidget) return;
    
    // 定义分类级别数组
    QStringList classifications[2] = {
        {
            m_plant1Classification.kingdom,
            m_plant1Classification.phylum,
            m_plant1Classification.class_,
            m_plant1Classification.order,
            m_plant1Classification.family,
            m_plant1Classification.genus,
            m_plant1Classification.species
        },
        {
            m_plant2Classification.kingdom,
            m_plant2Classification.phylum,
            m_plant2Classification.class_,
            m_plant2Classification.order,
            m_plant2Classification.family,
            m_plant2Classification.genus,
            m_plant2Classification.species
        }
    };
    
    // 更新表格内容
    for (int row = 0; row < 7; ++row) {
        QString plant1Value = classifications[0][row];
        QString plant2Value = classifications[1][row];
        
        // 设置植物一的分类信息
        QTableWidgetItem* item1 = ui->tableWidget->item(row, 0);
        if (item1) {
            item1->setText(plant1Value);
        }
        
        // 设置植物二的分类信息
        QTableWidgetItem* item2 = ui->tableWidget->item(row, 1);
        if (item2) {
            item2->setText(plant2Value);
        }
        
        // 检查是否需要合并单元格
        if (plant1Value == plant2Value && !plant1Value.isEmpty()) {
            // 相同值，合并单元格
            ui->tableWidget->setSpan(row, 0, 1, 2);
            
            // 设置合并后的单元格样式
            QTableWidgetItem* mergedItem = ui->tableWidget->item(row, 0);
            if (mergedItem) {
                mergedItem->setBackground(QColor(200, 255, 200)); // 浅绿色背景
                mergedItem->setForeground(QColor(0, 0, 0)); // 黑色文字
                mergedItem->setText(plant1Value);
            }
        } else {
            // 不同值，分开显示
            ui->tableWidget->setSpan(row, 0, 1, 1); // 取消合并
            ui->tableWidget->setSpan(row, 1, 1, 1); // 取消合并
            
            // 设置不同的背景色和文字颜色
            if (item1) {
                item1->setBackground(QColor(255, 255, 255)); // 白色背景
                item1->setForeground(QColor(0, 0, 0)); // 黑色文字
            }
            if (item2) {
                item2->setBackground(QColor(255, 255, 255)); // 白色背景
                item2->setForeground(QColor(0, 0, 0)); // 黑色文字
            }
        }
    }
    
    // 调整行高 - 紧凑布局
    for (int row = 0; row < 7; ++row) {
        ui->tableWidget->setRowHeight(row, 30);
    }
}
