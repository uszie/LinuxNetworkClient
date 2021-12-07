/***************************************************************************
*   Copyright (C) 2003 by Usarin Heininga                                 *
*   usarin@heininga.nl                                                    *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
***************************************************************************/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <sys/time.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <ifaddrs.h>

#define MAX_INTERFACES 10

struct network_interface
{
	char deviceName[ NAME_MAX + 1 ];
	char IP[ 16 ];
};

typedef struct network_interface NetworkInterface;


u_int64_t getCurrentTime ( void )
{
	struct timeval __t__;

	gettimeofday ( &__t__, NULL );
	return ( ( u_int64_t ) __t__.tv_sec * 1000000ULL ) + ( u_int64_t ) __t__.tv_usec;
}

unsigned short in_cksum ( unsigned short *addr, int len )
{
	int nleft = len;
	int sum = 0;
	unsigned short *w = addr;
	unsigned short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while ( nleft > 1 )
	{
		sum += *w++;
		nleft -= 2;
	}

	/* 4mop up an odd byte, if necessary */
	if ( nleft == 1 )
	{
		* ( unsigned char * ) ( &answer ) = * ( unsigned char * ) w ;
		sum += answer;
	}

	/* 4add back carry outs from top 16 bits to low 16 bits */
	sum = ( sum >> 16 ) + ( sum & 0xffff );  /* add hi 16 to low 16 */
	sum += ( sum >> 16 );                  /* add carry */
	answer = ~sum;                               /* truncate to 16 bits */
	return ( answer );
}

void showUsage ( void )
{
	printf ( "\nNetwork Scanner v" VERSION " -- Scan for available hosts on a network address\n" );
	printf ( "\nUsage: networkscanner [options...]\nValid options are:\n" );
	printf ( "    -n,        --network               The network to scan\n" );
	printf ( "    -p,        --protocol              Comma separated list of protocol numbers to scan a host for\n" );
	printf ( "    -i,        --ignore                Comma separated list of network addresses to ignore\n" );
	printf ( "    -b         --broadcast             Use broadcasting instead of pinging\n" );
	printf ( "    -h,        --help                  Print this message\n" );
	printf ( "\n" );
}

int ports[ BUFSIZ ];
int haveIP = 0;
int singleIP = 0;
int useBroadcast = 0;
in_addr_t networkIPs[ MAX_INTERFACES + 1 ];
in_addr_t ignoreIPs[ MAX_INTERFACES + 1 ];

int shouldBeIgnored( in_addr_t IP ) {
	int i;

	for ( i = 0; i < MAX_INTERFACES && ignoreIPs[ i ]; ++i )
		if ( ignoreIPs[ i ] == IP )
			return 1;

	return 0;
}

