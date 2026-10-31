#include "DatabaseManager.h"
#include <QSqlError>
#include <QFileInfo>
#include <QDebug>
#include <QUuid>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
    m_databasePath = getDatabasePath();
    m_imagesDirectory = getImagesDirectory();
}

DatabaseManager::~DatabaseManager()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
}

bool DatabaseManager::initializeDatabase()
{
    // 检查 SQL 驱动
    qDebug() << "[Database] 可用的 SQL 驱动:" << QSqlDatabase::drivers();

    // 创建必要的目录
    if (!createDirectories()) {
        emit databaseError("无法创建存储目录");
        return false;
    }
    
    // 设置数据库连接
    m_database = QSqlDatabase::addDatabase("QSQLITE", "HistoryDatabase");
    m_database.setDatabaseName(m_databasePath);

    qDebug() << "[Database] 数据库路径:" << m_databasePath;

    // 打开数据库
    if (!m_database.open()) {
        emit databaseError("无法打开数据库: " + m_database.lastError().text());
        return false;
    }
    
    // 创建表
    if (!createTables()) {
        emit databaseError("无法创建数据库表");
        return false;
    }
    
    qDebug() << "[Database] 数据库初始化成功:" << m_databasePath;
    return true;
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_database);
    
    // 创建历史记录表
    QString createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS history_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME NOT NULL,
            image_filename TEXT NOT NULL,
            plant_name TEXT NOT NULL,
            confidence REAL NOT NULL,
            is_favorite BOOLEAN DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createTableSQL)) {
        qWarning() << "[Database] 创建表失败:" << query.lastError().text();
        return false;
    }
    
    // 检查是否需要添加收藏字段（数据库迁移）
    QString checkColumnSQL = "PRAGMA table_info(history_records)";
    if (query.exec(checkColumnSQL)) {
        bool hasFavoriteColumn = false;
        while (query.next()) {
            if (query.value("name").toString() == "is_favorite") {
                hasFavoriteColumn = true;
                break;
            }
        }
        
        // 如果没有收藏字段，则添加
        if (!hasFavoriteColumn) {
            QString addColumnSQL = "ALTER TABLE history_records ADD COLUMN is_favorite BOOLEAN DEFAULT 0";
            if (!query.exec(addColumnSQL)) {
                qWarning() << "[Database] 添加收藏字段失败:" << query.lastError().text();
                return false;
            }
            qDebug() << "[Database] 成功添加收藏字段";
        }
    }
    
    // 创建索引以提高查询性能（分开执行，SQLite 不支持一次执行多条语句）
    QStringList indexSQLs = {
        "CREATE INDEX IF NOT EXISTS idx_timestamp ON history_records(timestamp)",
        "CREATE INDEX IF NOT EXISTS idx_plant_name ON history_records(plant_name)",
        "CREATE INDEX IF NOT EXISTS idx_confidence ON history_records(confidence)"
    };

    for (const QString& sql : indexSQLs) {
        if (!query.exec(sql)) {
            qWarning() << "[Database] 创建索引失败:" << query.lastError().text() << ", SQL:" << sql;
        }
    }
    
    qDebug() << "[Database] 数据库表创建成功";
    return true;
}

bool DatabaseManager::createDirectories()
{
    QDir dir;
    
    // 创建应用数据目录
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dir.exists(appDataDir)) {
        if (!dir.mkpath(appDataDir)) {
            qWarning() << "[Database] 无法创建应用数据目录:" << appDataDir;
            return false;
        }
    }
    
    // 创建图片存储目录
    if (!dir.exists(m_imagesDirectory)) {
        if (!dir.mkpath(m_imagesDirectory)) {
            qWarning() << "[Database] 无法创建图片存储目录:" << m_imagesDirectory;
            return false;
        }
    }
    
    qDebug() << "[Database] 目录创建成功:" << m_imagesDirectory;
    return true;
}

int DatabaseManager::addHistoryRecord(const HistoryRecord& record)
{
    QSqlQuery query(m_database);
    query.prepare("INSERT INTO history_records (timestamp, image_filename, plant_name, confidence, is_favorite) "
                  "VALUES (?, ?, ?, ?, ?)");

    query.addBindValue(record.timestamp);
    query.addBindValue(record.imageFilename);
    query.addBindValue(record.plantName);
    query.addBindValue(record.confidence);
    query.addBindValue(record.isFavorite ? 1 : 0);

    qDebug() << "[Database] 准备插入记录:" << record.plantName << ", confidence:" << record.confidence;
    qDebug() << "[Database] timestamp:" << record.timestamp << ", imageFilename:" << record.imageFilename;
    
    if (!query.exec()) {
        emit databaseError("添加历史记录失败: " + query.lastError().text());
        return -1;
    }
    
    int recordId = query.lastInsertId().toInt();
    qDebug() << "[Database] 历史记录添加成功, ID:" << recordId;
    emit recordAdded(recordId);
    return recordId;
}

