CXX = g++
CXXFLAGS = -std=c++17 -I.

TARGET = JengChat.exe
RESOURCE = jengchat_res.o

SOURCES = \
	main.cpp \
	win_icon.cpp \
	mac_bundle.cpp \
	networking.cpp \
	protocol.cpp \
	$(wildcard ui/*.cpp) \
	$(wildcard games/*.cpp) \
	$(wildcard games/cards/*.cpp)

LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32 -pthread


all: $(TARGET)


$(RESOURCE): jengchat.rc
	windres jengchat.rc -O coff -o $(RESOURCE)


$(TARGET): $(RESOURCE) $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) $(RESOURCE) -o $(TARGET) $(LIBS)


run: all
	./$(TARGET)


clean:
	rm -f $(TARGET) $(RESOURCE)


rebuild: clean all

release:
	./package_windows.sh