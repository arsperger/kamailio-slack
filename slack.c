/*
 * Copyright (C) 2020 arsperger arsperger@gmail.com
 * 
 * Based on code of xmpp gw. Copyright(C) Kamailio.
 * 
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <stdlib.h>

#include "slack.h"

MODULE_VERSION

static int mod_init(void);
static void mod_destroy(void);

static char *slack_webhook = SLACK_DEFAULT_WEBHOOK;
static char *slack_channel = SLACK_DEFAULT_CHANNEL;
static char *slack_username = SLACK_DEFAULT_USERNAME;
static char *slack_icon = SLACK_DEFAULT_ICON;

/**
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"slack_send_message", (cmd_function)slack_send_message, 0, 0, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/**
 * Exported parameters
 */
static param_export_t mod_params[] = {
	{ "webhook_uri", 		PARAM_STRING, &slack_webhook },
	{ "channel",			PARAM_STRING, &slack_channel },
	{ "username",			PARAM_STRING, &slack_username },
	{ "icon_emogi",			PARAM_STRING, &slack_icon },
	{0, 0, 0}
};

/**
 * Module description
 */
struct module_exports exports = {
	"slack",        /* 1 module name */
	DEFAULT_DLFLAGS, /* 2 dlopen flags */
	cmds,            /* 3 exported functions */
	mod_params,      /* 4 exported parameters */
	0,               /* 5 exported RPC functions */
	0,         		 /* 6 exported pseudo-variables */
	0,               /* 7 response function */
	mod_init,        /* 8 module initialization function */
	0,      		 /* 9 per-child init function */
	mod_destroy      /* 0 destroy function */
};

/**
 * Module init
 */
static int mod_init(void) {
	LM_INFO("slack module init\n");
	return(0);
}

/**
 * Module destroy
 */
static void mod_destroy() {
	LM_INFO("slack module destroy\n");
	return;
}


static size_t curl_recv(void *buffer, size_t size, size_t nmemb, void *userp) {
	str *recv_data = (str *) userp;
	int chunk_size = nmemb*size;
	int new_size = recv_data->len + chunk_size;
	LM_DBG("recv_bytes[%d]\n", chunk_size);

	recv_data->s = (char*) pkg_realloc(recv_data->s, new_size + 1);
	if (recv_data->s == NULL) {
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memcpy(recv_data->s + recv_data->len, buffer, chunk_size);
	recv_data->len = new_size;
	recv_data->s[recv_data->len] = '\0';

	size_t recv_bytes = size * nmemb;
	LM_INFO("recv_bytes[%ld][%s]\n", recv_bytes, (char*) buffer);
	return recv_bytes;
}

static int curl_send(const char* uri, str *post_data, str *recv_data){
	LM_DBG("sending to[%s]\n", uri);
	CURL *curl_handle;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();
	char send_data[BODY_MAX_SIZE];
	char curl_error[CURL_ERROR_SIZE + 1];
	curl_error[CURL_ERROR_SIZE] = '\0';

	snprintf(send_data, BODY_MAX_SIZE, BODY_FMT, slack_channel, slack_username, post_data->s, slack_icon);

	if (curl_handle) {
		res = curl_easy_setopt(curl_handle, CURLOPT_URL, uri);
		if (res != CURLE_OK)
			LM_INFO("Error: %s\n", curl_easy_strerror(res));
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_recv);
		if (res != CURLE_OK)
			LM_INFO("Error: %s\n", curl_easy_strerror(res));
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) recv_data);
		if (res != CURLE_OK)
			LM_INFO("Error: %s\n", curl_easy_strerror(res));
		curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, curl_error);
		if (res != CURLE_OK)
			LM_INFO("Error: %s\n", curl_easy_strerror(res));
		if (post_data->s) {
			res = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, send_data);
			if (res != CURLE_OK)
				LM_INFO("Error: %s\n", curl_easy_strerror(res));
		}

		res = curl_easy_perform(curl_handle);
		if (res != CURLE_OK)
			LM_INFO("Error: %s\n", curl_easy_strerror(res));

		LM_INFO("curl sent completed!\n");
		curl_easy_cleanup(curl_handle);
		curl_global_cleanup();
	}
	return 1;
}

/* send MESSAGE body to Slack */
static int slack_send_message(struct sip_msg* msg, char* param1, char* param2)
{
	str body, from_uri, dst;
	int mime;
	str recv_data = {0,0};

	LM_DBG("slack cmd_send_message\n");
	
	/* extract body */
	if (!(body.s = get_body(msg))) {
		LM_ERR("failed to extract body\n");
		return -1;
	}
	if (!msg->content_length) {
		LM_ERR("no content-length found\n");
		return -1;
	}
	body.len = get_content_length(msg);

	/* check content type */
	if ((mime = parse_content_type_hdr(msg)) < 1) {
		LM_ERR("failed parse content-type\n");
		return -1;
	}
	if (mime != (TYPE_TEXT << 16) + SUBTYPE_PLAIN) {
                LM_ERR("invalid content-type 0x%x\n", mime);
                return -1;
    }
	
	/* extract sender */
	if (parse_headers(msg, HDR_TO_F | HDR_FROM_F, 0) == -1 || !msg->to || !msg->from) {
		LM_ERR("no To/From headers\n");
		return -1;
	}
	if (parse_from_header(msg) < 0 || !msg->from->parsed) {
		LM_ERR("failed to parse From header\n");
		return -1;
	}

	from_uri = ((struct to_body *) msg->from->parsed)->uri;
	LM_DBG("message from <%s>\n", from_uri.s);

	/* extract recipient */
	dst.len = 0;
	if (msg->new_uri.len > 0) {
		LM_DBG("using new URI as destination\n");
		dst = msg->new_uri;
	} else if (msg->first_line.u.request.uri.s 
			&& msg->first_line.u.request.uri.len > 0) {
		LM_DBG("using R-URI as destination\n");
		dst = msg->first_line.u.request.uri;
	} else if (msg->to->parsed) {
		LM_DBG("using TO-URI as destination\n");
		dst = ((struct to_body *) msg->to->parsed)->uri;
	} else {
		LM_ERR("failed to find a valid destination\n");
		return -1;
	}

	LM_INFO("slack destination to <%s>\n", dst.s);
	
	// TODO: check dst.s == slack channel

	curl_send(slack_webhook, &body, &recv_data);
	LM_DBG("slack message sent to: %s!\n", dst.s);

	return 1;

}
