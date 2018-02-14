// base lib 
#include <BaseLibrary/Logging_All.hpp>
#include <BaseLibrary/Platform/System.hpp>
#include <BaseLibrary/Platform/Input.hpp>
#include <BaseLibrary/Platform/Window.hpp>
#include <BaseLibrary/Timer.hpp>

// include interfaces
#include <GraphicsApi_LL/IGraphicsApi.hpp>
#include <GraphicsApi_LL/HardwareCapability.hpp>
#include <GraphicsApi_LL/Exception.hpp>

// hacked includes - should use some factory
#include <GraphicsApi_D3D12/GxapiManager.hpp>
#include <GraphicsEngine_LL/GraphicsEngine.hpp>


#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

#define _WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <tchar.h>
#undef DELETE

#include "QCWorld.hpp"


using std::cout;
using std::endl;

using namespace inl;
using namespace inl::gxeng;
using namespace inl::gxapi;
using inl::gxapi_dx12::GxapiManager;
using namespace std::chrono_literals;

// -----------------------------------------------------------------------------
// Globals

std::ofstream logFile;
Logger logger;
LogStream systemLogStream = logger.CreateLogStream("system");
LogStream graphicsLogStream = logger.CreateLogStream("graphics");
std::experimental::filesystem::path logFilePath;

std::string errorMessage;

// -----------------------------------------------------------------------------
// Function prototypes
bool ProcessControls(int key, bool down);
bool ProcessRawInput(RAWINPUT* raw);
std::string SelectPipeline(IGraphicsApi* gxapi);
void OnTerminate();


// -----------------------------------------------------------------------------
// Helper classes

// Reports live GPU object when GraphicsApi is freed
struct ReportDeleter {
	void operator()(IGraphicsApi* obj) const {
		obj->ReportLiveObjects();
		delete obj;
	}
};

// Processes key and joy input to control the copter
class InputHandler {
public:
	InputHandler(QCWorld* qcWorld) : world(qcWorld) {}

	void OnJoystickMove(JoystickMoveEvent evt);
	void OnKey(KeyboardEvent evt);
private:
	QCWorld* world;
};


// -----------------------------------------------------------------------------
// main()

