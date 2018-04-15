// Set the value of variable `variableName` to value on the server at
// MachineName:port, where value is some data of length `dataLength`.
int smallSet(char *MachineName, int port, int SecretKey,
        char *variableName, char *value, int dataLength);

// Get the value of variable `variableName` on the server at MachineName:port,
// writing the result to `value` and storing the length of the result into the
// int pointed to by `resultLength`.
int smallGet(char *MachineName, int port, int SecretKey,
        char *variableName, char *value, int *resultLength);

// Get the SHA256 checksum of `data` on the server at MachineName:port and
// write the response to the memory pointed to by `result`, with length written
// to `resultLength`. The result will be at most 100 bytes long.
int smallDigest(char *MachineName, int port, int SecretKey,
        char *data, int dataLength, char *result, int *resultLength);

// Run the program specified by `request` on the server at MachineName:port and
// write the response to the memory pointed to by `result`, with length written
// to `resultLength`. The result will be at most 100 bytes long.
int smallRun(char *MachineName, int port, int SecretKey,
        char *request, char *result, int *resultLength);
