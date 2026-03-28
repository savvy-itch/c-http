#ifndef GLOBALS_H
#define GLOBALS_H

#define DEFAULT_BUFLEN 512
#define HTTP_V 1.1f

typedef struct {
  short status_code;
  char *reason_phrase;
} StatusCodes;

extern const char *allowed_methods[];
extern const char *implemented_methods[];
extern const char *allowed_resources[];
extern const short int am_size;
extern const short int im_size;
extern const int ar_size;
extern const StatusCodes supported_statuses[];
extern const int st_size;
extern const int ss_size;

#endif