int main(int argc, char* argv[]) {
	// Initialize logger
	logFile.open("engine_test.log");
	logFilePath = std::experimental::filesystem::current_path();
	logFilePath /= "engine_test.log";
	cout << "Log files can be found at:\n   ";
	cout << "   " << logFilePath << endl;

	if (logFile.is_open()) {
		logger.OpenStream(&logFile);
	}
	else {
		logger.OpenStream(&std::cout);
	}

	// Set exception terminate handler
	std::set_terminate(OnTerminate);


	// Create the window itself
	Window window;
	window.SetTitle("QC Simulator");
	window.SetSize({ 960, 640 });
	window.Maximize();
	

	// Create GraphicsEngine
	systemLogStream.Event("Initializing Graphics Engine...");

	std::unique_ptr<IGxapiManager> gxapiMgr;
	std::unique_ptr<IGraphicsApi, ReportDeleter> gxapi;
	std::unique_ptr<GraphicsEngine> engine;
	std::unique_ptr<QCWorld> qcWorld;
	std::unique_ptr<InputHandler> inputHandler;
	std::unique_ptr<Input> joyInput;

	try {
		// Create manager
		systemLogStream.Event("Creating GxApi Manager...");
		gxapiMgr.reset(new GxapiManager());
		auto adapters = gxapiMgr->EnumerateAdapters();
		std::string cardList;
		for (auto adapter : adapters) {
			cardList += "\n";
			cardList += adapter.name;
		}
		systemLogStream.Event("Available graphics cards:" + cardList);


		// Create graphics api
		int device = 0;
		if (argc == 3 && argv[1] == std::string("--device") && isdigit(argv[2][0])) {
			device = argv[2][0] - '0'; // works for single digits, good enough, lol
		}
		systemLogStream.Event("Creating GraphicsApi...");
		gxapi.reset(gxapiMgr->CreateGraphicsApi(adapters[device].adapterId));
		std::stringstream ss;
		ss << "Using graphics card: " << adapters[device].name;
		systemLogStream.Event(ss.str());


		// Create graphics engine
		systemLogStream.Event("Creating Graphics Engine...");

		GraphicsEngineDesc desc;
		desc.fullScreen = false;
		desc.graphicsApi = gxapi.get();
		desc.gxapiManager = gxapiMgr.get();
		desc.width = window.GetClientSize().x;
		desc.height = window.GetClientSize().y;
		desc.targetWindow = window.GetNativeHandle();
		desc.logger = &logger;

		engine.reset(new GraphicsEngine(desc));

		// Load graphics pipeline
		std::string pipelineFileName = SelectPipeline(gxapi.get());
		std::string exeDir = System::GetExecutableDir();
		std::ifstream pipelineFile(exeDir + "\\" + pipelineFileName);
		if (!pipelineFile.is_open()) {
			throw FileNotFoundException("Failed to open pipeline JSON.");
		}
		std::string pipelineDesc((std::istreambuf_iterator<char>(pipelineFile)), std::istreambuf_iterator<char>());
		engine->LoadPipeline(pipelineDesc);


		// Create mini world
		qcWorld.reset(new QCWorld(engine.get()));
		

		// Create input handling
		auto joysticks = Input::GetDeviceList(eInputSourceType::JOYSTICK);
		inputHandler = std::make_unique<InputHandler>(qcWorld.get());
		if (!joysticks.empty()) {
			joyInput = std::make_unique<Input>(joysticks.front().id);
			joyInput->SetQueueMode(eInputQueueMode::QUEUED);
			joyInput->OnJoystickMove += Delegate<void(JoystickMoveEvent)>{ &InputHandler::OnJoystickMove, inputHandler.get() };
		}

		window.OnResize += [&engine, &qcWorld](ResizeEvent evt) {
			engine->SetScreenSize(evt.clientSize.x, evt.clientSize.y);
			qcWorld->ScreenSizeChanged(evt.clientSize.x, evt.clientSize.y);
		};

		logger.Flush();
	}
	catch (Exception& ex) {
		errorMessage = std::string("Error creating GraphicsEngine: ") + ex.what() + "\n" + ex.StackTraceStr();
		systemLogStream.Event(errorMessage);
		logger.Flush();
	}
	catch (std::exception& ex) {
		errorMessage = std::string("Error creating GraphicsEngine: ") + ex.what();
		systemLogStream.Event(errorMessage);
		logger.Flush();
	}

	if (!qcWorld) {
		throw std::logic_error("Failed to create engine so I'm just throwing this from main() to trigger show log.");
	}


	// Main rendering loop
	Timer timer;
	timer.Start();
	double frameTime = 0.05, frameRateUpdate = 0;
	std::vector<double> frameTimeHistory;
	float avgFps = 0;

	auto CaptionHandler = [&window](Vec2i cursorPos) {
		Vec2i size = window.GetSize();
		RectI rc;
		rc.top = 5;
		rc.bottom = 20;
		rc.right = size.x - 5;
		rc.left = size.x - 20;
		if (rc.IsPointInside(cursorPos))
			return eWindowCaptionButton::CLOSE;
		rc.Move({ -20, 0 });
		if (rc.IsPointInside(cursorPos))
			return eWindowCaptionButton::MAXIMIZE;
		rc.Move({ -20, 0 });
		if (rc.IsPointInside(cursorPos))
			return eWindowCaptionButton::MINIMIZE;
		if (cursorPos.y < 25) {
			return eWindowCaptionButton::BAR;
		}
		return eWindowCaptionButton::NONE;
	};
	//window.SetBorderless(true);
	//window.Maximize();
	window.SetCaptionButtonHandler(CaptionHandler);
	//window.SetBorderless(false);

	while (!window.IsClosed()) {
		window.CallEvents();
		//std::this_thread::sleep_for(std::chrono::milliseconds(20));
		//continue;
		if (joyInput) {
			joyInput->CallEvents();
		}

		try {
			// Update world
			qcWorld->UpdateWorld(frameTime);
			qcWorld->RenderWorld(frameTime);

			// Calculate elapsed time for frame.
			frameTime = timer.Elapsed();
			timer.Reset();

			// Calculate average framerate
			frameRateUpdate += frameTime;
			if (frameRateUpdate > 0.5) {
				frameRateUpdate = 0;

				double avgFrameTime = 0.0;
				for (auto v : frameTimeHistory) {
					avgFrameTime += v;
				}
				avgFrameTime /= frameTimeHistory.size();
				avgFps = 1 / avgFrameTime;

				frameTimeHistory.clear();
			}
			frameTimeHistory.push_back(frameTime);

			// Set info text as window title
			unsigned width, height;
			engine->GetScreenSize(width, height);
			std::string title = "Graphics Engine Test | " + std::to_string(width) + "x" + std::to_string(height) + " | FPS=" + std::to_string((int)avgFps);
			window.SetTitle(title);
		}
		catch (Exception& ex) {
			std::stringstream trace;
			trace << "Graphics engine error:" << ex.what() << "\n";
			ex.PrintStackTrace(trace);
			systemLogStream.Event(trace.str());
			PostQuitMessage(0);
		}
		catch (std::exception& ex) {
			systemLogStream.Event(std::string("Graphics engine error: ") + ex.what());
			logger.Flush();
			PostQuitMessage(0);
		}
	}

	cout << "Shutting down." << endl;
	return 0;
}



