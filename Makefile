########################################
#   DIRETÓRIOS E OUTROS CAMINHOS
########################################
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include

########################################
#   ARQUIVOS
########################################
SHARED_SRC = $(SRC_DIR)/packet.cpp
SERVER_SRC = $(SRC_DIR)/mainServer.cpp $(SRC_DIR)/serverClass.cpp $(SHARED_SRC)
CLIENT_SRC = $(SRC_DIR)/clientClass.cpp $(SRC_DIR)/mainClient.cpp $(SHARED_SRC)
SERVER_OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/%, $(SERVER_SRC:.cpp=.o))
CLIENT_OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/%, $(CLIENT_SRC:.cpp=.o))
DEPS += $(shell find $(INCLUDE_DIR) -type f -name "*.h")

########################################
#   CONFIGS
########################################
CXX = g++
CXXFLAGS = -I$(INCLUDE_DIR) -std=c++17 -lpthread -Wall -Wextra
_CXXFLAGS_RELEASE = -O2
_CXXFLAGS_DEBUG = -g -DDEBUG -pg # Ferramenta de profiling (gprof)

########################################
#   PROGRAMAS
########################################
SERVER_NAME = $(BUILD_DIR)/server
CLIENT_NAME = $(BUILD_DIR)/myClient

########################################
#   REGRAS
########################################
.PHONY: all release debug clean build server client

all: release

release: CXXFLAGS += $(_CXXFLAGS_RELEASE)
release: build

debug: CXXFLAGS += $(_CXXFLAGS_DEBUG)
debug: build

build: $(SERVER_NAME) $(CLIENT_NAME)

# Compilar o servidor
$(SERVER_NAME): $(SERVER_SRC)
	$(CXX) -o $@ $^ $(CXXFLAGS)

# Compilar o cliente
$(CLIENT_NAME): $(CLIENT_SRC)
	$(CXX) -o $@ $^ $(CXXFLAGS)

# Compilação de código objeto
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(DEPS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

clean:
	rm -rf $(BUILD_DIR)/*
