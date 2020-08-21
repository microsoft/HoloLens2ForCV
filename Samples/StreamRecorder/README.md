# Introduction 
Recorder app to save to disk the following HL streams: RM (VLC, LT, AHAT), PV, head, hand, eye.

# Getting Started
After cloning and opening the StreamRecorder solution in Visual Studio 2019, build (ARM64) and deploy.

# Use the app
The streams to be captured should be specified at compile time, by modifying appropriately the first lines of AppMain.cpp.
For example:
```
std::vector<ResearchModeSensorType> AppMain::kEnabledRMStreamTypes = { ResearchModeSensorType::DEPTH_LONG_THROW };
std::vector<StreamTypes> AppMain::kEnabledStreamTypes = { StreamTypes::PV };
```

After app deployment, you should see a menu with two buttons, Start and Stop. Push Start to start the capture and Stop when you are done.

# Recorded data
It is possible to use the py/recorder_console.py script for data download and automated processing.

First of all enable the device portal as specified here: https://docs.microsoft.com/en-us/windows/mixed-reality/using-the-windows-device-portal

To use  the recorder console:
python py/recorder_console.py --workspace_path <output_folder>  --dev_portal_username <user> --dev_portal_password <password>

then use the "download" command to download from hololens to the output folder and then use the "process" command.

# Python postprocessing
Requirements: python3 with numpy, opencv-python, open3d.

The app comes with a set of python scripts for further processing of raw data. Note that all the functionalities provided by these scripts can be accessed via the py/recorder_console.py script which is launching the process_all.py script, so there is principle no need to use single scripts.
 
- PV frames are saved in raw format. To obtain RGB png images, you can run the py/convert_images.py script:

  python convert_images.py --workspace_path <path_to_capture_folder>

- To see tracked hand joints and eye gaze projection on PV images, you can run:

  python project_hand_eye_to_pv.py --workspace_path <path_to_capture_folder>

- To obtain (colored) point clouds from depth images and save them as obj files, you can run the py/save_pclouds.py script.

- To get tsdf integration with open3d you can run:

  python tsdf-integration.py --workspace_path <path_to_pinhole_projected_camera>
  


# Contribute
Feel free to clone the repository and address comments using the ticketing system of github.

If you want to learn more about creating good readme files then refer the following [guidelines](https://docs.microsoft.com/en-us/azure/devops/repos/git/create-a-readme?view=azure-devops). You can also seek inspiration from the below readme files:
- [ASP.NET Core](https://github.com/aspnet/Home)
- [Visual Studio Code](https://github.com/Microsoft/vscode)
- [Chakra Core](https://github.com/Microsoft/ChakraCore)