#include "HistoryWindow.h"
#include "./ui_HistoryWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QScrollBar>
#include <QDateTime>
#include <QDebug>
#include <QColor>

HistoryWindow::HistoryWindow(DatabaseManager* dbManager, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HistoryWindow)
    , m_dbManager(dbManager)
    , m_selectedRecordId(-1)
{
    ui->setupUi(this);
    setupUI();
    loadHistoryRecords();
}

HistoryWindow::~HistoryWindow()
{
    delete ui;
}

void HistoryWindow::setupUI()
{
    // 如果父窗口为 nullptr，设置为独立窗口
    if (parent() == nullptr) {
        setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        setAttribute(Qt::WA_DeleteOnClose, false); // 不自动删除，由主窗口管理
    }
    
    // 设置窗口初始大小，确保在大多数屏幕上都能完全显示
    resize(1200, 800);
    setMinimumSize(800, 500);
    
    // 设置状态栏样式
    QString statusStyle = R"(
        QLabel {
            color: rgb(84, 130, 53);
            font-weight: bold;
            font-size: 12px;
        }
        QProgressBar {
            border: 2px solid rgb(84, 130, 53);
            border-radius: 8px;
            background-color: white;
            text-align: center;
            font-weight: bold;
            color: black;
        }
        QProgressBar::chunk {
            background-color: rgb(84, 130, 53);
            border-radius: 6px;
        }
    )";
    
    ui->statusLabel->setStyleSheet(statusStyle);
    ui->progressBar->setStyleSheet(statusStyle);
    
    // UI 文件已经设置了基本的窗口属性，这里只需要进行额外的设置
    setupTable();
    setupSearchPanel();
    setupButtons();
    setupContextMenu();
}

void HistoryWindow::setupSearchPanel()
{
    // 设置日期编辑器的默认值
    ui->startDateEdit->setDate(QDate::currentDate().addDays(-30));
    ui->endDateEdit->setDate(QDate::currentDate());
    
    // 设置搜索面板组件样式
    QString inputStyle = R"(
        QLineEdit, QComboBox, QDateEdit, QSpinBox {
            background-color: white;
            border: 2px solid rgb(84, 130, 53);
            border-radius: 8px;
            padding: 6px;
            font-size: 12px;
            color: black;
        }
        QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QSpinBox:focus {
            border-color: rgb(84, 130, 53);
            background-color: white;
            color: black;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid rgb(84, 130, 53);
            margin-right: 5px;
        }
        QComboBox QAbstractItemView {
            border: 2px solid rgb(84, 130, 53);
            border-radius: 8px;
            background-color: white;
            color: black;
            selection-background-color: rgb(84, 130, 53);
            selection-color: white;
        }
        QComboBox QAbstractItemView::item {
            color: black;
            background-color: white;
        }
        QComboBox QAbstractItemView::item:selected {
            background-color: rgb(84, 130, 53);
            color: white;
        }
    )";
    
    // 应用样式到搜索组件
    ui->searchEdit->setStyleSheet(inputStyle);
    ui->searchTypeCombo->setStyleSheet(inputStyle);
    ui->startDateEdit->setStyleSheet(inputStyle);
    ui->endDateEdit->setStyleSheet(inputStyle);
    ui->minConfSpinBox->setStyleSheet(inputStyle);
    ui->maxConfSpinBox->setStyleSheet(inputStyle);
    
    // 设置标签样式
    QString labelStyle = R"(
        QLabel {
            color: rgb(84, 130, 53);
            font-weight: bold;
            font-size: 12px;
        }
    )";
    
    ui->searchTypeLabel->setStyleSheet(labelStyle);
    ui->dateSeparatorLabel->setStyleSheet(labelStyle);
    ui->confSeparatorLabel->setStyleSheet(labelStyle);
    
    // 连接信号
    connect(ui->searchTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        bool showDate = (index == 2);
        bool showConf = (index == 3);
        bool showSearch = (index == 1);
        
        ui->startDateEdit->setVisible(showDate);
        ui->endDateEdit->setVisible(showDate);
        ui->dateSeparatorLabel->setVisible(showDate);
        ui->minConfSpinBox->setVisible(showConf);
        ui->maxConfSpinBox->setVisible(showConf);
        ui->confSeparatorLabel->setVisible(showConf);
        ui->searchEdit->setVisible(showSearch);
        
        if (showDate) {
            ui->searchEdit->setPlaceholderText("按日期范围搜索");
        } else if (showConf) {
            ui->searchEdit->setPlaceholderText("按置信度范围搜索");
        } else {
            ui->searchEdit->setPlaceholderText("输入植物名称搜索...");
        }
    });
    
    connect(ui->searchButton, &QPushButton::clicked, this, &HistoryWindow::onSearchClicked);
    connect(ui->clearSearchButton, &QPushButton::clicked, this, &HistoryWindow::onClearSearchClicked);
}

