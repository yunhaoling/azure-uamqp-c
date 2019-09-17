// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "stdio.h"
#include <windows.h>
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/tlsio.h"

static int open_complete = 0;
static unsigned char done = 0;

char* to_send;
char* hostname;
static int BYTE_CNT = 64;
static int CLIENT_CNT = 1;

static void on_send_complete(void* context, IO_SEND_RESULT send_result)
{

	(void)context;
	(void)send_result;
	if (send_result != IO_SEND_OK) {
		done = 1;
		printf("send failed\n");
	}
	else {
		printf("send ok\n");
	}
}

DWORD WINAPI keep_send(LPVOID args) {

	XIO_HANDLE socketio = (XIO_HANDLE)args;

	while (!done)
	{
		if (xio_send(socketio, to_send, BYTE_CNT, on_send_complete, NULL) != 0)
		{
			(void)printf("Send failed\r\n");
		}
		xio_dowork(socketio);
	}
	return 0;
}

static void on_io_open_complete(void* context, IO_OPEN_RESULT open_result)
{
	(void)context, (void)open_result;
	(void)printf("Open complete called\r\n");

	if (open_result == IO_OPEN_OK)
	{
		(void)printf("Open success");
		open_complete = 1;
	}
	else
	{
		(void)printf("Open error\r\n");
	}

	CreateThread(
		NULL,
		0,
		keep_send,
		context,
		0,
		NULL);
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
	hostname = argv[1];
	char* client_cnt_str = argv[2];
	char* each_send_byte_size_str = argv[3];

	CLIENT_CNT = atoi(client_cnt_str);
	BYTE_CNT = atoi(each_send_byte_size_str);

	if (platform_init() != 0)
	{
		(void)printf("Cannot initialize platform.");
		return MU_FAILURE;
	}

	const IO_INTERFACE_DESCRIPTION* socketio_interface = socketio_get_interface_description();
	const IO_INTERFACE_DESCRIPTION* tls_interface = platform_get_default_tlsio();
	if (socketio_interface == NULL || tls_interface == NULL)
	{
		(void)printf("Error getting socketio interface description.");
		return MU_FAILURE;
	}


	XIO_HANDLE* tlsios = malloc(sizeof(XIO_HANDLE) * CLIENT_CNT);
	XIO_HANDLE* socketios = malloc(sizeof(XIO_HANDLE) * CLIENT_CNT);
	//SOCKETIO_CONFIG* socketio_configs = malloc(sizeof(SOCKETIO_CONFIG) * CLIENT_CNT);
	TLSIO_CONFIG* tlsio_configs = malloc(sizeof(TLSIO_CONFIG) * CLIENT_CNT);
	SOCKETIO_CONFIG* socketio_configs = malloc(sizeof(SOCKETIO_CONFIG) * CLIENT_CNT);
	for (int i = 0; i < CLIENT_CNT; i++) {
		socketio_configs[i].hostname = hostname;
		socketio_configs[i].port = 5671;
		socketio_configs[i].accepted_socket = NULL;
		socketios[i] = xio_create(socketio_interface, &socketio_configs[i]);

		tlsio_configs[i].hostname = hostname;
		tlsio_configs[i].port = 5671;
		tlsio_configs[i].underlying_io_interface = socketio_get_interface_description();
		tlsio_configs[i].underlying_io_parameters = (void*)(&socketio_configs[i]);
		
		tlsios[i] = xio_create(tls_interface, &tlsio_configs[i]);
		if (tlsios[i] == NULL)
		{
			(void)printf("Error creating tls IO.");
			return MU_FAILURE;

		}
	}

	to_send = malloc(sizeof(char) * BYTE_CNT);
	for (int i = 0; i < BYTE_CNT - 1; i++) {
		to_send[i] = 'a';
	}
	to_send[BYTE_CNT]= '\0';

	for (int i = 0; i < CLIENT_CNT; i++) {
		if (xio_open(tlsios[i], on_io_open_complete, tlsios[i], on_io_bytes_received, tlsios[i], on_io_error, tlsios[i]) != 0)
		{
			(void)printf("Error opening socket IO.");
			return MU_FAILURE;
		}
	}

	while (!done)
	{
		// do nothing
	}

	// WaitForMultipleObjects(CLIENT_CNT, thread_handler, TRUE, INFINITE);

	result = 0;
	// TODO: delete resources
	platform_deinit();

	return result;
}
