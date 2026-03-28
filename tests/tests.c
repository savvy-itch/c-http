#include "tests.h"

const char *test_reqs[] = {
  // VALID
  "GET /about HTTP/1.1\r\nHost: localhost:27015\r\n\r\n",
  // missing HTTP version
  "GET /path/to/resource\r\nHost: localhost:27015\r\n\r\n",
  // wrong method (POST instead of GET)
  "POST / HTTP/1.1\r\nHost: localhost:27015\r\n\r\n",
  // malformed HTTP version
  "GET / HTTP/2.0\r\nHost: localhost:27015\r\n\r\n",
  // missing path
  "GET  HTTP/1.1\r\nHost: localhost:27015\r\n\r\n",
  // extra spaces between tokens
  "GET  /  HTTP/1.1\r\nHost: localhost:27015\r\n\r\n",
  // no line ending
  "GET / HTTP/1.1 Host: localhost:27015\r\n\r\n",
  // lowercase method
  "get / HTTP/1.1\r\nHost: localhost:27015\r\n\r\n",
  // path with spaces
  "GET /path with spaces HTTP/1.1\r\nHost: localhost:27015\r\n\r\n"
};

const int tests_amount = sizeof(test_reqs) / sizeof(test_reqs[0]);
