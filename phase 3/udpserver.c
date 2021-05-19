// Server side implementation
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <dirent.h>
#include <stdbool.h>
#include <libgen.h>
#include "send_packet.h"

#define MAXBUFFLINE 2048	// maximum size of buffer to fill image file
#define MAXFILENAME 1024	// maximum length of filename
#define WINDOW_SIZE 7		// window size for implementing sliding window algorithm

int packet(unsigned char flag			/* (in)flags(forth element of packet): 0x1 - packet contains data
												  							   0x2 - packet contains ACK
												 							   0x4 - packet closes the connection */
		  , unsigned char ack_num		/* (in)sequence number of the last received packet(ACK - third element of packet) */
		  , unsigned char seq_num		/* (in)sequence number of this packet */		
		  , unsigned char *ucharPacket	/* (out)packet to be transferred */
)
{
	int lengthof_packet = sizeof(int) + 1 + 1 + 1 + 1;
	
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
	return lengthof_packet;
}

int procedure_on_receive(int  recv_count	/* count of buffer received from client */
		, const unsigned char *recv_buffer	/* received bufer */
		,          const char *dirname		/* directory name contains image file */
		,          const char *out_filename	/* output filename */
		,		unsigned char *seq_num		/* sequence number of this packet */
		,		unsigned char *ack_num		/* sequence number of the last received packet(ACK) */
)
{
	static int req_num = 0;					// unqiue number of the request
	unsigned char flag;						// flag
	char filename_from_recv[MAXFILENAME];	// basename of image file(third element of payloads part)
	char filename[MAXFILENAME];				// full path of image file(dirname + filename_from_recv)
	
	strcpy(filename, dirname);
	strcat(filename, "/");
	
	FILE *fp_image;		// file pointer of image file
	FILE *fp_out;		// file pointer of output file

	unsigned char *file_contents_image;				// contents of image file
	unsigned char file_contents_out[MAXBUFFLINE];	// contents of image file

	int file_size;				// length of image file contents
	int lengthof_filename = 0;	// length of basename of file		
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
		// procedure unique number of request
		int req_num_from_recv_buffer = 0;
		for (int i=sizeof(int)-1; i>=0; i--)
		{
			chTemp = recv_buffer[countof_packet + i];
			nTemp = (int)chTemp;
			nTemp &= 0xff;
			req_num_from_recv_buffer += nTemp;
			if (i > 0)
				req_num_from_recv_buffer <<= 8;			
		}		
		countof_packet += sizeof(int);
		
		if (req_num == req_num_from_recv_buffer) // received packet has expected request number
		{
//			printf("%d - %d\n", req_num, req_num_from_recv_buffer);
			
			// procedure length of filename			
			for (int i=sizeof(int)-1; i>=0; i--)
			{
				chTemp = recv_buffer[countof_packet + i];
				nTemp = (int)chTemp;
				nTemp &= 0xff;
				lengthof_filename += nTemp;
				if (i > 0)
					lengthof_filename <<= 8;			
			}
			countof_packet += sizeof(int);

			// procedure filename
			for (int i=0; i<lengthof_filename-1; i++)
			{
				filename_from_recv[i] = recv_buffer[countof_packet++];
			}			
			filename_from_recv[lengthof_filename-1] = 0;
			
			countof_packet++;	// this is for NULL character in end of filename from recv
			
			strcat(filename, filename_from_recv);
			
			// fill match contents
			strcpy(file_contents_out, "<");
			strcat(file_contents_out, filename_from_recv);
			strcat(file_contents_out, ">");
			
			// file operation	
			fp_image = fopen(filename, "r");
			if (fp_image == NULL)
				strcat(file_contents_out, " UNKNOWN\n");
			else
			{
				// get file size
				fseek(fp_image, 0L, SEEK_END);
				file_size = ftell(fp_image);			// length of file contents
				fclose(fp_image);

				file_contents_image = (unsigned char*) malloc(file_size);	// allocate buffer into heap

				// get file contents
				fp_image = fopen(filename, "r");
				fread(file_contents_image, sizeof(char), MAXBUFFLINE, fp_image);
				fclose(fp_image);				
				
				bool match = false;
				if (file_size == (lengthof_packet - countof_packet)) // compare length of file with length of buffer
				{
					for (int i=0; i<file_size; i++)
					{
						if (file_contents_image[i] != recv_buffer[countof_packet + i])
							match = false;
						else 
							match = true;
					}
				}
				
				free(file_contents_image);	// free buffer from heap
				
				if (match)
				{
					strcat(file_contents_out, " <");
					strcat(file_contents_out, filename);
					strcat(file_contents_out, ">\n");
				}
				else
					strcat(file_contents_out, "UNKNOWN\n");
				
				printf("%s\n", file_contents_out);
				fp_out = fopen(out_filename, "a+");
				
				if (fp_out)
				{
					fwrite(file_contents_out, sizeof(char), strlen(file_contents_out), fp_out);
					fclose(fp_out);
				}
			}			
			
			req_num ++;
			return (req_num - 1); // done go-back-n algorithm
		}
		else 
			return -2;
		
	}

	return -1;	// return terminate
}

