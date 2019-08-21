// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <windows.h>
#include <process.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_uamqp_c/uamqp.h"

/* This sample connects to an Event Hub, authenticates using SASL PLAIN (key name/key) and then it received all messages for partition 0 */
/* Replace the below settings with your own.*/

#define EH_HOST "<<<Replace with your own EH host (like myeventhub.servicebus.windows.net)>>>"
#define EH_KEY_NAME "<<<Replace with your own key name>>>"
#define EH_KEY "<<<Replace with your own key>>>"
#define EH_NAME "<<<Replace with your own EH name (like ingress_eh)>>>"

static int rec[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static int DATA_AMOUNT = 10000;
static int partition_cnt = 4;
static int start_partition = 0;
static int LINK_CREDIT = 300;
MESSAGE_RECEIVER_HANDLE* receiver_handlers;
CONNECTION_HANDLE* connect_handlers;
HANDLE *thread_handler;

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
	(void)message;
	int p_id = *(int*)context;
	rec[p_id]++;
	(void)printf("Message received from :%d, total received: %d.\r\n", p_id, rec[p_id]);
	return messaging_delivery_accepted();
}

static CONNECTION_HANDLE create_connection(int partition_id)
{
	XIO_HANDLE sasl_io = NULL;
	CONNECTION_HANDLE connection = NULL;
	SASL_PLAIN_CONFIG sasl_plain_config;
	SASL_MECHANISM_HANDLE sasl_mechanism_handle;
	TLSIO_CONFIG tls_io_config;
	const IO_INTERFACE_DESCRIPTION* tlsio_interface;
	XIO_HANDLE tls_io;
	SASLCLIENTIO_CONFIG sasl_io_config;

	/* create SASL plain handler */
	sasl_plain_config.authcid = EH_KEY_NAME;
	sasl_plain_config.authzid = NULL;
	sasl_plain_config.passwd = EH_KEY;

	sasl_mechanism_handle = saslmechanism_create(saslplain_get_interface(), &sasl_plain_config);

	/* create the TLS IO */
	tls_io_config.hostname = EH_HOST;
	tls_io_config.port = 5671;
	tls_io_config.underlying_io_interface = NULL;
	tls_io_config.underlying_io_parameters = NULL;

	tlsio_interface = platform_get_default_tlsio();
	tls_io = xio_create(tlsio_interface, &tls_io_config);

	/* create the SASL client IO using the TLS IO */
	sasl_io_config.underlying_io = tls_io;
	sasl_io_config.sasl_mechanism = sasl_mechanism_handle;
	sasl_io = xio_create(saslclientio_get_interface_description(), &sasl_io_config);

	/* create the connection, session and link */
	char p_str[3];
	sprintf(p_str, "%d", partition_id);
	STRING_HANDLE conn_str = STRING_construct("conn");
	STRING_concat(conn_str, p_str);
	connection = connection_create(sasl_io, EH_HOST, STRING_c_str(conn_str), NULL, NULL);
	return connection;
}

static MESSAGE_RECEIVER_HANDLE create_receiver(CONNECTION_HANDLE connection, int partition_id)
{
	SESSION_HANDLE session = session_create(connection, NULL, NULL);

	/* set incoming window to 100 for the session */
	session_set_incoming_window(session, 2147483647);
	session_set_outgoing_window(session, 65536);

	char p_str[3];
	sprintf(p_str, "%d", partition_id);

	STRING_HANDLE source_str = STRING_construct("amqps://" EH_HOST "/" EH_NAME "/ConsumerGroups/$Default/Partitions/");
	STRING_concat(source_str, p_str);
	AMQP_VALUE source = messaging_create_source(STRING_c_str(source_str));
	STRING_HANDLE target_str = STRING_construct("ingress-rx");
	STRING_concat(target_str, p_str);
	AMQP_VALUE target = messaging_create_target(STRING_c_str(target_str));
	STRING_HANDLE link_str = STRING_construct("receiver-link");
	STRING_concat(link_str, p_str);
	LINK_HANDLE link = link_create(session, STRING_c_str(link_str), role_receiver, source, target);
	link_set_rcv_settle_mode(link, receiver_settle_mode_first);
	link_set_max_link_credit(link, LINK_CREDIT);
	amqpvalue_destroy(source);
	amqpvalue_destroy(target);

	/* create a message receiver */
	MESSAGE_RECEIVER_HANDLE message_receiver = messagereceiver_create(link, NULL, NULL);
	int* p_id = malloc(sizeof(int));
	*p_id = partition_id;

	if ((message_receiver == NULL) ||
		(messagereceiver_open(message_receiver, on_message_received, p_id) != 0))
	{
		(void)printf("Cannot open the message receiver.");
		return NULL;
	}
	return message_receiver;
}

DWORD WINAPI pump(LPVOID args) {
	int partition_id = *(int *)args;
	while (rec[partition_id] < DATA_AMOUNT) {
		connection_dowork(connect_handlers[partition_id - start_partition]);
	}
	//_endthread();
	return 0;
}

int main(int argc, char** argv)
{
	// usage eg.: message_receiver_sample.exe 8 16 10000 3000
	// The first argument 8 means the first partition offset is 8
	// The second argument 16 means this program will start receving total partition cnt is 16
	//     combined with first argument,
	//     the program will receive messages from partition: 8th 9th ... 23th, total 16 partitions
	// The third argument 10000 means the receivers will pump 10000 messages from each partition
	// The forth argument 3000 means the link max credit

	char* start_position_str = argv[1];
	char* partition_cnt_str = argv[2];
	char* data_amount_str = argv[3];
	char* link_credit_str = argv[4];

	start_partition = atoi(start_position_str);
	partition_cnt = atoi(partition_cnt_str);
	DATA_AMOUNT = atoi(data_amount_str);
	LINK_CREDIT = atoi(link_credit_str);
	printf("start partition idx: %d, partition_cnt:%d, data_amout:%d, link_credit:%d\n", start_partition, partition_cnt, DATA_AMOUNT, LINK_CREDIT);
	int result = 0;

	(void)argc;
	(void)argv;

	if (platform_init() != 0)
	{
		result = -1;
	}
	else
	{
		gballoc_init();

		receiver_handlers = malloc(sizeof(MESSAGE_RECEIVER_HANDLE) * partition_cnt);
		connect_handlers = malloc(sizeof(CONNECTION_HANDLE) * partition_cnt);
		for (int i = 0; i < partition_cnt; i++) {
			CONNECTION_HANDLE connection = create_connection(i + start_partition);
			connect_handlers[i] = connection;
			MESSAGE_RECEIVER_HANDLE receiver = create_receiver(connection, i + start_partition);
			if (receiver == NULL)
				return -1;
			receiver_handlers[i] = receiver;
		}

		thread_handler = malloc(sizeof(HANDLE) * partition_cnt);

		time_t start_time;
		time_t end_time;
		time(&start_time);
		for (int i = 0; i < partition_cnt; i++) {
			int* partition = malloc(sizeof(int));
			*partition = (i + start_partition);
			thread_handler[i] = CreateThread(
				NULL,
				0,
				pump,
				partition,
				0,
				NULL);
		}

		WaitForMultipleObjects(partition_cnt, thread_handler, TRUE, INFINITE);

		time(&end_time);
		time_t duration = end_time - start_time;
		float msg_per_s = DATA_AMOUNT * partition_cnt / (float)(duration);
		printf("Total partition count: %d, Total time is: %lld\n, performance: %f", partition_cnt, duration, msg_per_s);

		// TODO: dealloc handlers
		platform_deinit();
		gballoc_deinit();
	}

	return result;
}
