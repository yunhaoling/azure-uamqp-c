// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "stdio.h"
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_c_shared_utility/platform.h"

static int open_complete = 0;
const char to_send[] = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";

static void on_send_complete(void* context, IO_SEND_RESULT send_result)
{
	(void)context;
	(void)send_result;
}

static void on_io_open_complete(void* context, IO_OPEN_RESULT open_result)
{
	(void)context, (void)open_result;
	(void)printf("Open complete called\r\n");

	if (open_result == IO_OPEN_OK)
	{
		(void)printf("Open success");
		open_complete = 1;
		//XIO_HANDLE socketio = (XIO_HANDLE)context;
		//// 100 bytes + '\0' = 101 bytes
		//const char to_send[] = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
		//(void)printf("Sending bytes ...\r\n");
		//if (xio_send(socketio, to_send, sizeof(to_send), on_send_complete, NULL) != 0)
		//{
		//	(void)printf("Send failed\r\n");
		//}
	}
	else
	{
		(void)printf("Open error\r\n");
	}
}

static void on_io_bytes_received(void* context, const unsigned char* buffer, size_t size)
{
	(void)context, (void)buffer;
	(void)printf("Received %lu bytes\r\n", (unsigned long)size);
}

static void on_io_error(void* context)
{
	(void)context;
	(void)printf("IO reported an error\r\n");
}

int main(int argc, char** argv)
{
	int result = 0;

	(void)argc, (void)argv;

	if (platform_init() != 0)
	{
		(void)printf("Cannot initialize platform.");
		return MU_FAILURE;
	}

	const IO_INTERFACE_DESCRIPTION* socketio_interface = socketio_get_interface_description();
	if (socketio_interface == NULL)
	{
		(void)printf("Error getting socketio interface description.");
		return MU_FAILURE;
	}

	SOCKETIO_CONFIG socketio_config = { "localhost", 5672, NULL };
	XIO_HANDLE socketio;
	socketio = xio_create(socketio_interface, &socketio_config);
	if (socketio == NULL)
	{
		(void)printf("Error creating socket IO.");
		return MU_FAILURE;

	}

	if (xio_open(socketio, on_io_open_complete, socketio, on_io_bytes_received, socketio, on_io_error, socketio) != 0)
	{
		(void)printf("Error opening socket IO.");
		return MU_FAILURE;
	}

	while (!open_complete) {
		// do nothing, wait for open
	}

	unsigned char done = 0;
	while (!done)
	{
		if (xio_send(socketio, to_send, sizeof(to_send), on_send_complete, NULL) != 0)
		{
			(void)printf("Send failed\r\n");
		}
		xio_dowork(socketio);
	}

	result = 0;
	xio_destroy(socketio);
	platform_deinit();

	return result;
}
