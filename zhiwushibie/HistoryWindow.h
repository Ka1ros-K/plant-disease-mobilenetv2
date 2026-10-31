#ifndef HISTORYWINDOW_H
#define HISTORYWINDOW_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QSpinBox>
#include <QProgressBar>
#include <QMessageBox>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>

#include "DatabaseManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class HistoryWindow; }
QT_END_NAMESPACE

class HistoryWindow : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWindow(DatabaseManager* dbManager, QWidget *parent = nullptr);
    ~HistoryWindow();

public slots:
    void refreshHistory();
    void showRecord(int recordId);

private slots:
    void onSearchClicked();
    void onClearSearchClicked();
    void onDeleteSelectedClicked();
    void onClearAllClicked();
    void onExportClicked();
    void onTableContextMenu(const QPoint& pos);
    void onTableDoubleClicked(int row, int column);
    void onDeleteRecord();
    void onViewImage();
    void onViewDetails();

private:
    void setupUI();
    void setupTable();
    void setupSearchPanel();
    void setupButtons();
    void loadHistoryRecords();
    void updateTable(const QList<HistoryRecord>& records);
    void showRecordDetails(const HistoryRecord& record);
    void showImage(const QString& imagePath);
    void exportToCSV(const QString& filePath);
    void exportToExcel(const QString& filePath);
    void setupContextMenu();
    
    // UI指针
    Ui::HistoryWindow *ui;
    
    // 右键菜单
    QMenu* m_contextMenu;
    QAction* m_viewImageAction;
    QAction* m_viewDetailsAction;
    QAction* m_deleteAction;
    
    // 数据
    DatabaseManager* m_dbManager;
    QList<HistoryRecord> m_currentRecords;
    int m_selectedRecordId;
    
    // 常量
    static const int COLUMN_ID = 0;
    static const int COLUMN_TIMESTAMP = 1;
    static const int COLUMN_PLANT_NAME = 2;
    static const int COLUMN_CONFIDENCE = 3;
    static const int COLUMN_FAVORITE = 4;
    static const int COLUMN_IMAGE = 5;
    static const int COLUMN_COUNT = 6;
};

#endif // HISTORYWINDOW_H
