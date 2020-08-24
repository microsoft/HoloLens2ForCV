# Introduction 
Recorder app to save to disk the following HL streams: RM (VLC, LT, AHAT), PV, hand, eye.

# Getting Started
After cloning and opening the StreamRecorder solution in Visual Studio, build (ARM64) and deploy.

First of all enable the device portal and research mode as specified here: https://docs.microsoft.com/en-us/windows/mixed-reality/research-mode

# Use the app
The streams to be captured should be specified at compile time, by modifying appropriately the first lines of AppMain.cpp.
For example:
```
std::vector<ResearchModeSensorType> AppMain::kEnabledRMStreamTypes = { ResearchModeSensorType::DEPTH_LONG_THROW };
std::vector<StreamTypes> AppMain::kEnabledStreamTypes = { StreamTypes::PV };
```
Note that, currently, the only StreamTypes that can be enabled/disabled are PV and eye tracking (hand tracking is always captured).

After app deployment, you should see a hologram with two buttons, Start and Stop. Push Start to start the capture and Stop when you are done.
Note that the interaction with the button panel might become suboptimal if many streams are captured concurrently (heavy load on the app).

# Recorded data
It is possible to use the py/recorder_console.py script for data download and automated processing.

To use  the recorder console:
python py/recorder_console.py --workspace_path <output_folder>  --dev_portal_username <user> --dev_portal_password <password>

then use the "download" command to download from hololens to the output folder and then use the "process" command.

# Python postprocessing
To post process the recorded data use the python scripts inside StreamRecorderConverter

Requirements: python3 with numpy, opencv-python, open3d.

The app comes with a set of python scripts for further processing of raw data. Note that all the functionalities provided by these scripts can be accessed via the py/recorder_console.py script which is launching the process_all.py script, so there is principle no need to use single scripts.

- All the following scripts are executed with the following script:
  python process_all.py --workspace_path <path_to_capture_folder>
 
- PV frames are saved in raw format. To obtain RGB ppm images, you can run the py/convert_to_ppm.py script:

  python convert_images.py --workspace_path <path_to_capture_folder>

- To see hand pose projection on PV images, one can run:

  python project_hand_eye_to_pv.py --workspace_path <path_to_capture_folder>

- To obtain (colored) point clouds from depth images and save them as obj files (tested only on LT), you can run the py/save_pclouds.py script:

  python save_pclouds.py --workspace_path <path_to_capture_folder> [cam]

  All the point clouds are put in the world coordinate system, unless the cam parameter is passed. If PV frames were captured, the script will try to color the point clouds accordingly.

- To get tsdf integration with open3d run

  python tsdf-integration.py --workspace_path <path_to_pinhole_projected_camera>
  


# Contribute
Feel free to clone the repository and address comments using the ticketing system of github.

If you want to learn more about creating good readme files then refer the following [guidelines](https://docs.microsoft.com/en-us/azure/devops/repos/git/create-a-readme?view=azure-devops). You can also seek inspiration from the below readme files:
- [ASP.NET Core](https://github.com/aspnet/Home)
- [Visual Studio Code](https://github.com/Microsoft/vscode)
- [Chakra Core](https://github.com/Microsoft/ChakraCore)
