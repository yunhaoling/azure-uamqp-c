// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_uamqp_c/uamqp.h"

/* This sample connects to an Event Hub, authenticates using SASL PLAIN (key name/key) and then it received all messages for partition 0 */
/* Replace the below settings with your own.*/

static AMQP_VALUE on_message_received(const void* context, MESSAGE_HANDLE message)
{
    (void)context;
    (void)message;

    (void)printf("Message received from 1.\r\n");

    return messaging_delivery_accepted();
}

static AMQP_VALUE on_message_received2(const void* context, MESSAGE_HANDLE message)
{
	(void)context;
	(void)message;

	(void)printf("Message received from 2.\r\n");

	return messaging_delivery_accepted();
}

int main(int argc, char** argv)
{
    int result;

    XIO_HANDLE sasl_io = NULL;
    CONNECTION_HANDLE connection = NULL;
    SESSION_HANDLE session = NULL;
	SESSION_HANDLE session2 = NULL;
    LINK_HANDLE link = NULL;
	LINK_HANDLE link2 = NULL;
    MESSAGE_RECEIVER_HANDLE message_receiver = NULL;
	MESSAGE_RECEIVER_HANDLE message_receiver2 = NULL;

    (void)argc;
    (void)argv;

    if (platform_init() != 0)
    {
        result = -1;
    }
    else
    {
        size_t last_memory_used = 0;
        AMQP_VALUE source;
        AMQP_VALUE target;
		AMQP_VALUE source2;
		AMQP_VALUE target2;
        SASL_PLAIN_CONFIG sasl_plain_config;
        SASL_MECHANISM_HANDLE sasl_mechanism_handle;
        TLSIO_CONFIG tls_io_config;
        const IO_INTERFACE_DESCRIPTION* tlsio_interface;
        XIO_HANDLE tls_io;
        SASLCLIENTIO_CONFIG sasl_io_config;

        gballoc_init();

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
        connection = connection_create(sasl_io, EH_HOST, "whatever", NULL, NULL);
        session = session_create(connection, NULL, NULL);

        /* set incoming window to 100 for the session */
        session_set_incoming_window(session, 100);

		session2 = session_create(connection, NULL, NULL);

		/* set incoming window to 100 for the session */
		session_set_incoming_window(session2, 100);

        /* listen only on partition 0 */
        source = messaging_create_source("amqps://" EH_HOST "/" EH_NAME "/ConsumerGroups/$Default/Partitions/0");
        target = messaging_create_target("ingress-rx");
        link = link_create(session, "receiver-link", role_receiver, source, target);
        link_set_rcv_settle_mode(link, receiver_settle_mode_first);
        amqpvalue_destroy(source);
        amqpvalue_destroy(target);

		/* listten also on partition 1*/
		source2 = messaging_create_source("amqps://" EH_HOST "/" EH_NAME "/ConsumerGroups/$Default/Partitions/1");
		target2 = messaging_create_target("ingress-rx2");
		link2 = link_create(session2, "receiver-link", role_receiver, source2, target2);
		link_set_rcv_settle_mode(link2, receiver_settle_mode_first);
		amqpvalue_destroy(source2);
		amqpvalue_destroy(target2);

        /* create a message receiver */
        message_receiver = messagereceiver_create(link, NULL, NULL);
		message_receiver2 = messagereceiver_create(link2, NULL, NULL);

		int open_res1 = messagereceiver_open(message_receiver, on_message_received, message_receiver);
		int open_res2 = messagereceiver_open(message_receiver2, on_message_received2, message_receiver2);

		if (message_receiver == NULL || open_res1 != 0) {
			(void)printf("Cannot open the message receiver1.");
			return -1;
		}

		if (message_receiver2 == NULL || open_res2 != 0) {
			(void)printf("Cannot open the message receiver2.");
			return -2;
		}

		int state = 0;
		int state2 = 0;
		do {
			connection_dowork(connection);
			state = get_message_state(message_receiver, false);
			state2 = get_message_state(message_receiver2, false);
			printf("state1: %d, state2: %d\n", state, state2);
		} while (state != 2 || state2 != 2);

		bool keep_running = true;
		while (keep_running)
		{
			size_t current_memory_used;
			size_t maximum_memory_used;

			current_memory_used = gballoc_getCurrentMemoryUsed();
			maximum_memory_used = gballoc_getMaximumMemoryUsed();

			connection_dowork(connection);

			if (current_memory_used != last_memory_used)
			{
				(void)printf("Current memory usage:%lu (max:%lu)\r\n", (unsigned long)current_memory_used, (unsigned long)maximum_memory_used);
				last_memory_used = current_memory_used;
			}
		}

		result = 0;
		/*
        if ((message_receiver == NULL) ||
            (messagereceiver_open(message_receiver, on_message_received, message_receiver) != 0))
        {
            (void)printf("Cannot open the message receiver.");
            result = -1;
        }
        else
        {
			int state = 0;
			do {
				connection_dowork(connection);
				state = get_message_state(message_receiver, false);
				printf("state: %d\n", state);
			} while (state != 2);

            bool keep_running = true;
            while (keep_running)
            {
                size_t current_memory_used;
                size_t maximum_memory_used;

                current_memory_used = gballoc_getCurrentMemoryUsed();
                maximum_memory_used = gballoc_getMaximumMemoryUsed();

				connection_dowork(connection);

                if (current_memory_used != last_memory_used)
                {
                    (void)printf("Current memory usage:%lu (max:%lu)\r\n", (unsigned long)current_memory_used, (unsigned long)maximum_memory_used);
                    last_memory_used = current_memory_used;
                }
            }

            result = 0;
        }
		*/

        messagereceiver_destroy(message_receiver);
        link_destroy(link);
		messagereceiver_destroy(message_receiver2);
		link_destroy(link2);
        session_destroy(session);
        connection_destroy(connection);
        platform_deinit();

        (void)printf("Max memory usage:%lu\r\n", (unsigned long)gballoc_getCurrentMemoryUsed());
        (void)printf("Current memory usage:%lu\r\n", (unsigned long)gballoc_getMaximumMemoryUsed());

        gballoc_deinit();
    }

    return result;
}
