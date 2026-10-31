#ifndef INFOWINDOW_H
#define INFOWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>

QT_BEGIN_NAMESPACE
namespace Ui { class InfoWindow; }
QT_END_NAMESPACE

class InfoWindow : public QWidget
{
    Q_OBJECT

public:
    explicit InfoWindow(QWidget *parent = nullptr);
    ~InfoWindow();

private slots:
    void onCloseClicked();

private:
    void setupUI();
    
    // UI指针
    Ui::InfoWindow *ui;
};

#endif // INFOWINDOW_H

