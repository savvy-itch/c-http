/*
The code related to Winsock TCP/IP logic taken from:
https://learn.microsoft.com/en-us/windows/win32/winsock/complete-server-code
*/

// exclude unused headers
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <winerror.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>

#include "globals.h"

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512
#define HTTP_V 1.1f
#define REQ_LINE_MAX_LEN 8000
#define HEADERS_MAX_LEN 8000
#define METHOD_MAX_LEN 7
#define PROTOCOL_NAME_LEN 5
#define BASE_FIELD_LINE_AMOUNT 25
#define MAX_FIELD_NAME_LEN 3000
#define MAX_FIELD_VAL_LEN 5000

typedef struct {
  char method[METHOD_MAX_LEN+1];
  char target[REQ_LINE_MAX_LEN+1];
  char protocol_name[PROTOCOL_NAME_LEN+1];
  float http_v;
} ReqLine;

typedef struct {
  char host[MAX_FIELD_VAL_LEN+1];
} RequiredFields;

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

static int init_socket(struct addrinfo **result);
static int handle_req(SOCKET *ClientSocket);
static int parse_req_line(const char *recvbuf, int *i, req_line_state *cur_state, ReqLine *req_msg, int *j);
static bool is_valid_req_line(ReqLine *req_msg);
static int parse_headers(const char ch, headers_state *cur_headers_state, char *cur_header_name, char *cur_field_val, int *j);
static int handle_res(SOCKET *ClientSocket, ReqLine *req_line, RequiredFields *headers, const int status_code);
static int status_cmp(void const *lhs, void const *rhs);

int main (void)
{
  // ********** CREATING A SOCKET FOR THE SERVER *********
  struct addrinfo *result = NULL, *ptr = NULL;
  int iresult;
  WSADATA wsa_data;
  
  // Initialize Winsock
  int res = init_socket(&result);
  if (res != 0) {
    WSACleanup();
    exit(EXIT_FAILURE);
  };

  // create a socket
  SOCKET ListenSocket = INVALID_SOCKET;
  ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

  if (ListenSocket == INVALID_SOCKET) {
    printf("error at socket(): %ld\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
    exit(EXIT_FAILURE);
  }


  // ********** BINDING A SOCKET ************
  iresult = bind(ListenSocket, result->ai_addr, (int) result->ai_addrlen);
  if (iresult == SOCKET_ERROR) {
    printf("bind failed with error: %d\n", WSAGetLastError());
    freeaddrinfo(result);
    closesocket(ListenSocket);
    WSACleanup();
    exit(EXIT_FAILURE);
  }

  printf("Waiting on http://localhost:%s\n", DEFAULT_PORT);
  // free the memory allocated by the getaddrinfo
  freeaddrinfo(result);

  // ************** LISTENING ON A SOCKET **************
  // SOMAXCONN instructs the Winsock provider for this socket to allow a maximum reasonable number of pending connections in the queue
  if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
    printf("listen failed with error: %d\n", WSAGetLastError());
    closesocket(ListenSocket);
    WSACleanup();
    exit(EXIT_FAILURE);
  }


  // ************ ACCEPTING A CONNECTION ************
  SOCKET ClientSocket;
  ClientSocket = INVALID_SOCKET;

  // accept a client socket
  ClientSocket = accept(ListenSocket, NULL, NULL);
  if (ClientSocket == INVALID_SOCKET) {
    printf("accept failed with error: %d\n", WSAGetLastError());
    closesocket(ListenSocket);
    WSACleanup();
    exit(EXIT_FAILURE);
  }

  closesocket(ListenSocket);

  // ******** RECEIVING AND SENDING DATA ON THE SERVER ************
  res = handle_req(&ClientSocket);
  if (res != 0) {
    closesocket(ClientSocket);
    WSACleanup();
    exit(EXIT_FAILURE);
  }

  // *********** DISCONNECTING THE SERVER *************
  iresult = shutdown(ClientSocket, SD_SEND);
  if (iresult == SOCKET_ERROR) {
    printf("shutdown failed: %d\n", WSAGetLastError());
    closesocket(ClientSocket);
    WSACleanup();
    exit(EXIT_FAILURE);
  }

  // cleanup
  closesocket(ClientSocket);
  WSACleanup();

  return 0;
}

