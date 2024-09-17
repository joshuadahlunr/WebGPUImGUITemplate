#define WEBGPU_CPP_IMPLEMENTATION
#include <glfw3webgpu.h>
#ifdef  __EMSCRIPTEN__
	#include "thirdparty/WebGPU-distribution/include-emscripten/webgpu/webgpu.hpp"
#else
	#include <webgpu/webgpu.hpp>
#endif
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>

#include <algorithm>
#include <iostream>

template<typename F, typename... Args>
auto make_function_pointer(const F& f, Args...) {
	static F ref = f;
	return +[](Args... args){ return ref(std::forward<Args>(args)...); };
}

wgpu::PresentMode getBestPresentMode(WGPUAdapter adapter, WGPUSurface surface) {
	wgpu::SurfaceCapabilities caps;
	static_cast<wgpu::Surface>(surface).getCapabilities(adapter, &caps);

	if(std::find(caps.presentModes, caps.presentModes + caps.presentModeCount, wgpu::PresentMode::Mailbox) != caps.presentModes + caps.presentModeCount)
		return wgpu::PresentMode::Mailbox;
	else if(std::find(caps.presentModes, caps.presentModes + caps.presentModeCount, wgpu::PresentMode::Immediate) != caps.presentModes + caps.presentModeCount)
		return wgpu::PresentMode::Immediate;
	// else if(std::find(caps.presentModes, caps.presentModes + caps.presentModeCount, wgpu::PresentMode::FifoRelaxed) != caps.presentModes + caps.presentModeCount)
	// 	return wgpu::PresentMode::FifoRelaxed;
	else return wgpu::PresentMode::Fifo;
}

wgpu::TextureFormat getBestSurfaceFormat(WGPUAdapter adapter, WGPUSurface surface) {
	constexpr static auto isSRGB = [](wgpu::TextureFormat fmt) {
		switch(fmt) {
			case wgpu::TextureFormat::RGBA8UnormSrgb:
			case wgpu::TextureFormat::BGRA8UnormSrgb:
			case wgpu::TextureFormat::BC1RGBAUnormSrgb:
			case wgpu::TextureFormat::BC2RGBAUnormSrgb:
			case wgpu::TextureFormat::BC3RGBAUnormSrgb:
			case wgpu::TextureFormat::BC7RGBAUnormSrgb:
			case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
			case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
			case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
			case wgpu::TextureFormat::ASTC4x4UnormSrgb:
			case wgpu::TextureFormat::ASTC5x4UnormSrgb:
			case wgpu::TextureFormat::ASTC5x5UnormSrgb:
			case wgpu::TextureFormat::ASTC6x5UnormSrgb:
			case wgpu::TextureFormat::ASTC6x6UnormSrgb:
			case wgpu::TextureFormat::ASTC8x5UnormSrgb:
			case wgpu::TextureFormat::ASTC8x6UnormSrgb:
			case wgpu::TextureFormat::ASTC8x8UnormSrgb:
			case wgpu::TextureFormat::ASTC10x5UnormSrgb:
			case wgpu::TextureFormat::ASTC10x6UnormSrgb:
			case wgpu::TextureFormat::ASTC10x8UnormSrgb:
			case wgpu::TextureFormat::ASTC10x10UnormSrgb:
			case wgpu::TextureFormat::ASTC12x10UnormSrgb:
			case wgpu::TextureFormat::ASTC12x12UnormSrgb:
				return true;
			default: return false;
		}
	};

	wgpu::SurfaceCapabilities caps;
	static_cast<wgpu::Surface>(surface).getCapabilities(adapter, &caps);

	for(size_t i = 0; i < caps.formatCount; ++i)
		if(isSRGB(caps.formats[i]))
			return caps.formats[i];
	return caps.formats[0];
}

void setupImGUI(GLFWwindow* window, WGPUDevice device, WGPUTextureFormat preferredFormat) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOther(window, true);
#ifdef __EMSCRIPTEN__
	ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
	ImGui_ImplWGPU_InitInfo init_info;
	init_info.Device = device;
	init_info.NumFramesInFlight = 3;
	init_info.RenderTargetFormat = preferredFormat;
	init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
	ImGui_ImplWGPU_Init(&init_info);
}

