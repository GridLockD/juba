#include <QApplication>
#include <QMainWindow>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceInfo>
#include <QBluetoothAddress>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsSceneMouseEvent>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QSet>
#include <QTimer>
#include <QIcon>
#include <QStyle>
#include <QPalette>
#include <cmath>

// Determine approximate distance from RSSI: simple model (meters)
static qreal distanceFromRSSI(qint16 rssi) {
    constexpr int txPower = -59;  // RSSI at 1 meter (typical)
    if (rssi == 0) return -1.0;  // Unknown
    qreal ratio = rssi * 1.0 / txPower;
    if (ratio < 1.0) return std::pow(ratio, 10);
    else return (0.89976) * std::pow(ratio, 7.7095) + 0.111;
}

// Forward declarations
class DeviceItem;

class DeviceDetailsDialog : public QDialog {
    Q_OBJECT
public:
    explicit DeviceDetailsDialog(DeviceItem* deviceItem, QWidget* parent = nullptr);

signals:
    void hideRequested(const QString& deviceAddress);
    void starRequested(const QString& deviceAddress);
    void focusRequested(const QString& deviceAddress);

private:
    DeviceItem* m_deviceItem;
};


class DeviceItem : public QObject, public QGraphicsEllipseItem {
    Q_OBJECT
public:
    explicit DeviceItem(const QBluetoothDeviceInfo& info, QColor baseColor, QGraphicsItem* parent = nullptr);

    QString address() const { return m_info.address().toString(); }
    QString name() const { return m_info.name(); }
    qreal distance() const { return m_distance; }
    void setDistanceFromRSSI(qint16 rssi);
    void setStarred(bool starred);
    bool isStarred() const { return m_starred; }

signals:
    void clicked(DeviceItem*);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QBluetoothDeviceInfo m_info;
    qreal m_distance = 1.0;
    bool m_starred = false;
    QColor m_baseColor;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private slots:
    void onAdapterSelected(int index);
    void onDeviceDiscovered(const QBluetoothDeviceInfo& device);
    void onScanFinished();
    void onDeviceItemClicked(DeviceItem* item);
    void onHideDevice(const QString& address);
    void onStarDevice(const QString& address);
    void onFocusDevice(const QString& address);

private:
    void startScan();
    void updateProximityGraph();
    void resetFocus();
    QColor detectAccentColor() const;

    // UI
    QComboBox* m_adapterCombo = nullptr;
    QGraphicsView* m_graphView = nullptr;
    QGraphicsScene* m_scene = nullptr;

    // Bluetooth
    QBluetoothDeviceDiscoveryAgent* m_discoveryAgent = nullptr;
    QString m_currentAdapterAddress;

    QMap<QString, DeviceItem*> m_deviceItems;                 // key=address
    QMap<QString, QBluetoothDeviceInfo> m_devicesInfo;

    QSet<QString> m_hiddenDevices;
    QSet<QString> m_starredDevices;

    QString m_focusedDevice;

    QColor m_deviceBaseColor;
};

////////////////////////////////////// Implementation //////////////////////////////////////

// DeviceItem

DeviceItem::DeviceItem(const QBluetoothDeviceInfo& info, QColor baseColor, QGraphicsItem* parent)
    : QGraphicsEllipseItem(parent), m_info(info), m_baseColor(baseColor)
{
    constexpr int radius = 28;
    setRect(-radius, -radius, radius*2, radius*2);
    setBrush(m_baseColor);
    setPen(QPen(Qt::black, 1));
    setFlag(QGraphicsItem::ItemIsSelectable);
    setToolTip(QString("%1\n%2\nRSSI: %3 dBm")
               .arg(info.name())
               .arg(info.address().toString())
               .arg(info.rssi()));
    setAcceptHoverEvents(true);
}

void DeviceItem::setDistanceFromRSSI(qint16 rssi)
{
    qreal dist = distanceFromRSSI(rssi);
    m_distance = dist > 0 ? dist : 1.0;
}

void DeviceItem::setStarred(bool starred)
{
    m_starred = starred;
    update();
}

void DeviceItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)
    emit clicked(this);
}

void DeviceItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    painter->setRenderHint(QPainter::Antialiasing);

    // Base ellipse
    painter->setBrush(brush());
    painter->setPen(pen());
    painter->drawEllipse(rect());

    // Draw device name below the ellipse
    painter->setPen(Qt::white);
    QFont font = painter->font();
    font.setPointSize(9);
    font.setBold(true);
    painter->setFont(font);
    QRectF textRect(rect().left(), rect().bottom() + 2, rect().width(), 20);
    painter->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, m_info.name());

    // Draw star icon if starred (yellow star top-left)
    if(m_starred) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::yellow);

        // Draw a simple 5-point star
        QPointF center = rect().topLeft() + QPointF(10, 10);
        static const QPointF starPoints[5] = {
            QPointF(0, -6),
            QPointF(2.4, -1.8),
            QPointF(7, -2.2),
            QPointF(3.5, 1.3),
            QPointF(4.5, 6),
        };

        QPolygonF starPolygon;
        for (int i = 0; i < 5; ++i)
            starPolygon << (starPoints[i] + center);

        painter->drawPolygon(starPolygon);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::darkYellow, 1));
        painter->drawPolygon(starPolygon);
    }
}

