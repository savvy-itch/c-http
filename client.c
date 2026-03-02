/*
The code related to Winsock TCP/IP logic taken from:
https://learn.microsoft.com/en-us/windows/win32/winsock/complete-client-code
*/

// exclude unused headers
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

static int init_socket(char *server_name, struct addrinfo **result);
// static int connect_to_socket(SOCKET *ConnectSocket, struct addrinfo **result);

int main (int argc, char *argv[])
{
  if (argc != 2) {
    printf("usage: %s server-name\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  WSADATA wsa_data;
  int iresult;

  // ************** CREATING A SOCKET FOR THE SERVER *********
  iresult = WSAStartup(MAKEWORD(2,2), &wsa_data);
  if (iresult != 0) {
    printf("WSAStartup failed: %d\n", iresult);
    exit(EXIT_FAILURE);
  }

  // *********** CREATE SOCKET FOR CLIENT *************

  struct addrinfo *result = NULL, *ptr = NULL;

  int res = init_socket(argv[1], &result);
  if (res != 0) {
    WSACleanup();
    exit(EXIT_FAILURE);
  }

  SOCKET ConnectSocket = INVALID_SOCKET;

  // ************ CONNECTING TO A SOCKET **************
  for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
      printf("Error at socket(): %ld\n", WSAGetLastError());
      freeaddrinfo(result);
      WSACleanup();
      exit(EXIT_FAILURE);
    }

    // connect to server
    iresult = connect(ConnectSocket, ptr->ai_addr, (int) ptr->ai_addrlen);
    if (iresult == SOCKET_ERROR) {
      closesocket(ConnectSocket);
      ConnectSocket = INVALID_SOCKET;
      continue;
    }
    break;
  }
  
  freeaddrinfo(result);

  if (ConnectSocket == INVALID_SOCKET) {
    printf("Unable to connect to server\n");
    WSACleanup();
    exit(EXIT_FAILURE);
  }

  // ************ SENDING AND RECEIVING DATA ON THE CLIENT **********
  int recvbuflen = DEFAULT_BUFLEN;
  const char *sendbuf = "this is a test";
  char recvbuf[DEFAULT_BUFLEN];

  // send an initial buffer
  iresult = send(ConnectSocket, sendbuf, (int) strlen(sendbuf), 0);
  if (iresult == SOCKET_ERROR) {
    printf("send failed: %d\n", WSAGetLastError());
    closesocket(ConnectSocket);
    WSACleanup();
    exit(EXIT_FAILURE);
  }

  printf("Bytes sent: %ld\n", iresult);

  // ************* DISCONNECTING THE CLIENT ************
  // shutdown the connection for sending since no more data will be sent 
  // the client can still use the ConnectSocket for receiving data
  iresult = shutdown(ConnectSocket, SD_SEND);
  if (iresult == SOCKET_ERROR) {
    printf("shutdown failed: %d\n", WSAGetLastError());
    closesocket(ConnectSocket);
    WSACleanup();
    exit(EXIT_FAILURE);
  }

  // Receive data until the server closes the connection
  do {
    iresult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
    
    if (result > 0) {
      printf("Bytes received: %d\n", iresult);
    } else if (iresult == 0) {
      printf("Connection closed\n");
    } else {
      printf("recv failed: %d\n", WSAGetLastError());
    }
  } while (iresult > 0);
  
  // cleanup
  closesocket(ConnectSocket);
  WSACleanup();

  return 0;
}

static int init_socket(char *server_name, struct addrinfo **result)
{
  struct addrinfo hints;
  int iresult;

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  iresult = getaddrinfo(server_name, DEFAULT_PORT, &hints, result);
  if (iresult != 0) {
    printf("getaddrinfo failed: %d\n", iresult);
  }
  
  return iresult;
}

// static int connect_to_socket(SOCKET *ConnectSocket, struct addrinfo **result)
// {
//   struct addrinfo *ptr = NULL;
//   int iresult;

//   for (ptr = &result; ptr != NULL; ptr = ptr->ai_next) {
//     // Create a SOCKET for connecting to server
//     *ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
//     if (*ConnectSocket == INVALID_SOCKET) {
//       printf("Error at socket(): %ld\n", WSAGetLastError());
//       freeaddrinfo(result);
//       WSACleanup();
//       exit(EXIT_FAILURE);
//     }

//     // connect to server
//     iresult = connect(*ConnectSocket, ptr->ai_addr, (int) ptr->ai_addrlen);
//     if (iresult == SOCKET_ERROR) {
//       closesocket(ConnectSocket);
//       ConnectSocket = INVALID_SOCKET;
//       continue;
//     }
//     break;
//   }
  
//   freeaddrinfo(&result);

//   if (ConnectSocket == INVALID_SOCKET) {
//     printf("Unable to connect to server\n");
//     WSACleanup();
//     exit(EXIT_FAILURE);
//   }
// }
