NOD_ROOT ?= port/build/nod
NOD_LIB := $(NOD_ROOT)/source/target/x86_64-unknown-linux-gnu/release/libnod.a
NOD_FLAGS := -DZENKAIPOWER_NOD_INSTALLER=1 -I$(NOD_ROOT)/source/nod-ffi/include
NOD_LINK := $(NOD_LIB) -ldl -lm

.PHONY: installer-test-driver
installer-test-driver: port/build/native-extract-test
port/build/native-extract-test: port/tests/installer/native_extract.cpp port/release/tests.mk port/build/setup/text.o $(NOD_LIB)
	$(CXX) -std=c++17 -O1 $(NOD_FLAGS) -MMD -MP -MF $@.d -MT $@ $< port/build/setup/text.o $(NOD_LINK) -lSDL3 -lpthread -o $@.tmp
	mv $@.tmp $@
-include port/build/native-extract-test.d

.PHONY: installer-retry-test
installer-retry-test: port/build/installer-retry-test
port/build/installer-retry-test: port/tests/installer/retry.cpp port/release/tests.mk $(wildcard port/build/setup/imgui*.o) port/build/setup/native.o port/build/setup/updater.o port/build/setup/text.o $(NOD_LIB)
	$(CXX) -std=c++17 -O1 -Iport/third_party/imgui -Iport/build/setup -MMD -MP -MF $@.d -MT $@ $< $(wildcard port/build/setup/imgui*.o) port/build/setup/native.o port/build/setup/updater.o port/build/setup/text.o $(NOD_LINK) -lSDL3 -lpthread -ldl -o $@.tmp
	mv $@.tmp $@
-include port/build/installer-retry-test.d

FONT_TEST_OBJECTS := $(addprefix port/build/setup/,imgui.o imgui_draw.o imgui_tables.o imgui_widgets.o)
.PHONY: installer-font-test
installer-font-test: port/build/installer-font-test
port/build/installer-font-test: port/tests/installer/font_load.cpp port/release/tests.mk $(FONT_TEST_OBJECTS)
	$(CXX) -std=c++17 -O1 -Iport/third_party/imgui -MMD -MP -MF $@.d -MT $@ $< $(FONT_TEST_OBJECTS) -o $@.tmp
	mv $@.tmp $@
-include port/build/installer-font-test.d

.PHONY: native-bot-test
native-bot-test: port/build/native-bot-test
port/build/native-bot-test: port/tests/bot/process.cpp port/release/tests.mk
	$(CXX) -std=c++17 -O1 -MMD -MP -MF $@.d -MT $@ $< -lSDL3 -lpthread -o $@.tmp
	mv $@.tmp $@
-include port/build/native-bot-test.d

.PHONY: bot-preferences-test
bot-preferences-test: port/build/bot-preferences-test
port/build/bot-preferences-test: port/tests/bot/preferences.cpp port/release/tests.mk
	$(CXX) -std=c++17 -O1 -MMD -MP -MF $@.d -MT $@ $< -lSDL3 -lpthread -o $@.tmp
	mv $@.tmp $@
-include port/build/bot-preferences-test.d

.PHONY: bot-battle-log-test
bot-battle-log-test: port/build/bot-battle-log-test
port/build/bot-battle-log-test: port/tests/bot/battle_log.cpp port/src/gs/bot_battle_log.h port/release/tests.mk
	$(CXX) -std=c++17 -O1 $< -o $@.tmp
	mv $@.tmp $@

HUD_NAME_SOURCES := port/tests/menu_font/hud_name.c port/src/gs/menu_text.c port/src/gs/tbo_text.c \
    port/src/gs/menu_labels.c port/src/gs/menu_translation.c port/src/gs/ui_text_extract.c \
    port/src/gs/ui_text_translate.c port/src/plat_settings.c port/src/plat_zenkai.c
.PHONY: hud-name-test
hud-name-test: port/build/hud-name-test
port/build/hud-name-test: $(HUD_NAME_SOURCES) port/src/gs/menu_text.h port/src/port_pack.h port/release/tests.mk
	$(CC) -O1 -DUI_TEXT_HOST_ALLOC -Iport/src/gs $(HUD_NAME_SOURCES) -lm -lpthread -lSDL3 -o $@.tmp
	mv $@.tmp $@

# Optional integration test using the developer's extracted disc data, never a release-build prerequisite.
HUD_FRAME_SOURCES := port/tests/menu_font/hud_frame.c $(filter-out port/tests/menu_font/hud_name.c,$(HUD_NAME_SOURCES))
.PHONY: hud-frame-test
hud-frame-test: port/build/hud-frame-test
port/build/hud-frame-test: $(HUD_FRAME_SOURCES) port/src/gs/menu_labels.h port/src/port_pack.h port/release/tests.mk
	$(CC) -O1 -DUI_TEXT_HOST_ALLOC -Iport/src/gs $(HUD_FRAME_SOURCES) -lm -lpthread -lSDL3 -o $@.tmp
	mv $@.tmp $@

.PHONY: release-logging-test
release-logging-test: port/build/release-logging-test
	port/build/release-logging-test
port/build/release-logging-test: port/tests/logging.c port/src/plat_log.c port/src/port_log.h port/release/tests.mk
	$(CC) -std=gnu99 -O1 -Wall -Wextra -Werror $< $$(pkg-config --cflags --libs sdl3) -pthread -o $@.tmp
	mv $@.tmp $@
