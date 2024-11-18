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

//PDU structure
struct pdu {
    char type;
    char data[MAX_DATA_SIZE];  //keep data section and manually split up by 10 bytes on receiving?
};

//error function **DONE
void error_exit(const char *msg) {
    perror(msg);
    //exit(1);
}

//get socket name **DONE
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

//handle client function (TCP) **IN PROGRESS
int handle_client(int sd) 
{
	char	*bp, buf[MAX_DATA_SIZE];
	int 	n, bytes_to_read;

    //sent 'D' type PDU with content name to trigger download
    struct pdu send_pdu;
    send_pdu.type = 'D';


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

//Register file **DONE
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
            temp[strcspn(temp, "\n")] = '\0'; // Remove newline character
            if (strlen(temp) > 10) {
                printf("File name is too long. Please enter a name with a maximum of 10 characters.\n");
            } 
            else if (access(temp, F_OK) != 0) {
                printf("File not found. Please enter a valid file name.\n");
            } 
            else {
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

//reaper function (TCP) **DONE
void	reaper(int sig)
{
	int	status;
	while(wait3(&status, WNOHANG, (struct rusage *)0) >= 0);

}

//reset pdu char arrays **DONE
void resetPDUs(struct pdu *send_pdu, struct pdu *receive_pdu){
    memset(send_pdu->data, 0, sizeof(send_pdu->data));
    memset(receive_pdu->data, 0, sizeof(receive_pdu->data));
}

//request download from another server **DONE
int request_download(struct pdu *send_pdu, int *udp_sock, struct sockaddr_in *sin, int alen){
    char temp[20];
    send_pdu->type = 'S';

    //get peer name
    while (1) {
        printf("Enter the peer name:\n");

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

    //file name
    while (1) {
        printf("Enter the name of the file you would like to download:\n");
        if (fgets(temp, sizeof(temp), stdin) != NULL) {
            temp[strcspn(temp, "\n")] = '\0'; //removes newline character
            if (strlen(temp) > 10) {
                printf("File name is too long. Please enter a name with a maximum of 10 characters.\n");
            } 
            else {
                snprintf(send_pdu->data + 10, 11, "%-10s", temp); //copy to send_pdu data
                break;
            }
        }
    }

    if(sendto(*udp_sock, send_pdu, sizeof(*send_pdu), 0, (struct sockaddr*)sin, alen) == -1){
        error_exit("Failed to send PDU");
    }
    printf("Sent download request to index server.\n");
    
}

//receive download request from content server **IN PROGRESS
void receive_download_request(char *host, int port, struct pdu *send_pdu){
    int tcp_sock;
    struct	hostent		*hp;
    struct sockaddr_in server;

    //connect to content server
    /* Create a stream socket	*/	
	if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Can't creat a socket\n");
		exit(1);
	}

	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if (hp = gethostbyname(host)) 
	  bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	else if ( inet_aton(host, (struct in_addr *) &server.sin_addr) ){
	  fprintf(stderr, "Can't get server's address\n");
	  exit(1);
	}

    /* Connecting to the content server */
	if (connect(tcp_sock, (struct sockaddr *)&server, sizeof(server)) == -1){
	  error_exit("Can't connect to content server \n");
	}
    else{
        send_pdu->type = 'D';
        size_t pdu_size = sizeof(struct pdu);
        if (write(tcp_sock, send_pdu, pdu_size) != pdu_size) {
            perror("Failed to send PDU");
            close(tcp_sock);
            exit(1);
        }
    }

}

//Display menu options **DONE
void display_menu(){
    //User input
    printf("Please select an option:\n");
    printf("Enter R to register a file for download.\n"
    "Enter D to download a file from the server.\n"
    "Enter T to deregister a file.\n"
    "Enter O to list the files available for download.\n"
    "Enter M to list these options again.\n"
    "Enter Q to quit.\n"
    );
}


int main(int argc, char **argv){
    int udp_sock, tcp_port, tcp_sock;
    struct pdu send_pdu, receive_pdu;
    struct sockaddr_in sin;
    int alen = sizeof(sin);
    struct hostent	*phe, *hp;
    int port = 3000;
    char *host = "localhost";
    fd_set afds, rfds;


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

    //TCP Setup
    /*------------------------------------------------------------------------------*/
    //setup tcp port for content hosting
    tcp_port = getsocketname(&tcp_sock);

    printf("TCP port for content hosting: %d\n", tcp_port); //testing
    /*------------------------------------------------------------------------------*/
    
    //Select attributes initialization
    FD_ZERO(&afds);
    FD_SET(tcp_sock, &afds);  // Add TCP listening socket
    FD_SET(udp_sock, &afds);  // Add UDP socket
    FD_SET(0, &afds);         // Add stdin

    printf("Welcome to the P2P network.\n");
    display_menu();

    //main loop
    while(1){
        memcpy(&rfds, &afds, sizeof(rfds));
        // printf("\n");
        // printf("Enter option or M to list options:\n");

        if (select(FD_SETSIZE, &rfds, NULL, NULL, NULL) < 0) {
            perror("select error");
            exit(1);
        }

        // Check if there’s an incoming TCP connection (content download)
        if (FD_ISSET(tcp_sock, &rfds)) {
            int client_len, new_tcp_sock;
            struct sockaddr_in client;
            (void) signal(SIGCHLD, reaper);

            while(1) {
            client_len = sizeof(client);
            new_tcp_sock = accept(tcp_sock, (struct sockaddr *)&client, &client_len);
            if(new_tcp_sock < 0){
                error_exit("Can't accept client \n");
            }
            switch (fork()){
            case 0:		/* child */
                (void) close(tcp_sock);
                exit(handle_client(new_tcp_sock));
            default:		/* parent */
                (void) close(new_tcp_sock);
                break;
            case -1:
                fprintf(stderr, "fork: error\n");
            }
            }
        }

        //check incoming udp packets
        if(FD_ISSET(udp_sock, &rfds)){
            resetPDUs(&send_pdu, &receive_pdu); //clear pdus

            if (recvfrom(udp_sock, &receive_pdu, sizeof(receive_pdu), 0, (struct sockaddr *)&sin, &alen) == -1) {
                   error_exit("Failed to receive response");
                }

            char rtype = receive_pdu.type;
            char content_server_host[11], content_server_port[11];

            switch(rtype){
                case 'A':
                    //registration success acknowledgment
                    printf("%s", receive_pdu.data );
                    printf("\n");
                    break;
                case 'E':
                    //download content
                    error_exit(receive_pdu.data);
                    break;
                case 'S':
                    //search content
                    

                    strncpy(content_server_host, receive_pdu.data, 10);
				    content_server_host[10] = '\0';
				    strncpy(content_server_port, receive_pdu.data + 10, 10);
				    content_server_port[10] = '\0';

                    printf("Content server host: %s\n", content_server_host); //testing
                    printf("Content server port: %s\n", content_server_port);

                    receive_download_request(content_server_host, atoi(content_server_port), &send_pdu);
                    break;
                case 'C':
                    //deregister content
                    break;
                case 'O':
                    //list available content
                    printf("Available content:\n%s\n", receive_pdu.data);
                    break;
                default:
                    printf("Invalid response from index server.\n");
                    break;
            }
        }

        // Check stdin for user input
        if (FD_ISSET(0, &rfds)) {
            
            char option;
            option = getchar();
            while (getchar() != '\n');
            //printf("Option selected: %c\n", option); //testing

            switch(option){
            case 'R':
                resetPDUs(&send_pdu, &receive_pdu);
                
                register_file(&send_pdu, tcp_port);
                
                //send content registration request to index server
                printf("send pdu type: %c\n", send_pdu.type);//testing
                printf("send pdu data: %s\n", send_pdu.data);//testing

                
                if (sendto(udp_sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr*)&sin, alen) == -1) {
                   error_exit("Failed to send PDU");
                }
                printf("Sent registration request to index server.\n");

                break;

            case 'D':
                resetPDUs(&send_pdu, &receive_pdu);
                
                //request download from another server
                request_download(&send_pdu, &udp_sock, &sin, alen);

                break;
            case 'T':
                resetPDUs(&send_pdu, &receive_pdu);

                //deregister_content();
                break;
            case 'O':
                resetPDUs(&send_pdu, &receive_pdu);

                //list available content
                send_pdu.type = 'O';
                if (sendto(udp_sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr*)&sin, alen) == -1) {
                   error_exit("Failed to send PDU");
                }
                printf("Sent request to list available content.\n");

                break;
            case 'Q':
                printf("Exiting P2P network.\n");
                close(udp_sock);
                close(tcp_sock);
                exit(0);
                break;
            case 'M':
                display_menu();
                break;
            default:
                printf("Invalid option. Please try again.\n");
                break;
        }

        
            
        }
    }
}

