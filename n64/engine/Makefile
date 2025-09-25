BUILD_DIR=build
SOURCE_DIR=src
PROJECT_NAME=engine

include $(N64_INST)/include/n64.mk
include $(N64_INST)/include/t3d.mk

N64_CXXFLAGS += -std=gnu++20 -fno-exceptions -Os -Iinclude \
	-Wall -Wextra -Werror \
	-Wformat-signedness -fno-common \
	-Wshadow -Wdouble-promotion -Wformat-security -Wformat-overflow -Wformat-truncation

src = $(wildcard src/*.cpp) $(wildcard src/vi/*.cpp) $(wildcard src/lib/*.cpp) \
	$(wildcard src/scene/*.cpp) $(wildcard src/audio/*.cpp)

all: $(BUILD_DIR)/$(PROJECT_NAME).a

$(BUILD_DIR)/$(PROJECT_NAME).a: $(src:%.cpp=$(BUILD_DIR)/%.o)
	@mkdir -p $(dir $@)
	@echo "    [LD_LIB] $<"
	$(N64_LD) -r -o $(BUILD_DIR)/$(PROJECT_NAME).a $^

# Sources
$(BUILD_DIR)/src/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "    [CC_LIB] $<"
	$(N64_CC) -c $(CFLAGS) $(N64_CXXFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD_DIR)

-include $(wildcard $(BUILD_DIR)/src/*.d)

.PHONY: all clean
