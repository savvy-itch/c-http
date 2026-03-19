#include <stdio.h>
#include <assert.h>

#include "include\res.h"
#include "include\globals.h"

static int status_cmp(void const *lhs, void const *rhs);
// static char *get_content(char *resource);

int handle_res(SOCKET *ClientSocket, ReqLine *req_line, const int status_code)
{
  // build response
  printf("handle_res()\n");
  if (status_code == 200) {
    printf("200\n\n");
    const int base_size = 100;
    // HTTP/1.1 SP 200 SP OK CRLF
    char res_line[base_size];
    StatusCodes const *sc = bsearch(&status_code, supported_statuses, ss_size, st_size, status_cmp);

    assert(sc != NULL);

    snprintf(res_line, base_size, "HTTP/%.1f %d %s\r\n", req_line->http_v, status_code, sc->reason_phrase);
    printf("%s\n\n", res_line);

    // make sure response-line has enough space
    char *sendbuf = malloc(base_size);
    if (sendbuf == NULL) {
      printf("X Response error: insufficient memory\n");
      return 1;
    }
    
    // create headers
    char headers_buf[HEADERS_MAX_LEN];
    char *methods_str = NULL;

    for (short int i = 0; i < am_size; i++) {
      // printf("Iterating over allowed_methods array: %d...\n", i);
      const short int mlen = strlen(allowed_methods[i]);
      if (i == 0) {
        methods_str = malloc(mlen+1);
        if (!methods_str) {
          printf("X Response error: insufficient memory\n");
          return 1;
        }
        strcpy(methods_str, allowed_methods[i]);
        printf("%s\n", methods_str);
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
        printf("%s\n", methods_str);
      }
    }

    assert(strlen(methods_str) > 0);
    // printf("%s\n\n", methods_str);

    // snprintf(headers_buf, HEADERS_MAX_LEN, "Allow: %s\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", methods_str);

    // char *content_buf;
    char *res_body = "<h1>Hello from server</h1>";
    int body_len = strlen(res_body);
    if (strcmp(req_line->method, "GET") == 0 || strcmp(req_line->method, "HEAD")) {
      // content_buf = get_content(req_line->target);
      snprintf(headers_buf, HEADERS_MAX_LEN, "Allow: %s\r\nContent-Length: %d\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", methods_str, body_len);
    } else {
      snprintf(headers_buf, HEADERS_MAX_LEN, "Allow: %s\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", methods_str);
    }
    printf("%s\n\n", headers_buf);
    
    free(methods_str);
    free(sendbuf);

    int isend_result = 0, i = 0, res_line_len = strlen(res_line);
    // send response-line
    do {
      // handle offset if the response wasn't sent entirely
      i += isend_result;
      res_line_len -= i;
      printf("send()\n");
      isend_result = send(*ClientSocket, &res_line[i], res_line_len, 0);
      if (isend_result == SOCKET_ERROR) {
        printf("send failed on line %d: %d\n", __LINE__, WSAGetLastError());
        return isend_result;
      }
    } while (isend_result != 0);

    // send headers
    i = 0;
    int res_len = strlen(headers_buf);
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

// static char *get_content(char *resource)
// {
//   FILE *fp;

//   if (strcmp(resource, "/") == 0) {
//     if (!(fp = fopen("index.html", "r"))) {
//       fprintf(stderr, "Couldn't open index.html\n");
//       return NULL;
//     }

//     // char *buf = malloc();

//     fclose(fp);
//   } else if (strcmp(resource, "/about") == 0) {

//   } else {
//     return NULL;
//   }
// }