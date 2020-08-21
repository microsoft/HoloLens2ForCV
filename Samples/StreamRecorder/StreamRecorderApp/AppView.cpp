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

#include "AppMain.h"

#include "../Cannon/DrawCall.h"
#include "../Cannon/MixedReality.h"

#include <intrin.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>

#include <memory>

using namespace std;

struct AppView : winrt::implements<AppView, winrt::Windows::ApplicationModel::Core::IFrameworkViewSource, winrt::Windows::ApplicationModel::Core::IFrameworkView>
{
	bool windowClosed = false;
	std::unique_ptr<AppMain> m_main;

	winrt::Windows::ApplicationModel::Core::IFrameworkView CreateView()
	{
		return *this;
	}

	void Initialize(winrt::Windows::ApplicationModel::Core::CoreApplicationView const& applicationView)
	{
		DrawCall::Initialize();
	}

	void AppView::Load(winrt::hstring const&)
	{
	}

	void AppView::Uninitialize()
	{
	}

	void SetWindow(winrt::Windows::UI::Core::CoreWindow const& window)
	{
		window.Closed({ this, &AppView::OnWindowClosed });

		if(!MixedReality::IsAvailable())
			DrawCall::InitializeSwapChain((unsigned)window.Bounds().Width, (unsigned)window.Bounds().Height, window);

		if (!m_main)
			m_main = make_unique<AppMain>();
	}

	void AppView::Run()
	{
		auto window = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread();
		window.Activate();

		while (!windowClosed)
		{
			winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(winrt::Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);
			m_main->Update();
			m_main->Render();
		}
	}

	void AppView::OnWindowClosed(winrt::Windows::UI::Core::CoreWindow const& sender, winrt::Windows::UI::Core::CoreWindowEventArgs const& args)
	{
		windowClosed = true;
	}
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	winrt::Windows::ApplicationModel::Core::CoreApplication::Run(winrt::make<AppView>());
}
