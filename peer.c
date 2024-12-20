#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>  
#include <sys/wait.h>                                                                          
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>

#define MAX_DATA_SIZE 100

struct pdu {
    char type;
    char data[MAX_DATA_SIZE];  //keep data section and manually split up by 10 bytes on receiving?
};

//error function
void error_exit(const char *msg) {
    perror(msg);
    exit(1);
}

int getsocketname(int *sd){
    socklen_t alen= sizeof (struct sockaddr_in);
    struct sockaddr_in reg_addr;

    *sd = socket(AF_INET, SOCK_STREAM, 0);
    reg_addr.sin_family = AF_INET;
    reg_addr.sin_port = htons(0); //requests TCP to choose unique port number for server
    reg_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(*sd, (struct sockaddr *)&reg_addr, sizeof(reg_addr));

    getsockname(*sd, (struct sockaddr *) &reg_addr, &alen);
    return ntohs(reg_addr.sin_port);
}

/* child tcp client downloads program	*/
int handle_client_downloads(int sd)
{
	char	*bp, buf[MAX_DATA_SIZE];
	int 	n, bytes_to_read;

	// Read filename from the TCP connection
    n = read(sd, buf, MAX_DATA_SIZE);
    if (n <= 0) {
        perror("Failed to receive filename");
        close(sd);
        return -1;
    }
    buf[n] = '\0'; // Null-terminate the filename

	// Open the file
	int fd = open(buf, O_RDONLY);
	char error_check_byte;
	if (fd < 0) {
		perror("Failed to open file");
		error_check_byte = '0';
		if (write(sd, &error_check_byte, 1) != 1) {
			perror("Failed to send error check byte");
		}
		close(sd);
		return -1; // Ensure function returns an integer value
	}

	error_check_byte = '1';
	if (write(sd, &error_check_byte, 1) != 1) {
		perror("Failed to send error check byte");
		close(fd);
		close(sd);
		return -1; // Ensure function returns an integer value
	}

    // Send the file content over the TCP connection
    while ((n = read(fd, buf, MAX_DATA_SIZE)) > 0) {
        if (write(sd, buf, n) != n) {
            perror("Failed to send file content");
            close(fd);
            close(sd);
            return -1;
        }
    }

    // Close the file and the socket
    close(fd);
	close(sd);

	return(0);
}

/*Register File*/
void register_file(struct pdu *send_pdu, int tcp_sock){

    char temp[20];
    send_pdu->type = 'R'; 

    //get peer name
    while (1) {
        printf("Enter the peer name you would like to assign to this file:\n");

        if (fgets(temp, sizeof(temp), stdin) != NULL) {
            temp[strcspn(temp, "\n")] = '\0';
            if (strlen(temp) > 10) {
                printf("Peer name is too long. Please enter a name with a maximum of 10 characters.\n");
            } else {
                snprintf(send_pdu->data, 11, "%-10s", temp);
                break;
            }
        }
    }

    //get file name
    while (1) {
        printf("Enter the name of the file you would like to register:\n");
        if (fgets(temp, sizeof(temp), stdin) != NULL) {
            // Remove newline character
            temp[strcspn(temp, "\n")] = '\0';

            // Check if filename length is within limit
            if (strlen(temp) > 10) {
                printf("File name is too long. Please enter a name with a maximum of 10 characters.\n");
            } 
            // Check if file exists and is accessible
            else if (access(temp, F_OK) != 0) {
                printf("File not found. Please enter a valid file name.\n");
            } 
            else {
                // Copy the filename to send_pdu.data if it exists and is valid
                snprintf(send_pdu->data + 10, 11, "%-10s", temp);
                break;
            }
        }
    }

    //assign TCP port to send_pdu.data
    //printf("Assigned TCP port: %d\n", tcp_sock); //testing
    snprintf(send_pdu->data + 20, 11, "%-10d", tcp_sock);

    //testing
    /* //print send_pdu.data
    //printf("send_pdu.data: %s\n", send_pdu.data);
    int i;
    printf("send_pdu.data: ");
    for(i = 0; i < 30; i++){
        printf("%c", send_pdu.data[i]);
    }
    printf("\n"); */

}

//reaper function (TCP)
void	reaper(int sig)
{
	int	status;
	while(wait3(&status, WNOHANG, (struct rusage *)0) >= 0);

}

//Host and download content
void content_hosting(int *sd){
    struct sockaddr_in hosting_address;
    socklen_t alen = sizeof(struct sockaddr_in);
    
    //testing
    printf("Content hosting on port: %d\n", ntohs(hosting_address.sin_port));

    // listen for connections
    if (listen(*sd, 5) < 0) {
        perror("Error listening on socket");
        close(*sd);
        exit(1);
    }

    (void) signal(SIGCHLD, reaper);

    // handle client connections
    struct sockaddr_in client;
    int client_len, new_sd;
    struct pdu send_pdu, receive_pdu;
    char clientAddrReadable[MAX_DATA_SIZE];

    while(1) {
	  client_len = sizeof(client);
	  new_sd = accept(*sd, (struct sockaddr *)&client, &client_len);
	  if(new_sd < 0){
	    fprintf(stderr, "Can't accept client \n");
	    exit(1);
	  }
	  switch (fork()){
	  case 0:		/* child */
		(void) close(*sd);
		exit(handle_client_downloads(new_sd));
	  default:		/* parent */
		(void) close(new_sd);
		break;
	  case -1:
		fprintf(stderr, "fork: error\n");
	  }
	}

    close(*sd);
}