static int init_socket(struct addrinfo **result)
{
  int iresult;
  WSADATA wsa_data;
  struct addrinfo hints;
  
  // Initialize Winsock
  iresult = WSAStartup(MAKEWORD(2,2), &wsa_data);
  if (iresult != 0) {
    printf("WSAStartup failed with error: %d\n", iresult);
    exit(EXIT_FAILURE);
  }
  
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET; // IPv4
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  // indicates the caller intends to use the returned socket address structure in a call to the bind function
  hints.ai_flags = AI_PASSIVE;
  
  // Resolve the server address and port
  iresult = getaddrinfo(NULL, DEFAULT_PORT, &hints, result);
  if (iresult != 0) {
    printf("getaddrinfo failed with error: %d\n", iresult);
  }

  return iresult;
} 

/*
  start-line CRLF
  *( field-line CRLF )
  CRLF
  [ message-body ]

*/
static int handle_req(SOCKET *ClientSocket)
{
  char recvbuf[DEFAULT_BUFLEN];
  int isend_result, recvbuflen = DEFAULT_BUFLEN, iresult, i = 0, j = 0;
  req_line_state cur_state = READING_METHOD;
  headers_state cur_headers_state = READING_FIELD_NAME;
  ReqLine req_msg;
  RequiredFields headers;
  char cur_header_name[MAX_FIELD_NAME_LEN+1] = "\0";
  char cur_field_val[MAX_FIELD_VAL_LEN+1] = "\0";
  req_msg.method[0] = '\0';
  req_msg.protocol_name[0] = '\0';
  req_msg.http_v = -1;
  // bool reading_headers_cr = false, reading_headers_lf = false;
  
  // Receive until the peer shuts down the connection
  do {
    iresult = recv(*ClientSocket, recvbuf, recvbuflen, 0);
    printf("--------\n");
    i = 0;

    if (iresult > 0) {
      printf("Bytes received: %d\n", iresult);
      // printf("%.*s\n\n", iresult, recvbuf);

      while (i < iresult) {
        if (cur_state != REQ_LINE_COMPLETE) {
          int res = parse_req_line(recvbuf, &i, &cur_state, &req_msg, &j);
          if (res != 0) {
            return 1;
          }

          if (cur_state == REQ_LINE_COMPLETE) {
            j = 0;
            if (!is_valid_req_line(&req_msg)) {
              printf("X 400 Bad Request (Invalid req-line)\n");
              return 1;
            }
            printf("+ Request-line validated\n");
          }
        } else if (cur_headers_state == FIELD_LINE_READ) {
          // possible headers end
          if (recvbuf[i] == '\r') {
            printf("+ CR found\n");
            cur_headers_state = READING_HEADERS_LF;
            // reading_headers_lf = true;
          } else {
            // continue parsing
            cur_headers_state = READING_FIELD_NAME;
            int res = parse_headers(recvbuf[i], &cur_headers_state, cur_header_name, cur_field_val, &j);
            if (res != 0) {
              return 1;
            }
          }
        } else if (cur_headers_state == READING_HEADERS_LF) {
          printf("Expecting LF...\n");
          if (recvbuf[i] == '\n') {
            printf("+ Headers end reached. Ready to send response...\n");
            if (handle_res(ClientSocket, &req_msg, &headers, 200) != 0) {
              printf("X An error occured while sending response\n");
              return 1;
            }
          } else {
            printf("X 400 Bad Request: expected \n, got %c\n", recvbuf[i]);
            return 1;
          }
        } else if (cur_headers_state != FIELD_LINE_READ) {
          int res = parse_headers(recvbuf[i], &cur_headers_state, cur_header_name, cur_field_val, &j);
          if (res != 0) {
            // printf("\n%s\n", recvbuf + i);
            return 1;
          }
          if (cur_headers_state == FIELD_LINE_READ) {
            if (strcmp(cur_field_val, "Host") == 0) {
              strcpy(headers.host, cur_field_val);
              cur_field_val[0] = '\0';
            }
          }
        }
        i++;
      }
    } else if (iresult == 0) {
      printf("connection closing...\n");
    } else {
      printf("recv failed with error: %d\n", WSAGetLastError());
      return iresult;
    }
  } while (iresult > 0);

  // printf("\nMethod: %s\n", req_msg.method);
  // printf("Target: %s\n", req_msg.target);
  // printf("Protocol name: %s\n", req_msg.protocol_name);
  // printf("HTTP version: %.1f\n", req_msg.http_v);
  // printf("%s: %s\n", cur_header_name, cur_field_val);

  return iresult;
}

