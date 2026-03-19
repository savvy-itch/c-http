#include <stdio.h>

#include "include\req.h"
#include "include\res.h"
#include "include\globals.h"

static int parse_req_line(const char *recvbuf, 
                          int *i, 
                          req_line_state *cur_state, 
                          ReqLine *req_msg, 
                          int *j);
static bool is_valid_req_line(ReqLine *req_msg);
static int parse_headers(const char ch, headers_state *cur_headers_state, char *cur_header_name, char *cur_field_val, int *j);

int handle_req(SOCKET *ClientSocket)
{
  char recvbuf[DEFAULT_BUFLEN];
  int recvbuflen = DEFAULT_BUFLEN, iresult, i = 0, j = 0;
  req_line_state cur_state = READING_METHOD;
  headers_state cur_headers_state = READING_FIELD_NAME;
  ReqLine req_msg;
  RequiredFields headers;
  char cur_header_name[MAX_FIELD_NAME_LEN+1] = "\0";
  char cur_field_val[MAX_FIELD_VAL_LEN+1] = "\0";
  req_msg.method[0] = '\0';
  req_msg.protocol_name[0] = '\0';
  req_msg.http_v = -1;
  
  // Receive until the peer shuts down the connection
  do {
    iresult = recv(*ClientSocket, recvbuf, recvbuflen, 0);
    printf("--------\n");
    i = 0;

    if (iresult > 0) {
      printf("Bytes received: %d\n", iresult);

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
            if (handle_res(ClientSocket, &req_msg, 200) != 0) {
              printf("X An error occured while sending response\n");
              return 1;
            }
          } else {
            printf("X 400 Bad Request: expected \\n, got %c\n", recvbuf[i]);
            printf("%s\n", &recvbuf[i]);
            return 1;
          }
        } else if (cur_headers_state != FIELD_LINE_READ) {
          int res = parse_headers(recvbuf[i], &cur_headers_state, cur_header_name, cur_field_val, &j);
          if (res != 0) {
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
