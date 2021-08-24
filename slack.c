/*
 * Copyright (C) 2021 Arsen Semenov arsperger@gmail.com
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

char *_slmsg_buf = NULL;
static int mod_init(void);
static void mod_destroy(void);

static int buf_size = 4096;
static char *slack_webhook = SLACK_DEFAULT_WEBHOOK;
static char *slack_channel = SLACK_DEFAULT_CHANNEL;
static char *slack_username = SLACK_DEFAULT_USERNAME;
static char *slack_icon = SLACK_DEFAULT_ICON;

/**
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"slack_message_fwd",	(cmd_function)slack_message_fwd,	0, 0, 0, REQUEST_ROUTE},
	{"slack_send",	  		(cmd_function)slack_slog1,			1, slack_fixup,  0, ANY_ROUTE},
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

	_slmsg_buf = (char*)pkg_malloc((buf_size+1)*sizeof(char));
	if(_slmsg_buf==NULL)
	{
		PKG_MEM_ERROR;
		return -1;
	}

	return(0);
}

/**
 * Module destroy
 */
static void mod_destroy() {
	LM_INFO("slack module destroy\n");
	if(_slmsg_buf)
		pkg_free(_slmsg_buf);
	return;
}

static int _sl_str_contact(str* a, str* b, str* dst) {
	if ( ( !a->s || a->len < 1 ) || ( !b->s || b->len < 1 ) ) {
		LM_ERR("empty strings\n");
		return -1;
	}
	dst->len = 0;
	dst->s = NULL;
	dst->s = (char*)shm_malloc(((a->len+b->len)+1+1));
	if (!dst->s) {
		LM_ERR("Error: can not allocate pkg memory [%d] bytes\n", a->len+b->len);
		return -1;
	}

	memcpy(dst->s, a->s, a->len);
	memcpy(dst->s + a->len, " ", 1); // +1
	memcpy(dst->s + a->len + 1, b->s, b->len);
	dst->s[a->len+b->len+1+1] = '\0'; // +1+1
	dst->len = a->len+b->len+1+1;
	LM_DBG("composed [%s]\n", dst->s);
	return 1;
}

static int _curl_send(const char* uri, str *post_data)
{
	int datasz;
	char* send_data;
	CURL *curl_handle;
	CURLcode res;
	LM_DBG("sending to[%s]\n", uri);

	datasz = snprintf(NULL, 0, BODY_FMT, slack_channel, slack_username, post_data->s, slack_icon);
	send_data = (char*)pkg_malloc((datasz+1)*sizeof(char));
	if(send_data==NULL) {
        LM_ERR("Error: can not allocate pkg memory [%d] bytes\n", datasz);
        return -1;
    }
    snprintf(send_data, datasz+1, BODY_FMT, slack_channel, slack_username, post_data->s, slack_icon);

	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();

	if (curl_handle) {
		curl_easy_setopt(curl_handle, CURLOPT_URL, uri);
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, send_data);
		res = curl_easy_perform(curl_handle);
		if (res != CURLE_OK) {
			LM_ERR("Error: %s\n", curl_easy_strerror(res));
		}
		curl_easy_cleanup(curl_handle);
		LM_INFO("slack msg sent ok, [%d]\n", datasz);
	}
	curl_global_cleanup();
	pkg_free(send_data);
	return 0;
}

/**
 *  send MESSAGE body to slack
 */
static int slack_message_fwd(struct sip_msg* msg, char* param1, char* param2)
{
	str body, from_uri, content;
	int mime;

	/* check content type */
	if ((mime = parse_content_type_hdr(msg)) < 1) {
		LM_ERR("failed parse content-type\n");
		return -1;
	}
	if (mime!=MIMETYPE(TEXT,PLAIN) && mime!=MIMETYPE(MESSAGE,CPIM) ) {
		LM_ERR("invalid content-type 0x%x\n", mime);
        return -1;
	}

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

	if (body.len > buf_size) {
		LM_ERR("msg body length is too big\n");
		return -1;
	}

	/* extract sender */
	if (parse_headers(msg, HDR_FROM_F, 0) == -1 || !msg->from) {
		LM_ERR("From headers not found\n");
		return -1;
	}
	if (parse_from_header(msg) < 0 || !msg->from->parsed) {
		LM_ERR("failed to parse From header\n");
		return -1;
	}

	from_uri = ((struct to_body*) msg->from->parsed)->uri;
	LM_DBG("message from <%s>\n", from_uri.s);

	// <arsen@arsperger.com> this is test message!
	_sl_str_contact(&from_uri, &body, &content);
	//content.len = from_uri.len + body.len;
	//content.s = strcat(from_uri)
	_curl_send(slack_webhook, &content);
	shm_free(content->s);
	LM_DBG("slack message sent\n");

	return 1;

}

static int slack_fixup_helper(void** param, int param_no)
{
	sl_msg_t *sm;
	str s;

	sm = (sl_msg_t*)pkg_malloc(sizeof(sl_msg_t));
	if(sm==NULL)
	{
		PKG_MEM_ERROR;
		return -1;
	}
	memset(sm, 0, sizeof(sl_msg_t));
	s.s = (char*)(*param);
	s.len = strlen(s.s);

	if(pv_parse_format(&s, &sm->m)<0)
	{
		LM_ERR("wrong format[%s]\n", (char*)(*param));
		pkg_free(sm);
		return E_UNSPEC;
	}
	*param = (void*)sm;
	return 0;
}

static int slack_fixup(void** param, int param_no)
{
	if(param_no!=1 || param==NULL || *param==NULL)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return E_UNSPEC;
	}
	return slack_fixup_helper(param, param_no);
}

/**
 * send text message to slack
 */
static inline int slog_helper(struct sip_msg* msg, sl_msg_t *sm)
{
	str txt;
	txt.len = buf_size;

	if(_slack_print_log(msg, sm->m, _slmsg_buf, &txt.len)<0)
		return -1;

	txt.s = _slmsg_buf;

	return _curl_send(slack_webhook, &txt);
}

static int slack_slog1(struct sip_msg* msg, char* frm, char* str2)
{
	return slog_helper(msg, (sl_msg_t*)frm);
}
