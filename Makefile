# Build for MinGW-w64 / MSYS2 UCRT64.
#
#   mingw32-make            release build -> build/wolf3d.exe
#   mingw32-make debug      -O0 -g, console attached for printf debugging
#   mingw32-make run        build then launch
#   mingw32-make clean

CXX      := g++
TARGET   := wolf3d.exe
BUILDDIR := build

SRCS := $(wildcard src/*.cpp src/core/*.cpp src/platform/*.cpp src/render/*.cpp src/game/*.cpp)
OBJS := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS  :=

# Static linking is required on this toolchain: a dynamically linked MinGW
# binary trips Windows Defender heuristics and fails to launch.
LDLIBS   := -static -static-libgcc -static-libstdc++ -lgdi32 -luser32

ifeq ($(MAKECMDGOALS),debug)
  CXXFLAGS += -O0 -g -DWOLF_DEBUG
else
  CXXFLAGS += -O2 -DNDEBUG
  # Windows subsystem: no console window behind the game.
  LDFLAGS  += -mwindows
endif

.PHONY: all debug run clean
all: $(BUILDDIR)/$(TARGET)
debug: $(BUILDDIR)/$(TARGET)

$(BUILDDIR)/$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)
	@echo "built $@"

$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(BUILDDIR)/$(TARGET)

clean:
	rm -rf $(BUILDDIR)

-include $(DEPS)
