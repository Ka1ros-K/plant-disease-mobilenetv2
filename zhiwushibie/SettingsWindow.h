#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui { class SettingsWindow; }
QT_END_NAMESPACE

class SettingsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent = nullptr);
    ~SettingsWindow();

    // 获取设置值
    bool isAutoSaveEnabled() const;

    // 设置值
    void setAutoSaveEnabled(bool enabled);

signals:
    // 设置变化信号
    void autoSaveChanged(bool enabled);

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onApplyClicked();
    void onAutoSaveToggled(bool checked);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void applySettings();
    
    // UI指针
    Ui::SettingsWindow *ui;
    
    // 设置存储
    QSettings *m_settings;
    
    // 当前设置值
    bool m_autoSaveEnabled;
    QString m_currentTheme;
    
    // 原始设置值（用于取消操作）
    bool m_originalAutoSaveEnabled;
    QString m_originalTheme;
};

#endif // SETTINGSWINDOW_H
