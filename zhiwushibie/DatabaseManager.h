#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QStandardPaths>
#include <QDir>

// 历史记录数据结构
struct HistoryRecord {
    int id;
    QDateTime timestamp;
    QString imageFilename;
    QString plantName;
    double confidence;
    bool isFavorite;  // 是否收藏
    QDateTime createdAt;
    
    HistoryRecord() : id(-1), confidence(0.0), isFavorite(false) {}
    HistoryRecord(const QString& imgFile, const QString& plant, double conf)
        : id(-1), timestamp(QDateTime::currentDateTime()), imageFilename(imgFile), 
          plantName(plant), confidence(conf), isFavorite(false), createdAt(QDateTime::currentDateTime()) {}
};

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();
    
    // 初始化数据库
    bool initializeDatabase();
    
    // 历史记录操作
    int addHistoryRecord(const HistoryRecord& record);  // 返回记录ID，失败返回-1
    bool deleteHistoryRecord(int id);
    QList<HistoryRecord> getAllHistoryRecords();
    QList<HistoryRecord> getHistoryRecordsByDateRange(const QDateTime& start, const QDateTime& end);
    QList<HistoryRecord> getHistoryRecordsByPlantName(const QString& plantName);
    QList<HistoryRecord> getHistoryRecordsByConfidenceRange(double minConf, double maxConf);
    
    // 收藏功能
    bool setRecordFavorite(int recordId, bool isFavorite);
    QList<HistoryRecord> getFavoriteRecords();
    bool isRecordFavorite(int recordId);
    
    // 图片文件管理
    QString saveImageFile(const QString& sourceImagePath);
    bool deleteImageFile(const QString& imageFilename);
    QString getImageFilePath(const QString& imageFilename);
    
    // 数据库维护
    bool clearAllHistory();
    int getRecordCount();
    bool optimizeDatabase();
    
    // 获取存储路径
    static QString getDatabasePath();
    static QString getImagesDirectory();

signals:
    void databaseError(const QString& error);
    void recordAdded(int recordId);
    void recordDeleted(int recordId);

private:
    QSqlDatabase m_database;
    QString m_databasePath;
    QString m_imagesDirectory;
    
    // 私有方法
    bool createTables();
    bool createDirectories();
    QString generateImageFilename(const QString& sourcePath);
    bool copyImageFile(const QString& sourcePath, const QString& destPath);
};

#endif // DATABASEMANAGER_H
