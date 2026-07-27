CXX = i686-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fno-exceptions -fno-rtti
LDFLAGS = -static -mwindows -lgdi32 -luser32 -lwinhttp

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

# Console harness for the relay socket; not part of `all`.
$(BUILD)/test_ws.exe: src/test_ws.cpp $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) -std=c++17 -O2 -Wall -o $@ src/test_ws.cpp -static -lwinhttp

run: $(TARGET)
	wine $(TARGET)

run-kdx: $(KDX)
	wine $(KDX)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
