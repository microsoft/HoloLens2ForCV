---
page_type: sample
name: Camera with CV and calibration sample
description: Sample Holographic UWP application using OpenCV to detect arUco markers in the two frontal gray-scale cameras and triangulate the detection on HoloLens 2.
languages:
- cpp
products:
- windows-mixed-reality
- hololens
---

# Camera with CV and calibration sample

![License](https://img.shields.io/badge/license-MIT-green.svg)

The sample is a Holographic UWP application that demonstrates uses OpenCV to detect arUco markers in the two frontal gray-scale cameras and triangulate the detections. Coordinate frames for sensors are rendered relative to the rig coordinate frame. The application is built using a [DX11 template app](https://docs.microsoft.com/windows/mixed-reality/creating-a-holographic-directx-project). The app adds a `CalibrationProjectionVisualizationScenario` class that manages and initializes sensors and creates DX rendering models for the app. It replaces the single spinning cube from the template with multiple models used to show triangulation and aruco detection visualizations. `OpenCvInstallArm64-412d` contains an arm64 header and library distribution of Opencv. These require git lfs. `One-Arruco-markers-DICT_6X6_250.pdf` is the aruco marker recognized by the app.

## Contents

| File/folder | Description |
|-------------|-------------|
| `CameraWithCVAndCalibration` | C++ application files and assets. |
| `OpenCvInstallArm64-412d` | Arm64 header and library distribution of OpenCV. |
| `CameraWithCVAndCalibration.sln` | Visual Studio solution file. |
| `One-Arruco-markers-DICT_6X6_250.pdf` | Aruco marker used by the app. |
| `README.md` | This README file. |

## Prerequisites

* Install the [latest Mixed Reality tools](https://docs.microsoft.com/windows/mixed-reality/develop/install-the-tools?tabs=unity)

## Setup

1. Open the **CameraWithCVAndCalibration.sln** file in Visual Studio, build (ARM64), and deploy
2. [Enable Device Portal and Research Mode](https://docs.microsoft.com/windows/mixed-reality/research-mode)

## Running the sample

1. Run from the debugger in Visual Studio by pressing F5
2. The app will show up in the start menu in HoloLens

## Key concepts

Consent Prompts
Main Sensor Reading Loop
Camera Sensors
Sensors > Sensor Frames 

## See also

* [Research Mode for HoloLens](https://docs.microsoft.com/windows/mixed-reality/develop/platform-capabilities-and-apis/research-mode)