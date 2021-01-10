# kamailio mod_slack

Kamailio slack integration.

This module provides capability to send messages to slack channel.

Slack channel, posting username and an icon is configurable via module parameters.

Module made as a "learning lesson" to expore Kamailio internals.  

Currently only one function is implemented:

`slack_send_message()`

This function must be invocked upon MESSAGE request received, w/o parameters.
Function will extract message body and send it to pre-configured slack webhook.  
Works only in request route.

## Example

module configuration
```
loadmodule "slack.so"
modparam("slack", "webhook_uri", "https://hooks.slack.com/services/AAAAA/BBBBB/CCCCCCC")
modparam("slack", "channel", "#webtest")
modparam("slack", "username", "webhookbot")
modparam("slack", "icon_emogi", ":ghost:")
```

```
request_route() {
...
	# extract text from MSG and send to slack
	if (is_method("MESSAGE")) {
		xlog("L_INFO","Got MESSAGE request to [$tu] send body to slack\n");
		slack_send_message();
	}
...
```

Logs:

```
 1(7) INFO: {1 28575 MESSAGE O0o3IuzX9mIcWvdnJZfN-JrXY4gUPAEg} <script>: Got MESSAGE request to [sip:7777@kamailio] send body to slack
 1(7) INFO: {1 28575 MESSAGE O0o3IuzX9mIcWvdnJZfN-JrXY4gUPAEg} slack [slack.c:214]: slack_send_message(): slack destination to <sip:7777@kamailio:5060;transport=udp SIP/2.0
Via: SIP/2.0/UDP 172.16.238.15:53979;rport;branch=z9hG4bKPjjRaoAfrls4r9bDWyczMvCmKqu1nsg.YR
Max-Forwards: 69
From: <sip:5555@kamailio>;tag=Faj6t4OA4KwTs0ka4vBYa4VvebK0TyNb
To: <sip:7777@kamailio>
Call-ID: O0o3IuzX9mIcWvdnJZfN-JrXY4gUPAEg
CSeq: 28575 MESSAGE
Accept: text/plain, application/im-iscomposing+xml
User-Agent: PJSUA v2.9 Linux-5.9.198.212/x86_64
Route: <sip:kamailio:5060;transport=udp;lr>
Route: <sip:kamailio:5060;transport=udp;lr>
Content-Type: text/plain
Content-Length:    24

hello! from Kamailio! :)>
 1(7) INFO: {1 28575 MESSAGE O0o3IuzX9mIcWvdnJZfN-JrXY4gUPAEg} slack [slack.c:108]: curl_recv(): recv_bytes[2][ok]
 1(7) INFO: {1 28575 MESSAGE O0o3IuzX9mIcWvdnJZfN-JrXY4gUPAEg} slack [slack.c:147]: curl_send(): curl sent completed!
 ```

## Requrements

* libcurl

## TODO

* implement function to send msg from any route for notification purposes.
* export to KEMI

## Reference

* [Slack integration](https://api.slack.com/messaging/webhooks)
* [Kamailio Development guide](http://www.asipto.com/pub/kamailio-devel-guide)
