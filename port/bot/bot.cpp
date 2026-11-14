#include "preferences.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h"
#include "../src/gs/ui_font.h"
#include <cstdio>

static std::mutex pickerLock;
static std::string picked;
static bool pickerReady;
static void SDLCALL picked_game(void *, const char *const *files, int) {
    if (files && files[0]) { std::lock_guard<std::mutex> guard(pickerLock); picked = files[0]; pickerReady = true; }
}
static std::string default_game() {
#ifdef _WIN32
    const char *name = "ZenkaiPower.exe", *platform = "windows-x64";
#else
    const char *name = "ZenkaiPower", *platform = "linux-x64";
#endif
    std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    for (const auto &candidate : {std::filesystem::u8path(base) / name,
            std::filesystem::u8path(base) / ".." / platform / name,
            std::filesystem::u8path(base).parent_path().parent_path() / name,
            std::filesystem::u8path(base) / ".." / "release" / platform / name,
            std::filesystem::current_path() / "release" / platform / name}) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate.u8string();
    }
    return "";
}
static ImFont *font(const char *name) {
    const std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    for (const auto &path : {base + "fonts/dejavu/" + name, base + "../fonts/dejavu/" + name,
                             std::string("./port/third_party/fonts/dejavu/") + name})
        if (ImFont *f = ui_font_load(path.c_str(), 18)) return f;
    return nullptr;
}
int main(int argc, char **argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ZenkaiPower Bot", SDL_GetError(), nullptr); return 1; }
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    const char *glsl = "#version 330";
    for (int attempt = 0; attempt < 2 && !context; attempt++) {
        SDL_GL_ResetAttributes();
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, attempt ? 0 : 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, attempt ? 0 : SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        glsl = attempt ? "#version 130" : "#version 330";
        window = SDL_CreateWindow("ZenkaiPower Bot", 780, 690, SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (window) context = SDL_GL_CreateContext(window);
        if (!context && window) { SDL_DestroyWindow(window); window = nullptr; }
    }
    if (!context) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ZenkaiPower Bot", SDL_GetError(), nullptr); SDL_Quit(); return 1; }
    SDL_GL_MakeCurrent(window, context); SDL_GL_SetSwapInterval(1);
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); io.IniFilename = nullptr; io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.FontDefault = font("DejaVuSansCondensed.ttf");
    ImFont *bold = font("DejaVuSansCondensed-Bold.ttf");
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(24, 20); style.FramePadding = ImVec2(10, 7);
    style.FrameRounding = 7; style.ItemSpacing = ImVec2(10, 10);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(.035f, .04f, .055f, 1);
    const ImVec4 yellow(1, .84f, .12f, 1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.35f, .30f, .10f, 1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(.5f, .42f, .10f, 1);
    ImGui_ImplSDL3_InitForOpenGL(window, context); ImGui_ImplOpenGL3_Init(glsl);
    char executable[2048] = {}, name[64] = "SparkingBot", access[64] = {}, code[16] = {}, host[256] = "127.0.0.1";
    int connection = 0, mode = 0, level = 5;
    bool retry = true, scroll = true, quit = false, delay = true;
    int delaySeconds = 5;
    BotPreferences preferences(BotPreferences::location());
    BotOptions saved; preferences.load(saved, scroll);
    SDL_strlcpy(executable, argc > 1 ? argv[1] : saved.game.empty() ? default_game().c_str() : saved.game.c_str(), sizeof(executable));
    SDL_strlcpy(name, saved.name.c_str(), sizeof(name)); SDL_strlcpy(access, saved.access.c_str(), sizeof(access));
    SDL_strlcpy(code, saved.code.c_str(), sizeof(code)); SDL_strlcpy(host, saved.host.c_str(), sizeof(host));
    connection = saved.connection; mode = saved.mode; level = saved.level; retry = saved.ranked_retry;
    delay = saved.ranked_delay; delaySeconds = saved.ranked_delay_seconds;
    BotProcess bot;
    const char *connections[] = {"Join room", "Host room", "Online matchmaking", "LAN"};
    const char *modes[] = {"1 vs 1", "Team", "DP 10", "DP 15", "DP 20", "Ranked 1 vs 1"};
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) quit = true;
        }
        { std::lock_guard<std::mutex> guard(pickerLock); if (pickerReady) { SDL_strlcpy(executable, picked.c_str(), sizeof(executable)); pickerReady = false; } }
        bot.update();
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("bot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushFont(bold, 30); ImGui::TextColored(yellow, "ZenkaiPower Bot"); ImGui::PopFont();
        ImGui::Separator();
        ImGui::BeginDisabled(bot.active());
        ImGui::TextUnformatted("Game executable");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100);
        ImGui::InputText("##game", executable, sizeof(executable)); ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(90, 0))) SDL_ShowOpenFileDialog(picked_game, nullptr, window, nullptr, 0, nullptr, false);
        if (ImGui::BeginTable("account", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn(); ImGui::TextUnformatted("Bot account name"); ImGui::SetNextItemWidth(-1); ImGui::InputText("##name", name, sizeof(name));
            ImGui::TableNextColumn(); ImGui::TextUnformatted("Access code (optional with saved login)"); ImGui::SetNextItemWidth(-1); ImGui::InputText("##access", access, sizeof(access), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_CharsUppercase);
            ImGui::EndTable();
        }
        ImGui::TextDisabled("Options and access code are remembered. Bot login is separate from your player account.");
        if (ImGui::BeginTable("battle", 3, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn(); ImGui::TextUnformatted("Connection"); ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##connection", &connection, connections, 4) && connection != 2 && mode == 5) mode = 0;
            ImGui::TableNextColumn(); ImGui::TextUnformatted("Battle mode"); ImGui::SetNextItemWidth(-1);
            ImGui::BeginDisabled(connection == 0); ImGui::Combo("##mode", &mode, modes, connection == 2 ? 6 : 5); ImGui::EndDisabled();
            ImGui::TableNextColumn(); ImGui::TextUnformatted("CPU level"); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##level", &level, 1, 5);
            ImGui::EndTable();
        }
        if (connection == 0) { ImGui::SetNextItemWidth(230); ImGui::InputTextWithHint("Room code", "Six letters or numbers", code, sizeof(code), ImGuiInputTextFlags_CharsUppercase); }
        if (connection == 3) { ImGui::SetNextItemWidth(300); ImGui::InputText("Host address", host, sizeof(host)); }
        if (connection == 2 && mode == 5) {
            ImGui::Checkbox("Rejoin ranked after the session ends", &retry);
            if (retry) {
                ImGui::Checkbox("Wait before rejoining", &delay); ImGui::SameLine();
                ImGui::BeginDisabled(!delay); ImGui::SetNextItemWidth(130);
                if (ImGui::InputInt("Seconds", &delaySeconds)) delaySeconds = delaySeconds < 0 ? 0 : delaySeconds > 3600 ? 3600 : delaySeconds;
                ImGui::EndDisabled();
            }
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        if (bot.active()) {
            if (ImGui::Button("Disconnect", ImVec2(150, 38))) bot.stop();
        } else if (ImGui::Button("Connect", ImVec2(150, 38))) {
            BotOptions options; options.game = executable; options.name = name; options.access = access;
            options.code = code; options.host = host; options.connection = connection;
            options.mode = mode; options.level = level; options.ranked_retry = retry;
            options.ranked_delay = delay; options.ranked_delay_seconds = delaySeconds;
            bot.start(options);
        }
        ImGui::SameLine(); ImGui::TextWrapped("%s", bot.status().c_str());
        auto room = bot.room();
        if (!room.empty()) {
            ImGui::TextColored(yellow, "Room code: %s", room.c_str()); ImGui::SameLine();
            if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(room.c_str());
        }
        auto results = bot.results();
        ImGui::TextDisabled("This run: %d wins   %d losses   %d draws", results[0], results[1], results[2]);
        ImGui::TextUnformatted("Activity"); ImGui::SameLine(); ImGui::Checkbox("Auto-scroll", &scroll);
        ImGui::SameLine(); if (ImGui::SmallButton("Clear")) bot.clearLog();
        ImGui::BeginChild("activity", ImVec2(0, 0), ImGuiChildFlags_Borders);
        for (const auto &line : bot.log()) ImGui::TextUnformatted(line.c_str());
        if (scroll) ImGui::SetScrollHereY(1);
        ImGui::EndChild();
        BotOptions selected; selected.game = executable; selected.name = name; selected.access = access;
        selected.host = host; selected.code = code; selected.connection = connection;
        selected.mode = mode; selected.level = level; selected.ranked_retry = retry;
        selected.ranked_delay = delay; selected.ranked_delay_seconds = delaySeconds;
        if (!preferences.save(selected, scroll)) ImGui::TextColored(ImVec4(1, .3f, .3f, 1), "%s", preferences.error.c_str());
        ImGui::End();
        ImGui::Render(); int w, h; SDL_GetWindowSizeInPixels(window, &w, &h);
        glViewport(0, 0, w, h); glClearColor(.035f, .04f, .055f, 1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (SDL_getenv("ZENKAIPOWER_BOT_SHOT") && ImGui::GetFrameCount() == 20 && w > 0 && h > 0) {
            std::vector<unsigned char> pixels((size_t)w*h*3); glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, w, h, 0x1907 /* GL_RGB */, GL_UNSIGNED_BYTE, pixels.data());
            if (FILE *fp = fopen(SDL_getenv("ZENKAIPOWER_BOT_SHOT"), "wb")) {
                fprintf(fp, "P6\n%d %d\n255\n", w, h);
                for (int y = h-1; y >= 0; y--) fwrite(pixels.data()+(size_t)y*w*3, 1, (size_t)w*3, fp);
                fclose(fp);
            }
        }
        SDL_GL_SwapWindow(window);
        if (SDL_getenv("ZENKAIPOWER_BOT_QUIT") && ImGui::GetFrameCount() > 30) quit = true;
    }
    bot.stop(); SDL_memset(access, 0, sizeof(access));
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
