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
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QComboBox>
#include <QSettings>
#include <QSignalBlocker>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <optional>
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
    QLabel#sessionSourceLabel {
        font-size: 11px;
        color: #8b919c;
        line-height: 1.35;
    }
    QLabel#aiFeedbackLabel {
        font-size: 11px;
        color: #9aab9e;
    }
    QComboBox {
        background-color: #2a303a;
        border: 1px solid #3d4450;
        border-radius: 6px;
        padding: 6px 12px;
        min-height: 22px;
    }
    QComboBox:hover {
        border-color: #c9a227;
    }
    QComboBox::drop-down {
        border: none;
        width: 24px;
    }
    QComboBox QAbstractItemView {
        background-color: #1c1f26;
        border: 1px solid #3d4450;
        selection-background-color: #c9a227;
        selection-color: #16181d;
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

struct AiModeEntry {
    QString label;
    AIMode mode;
};

const std::vector<AiModeEntry> kAiModes = {
    {QStringLiteral("No tracking"), AIMode::NoTracking},
    {QStringLiteral("Normal tracking"), AIMode::NormalTracking},
    {QStringLiteral("Upper body"), AIMode::UpperBody},
    {QStringLiteral("Close up"), AIMode::CloseUp},
    {QStringLiteral("Headless"), AIMode::Headless},
    {QStringLiteral("Lower body"), AIMode::LowerBody},
    {QStringLiteral("Desk mode"), AIMode::DeskMode},
    {QStringLiteral("Whiteboard"), AIMode::Whiteboard},
    {QStringLiteral("Hand"), AIMode::Hand},
    {QStringLiteral("Group"), AIMode::Group},
};

QString formatStartupBucket(int from_hw, int from_file, int from_default, int total) {
    if (total <= 0) {
        return QStringLiteral("—");
    }
    if (from_hw == total) {
        return QStringLiteral("camera");
    }
    if (from_file == total) {
        return QStringLiteral("saved file");
    }
    if (from_default == total) {
        return QStringLiteral("defaults");
    }
    return QStringLiteral("mixed");
}

}  // namespace