void HistoryWindow::setupTable()
{
    // 设置表格样式 - 与主界面保持一致
    QString tableStyle = R"(
        QTableWidget {
            background-color: white;
            border: 5px solid rgb(84, 130, 53);
            border-radius: 20px;
            gridline-color: rgb(200, 200, 200);
            selection-background-color: rgb(84, 130, 53);
            selection-color: white;
            color: black;
        }
        QTableWidget::item {
            padding: 8px;
            border: none;
            background-color: white;
            color: black;
        }
        QTableWidget::item:selected {
            background-color: rgb(84, 130, 53);
            color: white;
        }
        QTableWidget::item:hover {
            background-color: rgba(84, 130, 53, 0.1);
            color: black;
        }
        QTableWidget::item:selected:hover {
            background-color: rgb(84, 130, 53);
            color: white;
        }
        QHeaderView::section {
            background-color: rgb(84, 130, 53);
            color: white;
            padding: 8px;
            border: none;
            font-weight: bold;
        }
        QHeaderView::section:first {
            border-top-left-radius: 15px;
        }
        QHeaderView::section:last {
            border-top-right-radius: 15px;
        }
    )";
    
    ui->tableWidget->setStyleSheet(tableStyle);
    
    // 设置列宽
    QHeaderView* header = ui->tableWidget->horizontalHeader();
    header->setSectionResizeMode(COLUMN_ID, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COLUMN_TIMESTAMP, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COLUMN_PLANT_NAME, QHeaderView::Stretch);
    header->setSectionResizeMode(COLUMN_CONFIDENCE, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COLUMN_FAVORITE, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(COLUMN_IMAGE, QHeaderView::ResizeToContents);
    
    // 连接信号
    connect(ui->tableWidget, &QTableWidget::customContextMenuRequested, 
            this, &HistoryWindow::onTableContextMenu);
    connect(ui->tableWidget, &QTableWidget::cellDoubleClicked, 
            this, &HistoryWindow::onTableDoubleClicked);
}

void HistoryWindow::setupButtons()
{
    // 设置按钮样式 - 与主界面保持一致
    QString buttonStyle = R"(
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
        QPushButton:disabled {
            background-color: rgb(240, 240, 240);
            border-color: rgb(200, 200, 200);
            color: rgb(150, 150, 150);
        }
    )";
    
    // 应用样式到所有按钮
    ui->searchButton->setStyleSheet(buttonStyle);
    ui->clearSearchButton->setStyleSheet(buttonStyle);
    ui->deleteSelectedButton->setStyleSheet(buttonStyle);
    ui->clearAllButton->setStyleSheet(buttonStyle);
    ui->exportButton->setStyleSheet(buttonStyle);
    ui->refreshButton->setStyleSheet(buttonStyle);
    
    // 连接信号
    connect(ui->deleteSelectedButton, &QPushButton::clicked, this, &HistoryWindow::onDeleteSelectedClicked);
    connect(ui->clearAllButton, &QPushButton::clicked, this, &HistoryWindow::onClearAllClicked);
    connect(ui->exportButton, &QPushButton::clicked, this, &HistoryWindow::onExportClicked);
    connect(ui->refreshButton, &QPushButton::clicked, this, &HistoryWindow::refreshHistory);
}

void HistoryWindow::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_viewImageAction = new QAction("查看图片", this);
    m_viewDetailsAction = new QAction("查看详情", this);
    m_deleteAction = new QAction("删除记录", this);
    
    m_contextMenu->addAction(m_viewImageAction);
    m_contextMenu->addAction(m_viewDetailsAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_deleteAction);
    
    connect(m_viewImageAction, &QAction::triggered, this, &HistoryWindow::onViewImage);
    connect(m_viewDetailsAction, &QAction::triggered, this, &HistoryWindow::onViewDetails);
    connect(m_deleteAction, &QAction::triggered, this, &HistoryWindow::onDeleteRecord);
}

void HistoryWindow::loadHistoryRecords()
{
    qDebug() << "[HistoryWindow] ========== 加载历史记录调试信息 ==========";
    qDebug() << "[HistoryWindow] m_dbManager 指针:" << m_dbManager;

    if (!m_dbManager) {
        qCritical() << "[HistoryWindow] 错误：m_dbManager 为空指针！";
        ui->statusLabel->setText("错误：数据库管理器未初始化");
        ui->progressBar->setVisible(false);
        return;
    }

    ui->progressBar->setVisible(true);
    ui->progressBar->setRange(0, 0); // 不确定进度

    ui->statusLabel->setText("正在加载历史记录...");

    // 获取所有记录
    qDebug() << "[HistoryWindow] 调用 m_dbManager->getAllHistoryRecords()";
    m_currentRecords = m_dbManager->getAllHistoryRecords();
    qDebug() << "[HistoryWindow] 查询完成，记录数:" << m_currentRecords.size();

    updateTable(m_currentRecords);

    ui->progressBar->setVisible(false);
    ui->statusLabel->setText(QString("共 %1 条记录").arg(m_currentRecords.size()));
    qDebug() << "[HistoryWindow] ========== 调试信息结束 ==========";
}