static int parse_req_line(const char *recvbuf, 
                          int *i, 
                          req_line_state *cur_state, 
                          ReqLine *req_msg, 
                          int *j)
{
  switch (*cur_state) {
    case READING_METHOD:
      if (isalpha(recvbuf[*i]) && *j < METHOD_MAX_LEN) {
        // putchar(recvbuf[*i]);
        req_msg->method[*j] = recvbuf[*i];
        (*j)++;
      } else if (isspace(recvbuf[*i]) && *j < (METHOD_MAX_LEN+1)) {
        // printf(" SP ");
        req_msg->method[*j] = '\0';
        *cur_state = READING_TARGET;
        *j = 0;
      } else if (*j >= METHOD_MAX_LEN) {
        printf("X Max length for method reached.\n");
        return 1;
      }
      break;
    case READING_TARGET:
      if ((isalnum(recvbuf[*i]) || ispunct(recvbuf[*i])) && *j < REQ_LINE_MAX_LEN) {
        // putchar(recvbuf[*i]);
        req_msg->target[(*j)++] = recvbuf[*i];
      } else if (isspace(recvbuf[*i]) && *j < (REQ_LINE_MAX_LEN+1)) {
        // printf(" SP ");
        req_msg->target[*j] = '\0';
        *cur_state = READING_PROTOCOL;
        *j = 0;
      } else if (*j >= REQ_LINE_MAX_LEN) {
        printf("X Max length for target reached.\n");
        return 1;
      }
      break;
    case READING_PROTOCOL:
      if (!isalpha(recvbuf[*i]) && recvbuf[*i] != '/') {
        printf("X Request-line error: Invalid symbol in protocol name - %c\n.", recvbuf[*i]);
        return 1;
      } else if (*j < PROTOCOL_NAME_LEN) {
        req_msg->protocol_name[(*j)++] = recvbuf[*i];
        if (*j == PROTOCOL_NAME_LEN) {
          req_msg->protocol_name[*j] = '\0';
          *cur_state = READING_VERSION;
          *j = 0;
        }
      }
      break;
    case READING_VERSION: {
      int ver_len = 0, res;
      res = sscanf(&recvbuf[*i], "%f%n", &req_msg->http_v, &ver_len);
      if (res != 1) {
        printf("X Request-line error: invalid version, res: %d.\n", res);
        return 1;
      } else {
        // advance i by the amount of chars consumed by scanf minus 1 as handle_req() increments i at the end of each loop iteration
        *i += ver_len - 1;
      }
      *cur_state = READING_CR;
      break;
    }
    case READING_CR:
      if (recvbuf[*i] == '\r') {
        *cur_state = READING_LF;
      } else {
        printf("X Request-line error: CR not found.\n");
        return 1;
      }
      break;
    case READING_LF:
      if (recvbuf[*i] == '\n') {
        *cur_state = REQ_LINE_COMPLETE;
        *j = 0;
        printf("+ Request-line parsed\n");
      } else {
        printf("X Request-line error: LF not found.\n");
        return 1;
      }
      break;
    default:
      break;
  }
  return 0;
}

