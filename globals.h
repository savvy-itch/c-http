typedef struct {
  int status_code;
  char *reason_phrase;
} StatusCodes;

extern const char *allowed_methods[];
extern const char *allowed_resources[];
extern const short int am_size;
extern const int ar_size;
extern const StatusCodes supported_statuses[];
extern const int st_size;
extern const int ss_size;
