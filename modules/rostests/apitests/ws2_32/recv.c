/*
 * PROJECT:     ReactOS API Tests
 * LICENSE:     GPL-2.0+ (https://spdx.org/licenses/GPL-2.0+)
 * PURPOSE:     Test for recv
 * COPYRIGHT:   Copyright 2008 Colin Finck (colin@reactos.org)
 *              Copyright 2012 Cameron Gutman (cameron.gutman@reactos.org)
 *              Copyright 2017 Thomas Faber (thomas.faber@reactos.org)
 */

#include "ws2_32.h"

#include <ndk/exfuncs.h>
#include <ndk/iofuncs.h>
#include <ndk/obfuncs.h>

#define RECV_BUF   4

/* For valid test results, the ReactOS Website needs to return at least 8 bytes on a "GET / HTTP/1.0" request.
   Also the first 4 bytes and the last 4 bytes need to be different.
   Both factors usually apply on standard HTTP responses. */

int Test_recv()
{
    const char szDummyBytes[RECV_BUF] = {0xFF, 0x00, 0xFF, 0x00};

    char szBuf1[RECV_BUF];
    char szBuf2[RECV_BUF];
    int iResult;
    SOCKET sck;
    WSADATA wdata;
    NTSTATUS status;
    IO_STATUS_BLOCK readIosb;
    HANDLE readEvent;
    LARGE_INTEGER readOffset;

    /* Start up Winsock */
    iResult = WSAStartup(MAKEWORD(2, 2), &wdata);
    ok(iResult == 0, "WSAStartup failed, iResult == %d\n", iResult);

    /* If we call recv without a socket, it should return with an error and do nothing. */
    memcpy(szBuf1, szDummyBytes, RECV_BUF);
    iResult = recv(0, szBuf1, RECV_BUF, 0);
    ok(iResult == SOCKET_ERROR, "iRseult = %d\n", iResult);
    ok(!memcmp(szBuf1, szDummyBytes, RECV_BUF), "not equal\n");

    /* Create the socket */
    if (!CreateSocket(&sck))
    {
        ok(0, "CreateSocket failed. Aborting test.\n");
        return 0;
    }

    /* Now we can pass at least a socket, but we have no connection yet. Should return with an error and do nothing. */
    memcpy(szBuf1, szDummyBytes, RECV_BUF);
    iResult = recv(sck, szBuf1, RECV_BUF, 0);
    ok(iResult == SOCKET_ERROR, "iResult = %d\n", iResult);
    ok(!memcmp(szBuf1, szDummyBytes, RECV_BUF), "not equal\n");

    /* Connect to "www.reactos.org" */
    if (!ConnectToReactOSWebsite(sck))
    {
        ok(0, "ConnectToReactOSWebsite failed. Aborting test.\n");
        return 0;
    }

    /* Send the GET request */
    if (!GetRequestAndWait(sck))
    {
        ok(0, "GetRequestAndWait failed. Aborting test.\n");
        return 0;
    }

    /* Receive the data.
       MSG_PEEK will not change the internal number of bytes read, so that a subsequent request should return the same bytes again. */
    SCKTEST(recv(sck, szBuf1, RECV_BUF, MSG_PEEK));
    SCKTEST(recv(sck, szBuf2, RECV_BUF, 0));
    ok(!memcmp(szBuf1, szBuf2, RECV_BUF), "not equal\n");

    /* The last recv() call moved the internal file pointer, so that the next request should return different data. */
    SCKTEST(recv(sck, szBuf1, RECV_BUF, 0));
    ok(memcmp(szBuf1, szBuf2, RECV_BUF), "equal\n");

    /* Create an event for NtReadFile */
    readOffset.QuadPart = 0LL;
    memcpy(szBuf1, szBuf2, RECV_BUF);
    status = NtCreateEvent(&readEvent,
                           EVENT_ALL_ACCESS,
                           NULL,
                           NotificationEvent,
                           FALSE);
    if (status != 0)
    {
        ok(0, "Failed to create event\n");
        return 0;
    }

    /* Try reading the socket using the NT file API */
    status = NtReadFile((HANDLE)sck,
                        readEvent,
                        NULL,
                        NULL,
                        &readIosb,
                        szBuf1,
                        RECV_BUF,
                        &readOffset,
                        NULL);
    if (status == STATUS_PENDING)
    {
        WaitForSingleObject(readEvent, INFINITE);
        status = readIosb.Status;
    }

    ok(status == 0, "Read failed with status 0x%x\n", (unsigned int)status);
    ok(memcmp(szBuf2, szBuf1, RECV_BUF), "equal\n");
    ok(readIosb.Information == RECV_BUF, "Short read\n");

    NtClose(readEvent);
    closesocket(sck);
    WSACleanup();
    return 1;
}

