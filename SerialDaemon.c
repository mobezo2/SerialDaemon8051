/*
 * SerialDaemon.c
 *
 *     Author: mbezold
 */


/* Necessary to do define F_SETSIG */
#define _GNU_SOURCE

#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <semaphore.h>
#include <time.h>
#include <mqueue.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

#include "tlpi_hdr.h"
#include "tty_functions.h"

#include "SerialPacket.h"
#include "SerialDaemon.h"
#include "SerialLib8051.h"
#include "typedef.h"
#include "become_daemon.h"


/* Something required by RT Signals */
/* Supposedly already defined in /arm-linux-gneabihf/include/features.h */
//#define _POSIX_C_SOURCE 199309
#define DEBUG_LEVEL 11

/* RT Signal to indicate Serial Available */
#define SERIAL_RX_SIG 1

/* Run in the foreground, not as a daemon */
/* #define FOREGROUND_RUN */

/* Globals set by the signal handler that the application
 * can use to determine what action to compelete when it
 * receives signal */
static volatile sig_atomic_t gotSigio = 0, gotSigUsr1 = 0;

/* Original Termios Settings, for restoring at the dameon
 * exit  */
struct termios OrigTermios;

/* Log the Error Message */
LogErrno(){
char UsrMsg[50];
char ErrMsg[50];
ErrMsg=strerror(errno);
sprintf(UsrMsg, "ERRNO = %i , %s", errno, ErrMsg);
syslog(LOG_INFO, "%s", UsrMsg);
}

int
SemaphoreInitialization(void){
	/* Set permissions such that anyone can read or write the SEM*/
	mode_t perms;
	int flags = 0;
	sem_t *sem;
	perms = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	/* Create named semaphore, if not available */
	flags |= O_CREAT;

	/* Use the PID as the value of the Semphore, something can see then see the Serial DaemonSerialT
	 * PID by checking the Sem Value */
	sem = sem_open(SERIAL_DAEMON_SEM, flags, perms, (unsigned int)getpid());
	if(sem == SEM_FAILED)
	{

		#if DEBUG_LEVEL > 0
			errMsg("SemaphoreInitialization: Sem_Open failed");
		#endif
		return SEM_OPEN_FAIL;
	}

	return 1;

}

/* Clear Message queue by grabbing the oldest message first,
 * up to the number of times specified by NumberToClear */
int ClearMessageQueue(mqd_t mqd , int NumberToClear){

	struct mq_attr attr;
	/*ARM_char_t *ASCII_Buff = (ARM_char_t*) malloc(attr.mq_msgsize);*/
	/*uint32_t Length = malloc(attr.mq_msgsize);*/
	ARM_char_t ASCII_Buff[2*MAX_MSG_SIZE + MSG_HEADER_LENGTH];
	int32_t  numRead = 1, numCleared = 0;
	uint32_t prio;

	    /* Get message attributes for allocating size */
	    if(mq_getattr(mqd, &attr) == -1)
	    {
			#if DEBUG_LEVEL > 5
	    		printf("SerialDaemon Clear Message QUEUE:: Failed to get message attributes");
			#endif
	      return SERIAL_RECEIVE_MESSAGE_ATTR_FAIL;
	    }

	    /* Read Message as long as specified by user, which gives them
	     * the ability to clear as many messages as they want, bail out if we stop
	     * receiving bytes as well */
	    while(NumberToClear > 0 && numRead > 0){

	    	numRead = mq_receive(mqd, (char *)&ASCII_Buff, attr.mq_msgsize, &prio);
	    	mq_getattr(mqd, &attr);

			#if DEBUG_LEVEL > 50
	    		printf("Serial Daemon clear Message Queue: Number of messages = %u \n", attr.mq_curmsgs);
			#endif

	    	NumberToClear--;
	    	numCleared++;
	    }

	 return numCleared;
}

