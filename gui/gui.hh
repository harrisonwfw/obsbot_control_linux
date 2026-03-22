// Copyright 2025 Harrison W.

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QVideoWidget>
#include <QCamera>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QMediaCaptureSession>

class CameraApp : public QWidget {
    Q_OBJECT

public:
    CameraApp(QWidget *parent = nullptr) : QWidget(parent) {
        // Get the default camera device
        QCameraDevice cameraDevice = QMediaDevices::defaultVideoInput();

        // Create a QCamera object with the default camera device
        QCamera *camera = new QCamera(cameraDevice, this);

        // Create a video widget and set it as the viewfinder for the camera
        QVideoWidget *videoWidget = new QVideoWidget(this);
        camera->start();

        QMediaCaptureSession mediaCaptureSession;
        mediaCaptureSession.setCamera(camera);
        mediaCaptureSession.setVideoOutput(videoWidget);

        // Create a layout and add the video widget
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(videoWidget);

        // Create start and stop buttons
        QPushButton *startButton = new QPushButton("Start", this);
        QPushButton *stopButton = new QPushButton("Stop", this);
        layout->addWidget(startButton);
        layout->addWidget(stopButton);

        // Connect buttons to camera start and stop slots
        connect(startButton, &QPushButton::clicked, camera, &QCamera::start);
        connect(stopButton, &QPushButton::clicked, camera, &QCamera::stop);

        setLayout(layout);
        setWindowTitle("Camera Stream");
        resize(800, 600);
    }

private slots:
    void setExposure() {
        // Implement exposure setting logic here
    }
};