void parseCommandLineArguments ( int argc, char *argv[] )
{
	char opt;
	int index;
	int i;
	int retval;
	char *token;
	char *ptr;
	struct hostent *host;
	struct ifaddrs *interfaceList;
	struct ifaddrs *interface;
	struct in_addr address;
	static struct option options[] =
	{
		{"help", 0, 0, 'h' },
		{"network", 1, 0, 'n'},
  		{"ignore", 1, 0, 'i'},
  		{"protocol", 1, 0, 'p'},
		{"single", 0, 0, 's' },
		{"broadcast", 0, 0, 's'},
		{0, 0, 0, 0}
	};

	ports[ 0 ] = 0;

	while ( ( opt = getopt_long ( argc, argv, "hn:i:p:sb", options, &index ) ) != -1 )
	{
		switch ( opt )
		{
			case 'h':
				showUsage();
				exit ( EXIT_SUCCESS );

			case 'n':
				if ( !optarg )
				{
					showUsage();
					exit ( EXIT_SUCCESS );
				}

				if ( strcasecmp ( optarg, "all" ) == 0 )
				{
					retval = getifaddrs ( &interfaceList );
					if ( retval > 0 )
					{
						perror ( "getifaddrs()" );
						exit ( EXIT_FAILURE );
					}

					i = 0;
					for ( interface = interfaceList; interface; interface = interface->ifa_next )
					{
						if ( i >= MAX_INTERFACES )
						{
							fprintf ( stderr, "Found to many network interfaces\n" );
							break;
						}

						if ( interface->ifa_addr->sa_family != AF_INET )
							continue;

						networkIPs[ i ] = ( ( struct sockaddr_in * ) interface->ifa_addr )->sin_addr.s_addr;
						if ( networkIPs[ i ] == INADDR_NONE )
						{
							fprintf ( stderr, "Network connection %s has an invalid IP address ( %s )\n", interface->ifa_name, inet_ntoa ( ( ( struct sockaddr_in * ) interface->ifa_addr )->sin_addr ) );
							continue;
						}

						address.s_addr = networkIPs[ i ];
						if ( strncmp ( "127.", inet_ntoa ( address ), 4 ) == 0 )
							continue;
						++i;
					}
					networkIPs[ i ] = 0;
				}
				else
				{
					ptr = optarg;
					for ( i = 0; ( token = strtok ( ptr, "," ) ); ++i )
					{
						ptr = NULL;
						if ( i == MAX_INTERFACES )
						{
							fprintf ( stderr, "You have selected to many IP addresses\n" );
							exit ( EXIT_FAILURE );
						}

						networkIPs[ i ] = inet_addr ( token );
						if ( networkIPs[ i ] == INADDR_NONE )
						{
							host = gethostbyname ( token );
							if ( ! host || host->h_addrtype != AF_INET )
							{
								fprintf ( stderr, "You specified a invalid network IP range ( %s )\n", token );
								exit ( EXIT_FAILURE );
							}

							networkIPs[ i ] = ( ( struct in_addr * ) host->h_addr )->s_addr;
						}
					}
					networkIPs[ i ] = 0;
				}

				haveIP = 1;
				break;

			case 'i':
				if ( !optarg )
				{
					showUsage();
					exit ( EXIT_SUCCESS );
				}

				ptr = optarg;
				for ( i = 0; ( token = strtok ( ptr, "," ) ); ++i )
				{
					ptr = NULL;
					if ( i == MAX_INTERFACES )
					{
						fprintf ( stderr, "You have selected to many IP addresses to ignore\n" );
						exit ( EXIT_FAILURE );
					}

					ignoreIPs[ i ] = inet_addr ( token );
					if ( ignoreIPs[ i ] == INADDR_NONE )
					{
						host = gethostbyname ( token );
						if ( ! host || host->h_addrtype != AF_INET )
						{
							fprintf ( stderr, "You specified a invalid IP to ignore ( %s )\n", token );
							exit ( EXIT_FAILURE );
						}

						ignoreIPs[ i ] = ( ( struct in_addr * ) host->h_addr )->s_addr;
					}
				}
				ignoreIPs[ i ] = 0;

				break;

			case 'p':
				if ( !optarg )
				{
					showUsage();
					exit ( EXIT_SUCCESS );
				}

				ptr = optarg;
				for ( i = 0; ( token = strtok ( ptr, "," ) ); ++i )
				{
					ptr = NULL;
					if ( i == BUFSIZ )
					{
						fprintf ( stderr, "You have selected to many ports\n" );
						exit ( EXIT_FAILURE );
					}
					ports[ i ] = atoi ( token );
				}
				ports[ i ] = 0;
				break;

			case 's':
				singleIP = 1;
				break;

			case 'b':
				useBroadcast = 1;
				singleIP = 1;
				break;

			default:
				showUsage();
				exit ( EXIT_SUCCESS );
		}
	}
}

char *getHostName ( struct in_addr *IP )
{
	struct hostent *hostent;

	hostent = gethostbyaddr ( ( void * ) IP, sizeof ( struct in_addr ), AF_INET );
	if ( !hostent )
		return inet_ntoa ( *IP );

	return hostent->h_name;
}

struct connection
{
	struct in_addr IP;
	int sock;
	int port;
};

struct connection connections[ 1000 ] = { { { 0 }, 0, 0 } };
int highestSock = 0;
fd_set initialSet = {{0}};
int nfds = 0;