void setupImGUIFrame() {
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

int main() {
	// Setup Window
	if(glfwInit() != GLFW_TRUE)
		return 1;
	glfwWindowHint(GLFW_NO_WINDOW_CONTEXT, true);
	glfwWindowHint(GLFW_RESIZABLE, true);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	auto window = glfwCreateWindow(800, 600, "WebGPU ImGUI Template", nullptr, nullptr);

	// Setup WebGPU
#ifndef __EMSCRIPTEN__
	auto instance = wgpu::createInstance(wgpu::Default);
#else
	wgpu::Instance instance{}; *((size_t*)&instance) = 1; // Set the value of the instance to 1
#endif
	wgpu::Surface surface = glfwCreateWindowWGPUSurface(instance, window);
	auto adapter = instance.requestAdapter(WGPURequestAdapterOptions{
		.compatibleSurface = surface,
		.powerPreference = wgpu::PowerPreference::HighPerformance,
	});
	auto presentMode = getBestPresentMode(adapter, surface);
	WGPUTextureFormat preferredFormat = getBestSurfaceFormat(adapter, surface);
	auto device = adapter.requestDevice(wgpu::Default);
	auto queue = device.getQueue();

	// Setup ImGui
	setupImGUI(window, device, preferredFormat);

	// Setup Surface Configuration (What happens on resize)
	const static auto configurator = [&](GLFWwindow* window, int width, int height){
		ImGui_ImplWGPU_InvalidateDeviceObjects();

		surface.configure(WGPUSurfaceConfiguration {
			.device = device,
			.format = preferredFormat,
			.usage = wgpu::TextureUsage::RenderAttachment,
			// .viewFormatCount = 1,
			// .viewFormats = &preferredFormat,
			.viewFormatCount = 0,
			.alphaMode = wgpu::CompositeAlphaMode::Opaque,
			.width = static_cast<uint32_t>(width),
			.height = static_cast<uint32_t>(height),
			.presentMode = presentMode
		});

		ImGui_ImplWGPU_CreateDeviceObjects();
	};
	glfwSetWindowSizeCallback(window, make_function_pointer(configurator, (GLFWwindow*)nullptr, int{}, int{}));
	configurator(window, 800, 600);

	// Main Loop
	bool showDemoWindow = true;
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(make_function_pointer([&]{
#else
	while(!glfwWindowShouldClose(window)) {
#endif
		// Poll GLFW Events
		glfwPollEvents();

		// ImGUI Rendering
		setupImGUIFrame();
		{
			// ImGui Rendering Goes Here
			if (showDemoWindow)
				ImGui::ShowDemoWindow(&showDemoWindow);
		}
		ImGui::Render();

		// Poll WebGPU Events
#ifndef __EMSCRIPTEN__
		device.tick();
#endif

		// Get texture to draw on
		wgpu::SurfaceTexture texture;
		surface.getCurrentTexture(&texture);
		auto view = static_cast<wgpu::Texture>(texture.texture).createView();

		// Bind it as first output from fragment shader
		wgpu::RenderPassColorAttachment colorAttachment = wgpu::Default;
		colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
		colorAttachment.loadOp = wgpu::LoadOp::Clear;
		colorAttachment.storeOp = wgpu::StoreOp::Store;
		colorAttachment.clearValue = {.45, .55, .6, 1};
		colorAttachment.view = view;
		// Submit draw commands
		auto encoder = device.createCommandEncoder();
		auto renderPass = encoder.beginRenderPass(WGPURenderPassDescriptor{
			.colorAttachmentCount = 1, .colorAttachments = &colorAttachment
		});
		ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
		renderPass.end();
		auto commandBuffer = encoder.finish();
		queue.submit(commandBuffer);

		// Update whats shown on the surface
#ifndef __EMSCRIPTEN__
		surface.present();
#endif

		// Release frame specific objects
		commandBuffer.release();
		renderPass.release();
		encoder.release();
		view.release();
#ifdef __EMSCRIPTEN__
	}), 0, true);
#else
	}
#endif

	// Cleanup ImGUI
	ImGui_ImplWGPU_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	// Cleanup WebGPU
	device.release();
	adapter.release();
	surface.release();
	instance.release();
	// Cleanup GLFW
	glfwDestroyWindow(window);
	glfwTerminate();
}