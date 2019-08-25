// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_uamqp_c/uamqp.h"

static unsigned int sent_messages = 0;
static const size_t msg_count = 1;
static XIO_HANDLE underlying_io;

static int socketio_connected = 0;

static size_t total_received_bytes_in_past_period = 0;
static int period_seconds = 5;
static TICK_COUNTER_HANDLE tick_counter;
static tickcounter_ms_t last_time;
static tickcounter_ms_t now_time;

static void on_bytes_received(void* context, const unsigned char* buffer, size_t size)
{
	(void*)context;
	(void*)buffer;
	(void*)size;

	total_received_bytes_in_past_period += size;

	tickcounter_get_current_ms(tick_counter, &now_time);
	tickcounter_ms_t duration = (now_time - last_time);
	if (duration > 1000 * period_seconds) {

		last_time = now_time;
		float speed = total_received_bytes_in_past_period * 1.0f / ((duration / 1000) * 1024);
		printf("in last :%d seconds, total received bytes: %d, speed is %f: kb/s\n", period_seconds, total_received_bytes_in_past_period, speed);
		total_received_bytes_in_past_period = 0;
	}
}

static void on_io_open_complete(void* context, IO_OPEN_RESULT open_result)
{
	(void*)context;
	(void*)open_result;
	switch (open_result)
	{
	case IO_OPEN_OK:
		printf("IO_OPEN_OK\n");
		break;
	case IO_OPEN_ERROR:
		printf("IO_OPEN_ERROR\n");
		break;
	case IO_OPEN_CANCELLED:
		printf("IO_OPEN_CANCELLED\n");
		break;
	default:
		break;
	}
}

static void on_io_error(void* context) {
	(void*)context;
}

static void on_socket_accepted(void* context, const IO_INTERFACE_DESCRIPTION* interface_description, void* io_parameters)
{
    (void)context;
    underlying_io = xio_create(interface_description, io_parameters);
	xio_open(underlying_io, on_io_open_complete, NULL, on_bytes_received, NULL, on_io_error, NULL);
	printf("get a socket connect request from client\n");
	socketio_connected = 1;
}

int main(int argc, char** argv)
{
    int result = 0;

	printf("Start the server\n");

    (void)argc;
    (void)argv;

    if (platform_init() != 0)
		return -1;

    SOCKET_LISTENER_HANDLE socket_listener;

    gballoc_init();

    socket_listener = socketlistener_create(5672);
	if (socketlistener_start(socket_listener, on_socket_accepted, NULL) != 0)
		return -1;

	while (!socketio_connected) {
		socketlistener_dowork(socket_listener);
	}

	tick_counter = tickcounter_create();
	tickcounter_get_current_ms(tick_counter, &last_time);
    bool keep_running = true;
    while (keep_running)
    {
		xio_dowork(underlying_io);
    }

    result = 0;

    socketlistener_destroy(socket_listener);
    platform_deinit();

    gballoc_deinit();


    return result;
}
