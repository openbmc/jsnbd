sbin_PROGRAMS = nbd-proxy

nbd_proxy_CPPFLAGS = \
	$(JSON_CFLAGS) \
	-DRUNSTATEDIR=\"$(runstatedir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\"

nbd_proxy_LDADD = \
	$(JSON_LIBS)