static bool is_valid_req_line(ReqLine *req_msg)
{
  bool req_line_valid = false;
  // "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE"
  // validate method
  bool found_match = false;
  for (short int i = 0; i < am_size; i++) {
    if (strcmp(allowed_methods[i], req_msg->method) == 0) {
      found_match = true;
    }
  }

  if (!found_match) {
    printf("X Request-line error: invalid method %s.\n", req_msg->method);
    return false;
  }

  // validate target
  found_match = false;
  for (short int i = 0; i < ar_size; i++) {
    if (strcmp(allowed_resources[i], req_msg->target) == 0) {
      found_match = true;
    }
  }

  if (!found_match) {
    printf("X Request-line error: invalid target %s.\n", req_msg->target);
    return false;
  }
  
  // validate protocol name
  if (strcmp(req_msg->protocol_name, "HTTP/") != 0) {
    printf("X Request-line error: invalid protocol name %s.\n", req_msg->protocol_name);
    return false;
  }
  
  // validate protocol version
  if (req_msg->http_v != HTTP_V) {
    printf("X Request-line error: invalid protocol version %f.\n", req_msg->http_v);
    return false;
  }

  return true;
}

static int parse_headers(const char ch, headers_state *cur_headers_state, char *cur_header_name, char *cur_field_val, int *j)
{
  switch (*cur_headers_state) {
    case READING_FIELD_NAME:
      if (*j >= MAX_FIELD_NAME_LEN) {
        printf("X Max length for field name has been reached.\n");
        return 1;
      }
      // OWS may precede field line, but not be in the middle of field name
      if (isspace(ch) && *j > 0) {
        printf("X Headers error: field name may not contain whitespace.\n");
        return 1;
      }
      if ((isalpha(ch) || ch == '-') && *j < MAX_FIELD_NAME_LEN) {
        cur_header_name[*j] = ch;
        (*j)++;
      } else if (ch == ':') {
        cur_header_name[*j] = '\0';
        // printf("%s: ", cur_header_name);
        *cur_headers_state = READING_FIELD_VAL;
        *j = 0;
      }
      break;
    case READING_FIELD_VAL:
      if (*j >= MAX_FIELD_VAL_LEN) {
        printf("X Max length for field value has been reached.\n");
        return 1;
      }
      if (isprint(ch) && *j < MAX_FIELD_VAL_LEN) {
        // skip the preceding whitespace if there is any
        if (ch == ' ' && *j == 0) break;
        cur_field_val[*j] = ch;
        (*j)++;
      } else if (ch == '\r') {
        *cur_headers_state = READING_FIELD_LF;
        cur_field_val[*j] = '\0';
      }
      break;
    case READING_FIELD_LF:
      if (ch == '\n') {
        *cur_headers_state = FIELD_LINE_READ;
        // printf("%s\n", cur_field_val);
        cur_field_val[0] = '\0';
        *j = 0;
      } else {
        printf("X Headers error: expected LF, instead got %d\ncurrent field: %s\ncurrent value: %s\n", ch, cur_header_name, cur_field_val);
        return 1;
      }
    default:
      break;
  }

  return 0;
}

