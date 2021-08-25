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

static char *_slmsg_buf = NULL;
static int mod_init(void);
static void mod_destroy(void);

static int buf_size = 4096;
static char *slack_url = NULL;
static char *slack_channel = SLACK_DEFAULT_CHANNEL;
static char *slack_username = SLACK_DEFAULT_USERNAME;
static char *slack_icon = SLACK_DEFAULT_ICON;

/**
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"slack_send",	  		(cmd_function)slack_send1,			1, slack_fixup,  0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/**
 * Exported parameters
 */
static param_export_t mod_params[] = {
	{ "slack_url", 			PARAM_STRING|USE_FUNC_PARAM, (void*)_slack_url_param},
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
	if(slack_url)
		pkg_free(slack_url);
	return;
}

/**
 * send message with curl
 * @return 0 on success, -1 on error
 */
static int _curl_send(const char* uri, str *post_data)
{
	int datasz;
	char* send_data;
	CURL *curl_handle;
	CURLcode res;
	// LM_DBG("sending to[%s]\n", uri);

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
		if (res != CURLE_OK)
			goto error;
		else
			LM_INFO("slack msg sent ok, [%d]\n", datasz);
		curl_easy_cleanup(curl_handle);
	}
	curl_global_cleanup();
	pkg_free(send_data);
	return 0;

error:
	LM_ERR("Error: %s\n", curl_easy_strerror(res));
	curl_easy_cleanup(curl_handle);
	return -1;
}

/**
 * parse slack_url param2
 */
static int _slack_parse_url_param(char *val)
{
	int len;
	len = strlen(val);
	if(len > SLACK_URL_MAX_SIZE) {
		LM_ERR("webhook url max size exceeded %d\n", SLACK_URL_MAX_SIZE);
		return -1;
	}
	if(strncmp(val, "https://hooks.slack.com", 23)) {
		LM_ERR("invalid slack webhook url [%s]\n", val);
		return -1;
	}
	// TODO: parse webhook to multiple channels eg.: chan1=>https://AAA/BBB/CC, chan2=>...
	slack_url = (char*)pkg_malloc(len + 1);
	strcpy(slack_url, val);
	slack_url[len] = '\0';

	return 0;
}

/**
 * parse slack_url param
 */
int _slack_url_param(modparam_t type, void *val)
{
	if(val==NULL) {
		LM_ERR("webhook url not specified\n");
		return -1;
	}

	return _slack_parse_url_param((char*)val);

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
static inline int slack_helper(struct sip_msg* msg, sl_msg_t *sm)
{
	str txt;
	txt.len = buf_size;

	if(_slack_print_log(msg, sm->m, _slmsg_buf, &txt.len)<0)
		return -1;

	txt.s = _slmsg_buf;

	return _curl_send(slack_url, &txt);
}

static int slack_send1(struct sip_msg* msg, char* frm, char* str2)
{
	return slack_helper(msg, (sl_msg_t*)frm);
}


/**
 * Kemi
 * send slack msg after evaluation of pvars
 */
static int ki_slack_send(sip_msg_t *msg, str *slmsg)
{
	pv_elem_t *xmodel=NULL;
	str txt = STR_NULL;
	int res;

	if(pv_parse_format(slmsg, &xmodel)<0) {
		LM_ERR("wrong format[%s]\n", slmsg->s);
		return -1;
	}
	if(pv_printf_s(msg, xmodel, &txt)!=0) {
		LM_ERR("Error: cannot eval reparsed value\n");
		pv_elem_free_all(xmodel);
		return -1;
	}

	res = _curl_send(slack_url, &txt);
	pv_elem_free_all(xmodel);
	return res;
}

/* clang-format off */
static sr_kemi_t sr_kemi_slack_exports[] = {
	{ str_init("slack"), str_init("slack_send"),
		SR_KEMIP_INT, ki_slack_send,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */


int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_slack_exports);
	return 0;
}
