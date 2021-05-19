// Client side implementation of UDP client-server model 
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <stdbool.h>
#include <libgen.h>
#include "send_packet.h"

#define MAXBUFFLINE 2048
#define MAXFILENAME 1024
#define WINDOW_SIZE 7

int packet(unsigned char flag	/* (in)flags(forth element of packet): 0x1 - packet contains data
												  							   0x2 - packet contains ACK
												 							   0x4 - packet closes the connection */
		  , unsigned char ack_num		/* (in)sequence number of the last received packet(ACK - third element of packet) */
		  , 		 int  req_num		/* (in)unique number of the request(first element of payloads part) */		
		  ,          char *filename		/* (in)filename of image file(third element of pauyloads part) */
		  , unsigned char *ucharPacket	/* (out)packet to be transferred */
)
{
	static unsigned char seq_num = 0;		// sequence number of this packet(second element of packet)
	int lengthof_packet = sizeof(int) + 4;	// total length of packet
	
	char *basenameof_filename; 		// basename of image file(third element of payloads part)
	FILE *fp; 						// file pointer of image file
	int file_size;					// length of file contents
	unsigned char *file_contents;	// contents of image file packet included
	int lengthof_filename;			// length of basename of file
	
	if (flag & 0x01)
	{
		basenameof_filename = basename(filename);
		fp = fopen(filename, "r");
	
		fseek(fp, 0L, SEEK_END);
		file_size = ftell(fp);
		fclose(fp);

		file_contents = (unsigned char*) malloc(file_size);	// allocate buffer fill with file contents into heap 

		fp = fopen(filename, "r");
		fread(file_contents, sizeof(char), file_size, fp);
		fclose(fp);
	
		lengthof_filename = strlen(basenameof_filename);
		lengthof_filename ++;
	
		lengthof_packet += sizeof(int) + sizeof(int) + lengthof_filename + file_size;
	}
	
	int countof_packet = 0;	// count length of total packet
	
	unsigned char chTemp;	// temporary variable
	int nTemp;				// temporary variable
	
	// append total length of packet
	for (int i=0; i<sizeof(int); i++)
	{
		nTemp = lengthof_packet;
		nTemp >>= (8 * i);
		nTemp &= 0xff;
		chTemp = (unsigned char) nTemp;
		ucharPacket[countof_packet ++] = chTemp;
	}
	
	// append sequence number of packet
	ucharPacket[countof_packet ++] = seq_num;
	
	// append sequence number of last received packet(ACK)
	ucharPacket[countof_packet ++] = ack_num;

	// append flag
	ucharPacket[countof_packet ++] = flag;

	// append unused(0x7f)
	ucharPacket[countof_packet ++] = 0x7f;
	
	if (flag & 0x01)
	{
		// payloads part
		// append unique number of the request
		for (int i=0; i<sizeof(int); i++)
		{
			nTemp = req_num;
			nTemp >>= (8 * i);
			nTemp &= 0xff;
			chTemp = (unsigned char) nTemp;
			ucharPacket[countof_packet ++] = chTemp;
		}

		// append length of filename
		for (int i=0; i<sizeof(int); i++)
		{
			nTemp = lengthof_filename;
			nTemp >>= (8 * i);
			nTemp &= 0xff;
			chTemp = (unsigned char) nTemp;
			ucharPacket[countof_packet ++] = chTemp;
		}

		// append filename
		for (int i=0; i<lengthof_filename; i++)
		{		
			ucharPacket[countof_packet ++] = basenameof_filename[i];
		}

		// append file contents
		for (int i=0; i<file_size; i++)
		{		
			ucharPacket[countof_packet ++] = file_contents[i];
		}	
	
		free(file_contents);	// free buffer from heap
	}
	
	seq_num ++;
	return lengthof_packet;
}

int procedure_on_receive(int  recv_count	// count of buffer received from client
		, const unsigned char *recv_buffer	// received bufer
		,		unsigned char *seq_num		// sequence number of this packet
		,		unsigned char *ack_num		// sequence number of the last received packet(ACK)
)
{
	static int req_num = 0;					// unqiue number of the request
	unsigned char flag;						// flag
	
	int lengthof_packet   = 0;	// total length of packet received	
	int countof_packet    = 0;	// count length of total packet
	
	unsigned char chTemp;	// temporary variable
	int nTemp = 0;			// temporary variable
	
	if (recv_count < sizeof(int))
		return -2;	// do nothing

	// procedure total length of packet
	for (int i=sizeof(int)-1; i>=0; i--)
	{
		chTemp = recv_buffer[i];
		nTemp = (int)chTemp;
		nTemp &= 0xff;
		lengthof_packet += nTemp;
		if (i > 0)
			lengthof_packet <<= 8;
		countof_packet ++;
	}
	
	if (recv_count < lengthof_packet)
		return -2; // do nothing
	
	// procedure sequence number of packet
	*seq_num = recv_buffer[countof_packet++];

	// procedure sequence number of the last received packet(ACK)
	*ack_num = recv_buffer[countof_packet++];
	
	// procedure flag
	flag = recv_buffer[countof_packet++];
	
	if (recv_buffer[countof_packet++] != 0x7f)
		return -2;	// do nothing
	
	if ((flag & 0x01)) // packet has payloads
	{
		return (req_num - 1); // done go-back-n algorithm
	}

	return -1;	// return terminate
}

