#include "RadarWidget.h"
#include "I18N.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QDateTime>
#include <QCloseEvent>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <cmath>
#include <ctime>

static QString radarConfigDir() { return QDir::homePath() + "/.config/5my104n_toolz"; }
static QString radarConfigFile() { return radarConfigDir() + "/radar_settings.json"; }

static QJsonObject loadRadarConfig() {
    QFile f(radarConfigFile());
    if (f.open(QIODevice::ReadOnly)) {
        auto d = QJsonDocument::fromJson(f.readAll());
        if (d.isObject()) return d.object();
    }
    QJsonObject def;
    def["zoom"] = 1.0; def["show_ble"] = true;
    def["show_classic"] = true; def["show_wifi"] = true;
    return def;
}

static void saveRadarConfig(const QJsonObject& cfg) {
    QDir().mkpath(radarConfigDir());
    QFile f(radarConfigFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
}

double RadarDevice::macToAngle(const QString& mac) {
    QByteArray hash = QCryptographicHash::hash(mac.toUtf8(), QCryptographicHash::Md5).toHex();
    bool ok;
    quint32 v = hash.left(4).toUInt(&ok, 16);
    return (ok ? v : 0) % 360;
}

double RadarDevice::rssiToDistance(int rssi) {
    const double txPower = -59.0, n = 3.0;
    if (rssi >= 0) return 0.1;
    if (rssi <= -100) return 100.0;
    double d = std::pow(10.0, (txPower - rssi) / (10.0 * n));
    return std::max(0.1, std::min(d, 100.0));
}

void RadarDevice::updateRssi(int newRssi) {
    lastSeen = std::time(nullptr);
    rssiHistory.append(newRssi);
    if (rssiHistory.size() > 10) rssiHistory.removeFirst();
    double sum = 0; for (int v : rssiHistory) sum += v;
    smoothedRssi = sum / rssiHistory.size();
    targetDistance = rssiToDistance((int)smoothedRssi);
}

void RadarDevice::animateStep(double speed) {
    currentDistance += (targetDistance - currentDistance) * speed;
}

void RadarDevice::getPosition(int cx, int cy, int maxRadius, int& x, int& y) const {
    double scaled = (currentDistance / 100.0) * maxRadius;
    double rad = angle * M_PI / 180.0;
    x = cx + (int)(scaled * std::cos(rad));
    y = cy + (int)(scaled * std::sin(rad));
}

RadarScanner::RadarScanner(QObject* parent) : QThread(parent) {}

static QString runProc(const QStringList& args, int timeoutMs = 5000) {
    QProcess p;
    p.start(args.first(), args.mid(1));
    if (!p.waitForStarted(1000)) return QString();
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
    }
    return QString::fromUtf8(p.readAllStandardOutput());
}

QList<RadarDevice> RadarScanner::scanBle() {
    QList<RadarDevice> out;
    try {
        QProcess proc;
        proc.start("hcitool", QStringList{"lescan", "--duplicates"});
        if (!proc.waitForStarted(1000)) return out;
        QThread::msleep(5000);
        proc.terminate();
        if (!proc.waitForFinished(2000)) {
            proc.kill();
            proc.waitForFinished(1000);
        }
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        static QRegularExpression re(R"(([0-9A-Fa-f:]{17})\s+\(.*?\),\s+RSSI:\s*(-?\d+))");
        for (const QString& line : output.split('\n')) {
            auto m = re.match(line);
            if (m.hasMatch()) {
                RadarDevice d;
                d.mac = m.captured(1).toUpper();
                d.rssi = m.captured(2).toInt();
                d.devType = "BLE";
                d.name = "Unknown";
                out.append(d);
            }
        }
    } catch (...) {}
    return out;
}

