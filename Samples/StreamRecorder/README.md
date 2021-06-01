---
page_type: sample
name: Stream Recorder sample
description: Sample Holographic UWP application that captures data streams and saves them to disk on HoloLens 2 devices.
languages:
- cpp
products:
- windows-mixed-reality
- hololens
---

# Stream Recorder sample 

![License](https://img.shields.io/badge/license-MIT-green.svg)

The StreamRecorder app captures and saves to disk the following HoloLens streams: VLC cameras, Long throw depth, AHAT depth, PV (i.e. RGB), head tracking, hand tracking, eye gaze tracking.

## Contents

| File/folder | Description |
|-------------|-------------|
| `StreamRecorderApp` | C++ application files and assets. |
| `StreamRecorderConverter` | Python conversion script resources. |
| `README.md` | This README file. |

## Prerequisites

* Install the [latest Mixed Reality tools](https://docs.microsoft.com/windows/mixed-reality/develop/install-the-tools)

## Setup

1. After cloning and opening the **StreamRecorderApp/StreamRecorder.snl** solution in Visual Studio, build (ARM64), and deploy.
2. [Enable Device Portal and Research Mode](https://docs.microsoft.com/windows/mixed-reality/research-mode)

## Running the app

1. Run from the debugger in Visual Studio by pressing F5
2. The app will show up in the start menu in HoloLens

## Key concepts

Refer to the **Docs/ResearchMode-ApiDoc.pdf** documentation, and pay special attention to the sections on: 
* **Consent Prompts**
* **Main Sensor Reading Loop**
* **Camera Sensors**
* **Sensors > Sensor Frames**
* **Sensor Coordinate Frames > Integrating with Perception APIs**
* **Sensor Coordinate Frames > Map and un-map APIs**

**Capturing Streams**

The streams to be captured should be specified at compile time, by modifying appropriately the first lines of AppMain.cpp.

For example:
```
std::vector<ResearchModeSensorType> AppMain::kEnabledRMStreamTypes = { ResearchModeSensorType::DEPTH_LONG_THROW };
std::vector<StreamTypes> AppMain::kEnabledStreamTypes = { StreamTypes::PV };
```

After app deployment, you should see a menu with two buttons, **Start** and **s**. Push Start to start the capture and Stop when you are done.

**Recorded data**

The files saved by the app can be accessed via Device Portal, under:
```
System->FileExplorer->LocalAppData->StreamRecorder->LocalState
```
The app creates one folder per capture.

However, it is possible (and recommended) to use the `StreamRecorderConverter/recorder_console.py` script for data download and automated processing.

To use the recorder console, you can run:
```
python StreamRecorderConverter/recorder_console.py --workspace_path <output_folder>  --dev_portal_username <user> --dev_portal_password <password>
```

then use the`download` command to download from HoloLens to the output folder and then use the `process` command.

**Python postprocessing**

To postprocess the recorded data, you can use the python scripts inside the `StreamRecorderConverter` folder.

Requirements: python3 with numpy, opencv-python, open3d.

The app comes with a set of python scripts. Note that all the functionalities provided by these scripts can be accessed via the `recorder_console.py` script, which in turn launches `process_all.py`, so there is in principle no need to use single scripts.

- All the scripts listed below can be launched by running `process_all.py`:
```
  python process_all.py --recording_path <path_to_capture_folder>
```

- PV (RGB) frames are saved in raw format. To obtain RGB png images, you can run the `convert_images.py` script:
```
  python convert_images.py --recording_path <path_to_capture_folder>
```

- To see hand tracking and eye gaze tracking results projected on PV images, you can run:
```
  python project_hand_eye_to_pv.py --recording_path <path_to_capture_folder>
```

- To obtain (colored) point clouds from depth images and save them as ply files, you can run the `save_pclouds.py` script.

All the point clouds are computed in the world coordinate system, unless the `cam_space` parameter is used. If PV frames were captured, the script will try to color the point clouds accordingly.

- To try our sample showcasing Truncated Signed Distance Function (TSDF) integration with open3d, you can run:
```
  python tsdf-integration.py --pinhole_path <path_to_pinhole_projected_camera>
```

## See also

* [Research Mode for HoloLens](https://docs.microsoft.com/windows/mixed-reality/develop/platform-capabilities-and-apis/research-mode)