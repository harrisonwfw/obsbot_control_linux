// Copyright 2025 Harrison W.

#include "usbio/usbio.hh"
#include "libs/camera_control.hh"

#include <QApplication>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

constexpr int kFrameW = 640;
constexpr int kFrameH = 480;

const char* kAppStylesheet = R"(
    QWidget {
        background-color: #16181d;
        color: #e6e8ec;
        font-size: 13px;
        selection-background-color: #c9a227;
        selection-color: #16181d;
    }
    QLabel#titleLabel {
        font-size: 20px;
        font-weight: 600;
        color: #f0f2f5;
        letter-spacing: 0.02em;
    }
    QLabel#subtitleLabel {
        font-size: 12px;
        color: #8b919c;
    }
    QLabel#controlLabel {
        color: #b8bec9;
        font-weight: 500;
    }
    QLabel#valueLabel {
        color: #7dd3c0;
        font-family: "DejaVu Sans Mono", "Consolas", "Liberation Mono", monospace;
        font-size: 12px;
    }
    QFrame#videoFrame {
        background-color: #0c0d10;
        border: 1px solid #2a2f38;
        border-radius: 8px;
    }
    QGroupBox {
        font-weight: 600;
        font-size: 12px;
        color: #c9a227;
        border: 1px solid #2f343e;
        border-radius: 8px;
        margin-top: 14px;
        padding: 14px 12px 12px 12px;
        background-color: #1c1f26;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        left: 12px;
        padding: 0 6px;
        background-color: #1c1f26;
    }
    QPushButton {
        background-color: #2a303a;
        border: 1px solid #3d4450;
        border-radius: 6px;
        padding: 8px 18px;
        font-weight: 600;
        min-height: 20px;
    }
    QPushButton:hover {
        background-color: #343b47;
        border-color: #c9a227;
    }
    QPushButton:pressed {
        background-color: #252a32;
    }
    QPushButton#startButton {
        background-color: #2d4a3e;
        border-color: #4a8f6f;
        color: #b8f0d4;
    }
    QPushButton#startButton:hover {
        background-color: #356652;
        border-color: #5cb88a;
    }
    QPushButton#stopButton {
        background-color: #4a2d32;
        border-color: #8f4a52;
        color: #f0c4c8;
    }
    QPushButton#stopButton:hover {
        background-color: #5c383e;
        border-color: #b85c66;
    }
    QSlider::groove:horizontal {
        height: 6px;
        background: #252a32;
        border-radius: 3px;
        border: 1px solid #323842;
    }
    QSlider::sub-page:horizontal {
        background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
            stop:0 #5a4a1a, stop:1 #c9a227);
        border-radius: 3px;
    }
    QSlider::handle:horizontal {
        width: 16px;
        height: 16px;
        margin: -6px 0;
        background: #e8d48b;
        border: 2px solid #c9a227;
        border-radius: 8px;
    }
    QSlider::handle:horizontal:hover {
        background: #fff2b3;
    }
    QScrollArea {
        background-color: transparent;
        border: none;
    }
    QSplitter::handle {
        background: #252a32;
        width: 3px;
    }
    QSplitter::handle:hover {
        background: #c9a227;
    }
)";

void yuyvToRgb(const unsigned char* yuyv, unsigned char* rgb, int width, int height) {
    for (int i = 0, j = 0; i < width * height * 2; i += 4, j += 6) {
        int y0 = yuyv[i + 0];
        int u = yuyv[i + 1] - 128;
        int y1 = yuyv[i + 2];
        int v = yuyv[i + 3] - 128;

        int c0 = y0 - 16;
        int c1 = y1 - 16;
        int d = u;
        int e = v;

        int r0 = (298 * c0 + 409 * e + 128) >> 8;
        int g0 = (298 * c0 - 100 * d - 208 * e + 128) >> 8;
        int b0 = (298 * c0 + 516 * d + 128) >> 8;

        int r1 = (298 * c1 + 409 * e + 128) >> 8;
        int g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
        int b1 = (298 * c1 + 516 * d + 128) >> 8;

        rgb[j + 0] = std::clamp(r0, 0, 255);
        rgb[j + 1] = std::clamp(g0, 0, 255);
        rgb[j + 2] = std::clamp(b0, 0, 255);

        rgb[j + 3] = std::clamp(r1, 0, 255);
        rgb[j + 4] = std::clamp(g1, 0, 255);
        rgb[j + 5] = std::clamp(b1, 0, 255);
    }
}

}  // namespace

