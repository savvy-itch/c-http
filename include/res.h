#ifndef RES_H
#define RES_H
#include <winsock2.h>

#include "req.h"

#define HEADERS_MAX_LEN 8000

int handle_res(SOCKET *ClientSocket, ReqLine *req_line, const short status_code);
#endif