static void Test_ReceiveBuffer(void)
{
    WSADATA wdata;
    SOCKET sin, sout;
    SOCKADDR_IN addr;
    int addrlen;
    int iResult;
    char buffer[16];
    char expected[16];
    unsigned i;
    u_long available;
    int bufsize;

    /* Start up Winsock */
    iResult = WSAStartup(MAKEWORD(2, 2), &wdata);
    ok(iResult == 0, "WSAStartup failed, iResult == %d\n", iResult);

    sin = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ok(sin != INVALID_SOCKET, "Invalid sin, error %d\n", WSAGetLastError());
    sout = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ok(sout != INVALID_SOCKET, "Invalid sout, error %d\n", WSAGetLastError());

    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
    iResult = bind(sin, (PVOID)&addr, sizeof(addr));
    ok(iResult == 0, "bind failed with %d\n", iResult);

    addrlen = sizeof(addr);
    iResult = getsockname(sin, (PVOID)&addr, &addrlen);
    ok(iResult == 0, "getsockname failed with %d\n", iResult);

    trace("Assigned port: %u\n", ntohs(addr.sin_port));

    bufsize = 0x55555555;
    addrlen = sizeof(bufsize);
    iResult = getsockopt(sin, SOL_SOCKET, SO_RCVBUF, (PVOID)&bufsize, &addrlen);
    ok(iResult == 0, "getsockopt failed with %d\n", iResult);
    trace("Buffer size: %d\n", bufsize);

    iResult = ioctlsocket(sin, FIONREAD, &available);
    ok(iResult == 0, "ioctlsocket failed with %d\n", iResult);
    ok(available == 0, "Available: %lu\n", available);

#define MSG_LENGTH 7

    /* Fill up the receive buffer */
    for (i = 0; available + MSG_LENGTH <= bufsize; i++)
    {
        StringCbPrintfA(buffer, sizeof(buffer), "A%-4uBC", i);
        ok(strlen(buffer) == MSG_LENGTH, "[%d] This test is broken, %zu\n", i, strlen(buffer));

        iResult = sendto(sout, buffer, MSG_LENGTH, 0, (PVOID)&addr, sizeof(addr));
        ok(iResult == MSG_LENGTH, "[%d] sendto failed with %d\n", i, iResult);

        iResult = ioctlsocket(sin, FIONREAD, &available);
        ok(iResult == 0, "[%d] ioctlsocket failed with %d\n", i, iResult);
        ok(available == (i + 1) * MSG_LENGTH ||
           available == i * MSG_LENGTH, "[%d] Available: %lu\n", i, available);
        if (available != (i + 1) * MSG_LENGTH)
        {
            Sleep(100);
            iResult = ioctlsocket(sin, FIONREAD, &available);
            ok(iResult == 0, "[%d] ioctlsocket failed with %d\n", i, iResult);
            ok(available == (i + 1) * MSG_LENGTH, "[%d] Available: %lu\n", i, available);
        }
    }

    memset(buffer, 'D', sizeof(buffer));
    iResult = sendto(sout, buffer, MSG_LENGTH, 0, (PVOID)&addr, sizeof(addr));
    ok(iResult == MSG_LENGTH, "sendto failed with %d\n", iResult);

    memset(buffer, 'E', sizeof(buffer));
    iResult = sendto(sout, buffer, MSG_LENGTH, 0, (PVOID)&addr, sizeof(addr));
    ok(iResult == MSG_LENGTH, "sendto failed with %d\n", iResult);

    memset(buffer, 'F', sizeof(buffer));
    iResult = sendto(sout, buffer, MSG_LENGTH, 0, (PVOID)&addr, sizeof(addr));
    ok(iResult == MSG_LENGTH, "sendto failed with %d\n", iResult);

    Sleep(100);

    iResult = ioctlsocket(sin, FIONREAD, &available);
    ok(iResult == 0, "ioctlsocket failed with %d\n", iResult);
    ok(available == bufsize, "Available: %lu\n", available);

    for (i = 0; (i + 1) * MSG_LENGTH < available; i++)
    {
        StringCbPrintfA(expected, sizeof(expected), "A%-4uBC", i);
        ok(strlen(expected) == MSG_LENGTH, "[%d] This test is broken, %zu\n", i, strlen(expected));

        memset(buffer, 0, sizeof(buffer));
        addrlen = sizeof(addr);
        iResult = recvfrom(sin, buffer, sizeof(buffer), 0, (PVOID)&addr, &addrlen);
        ok(iResult == MSG_LENGTH, "[%d] recvfrom returned %d\n", i, iResult);
        ok_str(buffer, expected);
    }

    iResult = ioctlsocket(sin, FIONREAD, &available);
    ok(iResult == 0, "ioctlsocket failed with %d\n", iResult);
    ok(available == MSG_LENGTH, "Available: %lu\n", available);

    memset(expected, 'D', sizeof(expected));
    memset(buffer, 0, sizeof(buffer));
    addrlen = sizeof(addr);
    iResult = recvfrom(sin, buffer, sizeof(buffer), 0, (PVOID)&addr, &addrlen);
    ok(iResult == MSG_LENGTH, "[%d] recvfrom returned %d\n", i, iResult);
    ok(!memcmp(buffer, expected, iResult), "Buffers not equal\n");

    iResult = ioctlsocket(sin, FIONREAD, &available);
    ok(iResult == 0, "ioctlsocket failed with %d\n", iResult);
    ok(available == 0, "Available: %lu\n", available);

    closesocket(sout);
    closesocket(sin);
    WSACleanup();
}

START_TEST(recv)
{
    Test_recv();
    Test_ReceiveBuffer();
}

