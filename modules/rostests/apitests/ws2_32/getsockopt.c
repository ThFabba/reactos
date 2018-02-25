/*
 * PROJECT:     ReactOS API Tests
 * LICENSE:     LGPL-2.1+ (https://spdx.org/licenses/LGPL-2.1+)
 * PURPOSE:     Test for getsockopt
 * COPYRIGHT:   Copyright 2018 Thomas Faber (thomas.faber@reactos.org)
 */

#include "ws2_32.h"

static
void
Test_SendRecvBuffer(void)
{
    const int DEFAULT_BUFFER_SIZE = 65536;
    SOCKET sock;
    INT ret;
    SOCKADDR_IN addr;
    int bufferSize;
    int length;

    /*
     * UDP socket
     */
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ok(sock != INVALID_SOCKET, "Invalid socket, error %d\n", WSAGetLastError());

    /* Fresh socket, buffer sizes are 0 */
    bufferSize = 0x55555555;
    length = sizeof(bufferSize);
    ret = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (PVOID)&bufferSize, &length);
    ok(ret == 0, "getsockopt failed with %d\n", ret);
    ok(bufferSize == DEFAULT_BUFFER_SIZE, "receive buffer size is %d\n", bufferSize);

    bufferSize = 0x55555555;
    length = sizeof(bufferSize);
    ret = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (PVOID)&bufferSize, &length);
    ok(ret == 0, "getsockopt failed with %d\n", ret);
    ok(bufferSize == DEFAULT_BUFFER_SIZE, "send buffer size is %d\n", bufferSize);

    /* Bind it */
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    ret = bind(sock, (PVOID)&addr, sizeof(addr));
    ok(ret == 0, "bind failed with %d\n", ret);

    /* Bound socket -- default buffer size (64k) */
    bufferSize = 0x55555555;
    length = sizeof(bufferSize);
    ret = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (PVOID)&bufferSize, &length);
    ok(ret == 0, "getsockopt failed with %d\n", ret);
    ok(bufferSize == DEFAULT_BUFFER_SIZE, "receive buffer size is %d\n", bufferSize);

    bufferSize = 0x55555555;
    length = sizeof(bufferSize);
    ret = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (PVOID)&bufferSize, &length);
    ok(ret == 0, "getsockopt failed with %d\n", ret);
    ok(bufferSize == DEFAULT_BUFFER_SIZE, "send buffer size is %d\n", bufferSize);

    closesocket(sock);
    
    /*
     * TCP socket
     */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ok(sock != INVALID_SOCKET, "Invalid socket, error %d\n", WSAGetLastError());

    /* Fresh socket, buffer sizes are 0 */
    bufferSize = 0x55555555;
    length = sizeof(bufferSize);
    ret = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (PVOID)&bufferSize, &length);
    ok(ret == 0, "getsockopt failed with %d\n", ret);
    ok(bufferSize == DEFAULT_BUFFER_SIZE, "receive buffer size is %d\n", bufferSize);

    bufferSize = 0x55555555;
    length = sizeof(bufferSize);
    ret = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (PVOID)&bufferSize, &length);
    ok(ret == 0, "getsockopt failed with %d\n", ret);
    ok(bufferSize == DEFAULT_BUFFER_SIZE, "send buffer size is %d\n", bufferSize);

    /* Bind it */
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    ret = bind(sock, (PVOID)&addr, sizeof(addr));
    ok(ret == 0, "bind failed with %d\n", ret);

    /* Bound socket -- default buffer size (64k) */
    bufferSize = 0x55555555;
    length = sizeof(bufferSize);
    ret = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (PVOID)&bufferSize, &length);
    ok(ret == 0, "getsockopt failed with %d\n", ret);
    ok(bufferSize == DEFAULT_BUFFER_SIZE, "receive buffer size is %d\n", bufferSize);

    bufferSize = 0x55555555;
    length = sizeof(bufferSize);
    ret = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (PVOID)&bufferSize, &length);
    ok(ret == 0, "getsockopt failed with %d\n", ret);
    ok(bufferSize == DEFAULT_BUFFER_SIZE, "send buffer size is %d\n", bufferSize);

    closesocket(sock);
}

START_TEST(getsockopt)
{
    WSADATA wdata;
    INT ret;

    /* Start up Winsock */
    ret = WSAStartup(MAKEWORD(2, 2), &wdata);
    ok(ret == 0, "WSAStartup failed, ret == %d\n", ret);

    Test_SendRecvBuffer();

    WSACleanup();
}
