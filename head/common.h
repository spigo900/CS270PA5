// The maximum length of a value stored in a variable.
#define MAX_VALUE_LENGTH 100

// The maximum length of the server's response to a command.
#define MAX_RESPONSE_SIZE 150

// Maximum length of a variable name, not including the terminating null.
#define MAX_VARNAME_LENGTH 15

// The maximum length of the data in a digest request.
#define MAX_DIGEST_LENGTH 100

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



int parseIntWithError(char* toParse, char* errorMsg);
