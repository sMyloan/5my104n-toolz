#pragma once
#include <QWidget>
#include <QDialog>
#include <QTimer>
#include <QThread>
#include <QMap>
#include <QColor>
#include <QList>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QCloseEvent>
#include <QPixmap>

class QMouseEvent;

struct RadarDevice {
    QString name;
    QString mac;
    int rssi = -70;
    QString devType;
    double firstSeen = 0;
    double lastSeen = 0;
    double angle = 0;
    double currentDistance = 100;
    double targetDistance = 100;
    QList<int> rssiHistory;
    double smoothedRssi = -70;

    static double macToAngle(const QString& mac);
    static double rssiToDistance(int rssi);
    void updateRssi(int newRssi);
    void animateStep(double speed = 0.08);
    void getPosition(int cx, int cy, int maxRadius, int& x, int& y) const;
};

class RadarScanner : public QThread {
    Q_OBJECT
public:
    explicit RadarScanner(QObject* parent = nullptr);
    void run() override;
    void requestStop() { m_stop = true; }
signals:
    void devicesFound(const QList<RadarDevice>& devices);
    void scanProgress(const QString& stage);
private:
    QList<RadarDevice> scanBle();
    QList<RadarDevice> scanClassic();
    QList<RadarDevice> scanWifi();
    QMap<QString, RadarDevice> m_devices;
    bool m_stop = false;
};

class RadarWidget : public QWidget {
    Q_OBJECT
public:
    explicit RadarWidget(QWidget* parent = nullptr);
    void setTheme(const QString& key);
    void updateDevices(const QList<RadarDevice>& devices);
    void setScanning(bool on);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    bool showBle = true, showClassic = true, showWifi = true;
    double zoom = 1.0;
protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
private slots:
    void animate();
private:
    void drawDevice(class QPainter& p, const RadarDevice& d, int cx, int cy, int radius);
    void rebuildStaticLayer();
    QList<RadarDevice> m_devices;
    QTimer m_animTimer;
    double m_minZoom = 0.3, m_maxZoom = 50.0;
    int m_maxRangeMeters = 100;
    QMap<QString, QColor> m_colors;
    bool m_scanning = false;
    double m_sweepAngle = 0;
    QPixmap m_staticLayer;
    bool m_staticDirty = true;
    void applyDefaultColors();
};

class RadarWindow : public QDialog {
    Q_OBJECT
public:
    explicit RadarWindow(QWidget* parent = nullptr);
    void setTheme(const QString& key);
protected:
    void closeEvent(QCloseEvent* event) override;
private slots:
    void onStart();
    void onStop();
    void onFilterChanged();
    void onDevicesFound(const QList<RadarDevice>& devices);
private:
    RadarWidget* m_radar = nullptr;
    RadarScanner* m_scanner = nullptr;
    QCheckBox* m_cbBle = nullptr;
    QCheckBox* m_cbClassic = nullptr;
    QCheckBox* m_cbWifi = nullptr;
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnStop = nullptr;
    QLabel* m_statsLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_legendLabel = nullptr;
    int m_deviceCount = 0;
};