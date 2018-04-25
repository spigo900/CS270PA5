// The maximum length of a value stored in a variable.
#define MAX_VALUE_LENGTH 100

// The maximum length of one of the client's requests.
#define MAX_REQUEST_SIZE 150

// The maximum length of the server's response to a command.
#define MAX_RESPONSE_SIZE 150

// Maximum length of a variable name, not including the terminating null.
#define MAX_VARNAME_LENGTH 15

// The maximum length of the data in a digest request.
#define MAX_DIGEST_LENGTH 100

// The maximum length of the data in the server's response to a request.
#define MAX_SERVER_DATA_LENGTH 100

// Maximum length of a run request, including the terminating null.
#define MAX_RUNREQ_LENGTH 8

// The length of a size specifier in the protocol, in bytes. Every time I saw a
// value specifying length or size in the protocol spec, it was 2 bytes, so
// that's what this is.
#define LENGTH_SPECIFIER_SIZE 2

// Size in bytes of the preamble common to every kind of client-to-server
// message.
#define CLIENT_PREAMBLE_SIZE 8

// Size in bytes of the common part of every server-to-client message (4 bytes;
// 1 for the return code and 3 of padding).
#define SERVER_PREAMBLE_SIZE 4

// The different types of messages we can send and receive.
typedef enum {
  SSERVER_MSG_SET = 0,
  SSERVER_MSG_GET = 1,
  SSERVER_MSG_DIGEST = 2,
  SSERVER_MSG_RUN = 3
} MessageType;

typedef struct {
  unsigned int secretKey;
  unsigned short msgType;
  char junk[2];
} ClientPreamble;

typedef struct {
  ClientPreamble pre;
  char varName[MAX_VARNAME_LENGTH + 1];
  unsigned short length;
  char value[MAX_VALUE_LENGTH];
} ClientSet;

typedef struct {
  ClientPreamble pre;
  char varName[MAX_VARNAME_LENGTH + 1];
} ClientGet;

typedef struct {
  ClientPreamble pre;
  unsigned short length;
  char value[MAX_DIGEST_LENGTH];
} ClientDigest;

typedef struct {
  ClientPreamble pre;
  char request[MAX_RUNREQ_LENGTH];
} ClientRun;

typedef struct {
  char status;
  char junk[3];
  unsigned short length;
  char data[MAX_SERVER_DATA_LENGTH + 1];
} ServerResponse;

int parseIntWithError(char *toParse, const char *errorMsg);
int isValidRunRequest(char *runRequest);
