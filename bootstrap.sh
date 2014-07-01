set -x

if [ ! -d config ];
then
	mkdir config;
fi

if [ ! -d m4 ];
then
	mkdir m4;
fi

aclocal && libtoolize --force && autoheader && automake --add-missing --copy && autoconf && \
chmod 644 lsvpd.spec.in Makefile.am README scsi_templates.conf \
INSTALL COPYING configure.ac ChangeLog cpu_mod_conv.conf