/*
bool ProcessControls(int key, bool down) {
	float enable = down ? 1.f : 0.f;
	switch (key) {
		case 'W': pQcWorld->TiltForward(enable); break;
		case 'A': pQcWorld->TiltLeft(enable); break;
		case 'S': pQcWorld->TiltBackward(enable); break;
		case 'D': pQcWorld->TiltRight(enable); break;
		case VK_LEFT: pQcWorld->RotateLeft(enable); break;
		case VK_RIGHT: pQcWorld->RotateRight(enable); break;
		case VK_UP: pQcWorld->Ascend(enable); break;
		case VK_DOWN: pQcWorld->Descend(enable); break;
		default: return false;
	}
	return true;
}

bool ProcessRawInput(RAWINPUT* raw) {
	if (raw->header.dwType == RIM_TYPEMOUSE) {
		// track up/down states
		static bool lmbDown = false, mmbDown = false, rmbDown = false;
		static POINT cursorPos{ 0, 0 };
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) {
			lmbDown = true;
		}
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
			mmbDown = true;
		}
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) {
			rmbDown = true;
		}
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) {
			lmbDown = false;
		}
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) {
			mmbDown = false;
		}
		if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) {
			rmbDown = false;
		}
		bool down = mmbDown || rmbDown;
		if (down) {
			SetCursorPos(cursorPos.x, cursorPos.y);
		}
		else {
			GetCursorPos(&cursorPos);
		}

		// check if relative
		bool relative = raw->data.mouse.usFlags == 0;
		if (!relative) {
			return false;
		}

		//  set motion
		float dx = raw->data.mouse.lLastX, dy = raw->data.mouse.lLastY;
		static float tiltfw = 0.0f, tiltr = 0.0f;
		static float turnr = 0.0f;
		static float heading = 0.0f;
		static float lookoff = 0.0f;
		static float look = 0.0f;

		if (rmbDown) {
			turnr += -dx / 400.f;
			pQcWorld->Heading(heading + turnr);
		}
		else {
			heading = pQcWorld->Heading();
			turnr = 0.0f;
		}

		static bool lastMmbDown = false;
		if (mmbDown) {
			tiltfw += -dy / 300.f;
			tiltr += dx / 300.f;
			pQcWorld->TiltForward(tiltfw);
			pQcWorld->TiltRight(tiltr);
		}
		else {
			if (lastMmbDown != mmbDown) {
				pQcWorld->TiltForward(0);
				pQcWorld->TiltRight(0);
			}
			tiltfw = tiltr = 0;
		}
		lastMmbDown = mmbDown;

		if (rmbDown) {
			lookoff += -dy / 400.f;
			pQcWorld->Look(look + lookoff);
		}
		else {
			look = pQcWorld->Look();
			lookoff = 0;
		}

		return true;
	}

	return false;
}
*/