void HistoryWindow::updateTable(const QList<HistoryRecord>& records)
{
    ui->tableWidget->setRowCount(records.size());
    
    for (int i = 0; i < records.size(); ++i) {
        const HistoryRecord& record = records[i];
        
        // ID
        ui->tableWidget->setItem(i, COLUMN_ID, new QTableWidgetItem(QString::number(record.id)));
        
        // 时间
        QString timeStr = record.timestamp.toString("yyyy-MM-dd hh:mm:ss");
        ui->tableWidget->setItem(i, COLUMN_TIMESTAMP, new QTableWidgetItem(timeStr));
        
        // 植物名称
        ui->tableWidget->setItem(i, COLUMN_PLANT_NAME, new QTableWidgetItem(record.plantName));
        
        // 置信度
        QString confStr = QString("%1%").arg(record.confidence * 100, 0, 'f', 1);
        ui->tableWidget->setItem(i, COLUMN_CONFIDENCE, new QTableWidgetItem(confStr));
        
        // 收藏状态
        QTableWidgetItem* favoriteItem = new QTableWidgetItem(record.isFavorite ? "★" : "☆");
        favoriteItem->setTextAlignment(Qt::AlignCenter);
        if (record.isFavorite) {
            favoriteItem->setForeground(QColor(255, 215, 0)); // 金色
            favoriteItem->setToolTip("已收藏");
        } else {
            favoriteItem->setForeground(QColor(200, 200, 200)); // 灰色
            favoriteItem->setToolTip("未收藏");
        }
        ui->tableWidget->setItem(i, COLUMN_FAVORITE, favoriteItem);
        
        // 图片
        QTableWidgetItem* imageItem = new QTableWidgetItem("查看");
        imageItem->setData(Qt::UserRole, record.imageFilename);
        ui->tableWidget->setItem(i, COLUMN_IMAGE, imageItem);
    }
}

void HistoryWindow::onSearchClicked()
{
    int searchType = ui->searchTypeCombo->currentIndex();
    QList<HistoryRecord> results;
    
    ui->progressBar->setVisible(true);
    ui->progressBar->setRange(0, 0);
    ui->statusLabel->setText("正在搜索...");
    
    switch (searchType) {
    case 0: // 全部
        results = m_dbManager->getAllHistoryRecords();
        break;
    case 1: // 植物名称
        results = m_dbManager->getHistoryRecordsByPlantName(ui->searchEdit->text());
        break;
    case 2: // 日期范围
        {
            QDateTime start = QDateTime(ui->startDateEdit->date(), QTime(0, 0, 0));
            QDateTime end = QDateTime(ui->endDateEdit->date(), QTime(23, 59, 59));
            results = m_dbManager->getHistoryRecordsByDateRange(start, end);
        }
        break;
    case 3: // 置信度范围
        {
            double minConf = ui->minConfSpinBox->value() / 100.0;
            double maxConf = ui->maxConfSpinBox->value() / 100.0;
            results = m_dbManager->getHistoryRecordsByConfidenceRange(minConf, maxConf);
        }
        break;
    }
    
    m_currentRecords = results;
    updateTable(results);
    
    ui->progressBar->setVisible(false);
    ui->statusLabel->setText(QString("找到 %1 条记录").arg(results.size()));
}

void HistoryWindow::onClearSearchClicked()
{
    ui->searchTypeCombo->setCurrentIndex(0);
    ui->searchEdit->clear();
    ui->startDateEdit->setDate(QDate::currentDate().addDays(-30));
    ui->endDateEdit->setDate(QDate::currentDate());
    ui->minConfSpinBox->setValue(0);
    ui->maxConfSpinBox->setValue(100);
    
    loadHistoryRecords();
}

void HistoryWindow::onDeleteSelectedClicked()
{
    QList<QTableWidgetItem*> selectedItems = ui->tableWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要删除的记录");
        return;
    }
    
    int ret = QMessageBox::question(this, "确认删除", 
                                   QString("确定要删除选中的 %1 条记录吗？").arg(selectedItems.size() / COLUMN_COUNT),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        QSet<int> rowsToDelete;
        for (QTableWidgetItem* item : selectedItems) {
            rowsToDelete.insert(item->row());
        }
        
        for (int row : rowsToDelete) {
            int recordId = ui->tableWidget->item(row, COLUMN_ID)->text().toInt();
            m_dbManager->deleteHistoryRecord(recordId);
        }
        
        refreshHistory();
    }
}