int checkActivePorts ( struct in_addr *IP, int ports[] )
{
	int sock;
	int i;
	int retval;
	long int options;
	struct sockaddr_in toAddress;

	toAddress.sin_family = AF_INET;
	toAddress.sin_addr = *IP;

	for ( i = 0; ports[ i ]; ++i )
	{
		toAddress.sin_port = htons ( ports[ i ] );
		sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		if ( sock < 0 )
		{
			fprintf ( stderr, "Could not create socket\n" );
			return -EXIT_FAILURE;
		}

		options = O_NONBLOCK | fcntl ( sock, F_GETFL );
		retval = fcntl ( sock, F_SETFL, options );
		if ( retval != 0 )
		{
			fprintf ( stderr, "Could not setup socket\n" );
			close ( sock );
			return -EXIT_FAILURE;
		}

		retval = connect ( sock, ( struct sockaddr * ) & toAddress, sizeof ( toAddress ) );
		if ( retval < 0 && errno != EINPROGRESS )
		{
			fprintf ( stderr, "Could not connect to %s: %s\n", inet_ntoa ( *IP ), strerror ( errno ) );
			close ( sock );
			continue;
		}

		connections[ nfds ].IP = toAddress.sin_addr;
		connections[ nfds ].port = ports[ i ];
		connections[ nfds ].sock = sock;
		++nfds;
		FD_SET ( sock, &initialSet );
		if ( sock > highestSock )
			highestSock = sock;
	}

	return EXIT_SUCCESS;
}

int receiveReplies ( void )
{
	fd_set writeFDS;
	int retval;
	struct timeval time;

	if ( highestSock <= 0 )
		return EXIT_SUCCESS;

	time.tv_sec = 0;
	time.tv_usec = 900000;//5
	FD_ZERO ( &writeFDS );
	writeFDS = initialSet;

	while ( ( retval = select ( highestSock + 1, 0, &writeFDS, 0, &time ) ) > 0 )
	{
		int j;
		int error = 0;
		socklen_t size = sizeof ( int );

		for ( j = 0; connections[ j ].sock != 0; ++j )
		{
			if ( FD_ISSET ( connections[ j ].sock, &writeFDS ) )
			{
				retval = getsockopt ( connections[ j ].sock, SOL_SOCKET, SO_ERROR, &error, &size );
				if ( retval == 0 && error == 0 )
					printf ( "%s\t%d\n", inet_ntoa ( connections[ j ].IP ), connections[ j ].port );

				close ( connections[ j ].sock );
				FD_CLR ( connections[ j ].sock, &initialSet );
				--nfds;
			}
		}

		writeFDS = initialSet;
		if ( nfds <= 0 )
		{
			break;
		}
	}

	return EXIT_SUCCESS;
}

int rawSocket;

