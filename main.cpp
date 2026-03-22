#include "header.h"
#include <SDL.h>

/*
NOTE : You are free to change the code as you wish, the main objective is to make the
       application work and pass the audit.

       It will be provided the main function with the following functions :

       - `void systemWindow(const char *id, ImVec2 size, ImVec2 position)`
            This function will draw the system window on your screen
       - `void memoryProcessesWindow(const char *id, ImVec2 size, ImVec2 position)`
            This function will draw the memory and processes window on your screen
       - `void networkWindow(const char *id, ImVec2 size, ImVec2 position)`
            This function will draw the network window on your screen
*/

// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h> // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h> // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h> // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h> // Initialize with gladLoadGL(...) or gladLoaderLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE      // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h> // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE        // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h> // Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

namespace
{
struct GraphState
{
    vector<float> values;
    bool paused;
    float fps;
    float yScale;
    Uint64 lastUpdateTicks;

    GraphState() : values(120, 0.0f), paused(false), fps(20.0f), yScale(100.0f), lastUpdateTicks(0) {}
};

string formatPercent(float value)
{
    ostringstream output;
    output << fixed << setprecision(1) << value << "%";
    return output.str();
}

string formatState(char state)
{
    switch (state)
    {
    case 'R':
        return "Running";
    case 'S':
        return "Sleeping";
    case 'D':
        return "Uninterruptible";
    case 'Z':
        return "Zombie";
    case 'T':
    case 't':
        return "Stopped";
    case 'I':
        return "Idle";
    default:
        return string(1, state);
    }
}

void pushGraphValue(GraphState &graph, float value)
{
    const Uint64 now = SDL_GetTicks64();
    const double intervalMs = 1000.0 / max(1.0f, graph.fps);
    if (!graph.paused && (graph.lastUpdateTicks == 0 || static_cast<double>(now - graph.lastUpdateTicks) >= intervalMs))
    {
        graph.lastUpdateTicks = now;
        graph.values.erase(graph.values.begin());
        graph.values.push_back(value);
    }
}

void renderMetricCard(const char *label, const string &value, float ratio, ImVec2 size)
{
    ratio = max(0.0f, min(1.0f, ratio));
    ImGui::BeginChild(label, size, true);
    ImGui::Text("%s", label);
    ImGui::Spacing();
    ImGui::ProgressBar(ratio, ImVec2(-1.0f, 18.0f), value.c_str());
    ImGui::EndChild();
}

void renderGraphControls(GraphState &graph, const char *pauseLabel, float minScale, float maxScale)
{
    ImGui::Checkbox(pauseLabel, &graph.paused);
    ImGui::SameLine();
    ImGui::SliderFloat("FPS", &graph.fps, 1.0f, 60.0f, "%.0f");
    ImGui::SliderFloat("Y Scale", &graph.yScale, minScale, maxScale, "%.0f");
}

void renderPlot(const char *label, GraphState &graph, const string &overlay, ImVec2 size)
{
    ImGui::PlotLines(label, graph.values.data(), static_cast<int>(graph.values.size()), 0, overlay.c_str(), 0.0f, graph.yScale, size);
}

void renderSummaryRow(const char *label, const string &value)
{
    ImGui::Text("%s", label);
    ImGui::SameLine(170.0f);
    ImGui::TextColored(ImVec4(0.70f, 0.85f, 1.0f, 1.0f), "%s", value.c_str());
}

bool filterMatches(const ProcessInfo &process, const string &needle)
{
    if (needle.empty())
        return true;

    string pid = to_string(process.pid);
    string haystack = process.name + " " + pid + " " + formatState(process.state);
    string loweredHaystack = haystack;
    string loweredNeedle = needle;
    transform(loweredHaystack.begin(), loweredHaystack.end(), loweredHaystack.begin(), ::tolower);
    transform(loweredNeedle.begin(), loweredNeedle.end(), loweredNeedle.begin(), ::tolower);
    return loweredHaystack.find(loweredNeedle) != string::npos;
}

void renderSystemGraphs()
{
    static CPUStats previousStats = readCPUStats();
    static GraphState cpuGraph;
    static GraphState fanGraph;
    static GraphState thermalGraph;

    const CPUStats currentStats = readCPUStats();
    const float cpuUsage = calculateCPUUsage(previousStats, currentStats);
    previousStats = currentStats;

    const FanInfo fan = getFanInfo();
    const ThermalInfo thermal = getThermalInfo();

    if (ImGui::BeginTabBar("system-tabs"))
    {
        if (ImGui::BeginTabItem("CPU"))
        {
            pushGraphValue(cpuGraph, cpuUsage);
            renderGraphControls(cpuGraph, "Pause CPU", 25.0f, 100.0f);
            renderPlot("##cpu-plot", cpuGraph, "CPU " + formatPercent(cpuUsage), ImVec2(-1.0f, 180.0f));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Fan"))
        {
            const float fanValue = fan.available ? static_cast<float>(fan.rpm) : 0.0f;
            pushGraphValue(fanGraph, fanValue);
            renderGraphControls(fanGraph, "Pause Fan", 500.0f, 6000.0f);
            renderSummaryRow("Status", fan.status);
            renderSummaryRow("Speed", fan.available ? to_string(fan.rpm) + " RPM" : "N/A");
            renderSummaryRow("Level", fan.available ? formatPercent(fan.levelPercent) : "N/A");
            renderPlot("##fan-plot", fanGraph, fan.available ? to_string(fan.rpm) + " RPM" : "No sensor", ImVec2(-1.0f, 150.0f));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Thermal"))
        {
            const float temperature = thermal.available ? thermal.temperatureC : 0.0f;
            pushGraphValue(thermalGraph, temperature);
            renderGraphControls(thermalGraph, "Pause Thermal", 30.0f, 120.0f);
            renderSummaryRow("Sensor", thermal.available ? thermal.label : "N/A");
            renderSummaryRow("Current", thermal.available ? to_string(static_cast<int>(thermal.temperatureC)) + " C" : "N/A");
            renderPlot("##thermal-plot", thermalGraph, thermal.available ? to_string(static_cast<int>(thermal.temperatureC)) + " C" : "No sensor", ImVec2(-1.0f, 150.0f));
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void renderProcessTable(unsigned long long memTotalKB)
{
    static CPUStats previousStats = readCPUStats();
    static map<int, unsigned long long> previousProcessTicks;
    static char filterBuffer[128] = "";

    const CPUStats currentStats = readCPUStats();
    const unsigned long long previousTotal = previousStats.user + previousStats.nice + previousStats.system + previousStats.idle + previousStats.iowait + previousStats.irq + previousStats.softirq + previousStats.steal;
    const unsigned long long currentTotal = currentStats.user + currentStats.nice + currentStats.system + currentStats.idle + currentStats.iowait + currentStats.irq + currentStats.softirq + currentStats.steal;
    const unsigned long long totalDelta = currentTotal > previousTotal ? currentTotal - previousTotal : 0;

    vector<ProcessInfo> processes = getProcesses(totalDelta, previousProcessTicks, memTotalKB);
    previousStats = currentStats;
    previousProcessTicks.clear();
    for (size_t i = 0; i < processes.size(); ++i)
        previousProcessTicks[processes[i].pid] = processes[i].totalTimeTicks;

    ImGui::InputTextWithHint("##process-filter", "Filter by PID, name or state", filterBuffer, sizeof(filterBuffer));
    const string filterText = filterBuffer;

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("process-table", 5, flags, ImVec2(0.0f, 255.0f)))
    {
        ImGui::TableSetupColumn("PID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("CPU Usage");
        ImGui::TableSetupColumn("Memory Usage");
        ImGui::TableHeadersRow();

        for (size_t index = 0; index < processes.size(); ++index)
        {
            const ProcessInfo &process = processes[index];
            if (!filterMatches(process, filterText))
                continue;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", process.pid); 

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", process.name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", formatState(process.state).c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", formatPercent(process.cpuPercent).c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%s", formatPercent(process.memoryPercent).c_str());
        }
        ImGui::EndTable();
    }
}

void renderNetworkTables(const vector<NetworkEntry> &entries)
{
    if (ImGui::BeginTabBar("network-tables"))
    {
        if (ImGui::BeginTabItem("RX"))
        {
            if (ImGui::BeginTable("rx-table", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("Interface");
                ImGui::TableSetupColumn("Bytes");
                ImGui::TableSetupColumn("Packets");
                ImGui::TableSetupColumn("Errs");
                ImGui::TableSetupColumn("Drop");
                ImGui::TableSetupColumn("Fifo");
                ImGui::TableSetupColumn("Frame");
                ImGui::TableSetupColumn("Compressed");
                ImGui::TableSetupColumn("Multicast");
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < entries.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", entries[i].name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", formatBytes(entries[i].rx.bytes).c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%llu", entries[i].rx.packets);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%llu", entries[i].rx.errs);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%llu", entries[i].rx.drop);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%llu", entries[i].rx.fifo);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%llu", entries[i].rx.frame);
                    ImGui::TableSetColumnIndex(7);
                    ImGui::Text("%llu", entries[i].rx.compressed);
                    ImGui::TableSetColumnIndex(8);
                    ImGui::Text("%llu", entries[i].rx.multicast);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("TX"))
        {
            if (ImGui::BeginTable("tx-table", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("Interface");
                ImGui::TableSetupColumn("Bytes");
                ImGui::TableSetupColumn("Packets");
                ImGui::TableSetupColumn("Errs");
                ImGui::TableSetupColumn("Drop");
                ImGui::TableSetupColumn("Fifo");
                ImGui::TableSetupColumn("Colls");
                ImGui::TableSetupColumn("Carrier");
                ImGui::TableSetupColumn("Compressed");
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < entries.size(); ++i)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", entries[i].name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", formatBytes(entries[i].tx.bytes).c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%llu", entries[i].tx.packets);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%llu", entries[i].tx.errs);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%llu", entries[i].tx.drop);
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%llu", entries[i].tx.fifo);
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%llu", entries[i].tx.colls);
                    ImGui::TableSetColumnIndex(7);
                    ImGui::Text("%llu", entries[i].tx.carrier);
                    ImGui::TableSetColumnIndex(8);
                    ImGui::Text("%llu", entries[i].tx.compressed);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void renderNetworkUsage(const vector<NetworkEntry> &entries)
{
    if (ImGui::BeginTabBar("network-usage"))
    {
        if (ImGui::BeginTabItem("RX Usage"))
        {
            for (size_t i = 0; i < entries.size(); ++i)
            {
                const string label = entries[i].name + " RX";
                ImGui::Text("%s", label.c_str());
                ImGui::ProgressBar(bytesToDisplayScale(entries[i].rx.bytes), ImVec2(-1.0f, 16.0f), formatBytes(entries[i].rx.bytes).c_str());
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("TX Usage"))
        {
            for (size_t i = 0; i < entries.size(); ++i)
            {
                const string label = entries[i].name + " TX";
                ImGui::Text("%s", label.c_str());
                ImGui::ProgressBar(bytesToDisplayScale(entries[i].tx.bytes), ImVec2(-1.0f, 16.0f), formatBytes(entries[i].tx.bytes).c_str());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
}

// systemWindow, display information for the system monitorization
void systemWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::Begin(id, NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const SystemIdentity identity = getSystemIdentity();
    const ProcessCounts counts = getProcessCounts();

    renderSummaryRow("Operating System", identity.osName);
    renderSummaryRow("User", identity.userName);
    renderSummaryRow("Hostname", identity.hostName);
    renderSummaryRow("CPU", identity.cpuName);
    ImGui::Separator();

    ImGui::Columns(2, "process-state-columns", false);
    renderSummaryRow("Total Tasks", to_string(counts.total));
    renderSummaryRow("Sleeping", to_string(counts.sleeping));
    renderSummaryRow("Running", to_string(counts.running));
    renderSummaryRow("Uninterruptible", to_string(counts.uninterruptible));
    ImGui::NextColumn();
    renderSummaryRow("Zombie", to_string(counts.zombie));
    renderSummaryRow("Stopped/Traced", to_string(counts.stopped));
    renderSummaryRow("Other", to_string(counts.other));
    ImGui::Columns(1);
    ImGui::Separator();

    renderSystemGraphs();
    ImGui::End();
}

// memoryProcessesWindow, display information for the memory and processes information
void memoryProcessesWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::Begin(id, NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const MemoryStats memory = getMemoryStats();
    const DiskStats disk = getDiskStats("/");

    const float memRatio = memory.memTotalKB > 0 ? static_cast<float>(memory.memUsedKB) / memory.memTotalKB : 0.0f;
    const float swapRatio = memory.swapTotalKB > 0 ? static_cast<float>(memory.swapUsedKB) / memory.swapTotalKB : 0.0f;
    const float diskRatio = disk.totalBytes > 0 ? static_cast<float>(disk.usedBytes) / disk.totalBytes : 0.0f;

    renderMetricCard("RAM", formatBytes(memory.memUsedKB * 1024ULL) + " / " + formatBytes(memory.memTotalKB * 1024ULL), memRatio, ImVec2(0.0f, 62.0f));
    renderMetricCard("SWAP", formatBytes(memory.swapUsedKB * 1024ULL) + " / " + formatBytes(memory.swapTotalKB * 1024ULL), swapRatio, ImVec2(0.0f, 62.0f));
    renderMetricCard("Disk", formatBytes(disk.usedBytes) + " / " + formatBytes(disk.totalBytes), diskRatio, ImVec2(0.0f, 62.0f));

    ImGui::Spacing();
    ImGui::Text("Processes");
    renderProcessTable(memory.memTotalKB);
    ImGui::End();
}

// network, display information network information
void networkWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::Begin(id, NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    const vector<NetworkEntry> entries = getNetworkEntries();
    const Networks addresses = getIPv4Addresses();

    ImGui::Text("IPv4 Addresses");
    for (size_t i = 0; i < addresses.ip4s.size(); ++i)
        ImGui::BulletText("%s: %s", addresses.ip4s[i].name.c_str(), addresses.ip4s[i].address.c_str());
    if (addresses.ip4s.empty())
        ImGui::Text("No IPv4 interfaces detected.");

    ImGui::Separator();
    renderNetworkTables(entries);
    ImGui::Separator();
    renderNetworkUsage(entries);
    ImGui::End();
}

// Main code
int main(int, char **)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("System Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
    bool err = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) == 0; // glad2 recommend using the windowing library loader instead of the (optionally) bundled one.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    bool err = false;
    glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    bool err = false;
    glbinding::initialize([](const char *name) { return (glbinding::ProcAddress)SDL_GL_GetProcAddress(name); });
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // render bindings
    ImGuiIO &io = ImGui::GetIO();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // background color
    // note : you are free to change the style of the application
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        {
            ImVec2 mainDisplay = io.DisplaySize;
            memoryProcessesWindow("== Memory and Processes ==",
                                  ImVec2((mainDisplay.x / 2) - 20, (mainDisplay.y / 2) + 30),
                                  ImVec2((mainDisplay.x / 2) + 10, 10));
            // --------------------------------------
            systemWindow("== System ==",
                         ImVec2((mainDisplay.x / 2) - 10, (mainDisplay.y / 2) + 30),
                         ImVec2(10, 10));
            // --------------------------------------
            networkWindow("== Network ==",
                          ImVec2(mainDisplay.x - 20, (mainDisplay.y / 2) - 60),
                          ImVec2(10, (mainDisplay.y / 2) + 50));
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
