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

#define MAXBUFFLINE 2048
#define MAXFILENAME 1024

int packet(unsigned char packet_type	/* (in)flags(forth element of packet): 0x1 - packet contains data
												  							   0x2 - packet contains ACK
												 							   0x4 - packet closes the connection */
		  , unsigned char ack_num		/* (in)sequence number of the last received packet(ACK - third element of packet) */
		  , 		 int  req_num		/* (in)unique number of the request(first element of payloads part) */		
		  ,          char *filename		/* (in)filename of image file(third element of pauyloads part) */
		  , unsigned char *ucharPacket	/* (out)packet to be transferred */
)
{
	static unsigned char seq_num = 0;				// sequence number of this packet(second element of packet)
	char *basenameof_filename = basename(filename);	// basename of image file(third element of payloads part)
	
	unsigned char *file_contents;		// contents of image file packet included
	FILE *fp = fopen(filename, "r");	// file pointer of image file	
	
	fseek(fp, 0L, SEEK_END);
	int file_size = ftell(fp);			// length of file contents
	fclose(fp);

	file_contents = (unsigned char*) malloc(file_size);	// allocate buffer fill with file contents into heap 

	fp = fopen(filename, "r");
	fread(file_contents, sizeof(char), file_size, fp);
	fclose(fp);
	
	int lengthof_filename = strlen(basenameof_filename);// length of basename of file
	lengthof_filename ++;
	
	int lengthof_packet = sizeof(int) + 1 + 1 + 1 + 1 + sizeof(int) + sizeof(int) + lengthof_filename + file_size;
	
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
	ucharPacket[countof_packet ++] = packet_type;

	// append unused(0x7f)
	ucharPacket[countof_packet ++] = 0x7f;

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
	
	return lengthof_packet;
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
	bool fIsInvalidFile = 0;			// flag whether file is valid
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
	
	int n, len; 
	
	// build packet	
	int lengthof_packet = packet(1, 0xff, 0, cszFilename[0], (unsigned char *)send_buffer);
	
	sendto(sockfd, (const char *)send_buffer, lengthof_packet, MSG_CONFIRM, 
			  (const struct sockaddr *) &servaddr, sizeof(servaddr));

	printf("packet sent.\n"); 
		
	n = recvfrom(sockfd, (char *)recv_buffer, MAXBUFFLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len); 
	recv_buffer[n] = '\0'; 
	
	printf("Server : \n"); 
	
	for (int i=0; i<n; i++)
	{
		printf("%x\n", recv_buffer[i]);
	}

	close(sockfd);
	
	return 0; 
} 