bool DatabaseManager::deleteHistoryRecord(int id)
{
    // 先获取图片文件名
    QSqlQuery selectQuery(m_database);
    selectQuery.prepare("SELECT image_filename FROM history_records WHERE id = ?");
    selectQuery.addBindValue(id);
    
    if (!selectQuery.exec() || !selectQuery.next()) {
        emit databaseError("查找记录失败");
        return false;
    }
    
    QString imageFilename = selectQuery.value(0).toString();
    
    // 删除数据库记录
    QSqlQuery deleteQuery(m_database);
    deleteQuery.prepare("DELETE FROM history_records WHERE id = ?");
    deleteQuery.addBindValue(id);
    
    if (!deleteQuery.exec()) {
        emit databaseError("删除历史记录失败: " + deleteQuery.lastError().text());
        return false;
    }
    
    // 删除对应的图片文件
    deleteImageFile(imageFilename);
    
    qDebug() << "[Database] 历史记录删除成功, ID:" << id;
    emit recordDeleted(id);
    return true;
}

QList<HistoryRecord> DatabaseManager::getAllHistoryRecords()
{
    qDebug() << "[Database] ========== 查询所有历史记录 ==========";
    qDebug() << "[Database] m_database 指针:" << &m_database;
    qDebug() << "[Database] m_database.isOpen:" << m_database.isOpen();

    QList<HistoryRecord> records;
    QSqlQuery query(m_database);

    query.prepare(R"(
        SELECT id, timestamp, image_filename, plant_name, confidence, is_favorite, created_at
        FROM history_records
        ORDER BY timestamp DESC
    )");
    
    if (!query.exec()) {
        emit databaseError("查询历史记录失败: " + query.lastError().text());
        return records;
    }
    
    while (query.next()) {
        HistoryRecord record;
        record.id = query.value(0).toInt();
        record.timestamp = query.value(1).toDateTime();
        record.imageFilename = query.value(2).toString();
        record.plantName = query.value(3).toString();
        record.confidence = query.value(4).toDouble();
        record.isFavorite = query.value(5).toBool();
        record.createdAt = query.value(6).toDateTime();
        records.append(record);
    }

    qDebug() << "[Database] 查询完成，记录数:" << records.size();
    qDebug() << "[Database] ========== 查询结束 ==========";
    return records;
}

QList<HistoryRecord> DatabaseManager::getHistoryRecordsByDateRange(const QDateTime& start, const QDateTime& end)
{
    QList<HistoryRecord> records;
    QSqlQuery query(m_database);
    
    query.prepare(R"(
        SELECT id, timestamp, image_filename, plant_name, confidence, created_at
        FROM history_records
        WHERE timestamp BETWEEN ? AND ?
        ORDER BY timestamp DESC
    )");
    
    query.addBindValue(start);
    query.addBindValue(end);
    
    if (!query.exec()) {
        emit databaseError("按日期范围查询失败: " + query.lastError().text());
        return records;
    }
    
    while (query.next()) {
        HistoryRecord record;
        record.id = query.value(0).toInt();
        record.timestamp = query.value(1).toDateTime();
        record.imageFilename = query.value(2).toString();
        record.plantName = query.value(3).toString();
        record.confidence = query.value(4).toDouble();
        record.createdAt = query.value(5).toDateTime();
        records.append(record);
    }
    
    return records;
}

QList<HistoryRecord> DatabaseManager::getHistoryRecordsByPlantName(const QString& plantName)
{
    QList<HistoryRecord> records;
    QSqlQuery query(m_database);
    
    query.prepare(R"(
        SELECT id, timestamp, image_filename, plant_name, confidence, created_at
        FROM history_records
        WHERE plant_name LIKE ?
        ORDER BY timestamp DESC
    )");
    
    query.addBindValue("%" + plantName + "%");
    
    if (!query.exec()) {
        emit databaseError("按植物名称查询失败: " + query.lastError().text());
        return records;
    }
    
    while (query.next()) {
        HistoryRecord record;
        record.id = query.value(0).toInt();
        record.timestamp = query.value(1).toDateTime();
        record.imageFilename = query.value(2).toString();
        record.plantName = query.value(3).toString();
        record.confidence = query.value(4).toDouble();
        record.createdAt = query.value(5).toDateTime();
        records.append(record);
    }
    
    return records;
}

QList<HistoryRecord> DatabaseManager::getHistoryRecordsByConfidenceRange(double minConf, double maxConf)
{
    QList<HistoryRecord> records;
    QSqlQuery query(m_database);
    
    query.prepare(R"(
        SELECT id, timestamp, image_filename, plant_name, confidence, created_at
        FROM history_records
        WHERE confidence BETWEEN ? AND ?
        ORDER BY timestamp DESC
    )");
    
    query.addBindValue(minConf);
    query.addBindValue(maxConf);
    
    if (!query.exec()) {
        emit databaseError("按置信度范围查询失败: " + query.lastError().text());
        return records;
    }
    
    while (query.next()) {
        HistoryRecord record;
        record.id = query.value(0).toInt();
        record.timestamp = query.value(1).toDateTime();
        record.imageFilename = query.value(2).toString();
        record.plantName = query.value(3).toString();
        record.confidence = query.value(4).toDouble();
        record.createdAt = query.value(5).toDateTime();
        records.append(record);
    }
    
    return records;
}

