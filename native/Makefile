CXX = i686-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fno-exceptions -fno-rtti
LDFLAGS = -static -mwindows -lgdi32 -luser32

BUILD = build
TARGET = $(BUILD)/SagradoTextEdit.exe
KDX = $(BUILD)/SagradoKDX.exe

HDRS = $(wildcard src/*.h)

all: $(TARGET) $(KDX)

$(TARGET): src/main.cpp $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp $(LDFLAGS)

$(KDX): src/kdx.cpp $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ src/kdx.cpp $(LDFLAGS)

run: $(TARGET)
	wine $(TARGET)

run-kdx: $(KDX)
	wine $(KDX)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
