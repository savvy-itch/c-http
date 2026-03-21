#include "tests.h"

const char *test_reqs[] = {
  // VALID
  "GET /about HTTP/1.1\r\n",
  // missing HTTP version
  "GET /path/to/resource\r\n",
  // wrong method (POST instead of GET)
  "POST / HTTP/1.1\r\n",
  // malformed HTTP version
  "GET / HTTP/2.0\r\n",
  // missing path
  "GET  HTTP/1.1\r\n",
  // extra spaces between tokens
  "GET  /  HTTP/1.1\r\n",
  // no line ending
  "GET / HTTP/1.1",
  // lowercase method
  "get / HTTP/1.1\r\n",
  // path with spaces
  "GET /path with spaces HTTP/1.1\r\n"
};

const int tests_amount = sizeof(test_reqs) / sizeof(test_reqs[0]);
