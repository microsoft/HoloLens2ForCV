---
page_type: sample
name: HoloLens2ForCV samples
description: HoloLens 2 Research Mode samples showcasing raw streams on device, including depth camera, gray-scale cameras, and IMU.
languages:
- cpp
products:
- windows-mixed-reality
- hololens
---

# HoloLens2ForCV samples

HoloLens 2 Research Mode enables access to the raw streams on device (depth camera, gray-scale cameras, IMU).

By releasing the Research Mode API together with a set of tools and sample apps, we want to make it easier to use HoloLens 2 as a powerful Computer Vision and Robotics research device.

The project was launched at ECCV 2020. We plan to extend it over time and welcome contributions from the research community.

# Contents

This repository contains documentation and samples for HoloLens 2 Research Mode.

Take a look at the [API documentation](https://github.com/microsoft/HoloLens2ForCV/blob/main/Docs/ResearchMode-ApiDoc.pdf) to get familiar with the new HoloLens 2 Research Mode API.

Take a look at the [slides of the ECCV tutorial](https://github.com/microsoft/HoloLens2ForCV/tree/main/Docs/ECCV2020-Tutorial) to obtain detailed information about Research Mode and its main features, read about how to set up your device, and learn how to use the apps in this repository.

Take a look at this [technical report](https://arxiv.org/pdf/2008.11239.pdf) for an introduction to Research Mode.

The repository contains four sample apps:   

   * The [CalibrationVisualization app](https://github.com/microsoft/HoloLens2ForCV/tree/main/Samples/CalibrationVisualization) shows a visualization of depth and gray-scale cameras coordinate frames live on device.
   * The [CameraWithCVAndCalibration app](https://github.com/microsoft/HoloLens2ForCV/tree/main/Samples/CameraWithCVAndCalibration) shows how to process Research Mode streams live on device: it uses OpenCV to detect arUco markers in the two frontal gray-scale cameras and triangulate the detections.
   * The [SensorVisualization app](https://github.com/microsoft/HoloLens2ForCV/tree/main/Samples/SensorVisualization) shows how to visualize Research Mode streams live on device.
   * The [StreamRecorder app](https://github.com/microsoft/HoloLens2ForCV/tree/main/Samples/StreamRecorder) shows how to capture simultaneously Research Mode streams (depth and gray-scale cameras) plus head, hand and eye tracking, save the streams to disk on device. It also contains a set of python scripts for offline postprocessing.

The StreamRecorder app uses [Cannon](https://github.com/microsoft/HoloLens2ForCV/tree/main/Samples/StreamRecorder/StreamRecorderApp/Cannon), a collection of wrappers and utility code for building native mixed reality apps using C++, Direct3D and Windows Perception APIs. It can be used as-is outside Research Mode for fast and easy native development.

# Setup

The earliest build that fully supports research mode is 19041.1364. Please join the Windows Insider Program to get preview builds. After that, in the device portal, enable research mode, different than recording mode. See https://github.com/microsoft/HoloLens2ForCV/blob/main/Docs/ECCV2020-Tutorial/ECCV2020-ResearchMode-Api.pdf (slides 6, 7 and 8) or Setup section in https://github.com/microsoft/HoloLens2ForCV/blob/main/Docs/ResearchMode-ApiDoc.pdf. Finally only arm64 applications are supported for now.

# Citing our work

If you find HoloLens 2 Research Mode useful for your research, please cite our work:

```
@article{hl2_rm,
         title     = {{HoloLens 2 Research Mode as a Tool for Computer Vision Research}},
         author    = {Dorin Ungureanu and Federica Bogo and Silvano Galliani and Pooja Sama and Xin Duan and Casey Meekhof and Jan St\{"u}hmer and Thomas J. Cashman and Bugra Tekin and Johannes L. Sch\{"o}nberger and Bugra Tekin and Pawel Olszta and Marc Pollefeys},
         journal   = {arXiv:2008.11239},
         year      = {2020}
}
```

# Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
