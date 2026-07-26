CXX = i686-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -fno-exceptions -fno-rtti
LDFLAGS = -static -mwindows -lgdi32 -luser32

BUILD = build
TARGET = $(BUILD)/SagradoTextEdit.exe

SRCS = src/main.cpp
HDRS = src/canvas.h src/chrome.h src/font.h src/hap.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDFLAGS)

run: $(TARGET)
	wine $(TARGET)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