QList<RadarDevice> RadarScanner::scanClassic() {
    QList<RadarDevice> out;
    try {
        QString s = runProc({"bluetoothctl", "devices"}, 5000);
        static QRegularExpression re(R"(Device\s+([0-9A-Fa-f:]{17})\s+(.+))");
        for (const QString& line : s.split('\n')) {
            auto m = re.match(line);
            if (m.hasMatch()) {
                RadarDevice d;
                d.mac = m.captured(1).toUpper();
                d.name = m.captured(2).trimmed();
                d.devType = "Classic";
                if (!d.name.isEmpty() && d.name.toLower() != "unknown") {
                    QString rssiOut = runProc({"hcitool", "rssi", d.mac}, 3000);
                    static QRegularExpression rr(R"(RSSI return value:\s*(-?\d+))");
                    auto rm = rr.match(rssiOut);
                    d.rssi = rm.hasMatch() ? rm.captured(1).toInt() : -70;
                    out.append(d);
                }
            }
        }
    } catch (...) {}
    return out;
}

QList<RadarDevice> RadarScanner::scanWifi() {
    QList<RadarDevice> out;
    try {
        QString s = runProc({"iw", "dev", "wlan0", "scan"}, 10000);
        QString curBssid, curSsid;
        int curSignal = -1000;
        auto flush = [&]() {
            if (!curBssid.isEmpty() && curSignal > -1000) {
                RadarDevice d;
                d.mac = curBssid;
                d.name = curSsid.isEmpty() ? "Unknown" : curSsid;
                d.rssi = curSignal;
                d.devType = "WiFi";
                out.append(d);
            }
        };
        static QRegularExpression bssRe(R"(([0-9A-Fa-f:]{17}))");
        static QRegularExpression sigRe(R"(signal:\s*(-?\d+\.\d+))");
        for (const QString& line : s.split('\n')) {
            if (line.contains("BSS")) {
                flush();
                auto m = bssRe.match(line);
                curBssid = m.hasMatch() ? m.captured(1).toUpper() : QString();
                curSsid.clear();
                curSignal = -1000;
            } else if (line.contains("SSID:")) {
                curSsid = line.mid(line.indexOf("SSID:") + 5).trimmed();
            } else if (line.contains("signal:")) {
                auto m = sigRe.match(line);
                if (m.hasMatch()) curSignal = (int)m.captured(1).toDouble();
            }
        }
        flush();
    } catch (...) {}
    return out;
}

void RadarScanner::run() {
    while (!m_stop) {
        try {
            emit scanProgress(trn("radar_scan_ble"));
            const QList<RadarDevice> ble = scanBle();
            if (m_stop) break;
            emit scanProgress(trn("radar_scan_classic"));
            const QList<RadarDevice> classic = scanClassic();
            if (m_stop) break;
            emit scanProgress(trn("radar_scan_wifi"));
            const QList<RadarDevice> wifi = scanWifi();
            auto all = ble + classic + wifi;
            for (const auto& d : all) {
                auto it = m_devices.find(d.mac);
                if (it == m_devices.end()) {
                    RadarDevice nd = d;
                    nd.firstSeen = nd.lastSeen = std::time(nullptr);
                    nd.angle = RadarDevice::macToAngle(d.mac);
                    nd.currentDistance = 100;
                    nd.targetDistance = RadarDevice::rssiToDistance(d.rssi);
                    nd.smoothedRssi = d.rssi;
                    nd.rssiHistory = {d.rssi};
                    m_devices[d.mac] = nd;
                } else {
                    it->updateRssi(d.rssi);
                    if (!d.name.isEmpty() && d.name != "Unknown") it->name = d.name;
                }
            }
            QList<RadarDevice> list;
            for (const auto& d : m_devices) list.append(d);
            emit devicesFound(list);
        } catch (...) {}
        for (int i = 0; i < 30 && !m_stop; ++i) QThread::msleep(100);
    }
}

RadarWidget::RadarWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(500, 500);
    setMouseTracking(true);
    auto cfg = loadRadarConfig();
    zoom = cfg.value("zoom").toDouble(1.0);
    showBle = cfg.value("show_ble").toBool(true);
    showClassic = cfg.value("show_classic").toBool(true);
    showWifi = cfg.value("show_wifi").toBool(true);
    applyDefaultColors();
    connect(&m_animTimer, &QTimer::timeout, this, &RadarWidget::animate);
    m_animTimer.start(16);
}