//reset pdu char arrays
void resetPDUs(struct pdu *send_pdu, struct pdu *receive_pdu){
    memset(send_pdu->data, 0, sizeof(send_pdu->data));
    memset(receive_pdu->data, 0, sizeof(receive_pdu->data));
}

int main(int argc, char **argv){
    int udp_sock, tcp_port, sd;
    struct pdu send_pdu, receive_pdu;
    struct sockaddr_in sin;
    int alen = sizeof(sin);
    struct hostent	*phe;
    int port = 3000;
    char *host = "localhost";


    //get host and port from command line
    switch (argc) {
	case 1:
		break;
	case 2:
		host = argv[1];
	case 3:
		host = argv[1];
		port = atoi(argv[2]);
		break;
	default:
		fprintf(stderr, "usage: [host [port]]\n");
		exit(1);
	}

    //UDP Connection
    /*------------------------------------------------------------------------------*/
    //clear pdu char arrays
    resetPDUs(&send_pdu, &receive_pdu);
    
    //set up UDP socket for communication with index server
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    /* Map host name to IP address, allowing for dotted decimal */
        if ( phe == gethostbyname(host) ){
                memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
        }
        else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
		fprintf(stderr, "Can't get host entry \n");

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        error_exit("Failed to create UDP socket");
    }
    /*------------------------------------------------------------------------------*/

    while(1){
        //User input
        printf("Welcome to the P2P network.\n");
        printf("Please select an option:\n");
        printf("Enter R to register a file for download.\n"
        "Enter D to download a file from the server.\n"
        "Enter S to search for a file.\n"
        "Enter T to deregister a file.\n"
        "Enter O to list the files available for download.\n"
        "Enter Q to quit.\n"
        );

        char option;
        option = getchar();
        while (getchar() != '\n');
        printf("Option selected: %c\n", option);

        switch(option){
            case 'R':
                resetPDUs(&send_pdu, &receive_pdu);
                tcp_port = 0;//reset tcp port

                //tcp port setup
                tcp_port = getsocketname(&sd);
                if (tcp_port == -1) {
                    error_exit("Failed to create or bind socket");  // Handle error if port is -1
                }
                printf("tcp port returned from content setup: %d\n", tcp_port);

                //fork content hosting to child process
                pid_t pid = fork();
                if(pid == 0){
                    printf("fork successful\n");
                    content_hosting(&sd);
                }
                
                register_file(&send_pdu, tcp_port);
                
                //send content registration request to index server
                printf("send pdu type: %c\n", send_pdu.type);//testing
                printf("send pdu data: %s\n", send_pdu.data);//testing

                
                if (sendto(udp_sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr*)&sin, alen) == -1) {
                   error_exit("Failed to send PDU");
                }
                printf("Sent registration request to index server.\n");

                
                //receive response from index server - possibly change to read
                if (recvfrom(udp_sock, &receive_pdu, sizeof(receive_pdu), 0, (struct sockaddr *)&sin, &alen) == -1) {
                   error_exit("Failed to receive response");
                }

                // //handle response
                if (receive_pdu.type == 'A') {
                   printf(receive_pdu.data );
                   printf("\n");
                } else if (receive_pdu.type == 'E') {
                   printf("Registration failed: Name conflict or error.\n");
                } else {
                     printf("Unexpected response from index server.\n");
                 }
                break;

            case 'D':
                resetPDUs(&send_pdu, &receive_pdu);

                //download_content();
                break;
            case 'S':
                resetPDUs(&send_pdu, &receive_pdu);

                //search_content();
                
                break;
            case 'T':
                resetPDUs(&send_pdu, &receive_pdu);

                //deregister_content();
                break;
            case 'O':
                resetPDUs(&send_pdu, &receive_pdu);

                //list_content();
                send_pdu.type = 'O';
                write(udp_sock, &send_pdu, sizeof(send_pdu));
                printf("Sent request to list available content.\n");

                read(udp_sock, &receive_pdu, sizeof(receive_pdu));
                if (receive_pdu.type == 'O') {
                    printf("Available content:\n%s\n", receive_pdu.data);
                } else if(receive_pdu.type == 'E') {
                    error_exit(receive_pdu.data);
                } else {
                    printf("Unexpected response from index server.\n");
                }

                break;
            case 'Q':
                printf("Exiting P2P network.\n");
                close(udp_sock);
                exit(0);
                break;
            default:
                printf("Invalid option. Please try again.\n");
                break;
        }
    }
}

