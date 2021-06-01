---
page_type: sample
name: Calibration visualization sample
description: Sample Holographic UWP application that demonstrates visualizing sensor calibration coordinate frames on HoloLens 2.
languages:
- cpp
products:
- windows-mixed-reality
- hololens
---

# Calibration visualization sample

![License](https://img.shields.io/badge/license-MIT-green.svg)

This sample is a Holographic UWP application that demonstrates visualizing sensor calibration coordinate frames on the device. Coordinate frames for sensors are rendered relative to the rig coordinate frame. The application is built using a [DX11 template app](https://docs.microsoft.com/windows/mixed-reality/creating-a-holographic-directx-project). The app adds a `CalibrationVisualizationScenario` class that manages and initializes sensors and creates DX rendering models for the app. It replaces the single spinning cube from the template with multiple models used to show calibration visualizations.

## Contents

| File/folder | Description |
|-------------|-------------|
| `CalibartionVisualization` | C++ application files and assets. |
| `CalibrationVisualization.sln` | Visual Studio solution file. |
| `README.md` | This README file. |

## Prerequisites

* Install the [latest Mixed Reality tools](https://docs.microsoft.com/windows/mixed-reality/develop/install-the-tools)

## Setup

1. After cloning and opening the **CalibartionVisualization.sln** solution in Visual Studio, build (ARM64), and deploy.
2. [Enable Device Portal and Research Mode](https://docs.microsoft.com/windows/mixed-reality/research-mode)

## Running the sample

1. Run from the debugger in Visual Studio by pressing F5
2. The app will show up in the start menu in HoloLens

## Key concepts

Refer to the **Docs/ResearchMode-ApiDoc.pdf** documentation, and pay special attention to the sections on:
* **Sensor Types** 
* **Sensor Coordinate frames**

## See also

* [Research Mode for HoloLens](https://docs.microsoft.com/windows/mixed-reality/develop/platform-capabilities-and-apis/research-mode)