int main(int argc, char *argv[])
{
	// checking for command line arguments
	if (argc < 4)
	{
		printf("Usage:\n");
		printf("Port_Number Directory_Name(containing image files) Out_Filename(for printing matches)\nPlease rety!\n");
		return 0;
	}
	
	int nPort 			 = atoi(argv[1]);	// port number should received packets
	char *pszDirname 	 = 		argv[2];	// directory name containing image files
	char *pszOutFilename = 		argv[3];	// filename for printing matches
	
	struct dirent *de;  			// Pointer for directory entry
	DIR *dr = opendir(pszDirname);	// opendir() returns a pointer of DIR type.

    if (dr == NULL)  // invalid directory
    { 
        printf("Could not open directory containing image files\nPlease input correct directory name!\n"); 
        return 0; 
    }
    
//	while ((de = readdir(dr)) != NULL) 
//		printf("%s\n", de->d_name); 
  
    closedir(dr);
	
	int  sockfd;					// file descriptor for socket
	char send_buffer[MAXBUFFLINE];	// buffer for sending to client
	char recv_buffer[MAXBUFFLINE];	// buffer for receiving from client
	char *hello = "Hello from server"; 
	struct sockaddr_in servaddr, clntaddr;	// address structure for both of server and client.

	// Creating socket file descriptor 
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
		perror("socket creation failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	memset(&servaddr, 0, sizeof(servaddr)); 
	memset(&clntaddr, 0, sizeof(clntaddr)); 
	
	// Filling server information 
	servaddr.sin_family = AF_INET; // IPv4 
	servaddr.sin_addr.s_addr = INADDR_ANY; 
	servaddr.sin_port = htons(nPort); 
	
	// Bind the socket with the server address 
	if ( bind(sockfd, (const struct sockaddr *)&servaddr, 
			sizeof(servaddr)) < 0 ) 
	{ 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	int len, recv_count;
	int next_proc;		// the result of processing buffer received

	len = sizeof(clntaddr); //len is value/result	
	
	unsigned char seq_num;	// sequence number of this packet
	unsigned char ack_num;	// sequence number of the last received packet(ACK)
	
	while(1)
	{
		recv_count = recvfrom(sockfd, (char *)recv_buffer, MAXBUFFLINE, MSG_WAITALL, ( struct sockaddr *) &clntaddr, &len);
		recv_buffer[recv_count] = '\0';			
	
		// processing buffer received
		next_proc = procedure_on_receive(recv_count, recv_buffer, pszDirname, pszOutFilename, &seq_num, &ack_num);
		
		if (next_proc >= -1) // packet was received and processed
		{
			ack_num = seq_num;
			// build packet for acknowledgement
			unsigned char flag = 2;
			if (next_proc == -1)
				flag = 4;
			int lengthof_packet = packet(flag, ack_num, seq_num, (unsigned char *)send_buffer);
			
			send_packet(sockfd, (const char *)send_buffer, lengthof_packet, MSG_CONFIRM, (const struct sockaddr *) &clntaddr, len); 
		
			if (next_proc == -1)
				break;
		}
	}
	
	return 0; 
} 