void InputHandler::OnJoystickMove(JoystickMoveEvent evt) {
	if (!world) {
		return;
	}
	//cout << "Axis" << evt.axis << " = " << evt.absPos << endl;
	switch (evt.axis) {
		case 0: world->TiltForward(-evt.absPos); break;
		case 1: world->TiltRight(evt.absPos); break;
		case 2: world->Ascend(-std::copysign(1.0f, evt.absPos)*std::max(0.0f, std::abs(evt.absPos)-0.15f)); break;
		case 3: world->RotateRight(evt.absPos); break;
		default:
			break;
	}
}


void InputHandler::OnKey(KeyboardEvent evt) {
	if (!world) {
		return;
	}
}




std::string SelectPipeline(IGraphicsApi* gxapi) {
	std::unique_ptr<ICapabilityQuery> query(gxapi->GetCapabilityQuery());

	auto binding = query->QueryResourceBinding();
	auto tiled = query->QueryTiledResources();
	auto conserv = query->QueryConservativeRasterization();
	auto heaps = query->QueryResourceHeaps();
	auto additional = query->QueryAdditional();

	int bindingTier = binding.GetDx12Tier();
	int tiledTier = tiled.GetDx12Tier();
	int conservTier = conserv.GetDx12Tier();
	int heapsTier = heaps.GetDx12Tier();

	int maxVmemResourceGB = (1ull << additional.virtualAddressBitsPerResource) / 1024/1024/1024;
	int maxVmemProcessGB = (1ull << additional.virtualAddressBitsPerProcess) / 1024/1024/1024;

	cout << "Selected GPU supports:" << endl;
	cout << "   Resource binding:      Tier" << bindingTier << endl;
	cout << "   Tiled resources:       Tier" << tiledTier << endl;
	cout << "   Conservative raster.:  " << (conservTier == 0 ? std::string("UNSUPPORTED") : "Tier" + std::to_string(conservTier)) << endl;
	cout << "   Resource heaps:        Tier" << heapsTier << endl;
	cout << "   Process virtual memory: " << maxVmemProcessGB << " GiB, resource vmem: " << maxVmemResourceGB << " GiB" << endl;
	cout << "   Shader model " << additional.shaderModelMajor << "." << additional.shaderModelMinor << endl;

	CapsRequirementSet giReqs;
	giReqs.conservativeRasterization = CapsConservativeRasterization::Dx12Tier1();
	giReqs.formats.push_back({ eFormat::R32G32B32A32_FLOAT, eCapsFormatUsage({eCapsFormatUsage::UNORDERED_ACCESS_LOAD, eCapsFormatUsage::UNORDERED_ACCESS_STORE, eCapsFormatUsage::TEXTURE_3D}) });

	bool supportsGi = query->SupportsAll(giReqs);
	if (supportsGi) {
		return "pipeline.json";
	}
	else {
		return "pipeline_nogi.json";
	}
	
}


void OnTerminate() {
	try {
		std::rethrow_exception(std::current_exception());
		systemLogStream.Event(std::string("Terminate called, shutting down services."));
	}
	catch (Exception& ex) {
		systemLogStream.Event(std::string("Terminate called, shutting down services.") + ex.what() + "\n" + ex.StackTraceStr());
	}
	catch (std::exception& ex) {
		systemLogStream.Event(std::string("Terminate called, shutting down services.") + ex.what());
	}
	logger.Flush();
	logger.OpenStream(nullptr);
	logFile.close();
	int ans = MessageBoxA(NULL, "Open logs?", "Unhandled exception", MB_YESNO);
	if (ans == IDYES) {
		system(logFilePath.string().c_str());
	}

	std::abort();
}