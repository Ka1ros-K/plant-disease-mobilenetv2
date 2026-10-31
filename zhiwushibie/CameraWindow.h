#ifndef CAMERAWINDOW_H
#define CAMERAWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QTimer>
#include <opencv2/opencv.hpp>

class CameraWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CameraWindow(QWidget *parent = nullptr);
    ~CameraWindow();

signals:
    void imageCaptured(const QPixmap& image);

private slots:
    void onCaptureClicked();
    void onCancelClicked();
    void onPreviewTimer();

private:
    void setupUI();
    void setupCamera();
    void showError(const QString& message);
    QPixmap matToQPixmap(const cv::Mat& mat);

    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_buttonLayout;
    QLabel* m_viewfinder;
    QPushButton* m_captureButton;
    QPushButton* m_cancelButton;
    QLabel* m_statusLabel;
    
    cv::VideoCapture* m_camera;
    QTimer* m_previewTimer;
    bool m_cameraActive;
};

#endif // CAMERAWINDOW_H