void RadarWidget::applyDefaultColors() {
    m_colors = {
        {"bg", QColor(0, 10, 0)},
        {"grid", QColor(0, 80, 0, 60)},
        {"device_ble", QColor(0, 200, 255)},
        {"device_classic", QColor(255, 200, 0)},
        {"device_wifi", QColor(255, 0, 100)},
        {"device_glow_ble", QColor(0, 200, 255, 100)},
        {"device_glow_classic", QColor(255, 200, 0, 100)},
        {"device_glow_wifi", QColor(255, 0, 100, 100)},
        {"text", QColor(180, 255, 180)},
        {"center", QColor(0, 255, 128)},
        {"border", QColor(0, 255, 128)},
    };
}

void RadarWidget::setTheme(const QString& key) {
    if (key == "hack") {
        m_colors = {
            {"bg", QColor(0,0,0)}, {"grid", QColor(0,255,0,40)},
            {"device_ble", QColor(0,255,0)}, {"device_classic", QColor(0,255,0)},
            {"device_wifi", QColor(0,255,0)},
            {"device_glow_ble", QColor(0,255,0,80)},
            {"device_glow_classic", QColor(0,255,0,80)},
            {"device_glow_wifi", QColor(0,255,0,80)},
            {"text", QColor(0,255,0)}, {"center", QColor(0,255,0)}, {"border", QColor(0,255,0)},
        };
    } else if (key == "arasaka") {
        m_colors = {
            {"bg", QColor(13,13,13)}, {"grid", QColor(204,0,0,40)},
            {"device_ble", QColor(204,0,0)}, {"device_classic", QColor(204,0,0)},
            {"device_wifi", QColor(204,0,0)},
            {"device_glow_ble", QColor(204,0,0,80)},
            {"device_glow_classic", QColor(204,0,0,80)},
            {"device_glow_wifi", QColor(204,0,0,80)},
            {"text", QColor(224,224,224)}, {"center", QColor(204,0,0)}, {"border", QColor(204,0,0)},
        };
    } else if (key == "light") {
        //исправлен контраст для светлой темы
        m_colors = {
            {"bg", QColor(245,245,245)}, {"grid", QColor(0,0,0,30)},
            {"device_ble", QColor(0,100,200)}, {"device_classic", QColor(0,100,200)},
            {"device_wifi", QColor(0,100,200)},
            {"device_glow_ble", QColor(0,100,200,80)},
            {"device_glow_classic", QColor(0,100,200,80)},
            {"device_glow_wifi", QColor(0,100,200,80)},
            {"text", QColor(30,30,30)}, {"center", QColor(0,100,200)}, {"border", QColor(0,100,200)},
        };
    } else {
        applyDefaultColors();
    }
    m_staticDirty = true;
    update();
}

void RadarWidget::setScanning(bool on) {
    if (m_scanning == on) return;
    m_scanning = on;
    update();
}

void RadarWidget::updateDevices(const QList<RadarDevice>& devices) {
    m_devices = devices;
    update();
}

void RadarWidget::zoomIn() {
    zoom = std::min(m_maxZoom, zoom * 1.2);
    m_staticDirty = true;
    QJsonObject cfg = loadRadarConfig(); cfg["zoom"] = zoom; saveRadarConfig(cfg);
    update();
}

void RadarWidget::zoomOut() {
    zoom = std::max(m_minZoom, zoom / 1.2);
    m_staticDirty = true;
    QJsonObject cfg = loadRadarConfig(); cfg["zoom"] = zoom; saveRadarConfig(cfg);
    update();
}

void RadarWidget::resetZoom() {
    zoom = 1.0;
    m_staticDirty = true;
    QJsonObject cfg = loadRadarConfig(); cfg["zoom"] = zoom; saveRadarConfig(cfg);
    update();
}

void RadarWidget::wheelEvent(QWheelEvent* e) {
    if (e->angleDelta().y() > 0) zoomIn(); else zoomOut();
}

void RadarWidget::animate() {
    for (auto& d : m_devices) d.animateStep(0.08);
    if (m_scanning) {
        m_sweepAngle += 2.0;
        if (m_sweepAngle >= 360.0) m_sweepAngle -= 360.0;
    }
    update();
}

