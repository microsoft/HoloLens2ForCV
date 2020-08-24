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

namespace BasicHologram
{
    // Sample gesture handler.
    // Hooks up events to recognize a tap gesture, and keeps track of input using a boolean value.
    class SpatialInputHandler
    {
    public:
        SpatialInputHandler();
        ~SpatialInputHandler();

        winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceState CheckForInput();

    private:
        // Interaction event handler.
        void OnSourcePressed(
            winrt::Windows::UI::Input::Spatial::SpatialInteractionManager const& sender,
            winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceEventArgs const& args);

        // API objects used to process gesture input, and generate gesture events.
        winrt::Windows::UI::Input::Spatial::SpatialInteractionManager       m_interactionManager = nullptr;

        // Event registration token.
        winrt::event_token                                                  m_sourcePressedEventToken;

        // Used to indicate that a Pressed input event was received this frame.
        winrt::Windows::UI::Input::Spatial::SpatialInteractionSourceState   m_sourceState = nullptr;
    };
}
