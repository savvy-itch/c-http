#include "include\globals.h"

const char *allowed_methods[] = {"GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE"};
const char *implemented_methods[] = {"GET", "HEAD", "OPTIONS"};
const char *allowed_resources[] = {"/", "/about"};
const short int am_size = sizeof(allowed_methods) / sizeof(allowed_methods[0]);
const short int im_size = sizeof(implemented_methods) / sizeof(implemented_methods[0]);
const int ar_size = sizeof(allowed_resources) / sizeof(allowed_resources[0]);

const StatusCodes supported_statuses[] = {
  { 200, "OK" },
  { 400, "Bad Request" },
  { 404, "Not Found" },
  { 405, "Method Not Allowed" },
  { 413, "Content Too Large"},
  { 414, "URI Too Long" },
  { 418, "I'm a teapot" },
  { 500, "Internal Server Error" },
  { 501, "Not Implemented" },
  { 507, "Insufficient Storage" }
};

const int st_size = sizeof(supported_statuses[0]);
const int ss_size = sizeof(supported_statuses) / st_size;
