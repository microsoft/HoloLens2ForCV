//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

//
// Comment out this preprocessor definition to disable all of the
// sample content.
//
// To remove the content after disabling it:
//     * Remove the unused code from your app's Main class.
//     * Delete the Content folder provided with this template.
//
#define DRAW_SAMPLE_CONTENT

#include "Common\DeviceResources.h"
#include "Common\StepTimer.h"
#include <researchmode\ResearchModeApi.h>
#include <Texture2D.h>


#ifdef DRAW_SAMPLE_CONTENT
#include "Content\ModelRenderer.h"
#include "Content\SlateCameraRenderer.h"
#include "Content\SpatialInputHandler.h"
#include "Content\AccelRenderer.h"
#include "Content\XAxisModel.h"
#include "Content\YAxisModel.h"
#include "Content\ZAxisModel.h"
#include "Content\XYZAxisModel.h"
#include "Content\VectorModel.h"
#endif

// Updates, renders, and presents holographic content using Direct3D.
namespace BasicHologram
{
    class Scenario
    {
    public:
        Scenario(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
            m_deviceResources(deviceResources)
        {
        }

        ~Scenario()
        {
        }

        virtual void IntializeSensors() = 0;
        virtual void IntializeModelRendering() = 0;
        virtual void PositionHologram(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, const DX::StepTimer& timer) = 0;
        virtual void PositionHologramNoSmoothing(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose) = 0;
        virtual void UpdateModels(DX::StepTimer &timer) = 0;
        virtual winrt::Windows::Foundation::Numerics::float3 const& GetPosition() = 0;
        virtual void RenderModels() = 0;

        virtual void UpdateState()
        {
        }

        void SetStationaryFrameOfReference(winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference const& stationaryReferenceFrame)
        {
            m_stationaryReferenceFrame = stationaryReferenceFrame;
        }

        virtual void OnDeviceLost() = 0;
        virtual void OnDeviceRestored() = 0;

    protected:
        std::shared_ptr<DX::DeviceResources>                        m_deviceResources;
        winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference m_stationaryReferenceFrame = nullptr;

    };

    class BasicHologramMain : public DX::IDeviceNotify
    {
    public:
        BasicHologramMain(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        ~BasicHologramMain();

        // Sets the holographic space. This is our closest analogue to setting a new window
        // for the app.
        void SetHolographicSpace(winrt::Windows::Graphics::Holographic::HolographicSpace const& holographicSpace);

        // Starts the holographic frame and updates the content.
        winrt::Windows::Graphics::Holographic::HolographicFrame Update();

        // Renders holograms, including world-locked content.
        bool Render(winrt::Windows::Graphics::Holographic::HolographicFrame const& holographicFrame);

        // Handle saving and loading of app state owned by AppMain.
        void SaveAppState();
        void LoadAppState();

        // Handle mouse input.
        void OnPointerPressed();

        void OnKeyPressed(uint32_t depthMode);

        // IDeviceNotify
        void OnDeviceLost() override;
        void OnDeviceRestored() override;

        void SetWorldSpace()
        {
            m_fUseWorldSpace = true;
        }

    private:
        // Asynchronously creates resources for new holographic cameras.
        void OnCameraAdded(
            winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
            winrt::Windows::Graphics::Holographic::HolographicSpaceCameraAddedEventArgs const& args);

        // Synchronously releases resources for holographic cameras that are no longer
        // attached to the system.
        void OnCameraRemoved(
            winrt::Windows::Graphics::Holographic::HolographicSpace const& sender,
            winrt::Windows::Graphics::Holographic::HolographicSpaceCameraRemovedEventArgs const& args);

        // Used to notify the app when the positional tracking state changes.
        void OnLocatabilityChanged(
            winrt::Windows::Perception::Spatial::SpatialLocator const& sender,
            winrt::Windows::Foundation::IInspectable const& args);

        // Used to be aware of gamepads that are plugged in after the app starts.
        void OnGamepadAdded(winrt::Windows::Foundation::IInspectable, winrt::Windows::Gaming::Input::Gamepad const& args);

        // Used to stop looking for gamepads that are removed while the app is running.
        void OnGamepadRemoved(winrt::Windows::Foundation::IInspectable, winrt::Windows::Gaming::Input::Gamepad const& args);

        // Used to respond to changes to the default spatial locator.
        void OnHolographicDisplayIsAvailableChanged(winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);

        // Clears event registration state. Used when changing to a new HolographicSpace
        // and when tearing down AppMain.
        void UnregisterHolographicEventHandlers();

        std::shared_ptr<Scenario> m_scenario;

#ifdef DRAW_SAMPLE_CONTENT
        // Listens for the Pressed spatial input event.
        std::shared_ptr<SpatialInputHandler>                        m_spatialInputHandler;
#endif

        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>                        m_deviceResources;

        // Render loop timer.
        DX::StepTimer                                               m_timer;

        // Represents the holographic space around the user.
        winrt::Windows::Graphics::Holographic::HolographicSpace     m_holographicSpace = nullptr;

        // SpatialLocator that is attached to the default HolographicDisplay.
        winrt::Windows::Perception::Spatial::SpatialLocator         m_spatialLocator = nullptr;

        // A stationary reference frame based on m_spatialLocator.
        winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference m_stationaryReferenceFrame = nullptr;

        // A reference frame attached to the holographic camera.
        winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_attachedReferenceFrame = nullptr;

        bool                                                        m_fUseWorldSpace = false;
        bool                                                        m_fFirstUpdate = true;

        // Event registration tokens.
        winrt::event_token                                          m_cameraAddedToken;
        winrt::event_token                                          m_cameraRemovedToken;
        winrt::event_token                                          m_locatabilityChangedToken;
        winrt::event_token                                          m_gamepadAddedEventToken;
        winrt::event_token                                          m_gamepadRemovedEventToken;
        winrt::event_token                                          m_holographicDisplayIsAvailableChangedEventToken;

        // Keep track of gamepads.
        struct GamepadWithButtonState
        {
            winrt::Windows::Gaming::Input::Gamepad gamepad;
            bool buttonAWasPressedLastFrame = false;
        };
        std::vector<GamepadWithButtonState>                         m_gamepads;

        // Keep track of mouse input.
        bool                                                        m_pointerPressed = false;
        bool                                                        m_keyPressed = false;
        int                                                         m_inputCount = 0;

        // Cache whether or not the HolographicCamera.Display property can be accessed.
        bool                                                        m_canGetHolographicDisplayForCamera = false;

        // Cache whether or not the HolographicDisplay.GetDefault() method can be called.
        bool                                                        m_canGetDefaultHolographicDisplay = false;

        // Cache whether or not the HolographicCameraRenderingParameters.CommitDirect3D11DepthBuffer() method can be called.
        bool                                                        m_canCommitDirect3D11DepthBuffer = false;
    };
}
