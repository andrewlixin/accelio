/*
 * Copyright (c) 2013 Mellanox Technologies®. All rights reserved.
 *
 * This software is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU General Public
 * License (GPL) Version 2, available from the file COPYING in the main
 * directory of this source tree, or the Mellanox Technologies® BSD license
 * below:
 *
 *      - Redistribution and use in source and binary forms, with or without
 *        modification, are permitted provided that the following conditions
 *        are met:
 *
 *      - Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *      - Neither the name of the Mellanox Technologies® nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#define __GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "libxio.h"
#include "xio_msg.h"
#include "xio_test_utils.h"
#include "xio_rdma_common.h"

#define XIO_DEF_ADDRESS		"127.0.0.1"
#define XIO_DEF_PORT		2061
#define XIO_TEST_VERSION	"1.0.0"

struct xio_test_config {
	char		server_addr[32];
	uint16_t	server_port;
};

static struct xio_test_config test_config = {
	XIO_DEF_ADDRESS,
	XIO_DEF_PORT,
};

static void print_test_config(void)
{
	printf(" =============================================\n");
	printf(" Server Address	: %s\n", test_config.server_addr);
	printf(" Server Port		: %u\n", test_config.server_port);
	printf(" =============================================\n");
}

static void usage(const char *argv0, int status)
{
	printf("Usage:\n");
	printf("  %s [OPTIONS] <host>\tConnect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");

	printf("\t-p, --port=<port> ");
	printf("\t\tConnect to port <port> (default %d)\n",
	       XIO_DEF_PORT);

	printf("\t-v, --version ");
	printf("\t\t\tPrint the version and exit\n");

	printf("\t-h, --help ");
	printf("\t\t\tDisplay this help and exit\n");

	exit(status);
}

/*---------------------------------------------------------------------------*/
/* parse_cmdline							     */
/*---------------------------------------------------------------------------*/
static int parse_cmdline(int argc, char **argv)
{
	static struct option const long_options[] = {
		{ .name = "port",	.has_arg = 1, .val = 'p'},
		{ .name = "version",	.has_arg = 0, .val = 'v'},
		{ .name = "help",	.has_arg = 0, .val = 'h'},
		{0, 0, 0, 0},
	};

	static char *short_options = "p:vh";
	optind = 0;
	opterr = 0;


	while (1) {
		int c;

		c = getopt_long(argc, argv, short_options,
				long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			test_config.server_port =
				(uint16_t)strtol(optarg, NULL, 0);
			break;
		case 'v':
			printf("version: %s\n", XIO_TEST_VERSION);
			exit(0);
			break;
		case 'h':
			usage(argv[0], 0);
			break;
		default:
			fprintf(stderr, " invalid command or flag.\n");
			fprintf(stderr,
				" please check command line and run again.\n\n");
			usage(argv[0], -1);
			exit(-1);
		}
	}
	if (optind == argc - 1) {
		strcpy(test_config.server_addr, argv[optind]);
	} else if (optind < argc) {
		fprintf(stderr,
			" Invalid command line.\n");
		exit(-1);
	}

	return 0;
}

static int on_session_event(struct xio_session *session,
			    struct xio_session_event_data *event_data,
			    void *cb_user_context)
{
	printf("session event: %s. session:%p, connection:%p, reason: %s\n",
	       xio_session_event_str(event_data->event),
	       session, event_data->conn,
	       xio_strerror(event_data->reason));

	switch (event_data->event) {
	case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
		xio_connection_destroy(event_data->conn);
		break;
	case XIO_SESSION_REJECT_EVENT:
	case XIO_SESSION_TEARDOWN_EVENT:
		xio_context_stop_loop(test_params.ctx);
		break;
	default:
		break;
	};

	return 0;
}

/*---------------------------------------------------------------------------*/
/* main									     */
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
	struct xio_session	*session;
	int			retval;
	char			url[256];
	struct xio_msg		*msg;
	struct xio_session_params params;
	struct xio_connection_params cparams;

	if (parse_cmdline(argc, argv) != 0)
		return -1;

	print_test_config();

	xio_init();

	memset(&test_params, 0, sizeof(struct test_params));
	memset(&params, 0, sizeof(params));
	memset(&cparams, 0, sizeof(cparams));

	test_params.ctx = xio_context_create(NULL, 0, 0);
	xio_assert(test_params.ctx != NULL);

	init_xio_rdma_common_test();

	sprintf(url, "rdma://%s:%d",
		test_config.server_addr,
		test_config.server_port);

	session_ops.on_session_event = on_session_event;
	params.type		= XIO_SESSION_CLIENT;
	params.ses_ops		= &session_ops;
	params.uri		= url;

	session = xio_session_create(&params);
	xio_assert(session != NULL);

	cparams.session		= session;
	cparams.ctx		= test_params.ctx;

	/* connect the session  */
	test_params.connection = xio_connect(&cparams);

	printf("**** starting ...\n");
	msg = msg_pool_get(test_params.pool);
	xio_assert(msg != NULL);
	vmsg_sglist_set_nents(&msg->in, 0);
	vmsg_sglist_set_nents(&msg->out, 0);
	msg->out.header.iov_base = "hello";
	msg->out.header.iov_len	= 6;
	retval = xio_send_request(test_params.connection, msg);
	xio_assert(retval == 0);

	/* the default xio supplied main loop */
	retval = xio_context_run_loop(test_params.ctx, XIO_INFINITE);
	xio_assert(retval == 0);

	/* normal exit phase */
	fprintf(stdout, "exit signaled\n");

	retval = xio_session_destroy(session);
	xio_assert(retval == 0);
	xio_context_destroy(test_params.ctx);

	fini_xio_rdma_common_test();

	xio_shutdown();

	fprintf(stdout, "exit complete\n");

	return 0;
}