void RadarWidget::rebuildStaticLayer() {
    const QSize sz = size();
    if (sz.isEmpty()) return;
    m_staticLayer = QPixmap(sz);
    m_staticLayer.fill(m_colors["bg"]);
    QPainter p(&m_staticLayer);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const int w = sz.width(), h = sz.height();
    const int cx = w / 2, cy = h / 2;
    const int baseR = std::min(w, h) / 2 - 30;
    const int radius = (int)(baseR * zoom);
    p.setPen(QPen(m_colors["grid"], 1));
    for (int i = 1; i <= 5; ++i) {
        const int r = radius * i / 5;
        p.drawEllipse(QPoint(cx, cy), r, r);
    }
    p.setPen(m_colors["text"]);
    p.setFont(QFont("Consolas", 8));
    for (int i = 1; i <= 5; ++i) {
        const int r = radius * i / 5;
        const int m = m_maxRangeMeters * i / 5;
        p.drawText(cx + 5, cy - r + 10, QString("%1m").arg(m));
    }
    p.setPen(QPen(m_colors["grid"], 1));
    for (int i = 0; i < 360; i += 30) {
        const double rad = i * M_PI / 180.0;
        p.drawLine(cx, cy, cx + (int)(radius * std::cos(rad)), cy + (int)(radius * std::sin(rad)));
    }
    m_staticDirty = false;
}

void RadarWidget::paintEvent(QPaintEvent*) {
    if (m_staticDirty || m_staticLayer.size() != size())
        rebuildStaticLayer();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.drawPixmap(0, 0, m_staticLayer);
    const int w = width(), h = height();
    const int cx = w / 2, cy = h / 2;
    const int baseR = std::min(w, h) / 2 - 30;
    const int radius = (int)(baseR * zoom);
    if (m_scanning && radius > 0) {
        QColor sweep = m_colors["center"];
        sweep.setAlpha(38);
        p.setPen(Qt::NoPen);
        p.setBrush(sweep);
        p.drawPie(QRect(cx - radius, cy - radius, radius * 2, radius * 2),
                  int(-(m_sweepAngle + 28.0) * 16), int(28 * 16));
        sweep.setAlpha(170);
        p.setPen(QPen(sweep, 2));
        const double rad = m_sweepAngle * M_PI / 180.0;
        p.drawLine(cx, cy, cx + (int)(radius * std::cos(rad)), cy + (int)(radius * std::sin(rad)));
    }
    for (const auto& d : m_devices) {
        if (d.devType == "BLE" && showBle) drawDevice(p, d, cx, cy, radius);
        else if (d.devType == "Classic" && showClassic) drawDevice(p, d, cx, cy, radius);
        else if (d.devType == "WiFi" && showWifi) drawDevice(p, d, cx, cy, radius);
    }
    p.setBrush(m_colors["center"]);
    p.setPen(QPen(m_colors["border"], 1));
    p.drawEllipse(QPoint(cx, cy), 4, 4);
}

void RadarWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPoint pos = event->position().toPoint();
    const int w = width(), h = height();
    const int cx = w / 2, cy = h / 2;
    const int baseR = std::min(w, h) / 2 - 30;
    const int radius = (int)(baseR * zoom);
    for (const auto& d : m_devices) {
        if (d.devType == "BLE" && !showBle) continue;
        if (d.devType == "Classic" && !showClassic) continue;
        if (d.devType == "WiFi" && !showWifi) continue;
        int x = 0, y = 0;
        d.getPosition(cx, cy, radius, x, y);
        if ((pos - QPoint(x, y)).manhattanLength() > 12) continue;
        const QString when = QDateTime::fromSecsSinceEpoch((qint64)d.lastSeen).toString("HH:mm:ss");
        QToolTip::showText(event->globalPosition().toPoint(),
                           QString("%1\nMAC: %2\nRSSI: %3 dBm\nLast seen: %4")
                           .arg(d.name.isEmpty() ? QString("Unknown") : d.name, d.mac)
                           .arg((int)d.smoothedRssi)
                           .arg(when),
                           this);
        return;
    }
    QToolTip::hideText();
    QWidget::mouseMoveEvent(event);
}

