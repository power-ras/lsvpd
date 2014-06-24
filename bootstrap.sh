set -x
aclocal && libtoolize --force && autoheader && automake --add-missing --copy && autoconf && \
chmod 644 lsvpd.spec.in Makefile.am NEWS README scsi_templates.conf \
INSTALL COPYING configure.ac ChangeLog cpu_mod_conv.conf