// DeviceDetailsDialog

DeviceDetailsDialog::DeviceDetailsDialog(DeviceItem* deviceItem, QWidget* parent)
    : QDialog(parent), m_deviceItem(deviceItem)
{
    setWindowTitle(QString("Device Details - %1").arg(deviceItem->name()));
    setWindowModality(Qt::WindowModal);
    setMinimumSize(320, 180);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* lblName = new QLabel(QString("<b>Name:</b> %1").arg(deviceItem->name()));
    QLabel* lblAddress = new QLabel(QString("<b>Address:</b> %1").arg(deviceItem->address()));
    QLabel* lblRSSI = new QLabel(QString("<b>Approx. Distance:</b> %1 meters")
                                .arg(deviceItem->distance(), 0, 'f', 2));

    mainLayout->addWidget(lblName);
    mainLayout->addWidget(lblAddress);
    mainLayout->addWidget(lblRSSI);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* btnHide = new QPushButton("Hide");
    QPushButton* btnStar = new QPushButton(deviceItem->isStarred() ? "Unstar" : "Star");
    QPushButton* btnFocus = new QPushButton("Focus");
    QPushButton* btnClose = new QPushButton("Close");

    btnLayout->addWidget(btnHide);
    btnLayout->addWidget(btnStar);
    btnLayout->addWidget(btnFocus);
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    mainLayout->addLayout(btnLayout);

    connect(btnHide, &QPushButton::clicked, this, [this]() {
        emit hideRequested(m_deviceItem->address());
        accept();
    });

    connect(btnStar, &QPushButton::clicked, this, [this, btnStar]() {
        emit starRequested(m_deviceItem->address());
        if (btnStar->text() == "Star") btnStar->setText("Unstar");
        else btnStar->setText("Star");
    });

    connect(btnFocus, &QPushButton::clicked, this, [this]() {
        emit focusRequested(m_deviceItem->address());
        accept();
    });

    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
}

// MainWindow

MainWindow::MainWindow()
{
    setWindowTitle("Juba - Bluetooth Proximity Scanner");
    resize(920, 680);

    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);

    m_adapterCombo = new QComboBox(this);
    m_adapterCombo->addItem("Select Bluetooth Adapter");
    mainLayout->addWidget(m_adapterCombo);

    m_scene = new QGraphicsScene(0, 0, 800, 600, this);
    m_graphView = new QGraphicsView(m_scene, this);
    m_graphView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_graphView->setMinimumSize(820, 620);
    mainLayout->addWidget(m_graphView);

    setCentralWidget(mainWidget);

    // Detect accent color or fallback for device icons
    m_deviceBaseColor = detectAccentColor();
    if (!m_deviceBaseColor.isValid())
        m_deviceBaseColor = QColor("#3399FF");  // fallback blue

    // Fill adapters list
    const auto adapters = QBluetoothLocalDevice::allDevices();
    if (adapters.isEmpty()) {
        m_adapterCombo->setEnabled(false);
        m_adapterCombo->clear();
        m_adapterCombo->addItem("No Bluetooth adapter found");
    } else {
        for (const QBluetoothHostInfo& adapter : adapters)
            m_adapterCombo->addItem(adapter.name(), adapter.address().toString());
    }

    connect(m_adapterCombo, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onAdapterSelected);
    m_discoveryAgent = nullptr;
}

void MainWindow::onAdapterSelected(int index)
{
    if (index < 1) {
        if (m_discoveryAgent) {
            m_discoveryAgent->stop();
            m_discoveryAgent->deleteLater();
            m_discoveryAgent = nullptr;
        }
        m_scene->clear();
        m_deviceItems.clear();
        m_devicesInfo.clear();
        m_hiddenDevices.clear();
        m_starredDevices.clear();
        m_focusedDevice.clear();
        return;
    }

    m_currentAdapterAddress = m_adapterCombo->itemData(index).toString();

    if (m_discoveryAgent) {
        m_discoveryAgent->stop();
        m_discoveryAgent->deleteLater();
        m_discoveryAgent = nullptr;
    }

    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(QBluetoothAddress(m_currentAdapterAddress), this);
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(7000);

    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &MainWindow::onDeviceDiscovered);

    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &MainWindow::onScanFinished);

    m_devicesInfo.clear();
    m_hiddenDevices.clear();
    m_starredDevices.clear();
    m_focusedDevice.clear();

    m_deviceItems.clear();
    m_scene->clear();

    m_discoveryAgent->start();
}

void MainWindow::onDeviceDiscovered(const QBluetoothDeviceInfo& device)
{
    const QString addr = device.address().toString();

    if (m_devicesInfo.contains(addr)) return; // already discovered

    m_devicesInfo[addr] = device;

    auto* item = new DeviceItem(device, m_deviceBaseColor);
    item->setDistanceFromRSSI(device.rssi());

    connect(item, &DeviceItem::clicked, this, &MainWindow::onDeviceItemClicked);

    // Starred status
    item->setStarred(m_starredDevices.contains(addr));

    m_scene->addItem(item);
    m_deviceItems[addr] = item;

    updateProximityGraph();
}

