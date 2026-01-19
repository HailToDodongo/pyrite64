BUILD_DIR=build
SOURCE_DIR=src
PROJECT_NAME=engine

include $(N64_INST)/include/n64.mk
include $(N64_INST)/include/t3d.mk

N64_CXXFLAGS += -std=gnu++20 -fno-exceptions -Os -Iinclude \
	-Wall -Wextra -Werror \
	-Wformat-signedness -fno-common \
	-Wshadow -Wdouble-promotion -Wformat-security -Wformat-overflow -Wformat-truncation \
	-Wfatal-errors

src = $(wildcard src/*.cpp) $(wildcard src/vi/*.cpp) $(wildcard src/lib/*.cpp)
src += $(wildcard src/scene/*.cpp) $(wildcard src/audio/*.cpp) $(wildcard src/assets/*.cpp)
src += $(wildcard src/collision/*.cpp) $(wildcard src/debug/*.cpp)
src += $(wildcard src/scene/components/*.cpp) $(wildcard src/renderer/*.cpp)
src += $(wildcard src/renderer/hdr/*.cpp) $(wildcard src/renderer/bigtex/*.cpp)
src += $(wildcard src/renderer/particles/*.cpp) $(wildcard src/script/*.cpp)
src += $(wildcard src/libdragon/*.cpp)

# HDR / Bloom ucode
extraObj = $(BUILD_DIR)/renderer/hdr/rsp_hdr.o
# BigTex ucode + raw ASM
extraObj += $(BUILD_DIR)/renderer/bigtex/applyTexture.o
extraObj += $(BUILD_DIR)/renderer/bigtex/rsp_bigtex.o

all: $(BUILD_DIR)/$(PROJECT_NAME).a

$(BUILD_DIR)/$(PROJECT_NAME).a: $(src:%.cpp=$(BUILD_DIR)/%.o) $(extraObj)
	@mkdir -p $(dir $@)
	@echo "    [LD_LIB] $<"
	$(N64_LD) -r -o $(BUILD_DIR)/$(PROJECT_NAME).a $^

# Sources
$(BUILD_DIR)/src/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "    [CC_LIB] $<"
	$(N64_CC) -c $(CFLAGS) $(N64_CXXFLAGS) -o $@ $<

$(BUILD_DIR)/renderer/bigtex/applyTexture.o: $(SOURCE_DIR)/renderer/bigtex/applyTexture.S
	@mkdir -p $(dir $@)
	echo "    [AS] $<"
	$(N64_CC) -c $(N64_ASFLAGS) -MMD -o $@ $<

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/src/*.d)

.PHONY: all clean
