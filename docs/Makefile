STYLE_PATH = /usr/share/sgml/docbook/stylesheet/xsl/nwalsh
MAN_STYLE = $(STYLE_PATH)/manpages/docbook.xsl
XHTML_STYLE = $(STYLE_PATH)/xhtml/docbook.xsl
FOP_STYLE = $(STYLE_PATH)/fo/docbook.xsl

MANPAGES = lsvpd.8 lscfg.8 lsmcode.8 lsvio.8 vpdupdate.8
HTMLS = install.html

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644

man8dir=man/man8

MANDEST=$(DESTDIR)/usr/share/

all:
	:

%.8:	%.xml
	xsltproc -o $@ --nonet $(MAN_STYLE) $<

%.html:	%.xml
	xsltproc -o $@ --nonet xhtml.xsl $<

%.pdf:	%.xml
	fop -xml $< -xsl fo.xsl -pdf $@

install:	all
	mkdir -p $(MANDEST)$(man8dir)
	for f in $(MANPAGES) ; do \
		$(INSTALL_DATA) $$f $(MANDEST)$(man8dir) ;\
		gzip -f $(MANDEST)$(man8dir)/$$f ;\
	done
	
distdir :
	:

# Do nothing!  We want those generated files in the tarball.
clean:
	:
	
distclean:
	:
	
uninstall:
	for f in $(MANPAGES) ; do \
		rm $(MANDEST)$(man8dir)/$$f ;\
		rm $(MANDEST)$(man8dir)/$$f.gz ;\
	done