void MainWindow::onScanFinished()
{
    // Restart scanning periodically for updated info
    QTimer::singleShot(12000, this, &MainWindow::startScan);
}

void MainWindow::startScan()
{
    if (m_discoveryAgent) {
        m_discoveryAgent->start();
    }
}

void MainWindow::updateProximityGraph()
{
    if (!m_focusedDevice.isEmpty()) {
        for (auto it = m_deviceItems.begin(); it != m_deviceItems.end(); ++it) {
            it.value()->setVisible(it.key() == m_focusedDevice);
            if (it.key() == m_focusedDevice)
                it.value()->setPos(400, 300);
        }
        return;
    }

    // Show all except hidden
    QList<DeviceItem*> visibleItems;
    for (auto it = m_deviceItems.begin(); it != m_deviceItems.end(); ++it) {
        bool hidden = m_hiddenDevices.contains(it.key());
        it.value()->setVisible(!hidden);
        if (!hidden)
            visibleItems.append(it.value());
    }

    if (visibleItems.empty())
        return;

    // Arrange in circle with position weighted by RSSI distance estimate
    int radius = 250;
    const int centerX = 400;
    const int centerY = 300;
    int n = visibleItems.size();

    for (int i = 0; i < n; ++i) {
        DeviceItem* item = visibleItems.at(i);
        qreal dist = item->distance();
        if (dist <= 0 || dist > 10.0) dist = 10.0;
        qreal distRatio = (10.0 - dist) / 10.0; // Closer -> bigger radius scale

        qreal angle = 2 * M_PI * i / n;
        qreal x = centerX + radius * distRatio * cos(angle);
        qreal y = centerY + radius * distRatio * sin(angle);

        item->setPos(x, y);
        item->setZValue(distRatio);
    }
}

void MainWindow::onDeviceItemClicked(DeviceItem* item)
{
    auto dlg = new DeviceDetailsDialog(item, this);

    connect(dlg, &DeviceDetailsDialog::hideRequested, this, &MainWindow::onHideDevice);
    connect(dlg, &DeviceDetailsDialog::starRequested, this, &MainWindow::onStarDevice);
    connect(dlg, &DeviceDetailsDialog::focusRequested, this, &MainWindow::onFocusDevice);

    dlg->exec();
    dlg->deleteLater();
}

void MainWindow::onHideDevice(const QString& address)
{
    m_hiddenDevices.insert(address);
    if (m_deviceItems.contains(address)) {
        m_deviceItems[address]->hide();
    }
}

void MainWindow::onStarDevice(const QString& address)
{
    if (m_starredDevices.contains(address)) {
        m_starredDevices.remove(address);
        if (m_deviceItems.contains(address))
            m_deviceItems[address]->setStarred(false);
    } else {
        m_starredDevices.insert(address);
        if (m_deviceItems.contains(address))
            m_deviceItems[address]->setStarred(true);
    }
    updateProximityGraph();
}

void MainWindow::onFocusDevice(const QString& address)
{
    if (m_focusedDevice == address) {
        m_focusedDevice.clear();
    } else {
        m_focusedDevice = address;
    }
    updateProximityGraph();
}

void MainWindow::resetFocus()
{
    m_focusedDevice.clear();
    updateProximityGraph();
}

QColor MainWindow::detectAccentColor() const
{
#if defined(Q_OS_WIN)
    // Windows accent color detection (return QColor or invalid)
    // Use Windows API via QSettings
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM", QSettings::NativeFormat);
    QVariant colorVar = settings.value("ColorizationColor");
    if (colorVar.isValid()) {
        uint colorVal = colorVar.toUInt();
        // Color is ARGB hex
        QRgb rgb = ((colorVal & 0x00FFFFFF) | 0xFF000000);
        QColor c(rgb);
        return c;
    }
    return QColor(); // invalid

#elif defined(Q_OS_LINUX)
    // On Linux, try to detect GTK theme color via environment variables or fallbacks
    // For simplicity, try palette from QApplication
    QPalette p = qApp->palette();
    QColor highlight = p.color(QPalette::Highlight);
    if (highlight.isValid() && highlight.alpha() > 0) {
        return highlight;
    }
    return QColor(); // invalid fallback

#else
    return QColor(); // invalid fallback
#endif
}

//////////////////// main //////////////
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("juba");
    app.setOrganizationName("juba");

    // Set app icon from resources or bundled file (see instruction below)
    app.setWindowIcon(QIcon(":/juba_icon.png"));

    if (QBluetoothLocalDevice::allDevices().isEmpty()) {
        QMessageBox::critical(nullptr, "Bluetooth Error", "No Bluetooth adapters found on this system.");
        return -1;
    }

    MainWindow w;
    w.show();

    return app.exec();
}

