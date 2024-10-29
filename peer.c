#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>                                                                            
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

int getsocketname(){
    int sd;
    socklen_t alen;
    struct sockaddr_in reg_addr;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    reg_addr.sin_family = AF_INET;
    reg_addr.sin_port = htons(0); //requests TCP to choose unique port number for server
    reg_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sd, (struct sockaddr *)&reg_addr, sizeof(reg_addr));

    alen = sizeof (struct sockaddr_in);
    getsockname(sd, (struct sockaddr *) &reg_addr, &alen);
    return ntohs(reg_addr.sin_port);
}

int main(int argc, char **argv){
    int udp_sock, tcp_sock, alen;
    struct sockaddr_in udp_server_addr, tcp_server_addr, receive_addr, sin;
    struct pdu send_pdu, receive_pdu;
    struct hostent	*phe;	/* pointer to host information entry	*/
    int port = 3000;
    char *host = "localhost";

    //clear pdu char arrays
    memset(send_pdu.data, 0, sizeof(send_pdu.data));
    memset(receive_pdu.data, 0, sizeof(receive_pdu.data));

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

    printf("Host: %s, Port: %d\n", host, port); //testing

    /*UDP AND TCP SOCKET SET UP*/
    /*--------------------------------------------------------------------------------------*/
    //set up TCP socket for content
    tcp_sock = getsocketname();
    if (tcp_sock < 0) {
        error_exit("Failed to create TCP socket");
    }
    //printf("Assigned TCP port: %d\n", tcp_sock); //testing

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
    
    alen = sizeof(sin);
    /*--------------------------------------------------------------------------------------*/

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
        printf("Option selected: %c\n", option);

        switch(option){
            case 'R':
                //register_content();
                break;
            case 'D':
                //download_content();
                break;
            case 'S':
                //search_content();
                break;
            case 'T':
                //deregister_content();
                break;
            case 'O':
                //list_content();
                break;
            case 'Q':
                printf("Exiting P2P network.\n");
                exit(0);
                break;
            default:
                printf("Invalid option. Please try again.\n");
                break;
        }
    }
    

    //user input for PDU type
    printf("Enter R to register a file for download:\n"); 
    send_pdu.type = getchar();  // Read a single character from input
    // Consume the newline character left in the input buffer by pressing Enter
    while (getchar() != '\n');
    printf("PDU type is: %c\n", send_pdu.type);

    //user input for registering content
    //**prompt user for different info if over 10 bytes
    char temp[20];  // Temporary buffer for user input
    //get peer name
    printf("Enter the peer name you would like to assign to this file:\n");
    if (fgets(temp, sizeof(temp), stdin) != NULL) {
        // Remove newline if present
        temp[strcspn(temp, "\n")] = '\0';
        // Copy to send_pdu.data, padded to exactly 10 bytes
        snprintf(send_pdu.data, 11, "%-10s", temp);
    }

    //get file name
    printf("Enter the name of the file you would like to register:\n");
    if (fgets(temp, sizeof(temp), stdin) != NULL) {
        temp[strcspn(temp, "\n")] = '\0';
        snprintf(send_pdu.data + 10, 11, "%-10s", temp);
    }

    //get port number
    snprintf(send_pdu.data + 20, 6, "%-10d", tcp_sock);

    // testing
    printf("Peer Name: '%.10s'\n", send_pdu.data);
    printf("File Name: '%.10s'\n", send_pdu.data + 10);
    printf("Port Number: '%.10s'\n", send_pdu.data + 20);

    //connect to UDP socket
    /* Connect the socket */
        if (connect(udp_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		fprintf(stderr, "Can't connect to %s %s \n", host, "Time");

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sin.sin_addr), ip, INET_ADDRSTRLEN);
    printf("UDP server address: %s:%d\n", ip, ntohs(sin.sin_port)); //testing
    
    //send content registration request to index server
    //if (sendto(udp_sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr*)&sin, alen)) {
    //    error_exit("Failed to send PDU");
    //}
    write(udp_sock, &send_pdu, sizeof(send_pdu));
    printf("Sent registration request to index server.\n");

    
    //receive response from index server - possibly change to read
    //if (recvfrom(udp_sock, &receive_pdu, sizeof(receive_pdu), 0, (struct sockaddr *)&sin, &alen) < 0) {
    //    error_exit("Failed to receive response");
    //}

    //handle response
    //if (receive_pdu.type == 'A') {
    //    printf("Registration successful.\n");
    //} else if (receive_pdu.type == 'E') {
    //    printf("Registration failed: Name conflict or error.\n");
    //}
    
}

