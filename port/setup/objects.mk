# Compiler-generated dependencies keep extraction edits from rebuilding ImGui.
IMGUI_SOURCES := imgui imgui_draw imgui_tables imgui_widgets imgui_impl_sdl3 imgui_impl_sdlgpu3 imgui_impl_opengl3
APP_SOURCES ?= native setup updater text
APP_DIR ?= port/setup
OBJECTS := $(addprefix $(O)/,$(addsuffix .o,$(IMGUI_SOURCES) $(APP_SOURCES)))
.PHONY: objects
objects: $(OBJECTS)

$(O)/%.o: $(I)/%.cpp $(O)/settings port/setup/objects.mk
	$(CXX) -std=c++17 -O2 -w -I$(I) -I$(O) $(SDL) -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	mv $@.tmp $@

$(O)/%.o: $(APP_DIR)/%.cpp $(O)/settings port/setup/objects.mk
	$(CXX) -std=c++17 -O2 -w -I$(I) -I$(O) $(SDL) -MMD -MP -MF $@.d -MT $@ -c $< -o $@.tmp
	mv $@.tmp $@

-include $(wildcard $(O)/*.o.d)