int perform_select(int sockfd, long sec, long usec)
{
	// Setup timeval variable
	struct timeval timeout;
	timeout.tv_sec = sec;
	timeout.tv_usec = usec;

	// Setup fd_set structure
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);

	/* Return value:
		 -1: error occurred
		  0: timed out
		> 0: data ready to be read */

	return select(sockfd + 1, &fds, 0, 0, &timeout);
}

// Driver code 
int main(int argc, char *argv[])
{
	if (argc < 5)
	{
		printf("Usage:\n");
		printf("IP_Address Port_Number Filename(contains a list of image files) Drop_Percentage(0~20)\nPlease rety!\n");
		return 0;
	}
	
	char *pszServAddr = argv[1];	// IP address or hostname of the machine where the server application runs
	int nPort = atoi(argv[2]);		// port number at which the server application should receive packets
	char *pszFilename = argv[3];	// a filename that contains a list of image filenames.
	int nDrop = atoi(argv[4]);		// a drop percentage
	
	FILE *fp = fopen(pszFilename, "r");	// file pointer of list
	FILE *fp1;							// auxilary file pointer
	bool fIsInvalidFile = 0;			// if file is valid
	char cszFilename[MAXFILENAME][MAXFILENAME];	// image file list
	int nFileCnt = 0;					// count of image files
	
	// checking whether filename is valid
	if (fp == NULL)
	{
		printf("Please input correct filename.\n");
		return 0;
	}
	
	// checking whether image files are valid
	while (!feof(fp))
	{
		if (fgets(cszFilename[nFileCnt], MAXFILENAME, fp))
		{
			strtok(cszFilename[nFileCnt], "\n");
			fp1 = fopen(cszFilename[nFileCnt], "r");
			if (fp1 == NULL)
			{
				fIsInvalidFile = 1;
				break;
			}
			else
				fclose(fp1);
			nFileCnt ++;
		}
	}	
	fclose(fp);
	
	if (fIsInvalidFile)
	{
		printf("Please check image files.\n");
		return 0;
	}
	
	int sockfd;						// file descriptor of socket
	char send_buffer[MAXBUFFLINE];	// buffer for sending to server
	char recv_buffer[MAXBUFFLINE];	// buffer for receiving from server
		
	struct sockaddr_in servaddr;	// structure for socket address(server)

	// Creating socket file descriptor 
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
		perror("socket creation failed"); 
		exit(EXIT_FAILURE); 
	} 

	memset(&servaddr, 0, sizeof(servaddr)); 
	
	// Filling server information 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_port = htons(nPort); 
	servaddr.sin_addr.s_addr = INADDR_ANY;// inet_addr(pszServAddr);
	
	int recv_count, len; 
	
	// build packet
	
	int seq_num = 0;
	int time_out;
	int lengthof_packet;
	
	int 			 flag = 1;		// 0x1 - packet contains data
									// 0x2 - packet contains ACK
									// 0x4 - packet closes the connection
	unsigned char ack_num = 0xff;	// sequence number of the last received packet(ACK)
	int 		  req_num = 0;		// unique number of the request
	
	while(1)
	{
		if (seq_num == 2)
			flag = 4;
		
		if (flag == 4)
			lengthof_packet = packet(flag, ack_num, req_num, NULL, (unsigned char *)send_buffer);
		else if (flag == 1)
			lengthof_packet = packet(flag, ack_num, req_num, cszFilename[seq_num], (unsigned char *)send_buffer);
	
		send_packet(sockfd, (const char *)send_buffer, lengthof_packet, MSG_CONFIRM, 
				  (const struct sockaddr *) &servaddr, sizeof(servaddr));
		printf("Sequencepacket sent.\n"); 
	
		time_out = perform_select(sockfd, 5, 0);
	
		switch (time_out)
		{
		case 0:
			// timed out, resend same packet
			send_packet(sockfd, (const char *)send_buffer, lengthof_packet, MSG_CONFIRM, 
				  (const struct sockaddr *) &servaddr, sizeof(servaddr));
			break;
		case -1:
			// Error occurred, maybe we should display an error message?

			break;
		default:
			// Ok the data is ready, call recvfrom() to get it then

			recv_count = recvfrom(sockfd, (char *)recv_buffer, MAXBUFFLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len); 
			recv_buffer[recv_count] = '\0';
			
			seq_num++;
			req_num++;
		}
		
		if (flag == 4)
			break;		
	}
	close(sockfd);
	
	return 0; 
} 

