
# Summary

The 'Samples\CalibrationVisualization' is a Holographic UWP application that demonstrates visualizing sensor streams calibration coordinate frames on the device. Coordinate frames for sensors are rendered relative to the rig coordinate frame. The application is built starting from a DX11 template app described here: https://docs.microsoft.com/en-us/windows/mixed-reality/creating-a-holographic-directx-project. The app adds a CalibrationVisualizationScenario class that manages and initializes sensors and creates DX rendering models for the app. It replaces the single spinning cube from the template with multiple models used to show calibration visualizations.

