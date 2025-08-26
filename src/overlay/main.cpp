#include <stdio.h>
#include <windows.h>
#include "util.h"
#include "log.h"

#include <glad/glad.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

static GLFWwindow* glfwWindow = nullptr;
static GLuint fboHandle = 0, fboTextureHandle = 0;
static int fboTextureWidth = 0, fboTextureHeight = 0;

const float MINIMIZED_MAX_FPS = 60.0f;

extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;

void CreateGLFWWindow()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, false);

#ifdef DEBUG_LOGS
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

	fboTextureWidth = 1200;
	fboTextureHeight = 800;

	glfwWindow = glfwCreateWindow(fboTextureWidth, fboTextureHeight, "Space Calibrator", nullptr, nullptr);
	if (!glfwWindow)
		throw std::runtime_error("Failed to create window");

	glfwMakeContextCurrent(glfwWindow);
	glfwSwapInterval(1);
	gladLoadGL();

	// Minimise the window
	glfwIconifyWindow(glfwWindow);
	HWND windowHwmd = glfwGetWin32Window(glfwWindow);
	// EnableDarkModeTopBar(windowHwmd);

	// Load icon and set it in the window
	// GLFWimage images[1] = {};
	// std::string iconPath = cwd;
	// iconPath += "\\taskbar_icon.png";
	// images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4);
	// glfwSetWindowIcon(glfwWindow, 1, images);
	// stbi_image_free(images[0].pixels);

#ifdef DEBUG_LOGS
	glDebugMessageCallback(openGLDebugCallback, nullptr);
	glEnable(GL_DEBUG_OUTPUT);
#endif

	ImGui::CreateContext();
	// ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.IniFilename = nullptr;
	// io.Fonts->AddFontFromMemoryCompressedTTF(DroidSans_compressed_data, DroidSans_compressed_size, 24.0f);

	ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	ImGui::StyleColorsDark();

	glGenTextures(1, &fboTextureHandle);
	glBindTexture(GL_TEXTURE_2D, fboTextureHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboTextureWidth, fboTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glGenFramebuffers(1, &fboHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, fboHandle);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fboTextureHandle, 0);

	GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		// @TODO: Avoid exceptions
		throw std::runtime_error("OpenGL framebuffer incomplete");
	}
}

double lastFrameStartTime = glfwGetTime();
void RunLoop() {
	while (!glfwWindowShouldClose(glfwWindow))
	{
		double time = glfwGetTime();

		int width, height;
		glfwGetFramebufferSize(glfwWindow, &width, &height);
		const bool windowVisible = (width > 0 && height > 0);

		auto& io = ImGui::GetIO();

		// These change state now, so we must execute these before doing our own modifications to the io state for VR
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();

		io.DisplaySize = ImVec2((float)fboTextureWidth, (float)fboTextureHeight);
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

		io.ConfigFlags = io.ConfigFlags & ~ImGuiConfigFlags_NoMouseCursorChange;
		// if (dashboardVisible) {
		// 	io.ConfigFlags = io.ConfigFlags | ImGuiConfigFlags_NoMouseCursorChange;
		// }

		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		ImGui::EndFrame();
		ImGui::Render();

		glBindFramebuffer(GL_FRAMEBUFFER, fboHandle);
		glViewport(0, 0, fboTextureWidth, fboTextureHeight);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (width && height)
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fboHandle);
			glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glfwSwapBuffers(glfwWindow);
		}


		const double dashboardInterval = 1.0 / 90.0; // fps
		// double waitEventsTimeout = std::max(CalCtx.wantedUpdateInterval, dashboardInterval);
		double waitEventsTimeout = dashboardInterval;

		glfwWaitEventsTimeout(waitEventsTimeout);

		// If we're minimized rendering won't limit our frame rate so we need to do it ourselves.
		if (glfwGetWindowAttrib(glfwWindow, GLFW_ICONIFIED))
		{
			double targetFrameTime = 1 / MINIMIZED_MAX_FPS;
			double waitTime = targetFrameTime - (glfwGetTime() - lastFrameStartTime);
			if (waitTime > 0)
			{
				std::this_thread::sleep_for(std::chrono::duration<double>(waitTime));
			}

			lastFrameStartTime += targetFrameTime;
		}
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    util::init();
    logging::Init(/* isOverlay */ true);

    if (!glfwInit())
    {
        const char* szGlfwError = nullptr;
        glfwGetError(&szGlfwError);
        LOG_FATAL("Failed to initialise GLFW, got {}.", szGlfwError);
        MessageBoxW(nullptr, L"Failed to initialize GLFW", L"An error occured initialising Space Calibrator Nova", 0);
        return 0;
    }

	CreateGLFWWindow();

	LOG_INFO("made window hiiii");

	RunLoop();

	if (fboHandle)
		glDeleteFramebuffers(1, &fboHandle);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	// ImPlot::DestroyContext();
	ImGui::DestroyContext();

    if (glfwWindow)
        glfwDestroyWindow(glfwWindow);

    glfwTerminate();
    return 0;
}