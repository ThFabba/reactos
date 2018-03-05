/*
 * PROJECT:     ReactOS API Tests
 * LICENSE:     LGPL-2.1+ (https://spdx.org/licenses/LGPL-2.1+)
 * PURPOSE:     Test for IOCTL_AFD_GET_INFO/IOCTL_AFD_SET_INFO
 * COPYRIGHT:   Copyright 2018 Thomas Faber (thomas.faber@reactos.org)
 */

#include "precomp.h"

static
VOID
TestWindowSize(void)
{
    ULONGLONG DefaultBufferSize;
    NTSTATUS Status;
    HANDLE SocketHandle;
    ULONGLONG BufferSize;

    Status = AfdCreateSocket(&SocketHandle, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ok(Status == STATUS_SUCCESS, "AfdCreateSocket failed with %lx\n", Status);

    /* Default buffer sizes */
    Status = AfdGetInfo(SocketHandle, AFD_INFO_RECEIVE_WINDOW_SIZE, &BufferSize);
    ok(Status == STATUS_SUCCESS, "AfdGetInfo failed with %lx\n", Status);
    ok(BufferSize == 8192 /* Win2003 */ ||
       BufferSize == 65536 /* Win10 */, "Receive buffer size is %I64u\n", BufferSize);
    DefaultBufferSize = BufferSize;

    Status = AfdGetInfo(SocketHandle, AFD_INFO_SEND_WINDOW_SIZE, &BufferSize);
    ok(Status == STATUS_SUCCESS, "AfdGetInfo failed with %lx\n", Status);
    ok(BufferSize == DefaultBufferSize, "Send buffer size is %I64u\n", BufferSize);

    /* Set larger buffer sizes */
    Status = AfdSetInfo(SocketHandle, AFD_INFO_RECEIVE_WINDOW_SIZE, 2 * DefaultBufferSize);
    ok(Status == STATUS_SUCCESS, "AfdSetInfo failed with %lx\n", Status);

    Status = AfdSetInfo(SocketHandle, AFD_INFO_SEND_WINDOW_SIZE, 2 * DefaultBufferSize);
    ok(Status == STATUS_SUCCESS, "AfdSetInfo failed with %lx\n", Status);

    /* Query still returns the default */
    Status = AfdGetInfo(SocketHandle, AFD_INFO_RECEIVE_WINDOW_SIZE, &BufferSize);
    ok(Status == STATUS_SUCCESS, "AfdGetInfo failed with %lx\n", Status);
    ok(BufferSize == DefaultBufferSize, "Receive buffer size is %I64u\n", BufferSize);

    Status = AfdGetInfo(SocketHandle, AFD_INFO_SEND_WINDOW_SIZE, &BufferSize);
    ok(Status == STATUS_SUCCESS, "AfdGetInfo failed with %lx\n", Status);
    ok(BufferSize == DefaultBufferSize, "Send buffer size is %I64u\n", BufferSize);

    /* Set smaller buffer sizes */
    Status = AfdSetInfo(SocketHandle, AFD_INFO_RECEIVE_WINDOW_SIZE, DefaultBufferSize / 2);
    ok(Status == STATUS_SUCCESS, "AfdSetInfo failed with %lx\n", Status);

    Status = AfdSetInfo(SocketHandle, AFD_INFO_SEND_WINDOW_SIZE, DefaultBufferSize / 2);
    ok(Status == STATUS_SUCCESS, "AfdSetInfo failed with %lx\n", Status);

    /* Query still returns the default */
    Status = AfdGetInfo(SocketHandle, AFD_INFO_RECEIVE_WINDOW_SIZE, &BufferSize);
    ok(Status == STATUS_SUCCESS, "AfdGetInfo failed with %lx\n", Status);
    ok(BufferSize == DefaultBufferSize, "Receive buffer size is %I64u\n", BufferSize);

    Status = AfdGetInfo(SocketHandle, AFD_INFO_SEND_WINDOW_SIZE, &BufferSize);
    ok(Status == STATUS_SUCCESS, "AfdGetInfo failed with %lx\n", Status);
    ok(BufferSize == DefaultBufferSize, "Send buffer size is %I64u\n", BufferSize);

    Status = NtClose(SocketHandle);
    ok(Status == STATUS_SUCCESS, "NtClose failed with %lx\n", Status);
}

START_TEST(info)
{
    TestWindowSize();
}
