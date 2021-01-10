/*
 * Copyright (C) 2020 arsperger arsperger@gmail.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the tertc of the GNU General Public License as published by
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


#ifndef slack_h
#define slack_h

#include "../../core/data_lump.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_content.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/clist.h"
//#include "../../core/parser/contact/parse_contact.h"

#include <string.h>
#include <curl/curl.h>

#define BODY_FMT "{\"channel\": \"%s\", \"username\": \"%s\", \"text\": \"%s\", \"icon_emoji\": \"%s\" }"
#define SLACK_DEFAULT_WEBHOOK "https://hooks.slack.com/services/XXXXXXXXXX/YYYYYYYYYY/ZZZZZZZZZZZZ"
#define SLACK_DEFAULT_CHANNEL "#webtest"
#define SLACK_DEFAULT_USERNAME "webhookbot" 
#define SLACK_DEFAULT_ICON ":ghost:"

#define BODY_MAX_SIZE 256
#define REQUEST_MAX_SIZE 1024
#define RESPONSE_MAX_SIZE 1024

static size_t curl_recv(void *buffer, size_t size, size_t nmemb, void *userp);
static int curl_send(const char* uri, str *post_data, str *recv_data);
static int slack_send_message(struct sip_msg* msg, char* param1, char* param2);

#endif /* slack_h */