void RadarWidget::drawDevice(QPainter& p, const RadarDevice& d, int cx, int cy, int radius) {
    int x, y;
    d.getPosition(cx, cy, radius, x, y);
    int strength = std::max(0, std::min(100, (int)(100 - std::abs(d.smoothedRssi))));
    int size = std::max(6, (int)(12.0 * strength / 100.0));
    QColor glow, col;
    if (d.devType == "BLE") { glow = m_colors["device_glow_ble"]; col = m_colors["device_ble"]; }
    else if (d.devType == "Classic") { glow = m_colors["device_glow_classic"]; col = m_colors["device_classic"]; }
    else { glow = m_colors["device_glow_wifi"]; col = m_colors["device_wifi"]; }
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(QPoint(x, y), size * 2, size * 2);
    p.setBrush(col);
    p.setPen(QPen(col, 1));
    p.drawEllipse(QPoint(x, y), size / 2, size / 2);
    if (d.currentDistance < 70) {
        p.setPen(m_colors["text"]);
        p.setFont(QFont("Consolas", 8));
        p.drawText(x + 10, y - 10, d.name.left(20));
        p.drawText(x + 10, y, QString("%1dBm").arg((int)d.smoothedRssi));
    }
}

RadarWindow::RadarWindow(QWidget* parent) : QDialog(parent) {
//переведенный заголовок окна
setWindowTitle(trn("radar_window_title"));
setMinimumSize(700, 750); resize(700, 750); setWindowFlags(Qt::Window);
auto* layout = new QVBoxLayout(this); layout->setSpacing(10);
m_titleLabel = new QLabel(trn("radar_title"));
m_titleLabel->setStyleSheet("QLabel{color:#00ff80;font-size:18px;font-weight:bold;font-family:Consolas;padding:10px;border-bottom:2px solid #00ff80;}");
m_titleLabel->setAlignment(Qt::AlignCenter); layout->addWidget(m_titleLabel);
m_radar = new RadarWidget(); layout->addWidget(m_radar);
auto* zoomRow = new QHBoxLayout();
auto* bIn = new QPushButton(trn("zoom_in")); connect(bIn, &QPushButton::clicked, m_radar, &RadarWidget::zoomIn);
auto* bOut = new QPushButton(trn("zoom_out")); connect(bOut, &QPushButton::clicked, m_radar, &RadarWidget::zoomOut);
auto* bRst = new QPushButton(trn("zoom_reset")); connect(bRst, &QPushButton::clicked, m_radar, &RadarWidget::resetZoom);
zoomRow->addWidget(bIn); zoomRow->addWidget(bOut); zoomRow->addWidget(bRst); layout->addLayout(zoomRow);
auto* fg = new QGroupBox(trn("device_filters"));
auto* fl = new QHBoxLayout(fg);
m_cbBle = new QCheckBox(trn("filter_ble")); m_cbBle->setChecked(m_radar->showBle);
connect(m_cbBle, &QCheckBox::checkStateChanged, this, &RadarWindow::onFilterChanged);
m_cbClassic = new QCheckBox(trn("filter_classic")); m_cbClassic->setChecked(m_radar->showClassic);
connect(m_cbClassic, &QCheckBox::checkStateChanged, this, &RadarWindow::onFilterChanged);
m_cbWifi = new QCheckBox(trn("filter_wifi")); m_cbWifi->setChecked(m_radar->showWifi);
connect(m_cbWifi, &QCheckBox::checkStateChanged, this, &RadarWindow::onFilterChanged);
fl->addWidget(m_cbBle); fl->addWidget(m_cbClassic); fl->addWidget(m_cbWifi); layout->addWidget(fg);
auto* btnRow = new QHBoxLayout();
m_btnStart = new QPushButton(trn("start_scan"));
connect(m_btnStart, &QPushButton::clicked, this, &RadarWindow::onStart);
m_btnStop = new QPushButton(trn("stop_scan"));
connect(m_btnStop, &QPushButton::clicked, this, &RadarWindow::onStop);
m_btnStop->setEnabled(false);
btnRow->addWidget(m_btnStart); btnRow->addWidget(m_btnStop); layout->addLayout(btnRow);
m_statsLabel = new QLabel(trn("devices_count").arg(0).arg(m_radar->zoom, 0, 'f', 1));
m_statsLabel->setAlignment(Qt::AlignCenter); layout->addWidget(m_statsLabel);
m_legendLabel = new QLabel(); layout->addWidget(m_legendLabel);
m_scanner = new RadarScanner(this);
connect(m_scanner, &RadarScanner::devicesFound, this, &RadarWindow::onDevicesFound);
connect(m_scanner, &RadarScanner::scanProgress, this, [this](const QString& stage) { if (m_statsLabel) m_statsLabel->setText(stage); });
}

