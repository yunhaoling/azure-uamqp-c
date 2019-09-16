// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_uamqp_c/uamqp.h"
#include "tls_server_io.h"

static unsigned int sent_messages = 0;
static const size_t msg_count = 1;

static int period_seconds = 5;

static int client_cur_idx = -1;
static int MAX_ALLOWED_CLIENT_CNT = 32;

static unsigned char* cert_buffer;
static size_t cert_size;

bool keep_running = true;

typedef struct SOCKETIO_INFO {
	XIO_HANDLE socket_io;
	int idx;
	size_t total_received_bytes_in_past_period;
	tickcounter_ms_t last_time;
	TICK_COUNTER_HANDLE tick_counter;

} SOCKETIO_CONTEXT;

typedef struct TLSIO_INFO {
	XIO_HANDLE tls_io;
	int idx;
	size_t total_received_bytes_in_past_period;
	tickcounter_ms_t last_time;
	TICK_COUNTER_HANDLE tick_counter;
} TLSIO_CONTEXT;

SOCKETIO_CONTEXT* all_io_context;
TLSIO_CONTEXT* all_io_context;

DWORD WINAPI pump(LPVOID args) {

	SOCKETIO_CONTEXT* cur_context = (SOCKETIO_CONTEXT*)(args);

	while (keep_running)
	{
		xio_dowork(cur_context->socket_io);
	}
	//_endthread();
	return 0;
}

static void on_bytes_received(void* context, const unsigned char* buffer, size_t size)
{
	(void*)context;
	(void*)buffer;
	(void*)size;

	SOCKETIO_CONTEXT* cur_context = (SOCKETIO_CONTEXT*)context;

	cur_context->total_received_bytes_in_past_period += size;

	tickcounter_ms_t now_time;
	tickcounter_get_current_ms(cur_context->tick_counter, &now_time);
	tickcounter_ms_t duration = (now_time - cur_context->last_time);
	if (duration > 1000 * period_seconds) {

		cur_context->last_time = now_time;
		float speed = cur_context->total_received_bytes_in_past_period * 1.0f / ((duration / 1000) * 1024);
		printf("client idx: %d, in last :%d seconds, total received bytes: %d, speed is %f: kb/s\n", cur_context->idx, period_seconds, cur_context->total_received_bytes_in_past_period, speed);
		cur_context->total_received_bytes_in_past_period = 0;
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
		// start a working thread
		CreateThread(
			NULL,
			0,
			pump,
			context,
			0,
			NULL);

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

	TLS_SERVER_IO_CONFIG tls_server_io_config;
	tls_server_io_config.certificate = cert_buffer;
	tls_server_io_config.certificate_size = cert_size;
	tls_server_io_config.underlying_io_interface = interface_description;
	tls_server_io_config.underlying_io_parameters = io_parameters;

	SOCKETIO_CONTEXT* cur_context = &all_io_context[++client_cur_idx];
	// init current context
	cur_context->idx = client_cur_idx;
	cur_context->total_received_bytes_in_past_period = 0;
	cur_context->tick_counter = tickcounter_create();
	cur_context->last_time = tickcounter_get_current_ms(cur_context->tick_counter, &(cur_context->last_time));
	cur_context->socket_io = xio_create(interface_description, io_parameters);


	xio_open(cur_context->socket_io, on_io_open_complete, cur_context, on_bytes_received, cur_context, on_io_error, NULL);
	printf("get a socket connect request from client\n");
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

	all_io_context = malloc(sizeof(SOCKETIO_CONTEXT) * MAX_ALLOWED_CLIENT_CNT);

    while (keep_running)
    {
		socketlistener_dowork(socket_listener);
    }

    result = 0;

    socketlistener_destroy(socket_listener);
    platform_deinit();

    gballoc_deinit();


    return result;
}
