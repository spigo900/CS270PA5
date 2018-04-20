SRC_DIR = src
INCLUDE_DIR = head
BUILD_DIR = build
CSAPP_OBJ = $(BUILD_DIR)/csapp.o
CFLAGS = -Wall -g -I$(INCLUDE_DIR)
CXXFLAGS = -Wall -g -I$(INCLUDE_DIR) -std=c++11
LDLIBS = -lpthread

SERVER = $(BUILD_DIR)/smalld
SERVER_SOURCES = $(SRC_DIR)/smalld.cpp
SMALL_CLIENTS = smallSet smallGet smallDigest smallRun
CLIENTS = $(addprefix $(BUILD_DIR)/, $(SMALL_CLIENTS))

# Compute the source file paths for the clients from the client names. Lots of
# messy string manipulation stuff.
CLIENT_SOURCES_COMMON = common.c sserver.c
CLIENT_COMMON =  $(addprefix $(SRC_DIR)/, $(CLIENT_SOURCES_COMMON:.c=.o))

COMMON_SRC = $(SRC_DIR)/common.c
COMMON = $(COMMON_SRC:.c=.o)
CSAPP = $(INCLUDE_DIR)/csapp.h $(BUILD_DIR)/csapp.c

OUR_HEADERS = $(INCLUDE_DIR)/common.h $(INCLUDE_DIR)/sserver.h
SUBMISSION_FILE = cs270pa5.tgz

all: $(CSAPP) $(SERVER) $(CLIENTS)

$(INCLUDE_DIR)/csapp.h:
	wget http://csapp.cs.cmu.edu/2e/ics2/code/include/csapp.h -P$(INCLUDE_DIR)

$(BUILD_DIR)/csapp.c:
	wget http://csapp.cs.cmu.edu/2e/ics2/code/src/csapp.c -P$(BUILD_DIR)

$(CSAPP_OBJ): $(INCLUDE_DIR)/csapp.h $(BUILD_DIR)/csapp.c

$(SERVER): $(CSAPP_OBJ) $(COMMON) $(SERVER_SOURCES:.cpp=.o)
	$(CXX) $(SERVER_SOURCES:.cpp=.o) $(COMMON) $(CSAPP_OBJ) $(LDLIBS) -o $(SERVER)

# Yay, it works!
$(CLIENTS): $(BUILD_DIR)/% : $(CSAPP_OBJ) $(CLIENT_COMMON) $(addprefix $(SRC_DIR)/,$(addsuffix .o,%))
	$(CC) $(subst $(BUILD_DIR),$(SRC_DIR),$(addsuffix .o,$@)) $(CLIENT_COMMON) $(CSAPP_OBJ) $(LDLIBS) -o $@

.PHONY: clean
clean:
	/bin/rm -rf $(SUBMISSION_FILE) $(SRC_DIR)/*.o $(SERVER) $(CLIENTS)

.PHONY: build client server
build: client server ;
client: $(CLIENTS)
server: $(SERVER)

# I think this includes everything... not sure.
submit:
	tar -czf cs270pa5.tgz README Makefile $(CLIENT_SOURCES) $(SERVER_SOURCES) \
		$(OUR_HEADERS)