void HistoryWindow::onClearAllClicked()
{
    int ret = QMessageBox::question(this, "确认清空", 
                                   "确定要清空所有历史记录吗？此操作不可恢复！",
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        if (m_dbManager->clearAllHistory()) {
            QMessageBox::information(this, "成功", "所有历史记录已清空");
            refreshHistory();
        } else {
            QMessageBox::warning(this, "错误", "清空历史记录失败");
        }
    }
}

void HistoryWindow::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出数据", 
                                                   "历史记录.csv", 
                                                   "CSV文件 (*.csv);;所有文件 (*.*)");
    
    if (!fileName.isEmpty()) {
        exportToCSV(fileName);
    }
}

void HistoryWindow::onTableContextMenu(const QPoint& pos)
{
    QTableWidgetItem* item = ui->tableWidget->itemAt(pos);
    if (item) {
        m_selectedRecordId = ui->tableWidget->item(item->row(), COLUMN_ID)->text().toInt();
        m_contextMenu->exec(ui->tableWidget->mapToGlobal(pos));
    }
}

void HistoryWindow::onTableDoubleClicked(int row, int column)
{
    if (column == COLUMN_IMAGE) {
        onViewImage();
    } else {
        onViewDetails();
    }
}

void HistoryWindow::onDeleteRecord()
{
    if (m_selectedRecordId > 0) {
        int ret = QMessageBox::question(this, "确认删除", "确定要删除这条记录吗？",
                                       QMessageBox::Yes | QMessageBox::No);
        
        if (ret == QMessageBox::Yes) {
            if (m_dbManager->deleteHistoryRecord(m_selectedRecordId)) {
                refreshHistory();
            }
        }
    }
}

void HistoryWindow::onViewImage()
{
    if (m_selectedRecordId > 0) {
        // 查找对应的记录
        for (const HistoryRecord& record : m_currentRecords) {
            if (record.id == m_selectedRecordId) {
                QString imagePath = m_dbManager->getImageFilePath(record.imageFilename);
                showImage(imagePath);
                break;
            }
        }
    }
}

void HistoryWindow::onViewDetails()
{
    if (m_selectedRecordId > 0) {
        // 查找对应的记录
        for (const HistoryRecord& record : m_currentRecords) {
            if (record.id == m_selectedRecordId) {
                showRecordDetails(record);
                break;
            }
        }
    }
}

void HistoryWindow::showRecordDetails(const HistoryRecord& record)
{
    QString details = QString(
        "记录ID: %1\n"
        "查询时间: %2\n"
        "植物名称: %3\n"
        "置信度: %4%\n"
        "图片文件: %5\n"
        "创建时间: %6"
    ).arg(record.id)
     .arg(record.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
     .arg(record.plantName)
     .arg(record.confidence * 100, 0, 'f', 1)
     .arg(record.imageFilename)
     .arg(record.createdAt.toString("yyyy-MM-dd hh:mm:ss"));
    
    QMessageBox::information(this, "记录详情", details);
}

void HistoryWindow::showImage(const QString& imagePath)
{
    if (QFile::exists(imagePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(imagePath));
    } else {
        QMessageBox::warning(this, "错误", "图片文件不存在: " + imagePath);
    }
}

void HistoryWindow::exportToCSV(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建导出文件");
        return;
    }
    
    QTextStream out(&file);
    // Qt6 中 QTextStream 默认使用 UTF-8 编码，无需设置
    
    // 写入BOM以支持Excel正确显示中文
    out << "\xEF\xBB\xBF";
    
    // 写入表头
    out << "ID,查询时间,植物名称,置信度,图片文件名\n";
    
    // 写入数据
    for (const HistoryRecord& record : m_currentRecords) {
        out << record.id << ","
            << record.timestamp.toString("yyyy-MM-dd hh:mm:ss") << ","
            << record.plantName << ","
            << QString::number(record.confidence * 100, 'f', 1) << "%" << ","
            << record.imageFilename << "\n";
    }
    
    file.close();
    QMessageBox::information(this, "成功", "数据已导出到: " + filePath);
}

void HistoryWindow::refreshHistory()
{
    loadHistoryRecords();
}

void HistoryWindow::showRecord(int recordId)
{
    // 查找并高亮显示指定记录
    for (int i = 0; i < ui->tableWidget->rowCount(); ++i) {
        int id = ui->tableWidget->item(i, COLUMN_ID)->text().toInt();
        if (id == recordId) {
            ui->tableWidget->selectRow(i);
            ui->tableWidget->scrollToItem(ui->tableWidget->item(i, 0));
            break;
        }
    }
}