class CameraApp : public QWidget {
    Q_OBJECT

public:
    CameraApp(QWidget* parent = nullptr) : QWidget(parent) {
        setStyleSheet(kAppStylesheet);
        setWindowTitle(QStringLiteral("Camera Control"));
        camera_hint_ = qEnvironmentVariable("CAMERA_CONTROL_HINT", QStringLiteral("OBSBOT Tiny"));

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
        auto* streamOuter = new QVBoxLayout(streamGroup);
        streamOuter->setSpacing(8);
        auto* streamLay = new QHBoxLayout();
        streamLay->setSpacing(10);
        auto* startBtn = new QPushButton(QStringLiteral("Start"), this);
        startBtn->setObjectName(QStringLiteral("startButton"));
        startBtn->setToolTip(QStringLiteral("Begin capture (V4L2 STREAMON)"));
        auto* stopBtn = new QPushButton(QStringLiteral("Stop"), this);
        stopBtn->setObjectName(QStringLiteral("stopButton"));
        stopBtn->setToolTip(QStringLiteral("Pause capture (V4L2 STREAMOFF)"));
        start_stream_btn_ = startBtn;
        stop_stream_btn_ = stopBtn;
        streamLay->addWidget(startBtn);
        streamLay->addWidget(stopBtn);
        release_camera_btn_ = new QPushButton(QStringLiteral("Release device"), this);
        release_camera_btn_->setToolTip(
            QStringLiteral("Close our handle so another app (e.g. a browser) can open the camera. "
                           "This app keeps running and will reconnect automatically."));
        streamLay->addWidget(release_camera_btn_);
        streamLay->addStretch();
        streamOuter->addLayout(streamLay);

        camera_status_label_ = new QLabel(this);
        camera_status_label_->setObjectName(QStringLiteral("sessionSourceLabel"));
        camera_status_label_->setWordWrap(true);
        camera_status_label_->setText(
            QStringLiteral("Looking for the camera… If another program is using it, close that program or "
                           "use “Release device” here after connecting, then open the other app."));
        streamOuter->addWidget(camera_status_label_);

        connect(release_camera_btn_, &QPushButton::clicked, this, &CameraApp::releaseCameraForOtherApps);

        auto* sessionGroup = new QGroupBox(QStringLiteral("Startup sources"), this);
        auto* sessionLay = new QVBoxLayout(sessionGroup);
        session_source_label_ = new QLabel(this);
        session_source_label_->setObjectName(QStringLiteral("sessionSourceLabel"));
        session_source_label_->setWordWrap(true);
        session_source_label_->setText(
            QStringLiteral("Resolving pan/tilt/zoom, image, and AI from camera or saved settings…"));
        sessionLay->addWidget(session_source_label_);

        auto* aiGroup = new QGroupBox(QStringLiteral("AI tracking"), this);
        auto* aiLay = new QVBoxLayout(aiGroup);
        ai_mode_combo_ = new QComboBox(this);
        ai_mode_combo_->setToolTip(
            QStringLiteral("UVC extension: read on startup when available; stored value used as fallback"));
        for (const auto& entry : kAiModes) {
            ai_mode_combo_->addItem(entry.label, QVariant(static_cast<int>(entry.mode)));
        }
        aiLay->addWidget(ai_mode_combo_);
        ai_feedback_label_ = new QLabel(
            QStringLiteral("Select a mode to update the device. Tracking needs good lighting and a clear subject."),
            this);
        ai_feedback_label_->setObjectName(QStringLiteral("aiFeedbackLabel"));
        ai_feedback_label_->setWordWrap(true);
        aiLay->addWidget(ai_feedback_label_);

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

        pan_slider_ = new QSlider(Qt::Horizontal, this);
        pan_slider_->setRange(-468000, 468000);
        pan_slider_->setValue(-180);
        pan_slider_->setPageStep(5000);
        pan_slider_->setSingleStep(500);
        pan_value_ = makeValueLabel();
        addPtzRow(0, QStringLiteral("Pan"), pan_slider_, pan_value_,
                    QStringLiteral("Horizontal aim (device units)"));

        tilt_slider_ = new QSlider(Qt::Horizontal, this);
        tilt_slider_->setRange(-324000, 324000);
        tilt_slider_->setValue(-43596);
        tilt_slider_->setPageStep(5000);
        tilt_slider_->setSingleStep(500);
        tilt_value_ = makeValueLabel();
        addPtzRow(1, QStringLiteral("Tilt"), tilt_slider_, tilt_value_,
                    QStringLiteral("Vertical aim (device units)"));

        zoom_slider_ = new QSlider(Qt::Horizontal, this);
        zoom_slider_->setRange(0, 100);
        zoom_slider_->setValue(0);
        zoom_slider_->setPageStep(5);
        zoom_slider_->setSingleStep(1);
        zoom_value_ = makeValueLabel();
        addPtzRow(2, QStringLiteral("Zoom"), zoom_slider_, zoom_value_,
                    QStringLiteral("Optical / digital zoom level"));

        struct SliderConfig {
            QString label;
            QString storage_key;
            int min;
            int max;
            int step;
            int initial;
            QString tip;
        };

        const std::vector<SliderConfig> imageConfigs = {
            {QStringLiteral("Brightness"), QStringLiteral("brightness"), 0, 100, 1, 50,
             QStringLiteral("Overall luma")},
            {QStringLiteral("Contrast"), QStringLiteral("contrast"), 0, 100, 1, 50,
             QStringLiteral("Tone separation")},
            {QStringLiteral("Saturation"), QStringLiteral("saturation"), 0, 100, 1, 50,
             QStringLiteral("Color intensity")},
            {QStringLiteral("Hue"), QStringLiteral("hue"), 0, 100, 1, 50, QStringLiteral("Color phase")},
            {QStringLiteral("Gain"), QStringLiteral("gain"), 1, 48, 1, 2,
             QStringLiteral("Analog gain (ISO-like)")},
            {QStringLiteral("White balance"), QStringLiteral("whiteBalance"), 2800, 6500, 100, 3100,
             QStringLiteral("Color temperature (K)")},
            {QStringLiteral("Sharpness"), QStringLiteral("sharpness"), 0, 100, 1, 50,
             QStringLiteral("Edge emphasis")},
            {QStringLiteral("Backlight"), QStringLiteral("backlight"), 0, 100, 1, 9,
             QStringLiteral("Backlight compensation")},
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

            connect(slider, &QSlider::valueChanged, this, [this, settingKey](int value) {
                setCameraSetting(settingKey, value);
                scheduleSave();
            });

            image_rows_.push_back({slider, valueLbl, settingKey, config.storage_key});

            ++imgRow;
        }

        auto* panel = new QWidget(this);
        auto* panelLay = new QVBoxLayout(panel);
        panelLay->setContentsMargins(4, 4, 4, 12);
        panelLay->setSpacing(4);
        panelLay->addWidget(streamGroup);
        panelLay->addWidget(sessionGroup);
        panelLay->addWidget(aiGroup);
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
        connect(pan_slider_, &QSlider::valueChanged, this, [this](int v) {
            if (!camera_) {
                return;
            }
            camera_->setPan(v);
            scheduleSave();
        });
        connect(tilt_slider_, &QSlider::valueChanged, this, [this](int v) {
            if (!camera_) {
                return;
            }
            camera_->setTilt(v);
            scheduleSave();
        });
        connect(zoom_slider_, &QSlider::valueChanged, this, [this](int v) {
            if (!camera_) {
                return;
            }
            camera_->setZoom(v);
            scheduleSave();
        });

        connect(ai_mode_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
                &CameraApp::onAiModeUserChanged);

        save_debounce_ = new QTimer(this);
        save_debounce_->setSingleShot(true);
        save_debounce_->setInterval(400);
        connect(save_debounce_, &QTimer::timeout, this, &CameraApp::saveSettings);

        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &CameraApp::captureFrame);

