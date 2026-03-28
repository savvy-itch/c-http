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
#include <assert.h>

#include "include\globals.h"
#include "include\req.h"

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define BASE_FIELD_LINE_AMOUNT 25

static int init_socket(struct addrinfo **result);

int main (void)
{
  // ********** CREATING A SOCKET FOR THE SERVER *********
  struct addrinfo *result = NULL;
  int iresult;
  
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
    printf("error at socket(): %d\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
    exit(EXIT_FAILURE);
  }


  // ********** BINDING A SOCKET TO AN IP ADDRESS AND PORT ************
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

  bool keep_alive = true;
  // keep connection persistent unless requested otherwise
  while (keep_alive) {
    // accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
      printf("accept failed with error: %d\n", WSAGetLastError());
      closesocket(ListenSocket);
      WSACleanup();
      exit(EXIT_FAILURE);
    }
    
    // ******** RECEIVING AND SENDING DATA ON THE SERVER ********
    res = handle_req(&ClientSocket, &keep_alive);
    if (res != 0) {
      closesocket(ClientSocket);
      WSACleanup();
      exit(EXIT_FAILURE);
    }
  }

  closesocket(ListenSocket);

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
