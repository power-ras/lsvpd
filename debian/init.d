#!/bin/sh

PATH=/bin:/usr/bin:/usr/sbin:/sbin:/usr/local/sbin

### BEGIN INIT INFO
# Provides:       boot.lsvpd
# Required-Start: boot.localfs
# Required-Stop:
# Default-Start:  B
# Default-Stop:
# Description:    lsvpd hardware inventory utilities
### END INIT INFO

RETVAL=0

case "$1" in
  start)
    echo -n "Running VPD Database updater                                           "
    vpdupdate
    RETVAL=$?
    
    if[ -e /etc/udev/rules.d/99-lsvpd.disabled ] ; then
		mv /etc/udev/rules.d/99-lsvpd.disabled \
			/etc/udev/rules.d/99-lsvpd.rules && udevcontrol reload_rules
	fi
	
	echo "[Done]"
    ;;
    
  stop)
    echo -n "Archiving old VPD Database                                             "
		
	if[ -e /etc/udev/rules.d/99-lsvpd.rules ] ; then
		mv /etc/udev/rules.d/99-lsvpd.rules \
			/etc/udev/rules.d/99-lsvpd.disabled && udevcontrol reload_rules
	fi
	
    vpdupdate -a
    RETVAL=$?
    
    echo "[Done]"
    ;;
    
  *)
    echo "Usage: lsvpd {start|stop}"
    exit 1
    ;;
esac

exit $RETVAL