        camera_retry_timer_ = new QTimer(this);
        camera_retry_timer_->setInterval(2500);
        connect(camera_retry_timer_, &QTimer::timeout, this, &CameraApp::tryAttachCamera);

        hardware_poll_timer_ = new QTimer(this);
        hardware_poll_timer_->setInterval(650);
        connect(hardware_poll_timer_, &QTimer::timeout, this, &CameraApp::pollHardwareToUi);
        hardware_poll_timer_->start();

        setControlsCameraEnabled(false);
        refreshCameraStatusText();
        tryAttachCamera();
        camera_retry_timer_->start();

        resize(1240, 780);
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        saveSettings();
        QWidget::closeEvent(event);
    }

private slots:
    void tryAttachCamera() {
        if (camera_) {
            return;
        }
        capture_available_ = false;
        try {
            auto cam = std::make_unique<Camera>(camera_hint_.toStdString());
            const int fd = cam->handle.fd();
            const bool has_capture = v4l2_fd_has_video_capture(fd);

            if (has_capture) {
                const int fmt_rc = set_format(fd, kFrameW, kFrameH, V4L2_PIX_FMT_YUYV);
                const int fmt_errno = errno;
                camera_ = std::move(cam);
                if (fmt_rc == -1) {
                    qDebug() << "set_format failed (often EBUSY while another app streams):" << strerror(fmt_errno);
                    capture_available_ = false;
                } else if (!initV4L2()) {
                    const int v4l_errno = errno;
                    qDebug() << "initV4L2 failed:" << strerror(v4l_errno);
                    freeCaptureBuffers();
                    capture_available_ = false;
                } else {
                    capture_available_ = true;
                }
            } else {
                qDebug() << "Opened V4L2 node without video capture — controls only (no in-app preview).";
                camera_ = std::move(cam);
            }

            try {
                applyHybridSettings();
            } catch (const std::exception& e) {
                qDebug() << "Startup settings partially failed (common if another app is streaming):" << e.what();
            }
            setControlsCameraEnabled(true);
            refreshCameraStatusText();
            if (capture_available_) {
                startCamera();
            }
        } catch (const std::exception& e) {
            qDebug() << "Camera not available:" << e.what();
            capture_available_ = false;
            freeCaptureBuffers();
            camera_.reset();
            setControlsCameraEnabled(false);
            refreshCameraStatusText();
        }
    }

    void releaseCameraForOtherApps() {
        if (!camera_) {
            return;
        }
        capture_available_ = false;
        if (streaming_) {
            stopCamera();
        }
        freeCaptureBuffers();
        camera_.reset();
        setControlsCameraEnabled(false);
        refreshCameraStatusText();
    }

    void pollHardwareToUi() {
        if (!camera_) {
            return;
        }
        auto syncSlider = [this](QSlider* s, QLabel* val, std::optional<int> (*tryRead)(Camera&)) {
            if (s == nullptr || val == nullptr || s->isSliderDown()) {
                return;
            }
            std::optional<int> hw = tryRead(*camera_);
            if (!hw) {
                return;
            }
            const int nv = std::clamp(*hw, s->minimum(), s->maximum());
            if (nv == s->value()) {
                return;
            }
            QSignalBlocker b(s);
            s->setValue(nv);
            val->setText(QLocale::system().toString(nv));
        };

        syncSlider(pan_slider_, pan_value_, [](Camera& c) { return c.try_get_pan(); });
        syncSlider(tilt_slider_, tilt_value_, [](Camera& c) { return c.try_get_tilt(); });
        syncSlider(zoom_slider_, zoom_value_, [](Camera& c) { return c.try_get_zoom(); });

        for (const auto& row : image_rows_) {
            if (row.slider->isSliderDown()) {
                continue;
            }
            std::optional<int> hw = tryReadImageForStorage(row.storage_key);
            if (!hw) {
                continue;
            }
            const int nv = std::clamp(*hw, row.slider->minimum(), row.slider->maximum());
            if (nv == row.slider->value()) {
                continue;
            }
            QSignalBlocker b(row.slider);
            row.slider->setValue(nv);
            row.value_label->setText(QLocale::system().toString(nv));
        }

        if (ai_mode_combo_ != nullptr && !ai_mode_combo_->hasFocus()) {
            if (auto m = camera_->try_get_ai_mode()) {
                const int raw = static_cast<int>(*m);
                int idx = -1;
                for (int i = 0; i < ai_mode_combo_->count(); ++i) {
                    if (ai_mode_combo_->itemData(i).toInt() == raw) {
                        idx = i;
                        break;
                    }
                }
                if (idx >= 0 && idx != ai_mode_combo_->currentIndex()) {
                    QSignalBlocker b(ai_mode_combo_);
                    ai_mode_combo_->setCurrentIndex(idx);
                }
            }
        }
    }

    void startCamera() {
        if (!camera_ || !capture_available_ || streaming_) {
            return;
        }
        const int fd = camera_->handle.fd();
        if (buffers_need_requeue_) {
            requeueAllCaptureBuffers();
            buffers_need_requeue_ = false;
        }
        if (ioctl(fd, VIDIOC_STREAMON, &bufferType) == -1) {
            qDebug() << "Failed to start streaming:" << strerror(errno);
            refreshCameraStatusText();
            return;
        }
        streaming_ = true;
        timer->start(33);
        refreshCameraStatusText();
    }

    void stopCamera() {
        if (!camera_ || !streaming_) {
            return;
        }
        timer->stop();
        const int fd = camera_->handle.fd();
        if (ioctl(fd, VIDIOC_STREAMOFF, &bufferType) == -1) {
            qDebug() << "Failed to stop streaming:" << strerror(errno);
        }
        streaming_ = false;
        // Buffers are returned to userspace; they must be QBUF'd again before the next STREAMON.
        buffers_need_requeue_ = true;
        refreshCameraStatusText();
    }

    void captureFrame() {
        if (!camera_ || !streaming_) {
            return;
        }
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(camera_->handle.fd(), VIDIOC_DQBUF, &buffer) == -1) {
            if (errno != EAGAIN) {
                qDebug() << "Failed to dequeue buffer:" << strerror(errno);
            }
            return;
        }

        auto* rgbData = new unsigned char[kFrameW * kFrameH * 3];
        yuyvToRgb(static_cast<unsigned char*>(buffers[buffer.index].start), rgbData, kFrameW, kFrameH);

        QImage frame(rgbData, kFrameW, kFrameH, QImage::Format_RGB888);
        videoLabel->setPixmap(QPixmap::fromImage(frame));

        delete[] rgbData;

        if (ioctl(camera_->handle.fd(), VIDIOC_QBUF, &buffer) == -1) {
            qDebug() << "Failed to queue buffer:" << strerror(errno);
        }
    }

    void onAiModeUserChanged(int index) {
        if (!camera_ || index < 0 || ai_mode_combo_ == nullptr || ai_feedback_label_ == nullptr) {
            return;
        }
        const int raw = ai_mode_combo_->itemData(index).toInt();
        const AIMode mode = static_cast<AIMode>(raw);
        if (camera_->set_ai_mode(mode)) {
            ai_feedback_label_->setText(
                QStringLiteral("Sent to camera. If tracking looks unchanged, check firmware / another app "
                               "locking the device."));
        } else {
            ai_feedback_label_->setText(
                QStringLiteral("Camera did not accept this mode (UVC SET_CUR failed). Try closing other "
                               "apps using the webcam."));
        }
        scheduleSave();
    }

    void setCameraSetting(const QString& setting, int value) {
        if (!camera_) {
            return;
        }
        if (setting == QStringLiteral("Brightness")) {
            camera_->setBrightness(value);
        } else if (setting == QStringLiteral("Contrast")) {
            camera_->setContrast(value);
        } else if (setting == QStringLiteral("Saturation")) {
            camera_->setSaturation(value);
        } else if (setting == QStringLiteral("Hue")) {
            camera_->setHue(value);
        } else if (setting == QStringLiteral("Gain")) {
            camera_->setGain(value);
        } else if (setting == QStringLiteral("Sharpness")) {
            camera_->setSharpness(value);
        } else if (setting == QStringLiteral("Backlight Compensation")) {
            camera_->setBacklightCompensation(value);
        } else if (setting == QStringLiteral("White Balance Temperature")) {
            camera_->setWhiteBalanceTemperature(value);
        }
    }