class CameraApp : public QWidget {
    Q_OBJECT

public:
    CameraApp(QWidget* parent = nullptr) : QWidget(parent), camera(Camera("OBSBOT Tiny")) {
        setStyleSheet(kAppStylesheet);
        setWindowTitle(QStringLiteral("Camera Control"));

        if (set_format(camera.handle.fd(), kFrameW, kFrameH, V4L2_PIX_FMT_YUYV) == -1) {
            qDebug() << "Failed to set format";
        }

        videoLabel = new QLabel(this);
        videoLabel->setFixedSize(kFrameW, kFrameH);
        videoLabel->setAlignment(Qt::AlignCenter);
        videoLabel->setStyleSheet(
            QStringLiteral("QLabel { background: #000; border-radius: 4px; border: 1px solid #1e2229; }"));

        auto* videoFrame = new QFrame(this);
        videoFrame->setObjectName(QStringLiteral("videoFrame"));
        videoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* videoInner = new QVBoxLayout(videoFrame);
        videoInner->setContentsMargins(16, 16, 16, 16);
        videoInner->addStretch();
        auto* videoRow = new QHBoxLayout();
        videoRow->addStretch();
        videoRow->addWidget(videoLabel);
        videoRow->addStretch();
        videoInner->addLayout(videoRow);
        videoInner->addStretch();

        auto* title = new QLabel(QStringLiteral("Live preview"));
        title->setObjectName(QStringLiteral("titleLabel"));
        auto* subtitle = new QLabel(QStringLiteral("%1 × %2 · YUYV").arg(kFrameW).arg(kFrameH));
        subtitle->setObjectName(QStringLiteral("subtitleLabel"));

        auto* leftColumn = new QWidget(this);
        auto* leftLay = new QVBoxLayout(leftColumn);
        leftLay->setContentsMargins(8, 8, 12, 8);
        leftLay->setSpacing(8);
        leftLay->addWidget(title);
        leftLay->addWidget(subtitle);
        leftLay->addWidget(videoFrame, 1);

        auto* streamGroup = new QGroupBox(QStringLiteral("Stream"), this);
        auto* streamLay = new QHBoxLayout(streamGroup);
        streamLay->setSpacing(10);
        auto* startBtn = new QPushButton(QStringLiteral("Start"), this);
        startBtn->setObjectName(QStringLiteral("startButton"));
        startBtn->setToolTip(QStringLiteral("Begin capture (V4L2 STREAMON)"));
        auto* stopBtn = new QPushButton(QStringLiteral("Stop"), this);
        stopBtn->setObjectName(QStringLiteral("stopButton"));
        stopBtn->setToolTip(QStringLiteral("Pause capture (V4L2 STREAMOFF)"));
        streamLay->addWidget(startBtn);
        streamLay->addWidget(stopBtn);
        streamLay->addStretch();

        auto* ptzGroup = new QGroupBox(QStringLiteral("Pan · tilt · zoom"), this);
        auto* ptzGrid = new QGridLayout(ptzGroup);
        ptzGrid->setColumnStretch(1, 1);
        ptzGrid->setHorizontalSpacing(10);
        ptzGrid->setVerticalSpacing(10);

        auto makeValueLabel = [this]() {
            auto* l = new QLabel(this);
            l->setObjectName(QStringLiteral("valueLabel"));
            l->setMinimumWidth(88);
            l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return l;
        };

        auto addPtzRow = [&](int row, const QString& text, QSlider* slider, QLabel* valueLbl,
                             const QString& tip) {
            auto* lab = new QLabel(text, this);
            lab->setObjectName(QStringLiteral("controlLabel"));
            lab->setMinimumWidth(36);
            slider->setToolTip(tip);
            ptzGrid->addWidget(lab, row, 0);
            ptzGrid->addWidget(slider, row, 1);
            ptzGrid->addWidget(valueLbl, row, 2);
            connect(slider, &QSlider::valueChanged, valueLbl, [valueLbl](int v) {
                valueLbl->setText(QLocale::system().toString(v));
            });
            valueLbl->setText(QLocale::system().toString(slider->value()));
        };

        auto* panSlider = new QSlider(Qt::Horizontal, this);
        panSlider->setRange(-468000, 468000);
        panSlider->setValue(-180);
        panSlider->setPageStep(5000);
        panSlider->setSingleStep(500);
        addPtzRow(0, QStringLiteral("Pan"), panSlider, makeValueLabel(),
                  QStringLiteral("Horizontal aim (device units)"));

        auto* tiltSlider = new QSlider(Qt::Horizontal, this);
        tiltSlider->setRange(-324000, 324000);
        tiltSlider->setValue(-43596);
        tiltSlider->setPageStep(5000);
        tiltSlider->setSingleStep(500);
        addPtzRow(1, QStringLiteral("Tilt"), tiltSlider, makeValueLabel(),
                  QStringLiteral("Vertical aim (device units)"));

        auto* zoomSlider = new QSlider(Qt::Horizontal, this);
        zoomSlider->setRange(0, 100);
        zoomSlider->setValue(0);
        zoomSlider->setPageStep(5);
        zoomSlider->setSingleStep(1);
        addPtzRow(2, QStringLiteral("Zoom"), zoomSlider, makeValueLabel(),
                  QStringLiteral("Optical / digital zoom level"));

        struct SliderConfig {
            QString label;
            int min;
            int max;
            int step;
            int initial;
            QString tip;
        };

        const std::vector<SliderConfig> imageConfigs = {
            {QStringLiteral("Brightness"), 0, 100, 1, 50, QStringLiteral("Overall luma")},
            {QStringLiteral("Contrast"), 0, 100, 1, 50, QStringLiteral("Tone separation")},
            {QStringLiteral("Saturation"), 0, 100, 1, 50, QStringLiteral("Color intensity")},
            {QStringLiteral("Hue"), 0, 100, 1, 50, QStringLiteral("Color phase")},
            {QStringLiteral("Gain"), 1, 48, 1, 2, QStringLiteral("Analog gain (ISO-like)")},
            {QStringLiteral("White balance"), 2800, 6500, 100, 3100,
             QStringLiteral("Color temperature (K)")},
            {QStringLiteral("Sharpness"), 0, 100, 1, 50, QStringLiteral("Edge emphasis")},
            {QStringLiteral("Backlight"), 0, 100, 1, 9, QStringLiteral("Backlight compensation")},
        };

        auto* imageGroup = new QGroupBox(QStringLiteral("Image tuning"), this);
        auto* imageGrid = new QGridLayout(imageGroup);
        imageGrid->setColumnStretch(1, 1);
        imageGrid->setHorizontalSpacing(10);
        imageGrid->setVerticalSpacing(8);

        int imgRow = 0;
        for (const auto& config : imageConfigs) {
            auto* slider = new QSlider(Qt::Horizontal, this);
            slider->setRange(config.min, config.max);
            slider->setSingleStep(config.step);
            slider->setPageStep(std::max(1, (config.max - config.min) / 20));
            slider->setValue(config.initial);
            slider->setToolTip(config.tip);

            auto* lab = new QLabel(config.label, this);
            lab->setObjectName(QStringLiteral("controlLabel"));
            lab->setMinimumWidth(120);

            auto* valueLbl = makeValueLabel();
            connect(slider, &QSlider::valueChanged, valueLbl, [valueLbl](int v) {
                valueLbl->setText(QLocale::system().toString(v));
            });
            valueLbl->setText(QLocale::system().toString(slider->value()));

            imageGrid->addWidget(lab, imgRow, 0);
            imageGrid->addWidget(slider, imgRow, 1);
            imageGrid->addWidget(valueLbl, imgRow, 2);

            const QString settingKey = (config.label == QStringLiteral("White balance"))
                ? QStringLiteral("White Balance Temperature")
                : (config.label == QStringLiteral("Backlight"))
                      ? QStringLiteral("Backlight Compensation")
                      : config.label;

            connect(slider, &QSlider::valueChanged, this,
                    [this, settingKey](int value) { setCameraSetting(settingKey, value); });

            ++imgRow;
        }

        auto* panel = new QWidget(this);
        auto* panelLay = new QVBoxLayout(panel);
        panelLay->setContentsMargins(4, 4, 4, 12);
        panelLay->setSpacing(4);
        panelLay->addWidget(streamGroup);
        panelLay->addWidget(ptzGroup);
        panelLay->addWidget(imageGroup);
        panelLay->addStretch();

        auto* scroll = new QScrollArea(this);
        scroll->setWidget(panel);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setMinimumWidth(400);

        auto* split = new QSplitter(Qt::Horizontal, this);
        split->addWidget(leftColumn);
        split->addWidget(scroll);
        split->setStretchFactor(0, 3);
        split->setStretchFactor(1, 1);
        split->setChildrenCollapsible(false);

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->addWidget(split);

        connect(startBtn, &QPushButton::clicked, this, &CameraApp::startCamera);
        connect(stopBtn, &QPushButton::clicked, this, &CameraApp::stopCamera);
        connect(panSlider, &QSlider::valueChanged, this, &CameraApp::setPan);
        connect(tiltSlider, &QSlider::valueChanged, this, &CameraApp::setTilt);
        connect(zoomSlider, &QSlider::valueChanged, this, &CameraApp::setZoom);

        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &CameraApp::captureFrame);

