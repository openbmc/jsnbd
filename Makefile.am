sbin_PROGRAMS = nbd-proxy

nbd_proxy_CPPFLAGS = \
	$(UDEV_CFLAGS) \
	-DRUNSTATEDIR=\"$(localstatedir)/run\" \
	-DSYSCONFDIR=\"$(sysconfdir)\"

nbd_proxy_LDADD = \
	$(UDEV_LIBS)

nbd_proxy_LDFLAGS= \
	-lcjson