private:
    void setControlsCameraEnabled(bool has_device) {
        const bool on = has_device;
        if (pan_slider_) {
            pan_slider_->setEnabled(on);
        }
        if (tilt_slider_) {
            tilt_slider_->setEnabled(on);
        }
        if (zoom_slider_) {
            zoom_slider_->setEnabled(on);
        }
        for (const auto& row : image_rows_) {
            if (row.slider) {
                row.slider->setEnabled(on);
            }
        }
        if (ai_mode_combo_) {
            ai_mode_combo_->setEnabled(on);
        }
        if (start_stream_btn_) {
            start_stream_btn_->setEnabled(on && capture_available_);
        }
        if (stop_stream_btn_) {
            stop_stream_btn_->setEnabled(on && capture_available_);
        }
        if (release_camera_btn_) {
            release_camera_btn_->setEnabled(on);
        }
    }

    void refreshCameraStatusText() {
        if (!camera_status_label_) {
            return;
        }
        if (!camera_) {
            camera_status_label_->setText(
                QStringLiteral("Disconnected — retrying every few seconds. If another app has /dev/video0, "
                               "this app tries higher /dev/videoN nodes first so pan/tilt may still work. "
                               "Set CAMERA_CONTROL_HINT to a device path (e.g. /dev/video2) if needed."));
            return;
        }
        if (!capture_available_) {
            camera_status_label_->setText(
                QStringLiteral("Controls only — no in-app preview. This happens if another app (e.g. Meet) is "
                               "already streaming, or this node has no capture. Pan/tilt/zoom/image/AI still "
                               "apply when the driver accepts them on this handle."));
            return;
        }
        if (streaming_) {
            camera_status_label_->setText(
                QStringLiteral("Preview on. Pan/tilt/zoom/image/AI sliders refresh from the camera when you "
                               "are not dragging them."));
        } else {
            camera_status_label_->setText(
                QStringLiteral("Camera open — Start shows preview; controls work without preview."));
        }
    }

    void freeCaptureBuffers() {
        if (timer) {
            timer->stop();
        }
        streaming_ = false;

        if (camera_) {
            const int fd = camera_->handle.fd();
            if (buffers && num_buffers_ > 0) {
                for (unsigned i = 0; i < num_buffers_; ++i) {
                    if (buffers[i].start != nullptr && buffers[i].start != MAP_FAILED) {
                        munmap(buffers[i].start, buffers[i].length);
                    }
                }
            }
            struct v4l2_requestbuffers req {};
            req.count = 0;
            req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            req.memory = V4L2_MEMORY_MMAP;
            (void)ioctl(fd, VIDIOC_REQBUFS, &req);
        }
        delete[] buffers;
        buffers = nullptr;
        num_buffers_ = 0;
        buffers_need_requeue_ = false;
    }

    std::optional<int> tryReadImageForStorage(const QString& key) {
        if (!camera_) {
            return std::nullopt;
        }
        if (key == QStringLiteral("brightness")) {
            return camera_->try_get_brightness();
        }
        if (key == QStringLiteral("contrast")) {
            return camera_->try_get_contrast();
        }
        if (key == QStringLiteral("saturation")) {
            return camera_->try_get_saturation();
        }
        if (key == QStringLiteral("hue")) {
            return camera_->try_get_hue();
        }
        if (key == QStringLiteral("gain")) {
            return camera_->try_get_gain();
        }
        if (key == QStringLiteral("whiteBalance")) {
            return camera_->try_get_white_balance_temperature();
        }
        if (key == QStringLiteral("sharpness")) {
            return camera_->try_get_sharpness();
        }
        if (key == QStringLiteral("backlight")) {
            return camera_->try_get_backlight_compensation();
        }
        return std::nullopt;
    }

    void applyHybridSettings() {
        if (!camera_) {
            return;
        }
        QSettings settings;
        settings.beginGroup(QStringLiteral("camera"));

        int ptz_hw = 0;
        int ptz_file = 0;
        int ptz_def = 0;
        auto applyPtz = [&](QSlider* sl, QLabel* val, const QString& storageKey,
                            std::optional<int> hw, const auto& pushFn) {
            const bool from_hw = hw.has_value();
            int v = sl->value();
            if (from_hw) {
                v = std::clamp(*hw, sl->minimum(), sl->maximum());
                ++ptz_hw;
            } else if (settings.contains(storageKey)) {
                v = std::clamp(settings.value(storageKey).toInt(), sl->minimum(), sl->maximum());
                ++ptz_file;
            } else {
                ++ptz_def;
            }
            {
                QSignalBlocker b(sl);
                sl->setValue(v);
                val->setText(QLocale::system().toString(v));
            }
            if (!from_hw) {
                pushFn(v);
            }
        };

        applyPtz(pan_slider_, pan_value_, QStringLiteral("pan"), camera_->try_get_pan(),
                 [this](int v) { camera_->setPan(v); });
        applyPtz(tilt_slider_, tilt_value_, QStringLiteral("tilt"), camera_->try_get_tilt(),
                 [this](int v) { camera_->setTilt(v); });
        applyPtz(zoom_slider_, zoom_value_, QStringLiteral("zoom"), camera_->try_get_zoom(),
                 [this](int v) { camera_->setZoom(v); });

        int img_hw = 0;
        int img_file = 0;
        int img_def = 0;
        for (const auto& row : image_rows_) {
            std::optional<int> hw = tryReadImageForStorage(row.storage_key);
            const bool from_hw = hw.has_value();
            int v = row.slider->value();
            if (from_hw) {
                v = std::clamp(*hw, row.slider->minimum(), row.slider->maximum());
                ++img_hw;
            } else if (settings.contains(row.storage_key)) {
                v = std::clamp(settings.value(row.storage_key).toInt(), row.slider->minimum(),
                                row.slider->maximum());
                ++img_file;
            } else {
                ++img_def;
            }
            {
                QSignalBlocker b(row.slider);
                row.slider->setValue(v);
                row.value_label->setText(QLocale::system().toString(v));
            }
            if (!from_hw) {
                setCameraSetting(row.camera_key, v);
            }
        }

        int ai_idx = 0;
        bool ai_from_hw = false;
        if (auto m = camera_->try_get_ai_mode()) {
            ai_from_hw = true;
            const int raw = static_cast<int>(*m);
            for (int i = 0; i < ai_mode_combo_->count(); ++i) {
                if (ai_mode_combo_->itemData(i).toInt() == raw) {
                    ai_idx = i;
                    break;
                }
            }
        } else if (settings.contains(QStringLiteral("aiMode"))) {
            const int raw = settings.value(QStringLiteral("aiMode")).toInt();
            for (int i = 0; i < ai_mode_combo_->count(); ++i) {
                if (ai_mode_combo_->itemData(i).toInt() == raw) {
                    ai_idx = i;
                    break;
                }
            }
        }
        {
            QSignalBlocker b(ai_mode_combo_);
            ai_mode_combo_->setCurrentIndex(ai_idx);
        }
        QString ai_bucket;
        if (ai_from_hw) {
            ai_bucket = QStringLiteral("camera");
        } else if (settings.contains(QStringLiteral("aiMode"))) {
            ai_bucket = QStringLiteral("saved file");
            const AIMode mode = static_cast<AIMode>(ai_mode_combo_->currentData().toInt());
            if (!camera_->set_ai_mode(mode)) {
                ai_feedback_label_->setText(
                    QStringLiteral("Saved AI mode could not be applied (device rejected UVC command)."));
            } else {
                ai_feedback_label_->setText(
                    QStringLiteral("Restored AI mode from saved settings on the camera."));
            }
        } else {
            ai_bucket = QStringLiteral("defaults");
            ai_feedback_label_->setText(
                QStringLiteral("Select a mode to update the device. Tracking needs good lighting and a clear "
                               "subject."));
        }

        const int n_img = static_cast<int>(image_rows_.size());
        session_source_label_->setText(
            QStringLiteral(
                "Loaded PTZ from: %1 · Image from: %2 · AI from: %3.\n"
                "Hybrid uses the camera when reads succeed; otherwise values come from the saved file.")
                .arg(formatStartupBucket(ptz_hw, ptz_file, ptz_def, 3))
                .arg(formatStartupBucket(img_hw, img_file, img_def, n_img))
                .arg(ai_bucket));

        if (ai_from_hw) {
            ai_feedback_label_->setText(
                QStringLiteral("AI mode matches the camera. Change the selection to send a new command."));
        }
    }

    void saveSettings() {
        QSettings settings;
        settings.beginGroup(QStringLiteral("camera"));
        settings.setValue(QStringLiteral("pan"), pan_slider_->value());
        settings.setValue(QStringLiteral("tilt"), tilt_slider_->value());
        settings.setValue(QStringLiteral("zoom"), zoom_slider_->value());
        for (const auto& row : image_rows_) {
            settings.setValue(row.storage_key, row.slider->value());
        }
        if (ai_mode_combo_->currentIndex() >= 0) {
            settings.setValue(QStringLiteral("aiMode"), ai_mode_combo_->currentData().toInt());
        }
    }

    void scheduleSave() {
        if (save_debounce_) {
            save_debounce_->start();
        }
    }

    void requeueAllCaptureBuffers() {
        if (!camera_ || !buffers || num_buffers_ == 0) {
            return;
        }
        const int fd = camera_->handle.fd();
        for (unsigned i = 0; i < num_buffers_; ++i) {
            struct v4l2_buffer buf {};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
                qDebug() << "QBUF index" << i << ":" << strerror(errno);
            }
        }
    }

    bool initV4L2() {
        if (!camera_) {
            return false;
        }
        freeCaptureBuffers();

        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(camera_->handle.fd(), VIDIOC_REQBUFS, &req) == -1) {
            qDebug() << "Failed to request buffers";
            return false;
        }

        buffers = new Buffer[req.count];
        buffers_need_requeue_ = false;
        num_buffers_ = 0;
        for (size_t i = 0; i < req.count; ++i) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (ioctl(camera_->handle.fd(), VIDIOC_QUERYBUF, &buf) == -1) {
                qDebug() << "Failed to query buffer";
                freeCaptureBuffers();
                return false;
            }

            buffers[i].length = buf.length;
            buffers[i].start =
                mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera_->handle.fd(), buf.m.offset);

            if (buffers[i].start == MAP_FAILED) {
                qDebug() << "Failed to map buffer";
                freeCaptureBuffers();
                return false;
            }

            if (ioctl(camera_->handle.fd(), VIDIOC_QBUF, &buf) == -1) {
                qDebug() << "Failed to queue buffer";
                freeCaptureBuffers();
                return false;
            }
            num_buffers_ = static_cast<unsigned>(i) + 1U;
        }
        return true;
    }

    struct Buffer {
        void* start;
        size_t length;
    };

    struct ImageControlRow {
        QSlider* slider{};
        QLabel* value_label{};
        QString camera_key;
        QString storage_key;
    };

    std::unique_ptr<Camera> camera_;
    QString camera_hint_;
    QPushButton* start_stream_btn_{};
    QPushButton* stop_stream_btn_{};
    QPushButton* release_camera_btn_{};
    QLabel* camera_status_label_{};
    QTimer* camera_retry_timer_{};
    QTimer* hardware_poll_timer_{};
    QSlider* pan_slider_{};
    QSlider* tilt_slider_{};
    QSlider* zoom_slider_{};
    QLabel* pan_value_{};
    QLabel* tilt_value_{};
    QLabel* zoom_value_{};
    std::vector<ImageControlRow> image_rows_;
    QComboBox* ai_mode_combo_{};
    QLabel* ai_feedback_label_{};
    QLabel* session_source_label_{};
    QTimer* save_debounce_{};
    QTimer* timer{};
    QLabel* videoLabel{};
    Buffer* buffers{};
    unsigned num_buffers_{};
    bool streaming_{false};
    bool capture_available_{false};
    bool buffers_need_requeue_{false};
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
