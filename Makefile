SRC_DIR = src
INCLUDE_DIR = head
BUILD_DIR = build
CSAPP_OBJ = $(BUILD_DIR)/csapp.o
CFLAGS = -Wall -g -I$(INCLUDE_DIR)
CXXFLAGS = -Wall -g -I$(INCLUDE_DIR)
LDLIBS = -lpthread

SERVER = $(BUILD_DIR)/smalld
SERVER_SOURCES = $(SRC_DIR)/smalld.cpp
SMALL_CLIENTS = smallSet smallGet smallDigest smallRun
CLIENTS = $(addprefix $(BUILD_DIR)/, $(SMALL_CLIENTS))

CLIENT_SOURCES = $(addprefix $(SRC_DIR)/, $(addsuffix ".c", $(SMALL_CLIENTS)))
CLIENT_SOURCES_COMMON =  client_common.c sserver.c
CLIENT_COMMON =  $(addprefix $(SRC_DIR)/, $(CLIENT_SOURCES_COMMON:.c=.o))
CSAPP = $(INCLUDE_DIR)/csapp.h $(BUILD_DIR)/csapp.c

all: $(CSAPP) $(SERVER) $(CLIENTS)

$(INCLUDE_DIR)/csapp.h:
	wget http://csapp.cs.cmu.edu/2e/ics2/code/include/csapp.h -P$(INCLUDE_DIR)

$(BUILD_DIR)/csapp.c:
	wget http://csapp.cs.cmu.edu/2e/ics2/code/src/csapp.c -P$(BUILD_DIR)

$(CSAPP_OBJ): $(INCLUDE_DIR)/csapp.h $(BUILD_DIR)/csapp.c

$(SERVER): $(CSAPP_OBJ) $(SERVER_SOURCES:.cpp=.o)
	$(CXX) $(SERVER_SOURCES:.cpp=.o) $(LDLIBS) -o $(SERVER)

# TODO: fix this fucking thing
# $(subst $(BUILD_DIR),$(SRC_DIR),%.o)
$(CLIENTS): $(BUILD_DIR)/% : $(CSAPP_OBJ) $(CLIENT_COMMON) $(addprefix $(SRC_DIR)/,$(addsuffix .o,%))
	$(CC) $(subst $(BUILD_DIR),$(SRC_DIR),$(addsuffix .o,$@)) $(CLIENT_COMMON) $(CSAPP_OBJ) $(LDLIBS) -o $@

.PHONY: clean 
clean:
	/bin/rm -rf csapp.h csapp.c $(SRC_DIR)/*.o $(SERVER) $(CLIENTS)

.PHONY: build client server
build: client server ;
client: $(CLIENT)
server: $(SERVER)

