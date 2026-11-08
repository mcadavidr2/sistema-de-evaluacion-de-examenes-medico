# Makefile del evaluador de exámenes médicos.
#
#   Linux:    make
#   Windows:  mingw32-make   (desde la consola MSYS2 UCRT64; ver Install.md)
#   Cruzado:  make TARGET_OS=windows CXX=x86_64-w64-mingw32-g++
#
# Genera en bin/: evaluator, eval-init, eval-reg, eval-ctrl, eval-rep (con .exe en Windows).

ifeq ($(OS),Windows_NT)
  TARGET_OS ?= windows
else
  TARGET_OS ?= linux
endif

ifeq ($(TARGET_OS),windows)
  EXE         := .exe
  CXXFLAGS_OS := -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -pthread
  # Enlace estático: el .exe no depende de DLL de MinGW en el equipo donde se ejecute.
  LDFLAGS_OS  := -static -static-libgcc -static-libstdc++ -pthread
  LDLIBS_OS   :=
else
  EXE         :=
  CXXFLAGS_OS := -pthread
  LDFLAGS_OS  := -pthread
  LDLIBS_OS   := -lrt
endif

CXXFLAGS ?= -O2
override CXXFLAGS += -std=c++17 -Wall -Wextra -MMD -MP $(CXXFLAGS_OS)

OBJ_DIR    := build/$(TARGET_OS)
SHARED_SRC := $(wildcard src/common/*.cpp) $(wildcard src/platform/$(TARGET_OS)/*.cpp)
SHARED_OBJ := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SHARED_SRC))

BINS := bin/evaluator$(EXE) bin/eval-init$(EXE) bin/eval-reg$(EXE) bin/eval-ctrl$(EXE) bin/eval-rep$(EXE)

PREFIX ?= /usr/local

.PHONY: all clean test install

all: $(BINS)

bin/evaluator$(EXE):  $(OBJ_DIR)/main_evaluator.o $(SHARED_OBJ)
bin/eval-init$(EXE):  $(OBJ_DIR)/init/main.o $(SHARED_OBJ)
bin/eval-reg$(EXE):   $(OBJ_DIR)/reg/main.o $(SHARED_OBJ)
bin/eval-ctrl$(EXE):  $(OBJ_DIR)/ctrl/main.o $(SHARED_OBJ)
bin/eval-rep$(EXE):   $(OBJ_DIR)/rep/main.o $(SHARED_OBJ)

$(BINS):
	@mkdir -p bin
	$(CXX) $(LDFLAGS) $(LDFLAGS_OS) $^ -o $@ $(LDLIBS) $(LDLIBS_OS)

$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: all
	bash tests/run_tests.sh

# Los cinco binarios deben quedar juntos: el wrapper busca eval-* en su directorio.
install: all
	@mkdir -p $(PREFIX)/bin
	cp $(BINS) $(PREFIX)/bin/

clean:
	rm -rf build bin

-include $(wildcard $(OBJ_DIR)/*.d $(OBJ_DIR)/*/*.d $(OBJ_DIR)/*/*/*.d)
