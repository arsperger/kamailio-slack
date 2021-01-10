include ../../Makefile.defs
auto_gen=
NAME=slack.so

ifneq ($(CURL_BUILDER),)
        CURLDEFS += $(shell $(CURL_BUILDER) --cflags)
        CURLLIBS += $(shell $(CURL_BUILDER) --libs)
else
        CURLDEFS+=-I$(LOCALBASE)/include -I$(SYSBASE)/include
        CURLLIBS+=-L$(LOCALBASE)/lib -L$(SYSBASE)/lib -lcurl
endif

DEFS+=$(CURLDEFS)
LIBS=$(CURLLIBS)

include ../../Makefile.modules
