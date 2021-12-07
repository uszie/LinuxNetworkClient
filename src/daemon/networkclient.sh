#!/bin/bash
#
#	/etc/rc.d/init.d/networkclient
#
# Starts the networkclient daemon
#
# chkconfig: 345 95 5
# description: This daemon provides a Linux network neighboorhood environment
# processname: networkclient

# Source function library.
. /etc/rc.d/init.d/functions

test -x /usr/bin/networkclient || exit 0

RETVAL=0

#
# See how we were called.
#

start() {
	# Check if it is already running
	if [ ! -f /var/lock/subsys/networkclient ]; then
	    gprintf "Starting networkclient daemon: "
	    daemon /usr/bin/networkclient
	    RETVAL=$?
	    [ $RETVAL -eq 0 ] && touch /var/lock/subsys/networkclient
	    echo
	fi
	return $RETVAL
}

stop() {
	gprintf "Stopping networkclient daemon: "
	killproc /usr/bin/networkclient
	RETVAL=$?
        pidofproc networkclient > /dev/null
        STATUS=$?
	[ $RETVAL -eq 0 -o $STATUS -eq 1 ] && rm -f /var/lock/subsys/networkclient
	echo
        return $RETVAL
}


restart() {
	stop
	start
}

case "$1" in
start)
	start
	;;
stop)
	stop
	;;
restart)
	restart
	;;
condrestart)
	if [ -f /var/lock/subsys/networkclient ]; then
	    restart
	fi
	;;
status)
	status networkclient
	;;
*)
	INITNAME=`basename $0`
	gprintf "Usage: %s {start|stop|restart|condrestart|status}\n" "$INITNAME"
	exit 1
esac

exit $RETVAL