int setupSocket ( void )
{
	int retval;
	int bufferSize = 1024 * 1024;
	int on = 1;

	rawSocket = socket ( AF_INET, SOCK_RAW, IPPROTO_ICMP );
	if ( rawSocket < 0 )
	{
		perror ( "socket" );
		return -EXIT_FAILURE;
	}

	retval = setsockopt ( rawSocket, SOL_SOCKET, SO_RCVBUF, ( void * ) & bufferSize, sizeof ( bufferSize ) );
	if ( retval < 0 )
	{
		perror ( "setsockopt" );
		close ( rawSocket );
		return -EXIT_FAILURE;
	}

	retval = setsockopt ( rawSocket, SOL_SOCKET, SO_SNDBUF, ( void * ) & bufferSize, sizeof ( bufferSize ) );
	if ( retval < 0 )
	{
		perror ( "setsockopt" );
		close ( rawSocket );
		return -EXIT_FAILURE;
	}

	retval = setsockopt ( rawSocket, SOL_SOCKET, SO_BROADCAST, ( void * ) & on, sizeof ( on ) );
	if ( retval < 0 )
	{
		perror ( "setsockopt" );
		close ( rawSocket );
		return -EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int pingScan ( in_addr_t networkIPs[] )
{
	int iType;
	int icmpOffset;
	int startIP;
	int endIP;
	int i;
	int j;
	int cnt;
	int retval;
	int received = 0;
	char receiveBuf[ 16 * 1024 ];
	in_addr_t networkIP;
	struct sockaddr_in fromAddress;
	struct sockaddr_in toAddress;
	struct timeval tv1;
	struct ip *ipFrame;
	struct icmp *recIcmpFrame;
	struct icmp echo;
	socklen_t length = sizeof ( fromAddress );
	fd_set sockFDset;
	pid_t programPID;
	u_int64_t startTime;

	for ( cnt = 0; networkIPs[ cnt ];  ++cnt )
	{
		networkIP = networkIPs[ cnt ];

		if ( singleIP )
		{
			if ( useBroadcast )
				startIP = endIP = 255;
			else
				startIP = endIP = networkIP >> 24;

		}
		else
		{
			startIP = ( ( networkIP & 0xffffff ) | 0x1000000 ) >> 24;
			endIP = 254;
		}

		networkIP &= ~ ( 255 << 24 );
		programPID = getpid() & 0xFFFF;

		tv1.tv_sec = 0;
		tv1.tv_usec = 0;
		fd_set clearSet;
		FD_ZERO ( &clearSet );
		FD_SET ( rawSocket, &clearSet );
		while ( select ( rawSocket, &clearSet, 0, 0, &tv1 ) > 0 )
		{
			recvfrom ( rawSocket, ( char* ) & receiveBuf, 16 * 1024, 0, 0, 0 );
			tv1.tv_sec = 0;
			tv1.tv_usec = 0;
			FD_ZERO ( &clearSet );
			FD_SET ( rawSocket, &clearSet );
		}

		j = 0;
		for ( i = startIP; i <= endIP; ++i )
		{
			if ( i == 0 )
				continue;

			memset ( &echo, 0, sizeof ( echo ) );
			echo.icmp_type = ICMP_ECHO;
			echo.icmp_code = 0;
			echo.icmp_id = programPID;
			echo.icmp_seq = i;
			echo.icmp_cksum = 0;
			echo.icmp_cksum = in_cksum ( ( unsigned short * ) & echo, sizeof ( echo ) );

			toAddress.sin_family = AF_INET;
			toAddress.sin_addr.s_addr = networkIP;
			toAddress.sin_addr.s_addr += ( i << 24 );
			toAddress.sin_port = 0;

			if ( shouldBeIgnored( toAddress.sin_addr.s_addr ) )
				continue;

			startTime = getCurrentTime();
		retry:
			retval = sendto ( rawSocket, ( void * ) & echo, sizeof ( echo ), 0, ( struct sockaddr * ) & toAddress, sizeof ( toAddress ) );
			if ( retval < 0 && errno == ENOBUFS )
			{
				if ( getCurrentTime() < startTime + 3000000 )
				{
					usleep ( 100000 );
					goto retry;
				}
			}

			if ( retval < 0 )
			{
				perror ( "sendto" );
				return -EXIT_FAILURE;
			}
		}
	}

	FD_ZERO ( &sockFDset );
	FD_SET ( rawSocket, &sockFDset );
	tv1.tv_sec = 0;
	tv1.tv_usec = 900000;//3

	j = 0;
	while ( select ( rawSocket + 1, &sockFDset, 0, 0, &tv1 ) > 0 )
	{
		received = recvfrom ( rawSocket, ( char * ) & receiveBuf, 16 * 1024, 0, ( struct sockaddr * ) & fromAddress, &length );
		if ( received < 0 )
		{
			perror ( "recvfrom" );
			return -EXIT_FAILURE;
		}

		ipFrame = ( struct ip * ) & receiveBuf;
		icmpOffset = ( ipFrame->ip_hl ) * 4;
		recIcmpFrame = ( struct icmp * ) ( receiveBuf + icmpOffset );
		iType = recIcmpFrame->icmp_type;
		if ( iType == ICMP_ECHOREPLY && recIcmpFrame->icmp_id == programPID )
		{
			++j;
			if ( ports[ 0 ] == 0 )
				printf ( "%s\t%s\n", inet_ntoa ( fromAddress.sin_addr ), getHostName ( &fromAddress.sin_addr ) );
			else
				checkActivePorts ( &fromAddress.sin_addr, ports );

			if ( singleIP )
				break;
		}

		FD_ZERO ( &sockFDset );
		FD_SET ( rawSocket, &sockFDset );
	}

	receiveReplies();

	return EXIT_SUCCESS;
}

int main ( int argc, char *argv[] )
{
	int retval;

	if ( argc < 2 )
	{
		showUsage();
		return -EXIT_FAILURE;
	}

	parseCommandLineArguments ( argc, argv );

	if ( !haveIP )
	{
		showUsage();
		return -EXIT_FAILURE;
	}

	retval = setupSocket();
	if ( retval < 0 )
	{
		fprintf ( stderr, "Couldn't setup a socket\n" );
		exit ( EXIT_FAILURE );
	}

	retval = pingScan ( networkIPs );
	if ( retval < 0 )
	{
		fprintf( stderr, "Couldn't ping network range\n" );
		close ( rawSocket );
		exit ( EXIT_FAILURE );
	}

	close ( rawSocket );

	return EXIT_SUCCESS;
}