QString DatabaseManager::saveImageFile(const QString& sourceImagePath)
{
    QFileInfo sourceInfo(sourceImagePath);
    if (!sourceInfo.exists()) {
        emit databaseError("源图片文件不存在: " + sourceImagePath);
        return QString();
    }
    
    QString filename = generateImageFilename(sourceImagePath);
    QString destPath = QDir(m_imagesDirectory).filePath(filename);
    
    if (!copyImageFile(sourceImagePath, destPath)) {
        emit databaseError("复制图片文件失败");
        return QString();
    }
    
    qDebug() << "[Database] 图片文件保存成功:" << filename;
    return filename;
}

bool DatabaseManager::deleteImageFile(const QString& imageFilename)
{
    QString filePath = getImageFilePath(imageFilename);
    QFile file(filePath);
    
    if (file.exists()) {
        if (file.remove()) {
            qDebug() << "[Database] 图片文件删除成功:" << imageFilename;
            return true;
        } else {
            qWarning() << "[Database] 图片文件删除失败:" << imageFilename;
            return false;
        }
    }
    
    return true; // 文件不存在也算删除成功
}

QString DatabaseManager::getImageFilePath(const QString& imageFilename)
{
    return QDir(m_imagesDirectory).filePath(imageFilename);
}

bool DatabaseManager::clearAllHistory()
{
    QSqlQuery query(m_database);
    
    // 删除所有数据库记录
    if (!query.exec("DELETE FROM history_records")) {
        emit databaseError("清空历史记录失败: " + query.lastError().text());
        return false;
    }
    
    // 删除所有图片文件
    QDir imagesDir(m_imagesDirectory);
    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif";
    QFileInfoList files = imagesDir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : files) {
        QFile::remove(fileInfo.absoluteFilePath());
    }
    
    qDebug() << "[Database] 所有历史记录已清空";
    return true;
}

int DatabaseManager::getRecordCount()
{
    QSqlQuery query(m_database);
    if (!query.exec("SELECT COUNT(*) FROM history_records")) {
        emit databaseError("获取记录数量失败: " + query.lastError().text());
        return -1;
    }
    
    if (query.next()) {
        return query.value(0).toInt();
    }
    
    return 0;
}

bool DatabaseManager::optimizeDatabase()
{
    QSqlQuery query(m_database);
    if (!query.exec("VACUUM")) {
        emit databaseError("数据库优化失败: " + query.lastError().text());
        return false;
    }
    
    qDebug() << "[Database] 数据库优化完成";
    return true;
}

QString DatabaseManager::getDatabasePath()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appDataDir).filePath("history.db");
}

QString DatabaseManager::getImagesDirectory()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appDataDir).filePath("images");
}

// 收藏功能实现
bool DatabaseManager::setRecordFavorite(int recordId, bool isFavorite)
{
    QSqlQuery query(m_database);
    query.prepare("UPDATE history_records SET is_favorite = ? WHERE id = ?");
    query.addBindValue(isFavorite ? 1 : 0);
    query.addBindValue(recordId);
    
    if (!query.exec()) {
        emit databaseError("设置收藏状态失败: " + query.lastError().text());
        return false;
    }
    
    qDebug() << "[Database] 记录" << recordId << "收藏状态已设置为:" << isFavorite;
    return true;
}

QList<HistoryRecord> DatabaseManager::getFavoriteRecords()
{
    QList<HistoryRecord> records;
    QSqlQuery query(m_database);
    
    query.prepare(R"(
        SELECT id, timestamp, image_filename, plant_name, confidence, is_favorite, created_at
        FROM history_records
        WHERE is_favorite = 1
        ORDER BY timestamp DESC
    )");
    
    if (!query.exec()) {
        emit databaseError("查询收藏记录失败: " + query.lastError().text());
        return records;
    }
    
    while (query.next()) {
        HistoryRecord record;
        record.id = query.value(0).toInt();
        record.timestamp = query.value(1).toDateTime();
        record.imageFilename = query.value(2).toString();
        record.plantName = query.value(3).toString();
        record.confidence = query.value(4).toDouble();
        record.isFavorite = query.value(5).toBool();
        record.createdAt = query.value(6).toDateTime();
        records.append(record);
    }
    
    return records;
}

bool DatabaseManager::isRecordFavorite(int recordId)
{
    QSqlQuery query(m_database);
    query.prepare("SELECT is_favorite FROM history_records WHERE id = ?");
    query.addBindValue(recordId);
    
    if (!query.exec() || !query.next()) {
        return false;
    }
    
    return query.value(0).toBool();
}

QString DatabaseManager::generateImageFilename(const QString& sourcePath)
{
    QFileInfo sourceInfo(sourcePath);
    QString extension = sourceInfo.suffix();
    if (extension.isEmpty()) {
        extension = "jpg"; // 默认扩展名
    }
    
    // 使用时间戳和UUID生成唯一文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    
    return QString("history_%1_%2.%3").arg(timestamp, uuid, extension);
}

bool DatabaseManager::copyImageFile(const QString& sourcePath, const QString& destPath)
{
    return QFile::copy(sourcePath, destPath);
}
