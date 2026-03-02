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

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512
#define REQ_LINE_MAX_LEN 8000
#define METHOD_MAX_LEN 7
#define BASE_FIELD_LINE_AMOUNT 25
#define PROTOCOL_NAME_LEN 5

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

static int init_socket(struct addrinfo **result);
static int handle_req(SOCKET *ClientSocket);
static int parse_req_line(const char *recvbuf, int *i, req_line_state *cur_state, ReqLine *req_msg, int *j);
static bool is_valid_req_line(ReqLine *req_msg);

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
// GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE
static int handle_req(SOCKET *ClientSocket)
{
  char recvbuf[DEFAULT_BUFLEN];
  int isend_result, recvbuflen = DEFAULT_BUFLEN, iresult, i = 0, j = 0;
  req_line_state cur_state = READING_METHOD;
  bool headers_read = false;
  ReqLine req_msg;
  req_msg.method[0] = '\0';
  req_msg.protocol_name[0] = '\0';
  req_msg.http_v = -1;
  
  // Receive until the peer shuts down the connection
  do {
    iresult = recv(*ClientSocket, recvbuf, recvbuflen, 0);
    printf("--------\n");
    int recv_len = strlen(recvbuf);
    i = 0;

    if (iresult > 0) {
      printf("Bytes received: %d\n", iresult);

      while (i < recv_len) {
        if (cur_state != REQ_LINE_COMPLETE) {
          int res = parse_req_line(recvbuf, &i, &cur_state, &req_msg, &j);
          if (res != 0) {
            return 1;
          }
          
          if (!is_valid_req_line(&req_msg)) {
            printf("400 Bad Request\n");
            return 1;
          }
        } else if (!headers_read) {
          // printf("Request line is parsed.\n");
          // parse the header
        } else {
          // parse the body
        }
        i++;
      }
      
      printf("\nMethod: %s\n", req_msg.method);
      printf("Target: %s\n", req_msg.target);
      printf("Protocol name: %s\n", req_msg.protocol_name);
      printf("HTTP version: %f\n", req_msg.http_v);
      // printf("Content: %s\n", recvbuf);
      // send response
      // isend_result = send(*ClientSocket, recvbuf, iresult, 0);
      // if (isend_result == SOCKET_ERROR) {
      //   printf("send failed on line %d: %d\n", __LINE__, WSAGetLastError());
      //   return isend_result;
      // }
      // printf("Bytes sent: %d\n", isend_result);
    } else if (iresult == 0) {
      printf("connection closing...\n");
    } else {
      printf("recv failed with error: %d\n", WSAGetLastError());
      return iresult;
    }
  } while (iresult > 0);

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
        putchar(recvbuf[*i]);
        req_msg->method[*j] = recvbuf[*i];
        (*j)++;
      } else if (isspace(recvbuf[*i]) && *j < (METHOD_MAX_LEN+1)) {
        printf(" SP ");
        req_msg->method[*j] = '\0';
        *cur_state = READING_TARGET;
        *j = 0;
      } else if (*j >= METHOD_MAX_LEN) {
        printf("Max length for method reached. Invalid method.\n");
        return 1;
      }
      break;
    case READING_TARGET:
      if ((isalnum(recvbuf[*i]) || ispunct(recvbuf[*i])) && *j < REQ_LINE_MAX_LEN) {
        putchar(recvbuf[*i]);
        req_msg->target[(*j)++] = recvbuf[*i];
      } else if (isspace(recvbuf[*i]) && *j < (REQ_LINE_MAX_LEN+1)) {
        printf(" SP ");
        req_msg->target[*j] = '\0';
        *cur_state = READING_PROTOCOL;
        *j = 0;
      } else if (*j >= REQ_LINE_MAX_LEN) {
        printf("Max length for target reached.\n");
        return 1;
      }
      break;
    case READING_PROTOCOL:
      if (!isalpha(recvbuf[*i]) && recvbuf[*i] != '/') {
        printf("Request-line error: Invalid protocol name\n.");
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
        printf("Request-line error: invalid version, res: %d.\n", res);
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
        printf("Request-line error: CR not found.\n");
        return 1;
      }
      break;
    case READING_LF:
      if (recvbuf[*i] == '\n') {
        *cur_state = REQ_LINE_COMPLETE;
      } else {
        printf("Request-line error: LF not found.\n");
        return 1;
      }
      break;
    default:
      break;
  }
  return 0;
}

/*
typedef struct {
  char method[METHOD_MAX_LEN+1];
  char target[REQ_LINE_MAX_LEN+1];
  char protocol_name[PROTOCOL_NAME_LEN+1];
  float http_v;
} ReqLine;
*/
static bool is_valid_req_line(ReqLine *req_msg)
{
  bool req_line_valid = false;
  // "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE"
  const char *supported_methods[] = {"GET", "HEAD"};
  const int len = sizeof(supported_methods) / sizeof(supported_methods[0]);
  // validate method
  bool found_match = false;
  for (int i = 0; i < len; i++) {
    if (strcmp(supported_methods[i], req_msg->method) == 0) {
      found_match = true;
    }
  }

  if (!found_match) return false;

  // validate target
  
  // validate protocol name
  if (strcmp(req_msg->protocol_name, "HTTP/") != 0) return false;

  // validate protocol version
  if (req_msg->http_v != 1.1) return false;
}

static int parse_headers(const char *recvbuf, int *i)
{
  if (recvbuf[*i] == '\n' || recvbuf[*i] == '\r' || recvbuf[*i] == '\0') {
    printf("Headers parsing error: invalid character in field value\n");
    return 1;
  }

  return 0;
}
