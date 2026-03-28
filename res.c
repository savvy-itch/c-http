#include <stdio.h>
#include <assert.h>

#include "include\res.h"
#include "include\globals.h"

static int status_cmp(void const *lhs, void const *rhs);
static int send_data(SOCKET *ClientSocket, const char *data_buf);
static char *get_content(const char *resource, short *status_code);
static char *read_file(const char *file_path, short *status_code);
static char *create_allow_header_val(short *status_code);

int handle_res(SOCKET *ClientSocket, ReqLine *req_line, short *status_code)
{
  // build response
  // HTTP/1.1 SP 200 SP OK CRLF

  StatusCodes *sc = bsearch(status_code, supported_statuses, ss_size, st_size, status_cmp);
  assert(sc != NULL);
  
  const int rp_len = strlen(sc->reason_phrase);
  assert(rp_len >= 2);
  const int res_line_size = rp_len + 17;
  char res_line[res_line_size];
  snprintf(res_line, res_line_size, "HTTP/%.1f %hd %s\r\n", req_line->http_v, *status_code, sc->reason_phrase);
  printf("%s\n\n", res_line);
  
  // create headers
  char headers_buf[HEADERS_MAX_LEN];
  char *methods_str = NULL;
  int send_result;

  // 405 and OPTIONS responses require Allow header
  if (strcmp(req_line->method, "OPTIONS") || *status_code == 405) {
    methods_str = create_allow_header_val(status_code);
    if (!methods_str) {
      send_result = send_data(ClientSocket, "HTTP/1.1 507 Insufficient memory\r\n");
      return 1;
    }
  }

  assert(strlen(methods_str) > 0);

  char *res_body = NULL;
  switch (*status_code) {
    case 200:
      res_body = get_content(req_line->target, status_code);
      if (!res_body) {
        *status_code = 507;
        send_result = send_data(ClientSocket, "HTTP/1.1 507 Insufficient memory\r\n");
        return 1;
      }
      break;
    default:
      res_body = malloc(rp_len + 20);
      if (!res_body) {
        *status_code = 507;
        send_result = send_data(ClientSocket, "HTTP/1.1 507 Insufficient memory\r\n");
        return 1;
      }
      snprintf(res_body, rp_len+20, "<h1>%hd %s</h1>", sc->status_code, sc->reason_phrase);
      break;
  }

  if (!res_body) {
    free(methods_str);
    return 1;
  }

  int body_len = strlen(res_body);
  // for 405 or OPTIONS
  if (methods_str) {
    snprintf(headers_buf, HEADERS_MAX_LEN, "Allow: %s\r\nContent-Length: %d\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", methods_str, body_len);
    free(methods_str);
  } else {
    snprintf(headers_buf, HEADERS_MAX_LEN, "Content-Length: %d\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n", body_len);
  }

  // send response-line
  send_result = send_data(ClientSocket, res_line);
  if (send_result != 0) {
    free(res_body);
    return 1;
  }

  // send headers
  send_result = send_data(ClientSocket, headers_buf);
  if (send_result != 0) {
    free(res_body);
    return 1;
  }

  if (strcmp(req_line->method, "HEAD") != 0) {
    // send body
    send_result = send_data(ClientSocket, res_body);
    if (send_result != 0) {
      free(res_body);
      return 1;
    }
  }

  free(res_body);
  return 0;
}

static int status_cmp(void const *lhs, void const *rhs)
{
  StatusCodes const *const l = lhs;
  StatusCodes const *const r = rhs;

  if (l->status_code < r->status_code) return -1;
  else if (l->status_code > r->status_code) return 1;
  return 0;
}

static int send_data(SOCKET *ClientSocket, const char *data_buf)
{
  printf("%s", data_buf);
  int i = 0, isend_result = 0, data_len = strlen(data_buf);
  do {
    i += isend_result;
    data_len -= i;
    isend_result = send(*ClientSocket, &data_buf[i], data_len, 0);
    if (isend_result == SOCKET_ERROR) {
      printf("send failed: %d\n", WSAGetLastError());
      return isend_result;
    }
  } while (isend_result != 0);

  return 0;
}

static char *get_content(const char *resource, short *status_code)
{
  if (strcmp(resource, "/") == 0) {
    return read_file("tests/index.html", status_code);
  }
  if (strcmp(resource, "/about") == 0) {
    return read_file("tests/about.html", status_code);
  }
  return NULL;
}

static char *read_file(const char *file_path, short *status_code)
{
  FILE *fp;
  if (!(fp = fopen(file_path, "rb"))) {
    *status_code = 500;
    fprintf(stderr, "Couldn't open file\n");
    return NULL;
  }

  int base_size = 100;
  char *buf = malloc(base_size);
  if (!buf) {
    *status_code = 507;
    fprintf(stderr, "Insufficient memory\n");
    fclose(fp);
    return NULL;
  }
  
  int ch, i = 0, buf_size = base_size;
  while ((ch = getc(fp)) != EOF) {
    if (i+1 >= buf_size) {
      buf_size += base_size;
      char *tmp = realloc(buf, buf_size);
      if (!tmp) {
        *status_code = 507;
        fprintf(stderr, "Insufficient memory\n");
        fclose(fp);
        return NULL;
      }
      buf = tmp;
    }

    buf[i++] = ch;
  }

  buf[i] = '\0';

  fclose(fp);
  return buf;
}

static char *create_allow_header_val(short *status_code)
{
  char *methods_str = NULL;
  for (short i = 0; i < am_size; i++) {
    const short mlen = strlen(allowed_methods[i]);
    if (i == 0) {
      methods_str = malloc(mlen+1);
      if (!methods_str) {
        *status_code = 507;
        printf("X Response error: insufficient memory\n");
        return NULL;
      }
      strcpy(methods_str, allowed_methods[i]);
    } else {
      int new_len = strlen(methods_str) + mlen + 3; // ", SP METHOD\0"
      char *tmp = realloc(methods_str, new_len);
      if (!tmp) {
        *status_code = 507;
        printf("X Response error: insufficient memory\n");
        return NULL;
      }
      methods_str = tmp;
      strcat(methods_str, ", ");
      strncat(methods_str, allowed_methods[i], mlen);
    }
  }

  return methods_str;
}