        initV4L2();
        startCamera();

        resize(1240, 780);
    }

private slots:
    void startCamera() {
        if (ioctl(camera.handle.fd(), VIDIOC_STREAMON, &bufferType) == -1) {
            qDebug() << "Failed to start streaming";
            return;
        }
        timer->start(33);
    }

    void stopCamera() {
        timer->stop();
        if (ioctl(camera.handle.fd(), VIDIOC_STREAMOFF, &bufferType) == -1) {
            qDebug() << "Failed to stop streaming";
        }
    }

    void captureFrame() {
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(camera.handle.fd(), VIDIOC_DQBUF, &buffer) == -1) {
            qDebug() << "Failed to dequeue buffer";
            return;
        }

        auto* rgbData = new unsigned char[kFrameW * kFrameH * 3];
        yuyvToRgb(static_cast<unsigned char*>(buffers[buffer.index].start), rgbData, kFrameW, kFrameH);

        QImage frame(rgbData, kFrameW, kFrameH, QImage::Format_RGB888);
        videoLabel->setPixmap(QPixmap::fromImage(frame));

        delete[] rgbData;

        if (ioctl(camera.handle.fd(), VIDIOC_QBUF, &buffer) == -1) {
            qDebug() << "Failed to queue buffer";
        }
    }

    void setPan(int value) { camera.setPan(value); }

    void setTilt(int value) { camera.setTilt(value); }

    void setZoom(int value) { camera.setZoom(value); }

    void setCameraSetting(const QString& setting, int value) {
        if (setting == QStringLiteral("Brightness")) {
            camera.setBrightness(value);
        } else if (setting == QStringLiteral("Contrast")) {
            camera.setContrast(value);
        } else if (setting == QStringLiteral("Saturation")) {
            camera.setSaturation(value);
        } else if (setting == QStringLiteral("Hue")) {
            camera.setHue(value);
        } else if (setting == QStringLiteral("Gain")) {
            camera.setGain(value);
        } else if (setting == QStringLiteral("Sharpness")) {
            camera.setSharpness(value);
        } else if (setting == QStringLiteral("Backlight Compensation")) {
            camera.setBacklightCompensation(value);
        } else if (setting == QStringLiteral("White Balance Temperature")) {
            camera.setWhiteBalanceTemperature(value);
        }
    }

private:
    void initV4L2() {
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(camera.handle.fd(), VIDIOC_REQBUFS, &req) == -1) {
            qDebug() << "Failed to request buffers";
            return;
        }

        buffers = new Buffer[req.count];
        for (size_t i = 0; i < req.count; ++i) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (ioctl(camera.handle.fd(), VIDIOC_QUERYBUF, &buf) == -1) {
                qDebug() << "Failed to query buffer";
                return;
            }

            buffers[i].length = buf.length;
            buffers[i].start =
                mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera.handle.fd(), buf.m.offset);

            if (buffers[i].start == MAP_FAILED) {
                qDebug() << "Failed to map buffer";
                return;
            }

            if (ioctl(camera.handle.fd(), VIDIOC_QBUF, &buf) == -1) {
                qDebug() << "Failed to queue buffer";
                return;
            }
        }
    }

    struct Buffer {
        void* start;
        size_t length;
    };

    Camera camera;
    QTimer* timer{};
    QLabel* videoLabel{};
    Buffer* buffers{};
    v4l2_buf_type bufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
};

#include "gui.moc"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Camera Control"));
    QApplication::setOrganizationName(QStringLiteral("camera-control"));

    CameraApp window;
    window.show();

    return app.exec();
}