// Response doesn't get sent
static int handle_res(SOCKET *ClientSocket, ReqLine *req_line, RequiredFields *headers, const int status_code)
{
  // build response
  printf("handle_res()\n");
  if (status_code == 200) {
    printf("200\n");
    const int base_size = 100;
    // HTTP/1.1 SP 200 SP OK CRLF
    char res_line[base_size];
    StatusCodes const *sc = bsearch(&status_code, supported_statuses, ss_size, st_size, status_cmp);

    assert(sc != NULL);

    int n = snprintf(res_line, base_size, "HTTP/%.1f %d %s\r\n", req_line->http_v, status_code, sc->reason_phrase);

    // make sure response-line has enough space
    char *sendbuf = malloc(base_size);
    if (sendbuf == NULL) {
      printf("X Response error: insufficient memory\n");
      return 1;
    }
    
    // create headers
    char headers_buf[HEADERS_MAX_LEN];
    const int base_methods_size = 5;
    char *methods_str = NULL;

    for (short int i = 0; i < am_size; i++) {
      const short int mlen = strlen(allowed_methods[i]);
      if (i = 0) {
        methods_str = malloc(mlen+1);
        if (!methods_str) {
          printf("X Response error: insufficient memory\n");
          return 1;
        }
        strcpy(methods_str, allowed_methods[i]);
      } else {
        int new_len = strlen(methods_str) + mlen + 3; // ", SP METHOD\0"
        char *tmp = realloc(methods_str, new_len);
        if (!tmp) {
          printf("X Response error: insufficient memory\n");
          return 1;
        }
        methods_str = tmp;
        strcat(methods_str, ", ");
        strncat(methods_str, allowed_methods[i], mlen);
      }
    }

    snprintf(headers_buf, HEADERS_MAX_LEN, "Allow: %s\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", methods_str);

    int content_len;
    char *content_buf;
    if (req_line->method == "GET" || req_line->method == "HEAD") {
      // content_buf = get_content(req_line->target);
      snprintf(headers_buf, HEADERS_MAX_LEN, "Allow: %s\r\nContent-Length: %d\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", methods_str, content_len);
    } else {
      snprintf(headers_buf, HEADERS_MAX_LEN, "Allow: %s\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", methods_str);
    }
    
    free(methods_str);
    free(sendbuf);

    // send headers
    int res_len = strlen(headers_buf), isend_result = 0, i = 0;
    do {
      // handle offset if the response wasn't sent entirely
      i += isend_result;
      res_len -= i;
      printf("send()\n");
      isend_result = send(*ClientSocket, &headers_buf[i], res_len, 0);
      if (isend_result == SOCKET_ERROR) {
        printf("send failed on line %d: %d\n", __LINE__, WSAGetLastError());
        return isend_result;
      }
    } while (isend_result != 0);

    // send body
    i = 0;
    char *res_body = "<h1>Hello from server</h1>";
    int body_len = strlen(res_body);
    do {
      // handle offset if the response wasn't sent entirely
      i += isend_result;
      body_len -= i;
      isend_result = send(*ClientSocket, &res_body[i], body_len, 0);
      if (isend_result == SOCKET_ERROR) {
        printf("send failed on line %d: %d\n", __LINE__, WSAGetLastError());
        return isend_result;
      }
    } while (isend_result != 0);
    // if (req_line->method == "GET") {
    //   i = 0;
    //   isend_result = 0;
    //   res_len = strlen(content_buf);
    //   do {
    //     // handle offset if the response wasn't sent entirely
    //     i += isend_result;
    //     res_len -= i;
    //     isend_result = send(*ClientSocket, &content_buf[i], content_len, 0);
    //     if (isend_result == SOCKET_ERROR) {
    //       printf("send failed on line %d: %d\n", __LINE__, WSAGetLastError());
    //       return isend_result;
    //     }
    //   } while (isend_result != 0);
    // }
    return 0;
  }

  /*
  HTTP/1.1 SP 200 SP OK CRLF
  Allow: SP GET, SP HEAD CRLF
  Content-Length: SP 48100 CRLF
  Content-Type: SP text/html; charset=UTF-8 CRLF
  CRLF
  Raw HTML
  */
  return 1;
}

static int status_cmp(void const *lhs, void const *rhs)
{
  StatusCodes const *const l = lhs;
  StatusCodes const *const r = rhs;

  if (l->status_code < r->status_code) return -1;
  else if (l->status_code > r->status_code) return 1;
  return 0;
}

static char *get_content(char *resource)
{
  FILE *fp;

  if (strcmp(resource, "/") == 0) {
    if (!(fp = fopen("index.html", "r"))) {
      fprintf(stderr, "Couldn't open index.html\n");
      return NULL;
    }

    // char *buf = malloc();

    fclose(fp);
  } else if (strcmp(resource, "/about") == 0) {

  } else {
    return NULL;
  }
}
