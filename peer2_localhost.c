/* peer2 localhost version */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>  
#include <sys/select.h>                                                                          
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>                                                                            
#include <netdb.h>
#include <ctype.h>

#define MAX_PORTS 5
#define MAX_DATA_SIZE 100

void error_exit(const char *msg) {
    perror(msg);
    exit(1);
}

void resetPDUs(void *send_pdu, void *receive_pdu) {
    // Implementation of resetPDUs
}

int getsocketname(int *sd) {
    // Implementation of getsocketname
    return 0; // Example return value
}

void content_hosting(int *sd) {
    // Implementation of content_hosting
}

void register_file(void *send_pdu, int tcp_port) {
    // Implementation of register_file
}

struct pdu {
		char type;
		char data[MAX_DATA_SIZE];
};  

struct file_info {
    char filename[MAX_DATA_SIZE];
    char peer_name[MAX_DATA_SIZE];
    struct sockaddr_in host_address;
};

//reset pdu char arrays
void resetPDUs(struct pdu *send_pdu, struct pdu *receive_pdu){
    memset(send_pdu->data, 0, sizeof(send_pdu->data));
    memset(receive_pdu->data, 0, sizeof(receive_pdu->data));
}

/*------------------------------------------------------------------------
 * main - client managing user input and file server subprocesses
 *------------------------------------------------------------------------
 */
int main(int argc, char **argv)
{
    int socks[MAX_PORTS]; // used to store the sockets for the file servers
    int num_tcp_socks = 0;
    int udp_sock, tcp_port, sd, new_sd;
    struct pdu send_pdu, receive_pdu;
    struct sockaddr_in sin;
    struct sockaddr_in servers[MAX_PORTS]; // used to store the addresses of the file servers
    int alen = sizeof(sin);
    struct hostent	*phe;
    int port = 3000;
    const char *host = "localhost";
    fd_set rfds, afds;

	switch (argc) {
	case 1:
		break;
	case 2:
		port = atoi(argv[1]);
		break;
	default:
		fprintf(stderr, "usage: [host [port]]\n");
		exit(1);
	}

    // Prepare UDP Socket
	memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;                                                                
    sin.sin_port = htons(port);
                                                                                        
    /* Map host name to IP address, allowing for dotted decimal */
    if ( phe = gethostbyname(host) ){
            memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
    }
    else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
    fprintf(stderr, "Can't get host entry \n");
                                                                                
    /* Allocate a UDP socket */
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        error_exit("Failed to create UDP socket");
    }
	                                                                    
    /* Connect the UDP socket to the server */
    if (connect(udp_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    fprintf(stderr, "Can't connect to %s %s \n", host, "Time");

    /*------------------------------------------------------------------------------*/
    // Main loop to handle user input and file server subprocesses
	while(1){
        // Prompt user for input
        printf("Please select an option:\n");
        printf("Enter R to register a file for download.\n"
        "Enter D to download a file from the server.\n"
        "Enter S to search for a file.\n"
        "Enter T to deregister a file.\n"
        "Enter O to list the files available for download.\n"
        "Enter Q to quit.\n"
        );

        FD_ZERO(&afds);
        //FD_SET(socks, &afds); /* Listening on TCP sockets */ Need to iterate through socks to add to afds
        FD_SET(udp_sock, &afds); /* Listening on UDP socket */
        FD_SET(0, &afds); /* Listening on stdin */
        memcpy(&rfds, &afds, sizeof(rfds));
        select(FD_SETSIZE, &rfds, NULL, NULL, NULL)

        if(FD_ISSET(0, &rfds)){
            char option;
            n = read(0, &option, 1);

            if (n > 0) {
            option = toupper(option); // Convert to uppercase to handle both cases
            printf("Option selected: %c\n", option);

            switch (option) {
                case 'R':
                    resetPDUs(&send_pdu, &receive_pdu);
                    tcp_port = 0; // Reset TCP port

                    // TCP port setup
                    tcp_port = getsocketname(&sd);
                    if (tcp_port == -1) {
                        error_exit("Failed to create or bind socket"); // Handle error if port is -1
                    }
                    printf("TCP port returned from content setup: %d\n", tcp_port);

                    // Fork content hosting to child process
                    pid_t pid = fork();
                    if (pid == 0) {
                        printf("Fork successful\n");
                        content_hosting(&sd);
                    }

                    register_file(&send_pdu, tcp_port);

                    // Send content registration request to index server
                    printf("Send PDU type: %c\n", send_pdu.type); // Testing
                    printf("Send PDU data: %s\n", send_pdu.data); // Testing

                    if (sendto(udp_sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr*)&sin, alen) == -1) {
                        error_exit("Failed to send PDU");
                    }
                    printf("Sent registration request to index server.\n");
                    break;

                case 'S':
                    // Handle search for a file
                    break;

                case 'T':
                    // Handle deregister a file
                    break;

                case 'O':
                    // Handle list files available for download
                    break;

                case 'Q':
                    // Handle quit
                    exit(0);

                default:
                    printf("Invalid option selected.\n");
                    break;
                }
            } else {
                printf("Failed to read input.\n");
            }
        }

        // Check if there's input from any of the TCP sockets /* AI Generated code block, edit to fit the program */
        for (int i = 0; i < num_tcp_socks; i++) {
            if (FD_ISSET(socks[i], &rfds)) {
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int new_sd = accept(socks[i], (struct sockaddr *)&client, &client_len);
                if (new_sd < 0) {
                    perror("accept");
                    continue;
                }
                printf("Accepted connection on port %d\n", ntohs(sin.sin_port));
                // Handle the new connection (e.g., fork a child process or use a thread)
                close(new_sd);
            }
        }

        // Handle UDP socket input if needed
        if (FD_ISSET(udp_sock, &rfds)) {
            // Handle incoming data on the UDP socket
        }
	}

	return 0;
}

/*	reaper		*/
void	reaper(int sig)
{
	int	status;
	while(wait3(&status, WNOHANG, (struct rusage *)0) >= 0);
}
