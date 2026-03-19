#ifndef REQ_H
#define REQ_H
#include <winsock2.h>
#include <stdbool.h>

#define METHOD_MAX_LEN 7
#define REQ_LINE_MAX_LEN 8000
#define PROTOCOL_NAME_LEN 5
#define MAX_FIELD_VAL_LEN 5000
#define MAX_FIELD_NAME_LEN 3000

typedef struct {
  char method[METHOD_MAX_LEN+1];
  char target[REQ_LINE_MAX_LEN+1];
  char protocol_name[PROTOCOL_NAME_LEN+1];
  float http_v;
} ReqLine;

typedef enum {
  READING_METHOD, 
  READING_TARGET, 
  READING_PROTOCOL, 
  READING_VERSION, 
  READING_CR, 
  READING_LF, 
  REQ_LINE_COMPLETE
} req_line_state;

typedef enum {
  READING_FIELD_NAME,
  READING_FIELD_VAL,
  READING_FIELD_CR, 
  READING_FIELD_LF, 
  FIELD_LINE_READ,
  READING_HEADERS_LF
} headers_state;

typedef struct {
  char host[MAX_FIELD_VAL_LEN+1];
} RequiredFields;

int handle_req(SOCKET *ClientSocket);
#endif
