
# Summary

The 'Samples\SensorVisualization' is a Holographic UWP application that demonstrates visualizing sensor streams live on the device. It shows visualizations of frames from the two front head tracking cameras, depth and AB frames from the depth camera and a visualization of IMU accelerations visualized as lengths of a 3d coordinate system. The application is built starting from a DX11 template app described here: https://docs.microsoft.com/en-us/windows/mixed-reality/creating-a-holographic-directx-project. The app adds a SensorVisualizationScenario class that manages and initializes sensors and creates DX rendering models for the app. It replaces the single spinning cube from the template with multiple models used to show sensor visualizations.

