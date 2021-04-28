---
page_type: sample
name: Sensor Visualization sample
description: Sample Holographic UWP application that demonstrates visualizing sensor streams live on HoloLens 2 devices.
languages:
- cpp
products:
- windows-mixed-reality
- hololens
---

# Sensor Visualization sample

![License](https://img.shields.io/badge/license-MIT-green.svg)

This sample is a Holographic UWP application that demonstrates visualizing sensor streams live on the device. It shows visualizations of frames from the two front head tracking cameras, depth and AB frames from the depth camera and a visualization of IMU accelerations visualized as lengths of a 3d coordinate system. The application is built starting from a [DX11 template app](https://docs.microsoft.com/windows/mixed-reality/creating-a-holographic-directx-project). The app adds a `SensorVisualizationScenario` class that manages and initializes sensors and creates DX rendering models for the app. It replaces the single spinning cube from the template with multiple models used to show sensor visualizations.

## Contents

| File/folder | Description |
|-------------|-------------|
| `SensorVisualization` | C++ application files and assets. |
| `SensorVisualization.sln` | Visual Studio solution file. |
| `README.md` | This README file. |

## Prerequisites

* Install the [latest Mixed Reality tools](https://docs.microsoft.com/windows/mixed-reality/develop/install-the-tools?tabs=unity)

## Setup

1. After cloning and opening the **CameraWithCVAndCalibration.sln** solution in Visual Studio, build (ARM64), and deploy.
2. [Enable Device Portal and Research Mode](https://docs.microsoft.com/windows/mixed-reality/research-mode)

## Running the sample

1. 

## Key concepts

## See also

* [Research Mode for HoloLens](https://docs.microsoft.com/windows/mixed-reality/develop/platform-capabilities-and-apis/research-mode)