void RadarWindow::setTheme(const QString& k) {
m_radar->setTheme(k);
QString color, bg, border, textColor;
if (k == "hack")      { color = "#00ff00"; bg = "#000a00"; border = "#00ff00"; textColor = "#00ff00"; }
else if (k == "arasaka") { color = "#cc0000"; bg = "#1a0000"; border = "#cc0000"; textColor = "#e0e0e0"; }
else if (k == "light")   {
//исправлен контраст для светлой темы
color = "#0064c8"; bg = "#e8e8e8"; border = "#0064c8"; textColor = "#1a1a1a";
}
else                     { color = "#00ff80"; bg = "#001a00"; border = "#00ff80"; textColor = "#d4d4d4"; }
QString titleStyle = QString("QLabel{color:%1;font-size:18px;font-weight:bold;font-family:Consolas;padding:10px;border-bottom:2px solid %1;}").arg(color);
QString statsStyle = QString("QLabel{color:%1;font-family:Consolas;font-size:11px;padding:5px;}").arg(textColor);
QString btnStyle = QString("QPushButton{background-color:%2;color:%3;border:2px solid %1;padding:8px;font-weight:bold;font-family:Consolas;}QPushButton:hover{background-color:%1;color:%2;}").arg(color, bg, textColor);
if (m_titleLabel) m_titleLabel->setStyleSheet(titleStyle);
if (m_statsLabel) m_statsLabel->setStyleSheet(statsStyle);
if (m_btnStart)   m_btnStart->setStyleSheet(btnStyle);
if (m_btnStop)    m_btnStop->setStyleSheet(btnStyle);
if (m_legendLabel) {
//переведенная легенда радара
QString legendHtml = QString("<div style='color:%1;font-family:Consolas;font-size:10px;padding:5px;'><b>%2</b><br>• %3<br>• %4<br>• %5<br>• %6<br>• %7<br>• %8<br>• %9</div>").arg(textColor, trn("legend_title"), trn("legend_ble"), trn("legend_classic"), trn("legend_wifi"), trn("legend_distance"), trn("legend_closer"), trn("legend_angle"), trn("legend_scroll"));
m_legendLabel->setText(legendHtml);
}
}

void RadarWindow::onFilterChanged() {
    m_radar->showBle = m_cbBle->isChecked();
    m_radar->showClassic = m_cbClassic->isChecked();
    m_radar->showWifi = m_cbWifi->isChecked();
    QJsonObject cfg = loadRadarConfig();
    cfg["show_ble"] = m_radar->showBle;
    cfg["show_classic"] = m_radar->showClassic;
    cfg["show_wifi"] = m_radar->showWifi;
    saveRadarConfig(cfg);
    m_radar->update();
}

void RadarWindow::onDevicesFound(const QList<RadarDevice>& devices) {
m_radar->updateDevices(devices); m_deviceCount = devices.size();
m_statsLabel->setText(trn("devices_count").arg(m_deviceCount).arg(m_radar->zoom, 0, 'f', 1));
}

void RadarWindow::onStart() {
    m_btnStart->setEnabled(false); m_btnStop->setEnabled(true);
    m_radar->setScanning(true);
    m_scanner->start();
}

void RadarWindow::onStop() {
    m_btnStart->setEnabled(true); m_btnStop->setEnabled(false);
    m_radar->setScanning(false);
    m_scanner->requestStop();
    m_scanner->wait(3000);
}

void RadarWindow::closeEvent(QCloseEvent* e) {
    m_radar->setScanning(false);
    m_scanner->requestStop();
    m_scanner->wait(3000);
    e->accept();
}