int SerialConfigure(const char *SerialFd, uint16_t BaudRate){

	int32_t ttyFd, flags = 0;

	struct termios ModifiedTermios;

	/* Open Options Described in: https://www.cmrr.umn.edu/~strupp/serial.html
		The O_NOCTTY flag tells UNIX that this program doesn't want to
	    be the "controlling terminal" for that port. If you don't specify this then any
	    input (such as keyboard abort signals and so forth) will affect your process. */
	 ttyFd= open(SerialFd, O_RDWR | O_NOCTTY | O_NDELAY );

	 if (ttyFd == -1)
	 {
	    syslog(LOG_INFO, "Failed to open Serial FD");
	    return OPEN_FAIL;

	 }
	 else{
		 syslog(LOG_INFO, "Serial Interface Opened Sucessfully");
	 }

	    /* Acquire Current Serial Terminal settings so that we can go ahead and
	      change and later restore them */
	 if(tcgetattr(ttyFd, &OrigTermios)==-1)
	 {
		syslog(LOG_INFO, "tcgetattr failure");
		return TCGETATTR_FAIL;

	 }

	 ModifiedTermios = OrigTermios;

	 //Prevent Line clear conversion to new line character
	 ModifiedTermios.c_iflag &= ~ICRNL;

	 /*Prevent Echoing of Input characters (Received characters
	 * retransmitted on TX line) */
	 ModifiedTermios.c_lflag &= ~ECHO;

	 /* Set Baud Rate */
	 if(cfsetospeed(&ModifiedTermios, BaudRate) == -1)
	 {
		syslog(LOG_INFO, "Failed to Set BaudRate");
		return BAUDRATE_FAIL;
	 }
	 /*  Put in Cbreak mode where break signals lead to interrupts */
	 if(tcsetattr(ttyFd, TCSAFLUSH, &ModifiedTermios)==-1)
	 {
		syslog(LOG_INFO, "Failed to modify terminal settings");
		return TCSETATTR_FAIL;
	 }


	 /* set owner process that is to receive "I/O possible" signal */
	 /*NOTE: replace stdin_fileno with the /dev/tty04 or whatever */
	 if (fcntl(ttyFd, F_SETOWN, getpid()) == -1)
	 {
		syslog(LOG_INFO, "ERROR: fcntl(F_SETOWN)");
		return SETOWN_FAIL;
	 }

	 syslog(LOG_INFO, "Serial Interface Sucessfully Configured");

	 /* enable "I/O Possible" signalling and make I/O nonblocking for FD
	  * O_ASYNC flag causes signal to be routed to the owner process, set
	  * above */
	 flags = fcntl(ttyFd, F_GETFL );
	 if (fcntl(ttyFd, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1 )
	 {
			syslog(LOG_INFO, "ERROR: FCTNL mode");
			return FCNTL_MODE;
	 }
	 else
	 {
		syslog(LOG_INFO, "FD I/O Signalling Sucessfully Configured");
	 }

	return ttyFd;
}



/* Grab data out of Tx Queue and write out to the 8051 File Descriptor */
static int
SerialTx(int ttyFd, const char *FileName)
{
	int32_t flags = 0;
	mqd_t mqd;

	uint32_t prio;
	struct mq_attr attr;

	int TotalTxBytes = 0, numRead = 0;
	struct timeval CurrentTime;
	char *CurrentTimeString;
	char *ErrMsg;
	char UsrMsg[100];
	size_t count = 0;

	SerialPacket * CurrentSerialPacket=malloc(sizeof(SerialPacket));

	/* Open for Read, Create if not open, Open non-blocking-rcv and send will
		 * fail unless they can complete immediately. */
	flags = O_RDONLY | O_NONBLOCK;

	mqd=mq_open( SERIAL_TX_QUEUE , flags);

	/* check for fail condition, if we failed asssume we need to open a
	* message queue for the first time, then go ahead and do so
	* the function below creates a message queue if not already open*/
	if(mqd == (mqd_t) -1)
	{
		/* Get Error and print to log*/
		ErrMsg=strerror(errno);
		sprintf(UsrMsg, "SerialDaemon TX: Message Queue Open Fails with Error: %s", ErrMsg);
		syslog(LOG_INFO, "%s", UsrMsg);

		#if DEBUG_LEVEL > 5
			errMsg("SerialDaemon TX: Message Open Failed");
		#endif

		return MSG_QUEUE_OPEN_FAIL;
	}

		/* Get message attributes for allocating size */
	if(mq_getattr(mqd, &attr) == -1)
	{
		#if DEBUG_LEVEL >5
			errMsg("SerialDaemon TX:: Failed to get message attributes");
		#endif

		/* Get Error and print to log*/
		ErrMsg=strerror(errno);
		sprintf(UsrMsg, "SerialDaemon TX: mq_getattr failed with Error: %s", ErrMsg);
		syslog(LOG_INFO, "%s", UsrMsg);

		return SERIAL_RECEIVE_MESSAGE_ATTR_FAIL;
	}

	ARM_char_t *ASCII_Buff = (ARM_char_t*) malloc(attr.mq_msgsize);

	numRead = mq_receive(mqd, (void*)ASCII_Buff, attr.mq_msgsize, &prio);

	if(numRead > 0)
	{
		RxMsgInfo* MessageInfo = (RxMsgInfo*) malloc(sizeof(RxMsgInfo));

			/* Figure out how many bytes are in this message so that we
			 * can know how many bytes to write. */
		ARM_char_t ProcessReturn = ProcessPacket(MessageInfo, ASCII_Buff );

		if(ProcessReturn < 0)
		{
				#if DEBUG_LEVEL > 10
					printf("SerialDaemon Tx:: ProcessPacket Fails with error = %u", ProcessReturn);
				#endif

			return ProcessReturn;
		}

		/* Count should include ASCII encoded bytes (multiply by 2),
		 * the header length, (+ MSG_HEADER_LENGTH) and one extra byte for the new line
		 * character */
		count = ((size_t)MessageInfo->MsgLength)*2 + MSG_HEADER_LENGTH +1;

		/* Read buffered Serial data using the file descriptor until we
		   don't receive anymore */

		TotalTxBytes=write(ttyFd, ASCII_Buff, count);

		if(TotalTxBytes < 0)
		{

			#if DEBUG_LEVEL > 5
				errMsg("SerialDaemonTx: Couldn't write to File Descriptor");
			#endif

			ErrMsg=strerror(errno);
			sprintf(UsrMsg, "SerialDaemon TX: Write to ttyfd failed with: %s", ErrMsg);
			syslog(LOG_INFO, "%s", UsrMsg);

			return SERIAL_TX_WRITE_FAIL;
		}


		if(TotalTxBytes == 0)
		{

			#if DEBUG_LEVEL > 5
				printf("SerialDaemonTx: Wrote Zero Bytes\n");
			#endif

			return SERIAL_TX_ZERO_BYTES;
		}

		/* Get Current System Time, copy it to our serial packet header */
		if (gettimeofday(&CurrentTime, NULL) == -1 )
		{

				#if DEBUG_LEVEL > 20
					printf("Couldn't get current time \n");
				#endif
				memset(&CurrentTime, 0x0, sizeof(CurrentTime));
		}
		else
		{
					CurrentTimeString = ctime(&CurrentTime.tv_sec);
					strcpy(CurrentSerialPacket->TimeReceived,CurrentTimeString);
		}

		#if DEBUG_LEVEL > 5
			printf("SerialDaemonTx: New Complete Message Sent \n");
		#endif

			free(CurrentSerialPacket);
			free(MessageInfo);
	}

	if(mq_close(mqd) < (int)0)
	{
		errMsg("SerialDaemonTx: Close Failed");
		ErrMsg=strerror(errno);
		sprintf(UsrMsg, "SerialDaemon TX: Message Queue Close Fails with Error: %s", ErrMsg);
		syslog(LOG_INFO, "%s", UsrMsg);
	}

return numRead;

}


/* Receive incoming serial data from ttyFd and place in message queue and log file */
static int
SerialRx(int ttyFd, const char *FileName)
{

	int TotalRxBytes = 0, AccumulatedRxByteCount = 0 , SndMsgRtn = 0;
	int flags = 0;
	int ClearReturn =0;
	mqd_t mqd;

	char RxBuffer[MAX_RX_BUFF_SIZE];
	char ReadBuff[MAX_READ_BYTES];
	char * CurrentArrayPosition;
	struct timeval CurrentTime;
	Boolean done = 0;
	char *CurrentTimeString;


	SerialPacket * CurrentSerialPacket=malloc(sizeof(SerialPacket));

	done=0;
	TotalRxBytes=0;
	AccumulatedRxByteCount = 0;
	memset(ReadBuff, 0, sizeof(ReadBuff));
	memset(RxBuffer, 0, sizeof(RxBuffer));

     /* Read buffered Serial data using the file descriptor until we
       don't receive anymore (signaled by done flag) */
	while ( !done)  {
		TotalRxBytes=read(ttyFd, ReadBuff, MAX_READ_BYTES);

		/*Terminate Loop if we see 0 bytes returned, or an ERROR */
		if(TotalRxBytes <= 0)
		{
			done=1;
			continue;
		}
		else
		{

			//Copy bytes received this loop iteration into the RxBuff
			CurrentArrayPosition=&RxBuffer[AccumulatedRxByteCount];
			strncpy(CurrentArrayPosition, ReadBuff, TotalRxBytes);
			memset(ReadBuff, 0, sizeof(ReadBuff));

			#if DEBUG_LEVEL > 10
				printf("\nRxBuffer %s ",RxBuffer);
			#endif

			AccumulatedRxByteCount+=TotalRxBytes;

			#if DEBUG_VERBOSITY > 150
			printf("\nCurrent Array Positions is %i \n", *CurrentArrayPosition);
			#endif
		}
	}

	/* Outside the while loop. Process our received data */

	/* Get Current System Time, copy it to our serial packet header */
		if (gettimeofday(&CurrentTime, NULL) == -1 )
		{
				usageErr("Couldn't get current time \n");
				memset(&CurrentTime, 0x0, sizeof(CurrentTime));
		}
		else
		{
				CurrentTimeString = ctime(&CurrentTime.tv_sec);
				strcpy(CurrentSerialPacket->TimeReceived,CurrentTimeString);
		}

	#if DEBUG_LEVEL > 15
		printf("New Complete Message Received \n");
	#endif

		/* Blast this message out on the MSG QUEUE */
		/* Open for Write, Create if not open, Open non-blocking-rcv and send will
		 * fail unless they can complete immediately. */
		flags = O_RDWR | O_NONBLOCK;


		mqd=mq_open( SERIAL_RX_QUEUE , flags);

		/* check for fail condition, if we failed asssume we need to open a
		 * message queue for the first time, then go ahead and do so
		 * the function below creates a message queue if not already open*/
		if(mqd == (mqd_t) -1)
		{
			mqd=Serial8051Open(SERIAL_RX_QUEUE);

			/* Return an error code now, something bad is happening */
			if(mqd == (mqd_t) -1)
			{
				#if DEBUG_LEVEL > 5
					errMsg("SerialDaemonWrite: Message Open Failed");
				#endif
			return MSG_QUEUE_OPEN_FAIL;
			}
		}

		/* Send the Message  */
		#if DEBUG_LEVEL > 150
		/* Print the raw hex values, for debugging */
			printf("\n RxBuffer ");
				int i=0;
				for(i=0; i<AccumulatedRxByteCount; i++)
				{
					printf(" %x",RxBuffer[i]);
				}

				struct mq_attr attr;
				mq_getattr(mqd, &attr);

				printf("\nSerialRx MqAttr = %u \n", attr.mq_msgsize);
		#endif

		SndMsgRtn = mq_send(mqd, RxBuffer, (size_t)(AccumulatedRxByteCount), 0);
		if(SndMsgRtn < 0)
		{

			/* EAGAIN occurs when we have a full message queue,
			 * we need to clear space to send new messages */
			if (errno==EAGAIN){
				/* This function will clear the message queue one message
				 * at a time, oldest message first, freing up space for our
				 * newest message */

				ClearReturn = ClearMessageQueue(mqd , 1);

				#if DEBUG_LEVEL > 50
					printf("SerialRx:EAGAIN encountered, cleared %i Messages \n", ClearReturn);
				#endif
				/* Reattampt our send */

				SndMsgRtn = mq_send(mqd, RxBuffer, (size_t)(AccumulatedRxByteCount), 0);


				/* If it fails this time, simply return */
				if(SndMsgRtn < 0)
				{
					#if DEBUG_LEVEL > 5
					syslog(LOG_INFO, "SerialDaemonRx: Msg RX Fails after attempting to clear queue");
					errMsg("SerialDaemonRx: Msg RX Fails, after attempting to clear queue");
					#endif
					return MSG_SEND_FAIL;
				}
			}
			else
			{
				#if DEBUG_LEVEL > 5
					syslog(LOG_INFO, "SerialDaemonRx: Msg RX Fails");
					errMsg("SerialDaemonRx: Msg Send Fails");
				#endif
			return MSG_SEND_FAIL;
			}
		}

		memset(RxBuffer, 0 ,MAX_RX_BUFF_SIZE);
		free(CurrentSerialPacket);


		if(mq_close(mqd) < (int)0)
		{
			errMsg("SerialDaemonRx: Close Failed");
		}

		/* If we haven't bailed out anywhere else up here, go ahead and send a positive
		 * int as a return, meaning success */
			return 1;
}

/* Signal Handler assigned to a Real Time signal, indicating
 * we have received data on serial.  This is more specific
 * than sigio, since we check the SI_CODE to indicated that
 * we have input data, whereas SIGIO is triggered on any kind
 * of input output */
static void
sigioHandler(int sig, siginfo_t *si, void *ucontext)
{
	if (sig == SERIAL_RX_SIG)
	{
		if(si->si_code == POLL_IN)
		{ /* Only set gotSigio if we have input data (determined using
		 	 si_code */
			gotSigio = 1;
		}
		else
		{
			gotSigio = 0;
		}
	}
}

/* Signal Handler assigned to the SIGUSR1 signal */
static void
sigusr1Handler(int sig)
{
	if ( sig == SIGUSR1 )
		gotSigUsr1 = 1;
}

int
main(int argc, char *argv[])
{
	struct termios OrigTermios, ModifiedTermios;
	struct timespec;

	struct sigevent sev;
	int flags = 0, Return = 0;
	int ttyFd;
	volatile int32_t TX_Return = 1;
	volatile int32_t TX_Active = 0;
	mqd_t mqd_tx , mqd_rx;
	char UsrMsg[100];
	char *ErrMsg;

	//Used by sig handler to control process behavior when the signal arrives
	struct sigaction sa, sa1;

	/* Mask to block and restore signals prior to system calls */
	sigset_t blockSet, emptyMask;

#ifndef FOREGROUND_RUN
	openlog(DAEMON8051_LOG_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
	syslog(LOG_INFO, "Starting Daemon");

	/* Start the process as a daemon,
	 * (forks and creates a child without controlling terminal, parent exits */
	if(becomeDaemon(BD_NO_CHDIR | BD_NO_CLOSE_FILES ) < 0 )
	{	 /* set owner process that is to receive "I/O possible" signal */
		 /*NOTE: replace stdin_fileno with the /dev/tty04 or whatever */
		 if (fcntl(ttyFd, F_SETOWN, getpid()) == -1)
		 {
			syslog(LOG_INFO, "fcntl(F_SETOWN)");
			closelog();
			errExit("fcntl(F_SETOWN)");
		 }
		syslog(LOG_INFO, "Failed to Become Daemon");
		closelog();
		return(DAEMON_FAIL);
	}

	openlog(DAEMON8051_LOG_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
	syslog(LOG_INFO, "Executing as Daemon");
#else
	openlog(DAEMON8051_LOG_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
	syslog(LOG_INFO, "Executing Daemon in Foreground");
#endif


	/* COMPLETE ALL CONFIGURATION OF SERIAL TERMINAL AND IO FILES before messing around with signals,
	 * because we don't want signals to interrupt any of this stuff */

	/* Create the semaphore for communication of PID with external Interface functions,
	 * so they can send a signal to this process usings it's PID */
	if(SemaphoreInitialization() == SEM_OPEN_FAIL)
	{
		syslog(LOG_INFO, "Sem Init Failed");
		closelog();
		errExit("SerialDaemon: Failed  to Open PID semaphore, cannot communicate with SerialWrite Message Queues!!!");
	}

	ttyFd = SerialConfigure(SERIAL_FILEPATH, TTYBAUDRATE);

	if(ttyFd < 0)
	{
		syslog(LOG_INFO, "Serial Open Failed, Exiting");
		closelog();
		errExit("Serial Open Failed");
	}

	/*
	/* Open Options Described in: https://www.cmrr.umn.edu/~strupp/serial.html
		The O_NOCTTY flag tells UNIX that this program doesn't want to
	    be the "controlling terminal" for that port. If you don't specify this then any
	    input (such as keyboard abort signals and so forth) will affect your process.
	 ttyFd= open(SERIAL_FILEPATH, O_RDWR | O_NOCTTY | O_NDELAY );

	 if (ttyFd == -1)
	 {
	    syslog(LOG_INFO, "Failed to open Serial FD");
		closelog();
	    errExit("Serial Terminal Open Failed");
	 }
	 else{
		 syslog(LOG_INFO, "Serial Interface Opened Sucessfully");
	 }

	    /* Acquire Current Serial Terminal settings so that we can go ahead and
	      change and later restore them
	 if(tcgetattr(ttyFd, &OrigTermios)==-1)
	 {
		syslog(LOG_INFO, "tcgetattr failure");
		closelog();
	    errExit("TCGETATTR failed ");
	 }

	 ModifiedTermios = OrigTermios;

	 //Prevent Line clear conversion to new line character
	 ModifiedTermios.c_iflag &= ~ICRNL;

	 /*Prevent Echoing of Input characters (Received characters
	 * retransmitted on TX line)
	 ModifiedTermios.c_lflag &= ~ECHO;

	 /*  Put in Cbreak mode where break signals lead to interrupts
	 if(tcsetattr(ttyFd, TCSAFLUSH, &ModifiedTermios)==-1)
	 {
		syslog(LOG_INFO, "Failed to modify terminal settings");
		closelog();
		errExit(" Couldn't modify terminal settings ");
	 }

	 /* set owner process that is to receive "I/O possible" signal
	 NOTE: replace stdin_fileno with the /dev/tty04 or whatever
	 if (fcntl(ttyFd, F_SETOWN, getpid()) == -1)
	 {
		syslog(LOG_INFO, "fcntl(F_SETOWN)");
		closelog();
		errExit("fcntl(F_SETOWN)");
	 }

	 syslog(LOG_INFO, "Serial Interface Sucessfully Configured");

	 /* enable "I/O Possible" signalling and make I/O nonblocking for FD
	  * O_ASYNC flag causes signal to be routed to the owner process, set
	  * above
	 flags = fcntl(ttyFd, F_GETFL );
	 if (fcntl(ttyFd, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1 )
	 {
			syslog(LOG_INFO, "ERROR: FCTNL mode");
			closelog();
			errExit("fcntl(F_SETFL)");
	 }
	 else
	 {
		syslog(LOG_INFO, "FD I/O Signalling Sucessfully Configured");
	 }
*/
	/* Create signal block set that we can use block signals
	 * during filesystem calls, particularly SIGIO, since
	 * we want to replace it with an RT signal later, and we
	 * don't want this process interrupted until that happens */
	sigemptyset(&blockSet);
	sigaddset(&blockSet, SIGUSR1);
	sigaddset(&blockSet, SIGIO);

	/* Block Signals while we are configuring them */
	if(sigprocmask(SIG_BLOCK, &blockSet, NULL)==-1)
	{
		syslog(LOG_INFO, "ERROR: SerialDameon Main: sigprocmask ");
		closelog();
		errExit("SerialDameon Main: sigprocmask");
	}

	/*Initialize  Signal Mask*/
	sigemptyset(&sa1.sa_mask);

	/*Register Signal Handler for sigusr1*/
	sa1.sa_handler = sigusr1Handler;
	sa1.sa_flags    = 0;

	/* Configure the sigevent struct, which is used to pass on information
	 * to the mqnotify call about how to notify this function */
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGUSR1;

	syslog(LOG_INFO, "Opening Serial_TX Queues ");
	/* Open the message queues, and configure the notification for the
	 * write side (messages from SerialLib8051 write to this interface) */
	mqd_tx = Serial8051Open(SERIAL_TX_QUEUE);
	if(mqd_tx == (mqd_t) -1 )
	{
		syslog(LOG_INFO, "SERIAL_TX mq_open Failed ");
		closelog();
		errExit("SerialDameon Main: SERIAL_TX mq_open");
	}
	else
	{
		syslog(LOG_INFO, "SERIAL_TX mq_open Sucessful ");
	}

	syslog(LOG_INFO, "Opening Serial_RX Queues ");
	mqd_rx = Serial8051Open(SERIAL_RX_QUEUE);
	if(mqd_rx == (mqd_t) -1 )
	{
		syslog(LOG_INFO, "SERIAL_RX mq_open Failed");
		closelog();
		errExit("SerialDameon Main: SERIAL_RX mq_open");
	}
	else
	{
		syslog(LOG_INFO, "SERIAL_RX mq_open Sucessful ");
	}

	/*Initialize  Signal Mask for Serial Receive*/
	sigemptyset(&sa.sa_mask);

	/*Register Signal Handler for SERIAL_RX_SIG */
	sa.sa_sigaction = sigioHandler;
	sa.sa_flags    = SA_SIGINFO;

	/* Block all signals (specific by filling sa_mask) when
	 * the handler specified in in sa is invoked. We make
	 * an exception for SIGUSR1, since we don't mind it interrupting
	 * and setting the gotSigUsr1 bit. */
	sigfillset(&sa.sa_mask);
	sigdelset(&sa.sa_mask, SIGUSR1);

	/* Attach SERIAL_RX_SIG to this process (SERIAL_RX_SIG sent by kernel
		 * when FD that process owns is written too */
	if (sigaction(SERIAL_RX_SIG, &sa, NULL) == -1 )
	{
		syslog(LOG_INFO, "SerialDameon Main: sigaction - SIGIO");
		closelog();
		errExit("SerialDameon Main: sigaction - SIGIO");
	}

	/* Tell the kernel that we want to see an alternative signal
	 * delivered to the process whenever we see activity on the serial FD */

	if(fcntl(ttyFd, F_SETSIG, SERIAL_RX_SIG)==-1)
	{
		syslog(LOG_INFO, "SerialDameon Main: Couldn't set SERIAL_RX_SIG");
		closelog();
		errExit("SerialDameon Main: Couldn't set SERIAL_RX_SIG");

	}

	/* Register SIGURS1, which is raised by processes that
	 * has added data to the message queue to be transmitted out
	 * via serial */
	if (sigaction(SIGUSR1, &sa1, NULL) == -1)
	{
		syslog(LOG_INFO, "SerialDameon Main: sigaction - SIGUSR1");
		closelog();
		errExit("SerialDameon Main: SIGUSR1");
	}

	/* configure the notification to notify when message available in the
	 * write queue (messages from SerialLib8051 write to this interface) */
	if (mq_notify(mqd_tx, &sev)==-1)
	{
		syslog(LOG_INFO, "SerialDameon Main: mq_notify");
		closelog();
		errExit("SerialDameon Main: mq_notify");
	}

	TX_Return = 1;

	/* Create an Empty Signal Mask that sigsuspend will use to
	 * mask / block signals during non-interuptible system calls  */
	sigemptyset( &emptyMask );
	/* Block SIGIO, just to be safe */
	sigaddset(&emptyMask, SIGIO);

	syslog(LOG_INFO, " Initialization Complete, Waiting for Messages ");

	for ( ;; )
	{
		/* Wait for signal, if we receive one, apply empty mask to block incoming signals.
		 * Complete tasks below uninterrupted, and once the loop restarts, call to same function
		 * activates signals again and waits for incoming message. */
		sigsuspend( &emptyMask );

		/* Reinitialize our mq_notify mechanism, if gotSigUsr1 signal caused sigsuspend to
		 * end, and not gotSigio */
	/*	if(!gotSigio){
				if (mq_notify(mqd_tx, &sev)==-1)
				{
					syslog(LOG_INFO, "FAILURE: SerialDameon Main: mq_notify(post sig suspend)");
					closelog();
					errExit("SerialDameon Main: mq_notify(post sig suspend)");
				}
		} */

		/* SIGO is sent when we have received data at our file descriptor */
		if(gotSigio ){
			gotSigio = 0;

     	 	 #if DEBUG_LEVEL > 10
				syslog(LOG_INFO, "SerialDameon Main: SERIAL_RX received");
     	 	 #endif

			/* Retrieves message from FD belong to the Serial Interface, places it in outgoing message
			 * queue that can be accessed by interface layer */
			Return = SerialRx(ttyFd, SERIAL_RX_LOG_FILENAME);

			if(Return<0)
			{
				#if DEBUG_LEVEL > 10
					sprintf(UsrMsg, "Serial Daemon SerialRx fails with error code = %i \n", Return);
					syslog(LOG_INFO, "%s", UsrMsg);
				#endif
			}

		}

		/* Sent when user places a message in the outgoing queue, via Serial8051Write */
		if(gotSigUsr1)
		{
			gotSigUsr1 = 0;

			#if DEBUG_LEVEL > 10
				syslog(LOG_INFO, "Got SigHandlerio1");
			#endif

			/* Transmit all messages in queue, until we see a failure */
			TX_Return = 1;
			TX_Active = 0;
			while(TX_Return > 0)
			{
					TX_Return = SerialTx(ttyFd, SERIAL_RX_LOG_FILENAME);

					#if DEBUG_LEVEL > 10
						if(TX_Return>0)
							syslog(LOG_INFO, "New TX Message Sent");
					#endif

					/*  TX_Active flag is used to block syslog error messages
					 * that are generated from the return being negative
					 * the last time SerialTx is called in the loop, due to all
					 * the messages being read and the queue being empty. This is
					 * opposed to a real error message which are not nominal */
					if(TX_Return>0)
						TX_Active +=1;

					if((TX_Return) < 0 && (TX_Active == 0))
					{
						sprintf(UsrMsg, "SerialTx Error with Code = %i", TX_Return);
						syslog(LOG_INFO, "%s", UsrMsg);
						ErrMsg=strerror(errno);
						sprintf(UsrMsg, "SerialDaemon TX: Message Queue Open Fails with Error: %s", ErrMsg);
						syslog(LOG_INFO, "%s", UsrMsg);
					}

					if((TX_Return < 0) && (TX_Active > 1))
						TX_Active = 0;

			}
			if (mq_notify(mqd_tx, &sev)==-1)
				{
					syslog(LOG_INFO, "FAILURE: SerialDameon Main: mq_notify(inside loop)");
					closelog();
					errExit("SerialDameon Main: mq_notify(post sig suspend)");
				}

		}

	}

	syslog(LOG_INFO, "Exiting Loop");

	/* Close system log prior to exiting */
	syslog(LOG_INFO, "Daemon Exiting, Restoring Original Settings");

	/* Restore original terminal settings */
	if(tcsetattr(ttyFd, TCSAFLUSH, &OrigTermios)==-1)
	{
		syslog(LOG_INFO, "Failed to restore original terminal settings");
		closelog();
		errExit(" Couldn't restore original terminal settings ");
	}

	/* Close Serial Daemon Semaphore, which will make it very obvious that the Serial Daemon is not running */
	if(sem_unlink(SERIAL_DAEMON_SEM)==-1)
	{
		syslog(LOG_INFO, "Failed to unlink semaphore");
		closelog();
		errExit("SerialDaemon: Failed to close Semaphore");
	}

    if (mq_unlink(SERIAL_TX_QUEUE) == -1)
        syslog(LOG_INFO, "Failed to unlink Serial Tx Queue");

    if (mq_unlink(SERIAL_RX_QUEUE) == -1)
        syslog(LOG_INFO, "Failed to unlink Serial Rx Queue");

	/* Close system log prior to exiting */
	syslog(LOG_INFO, "Daemon Cleanup complete");

	closelog();
	exit(1);

}

