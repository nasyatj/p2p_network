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

//Registered sockets nodes
struct SocketNode {
    int socket;
    char peer_name[11];
    char file_name[11];
    int tcp_port;
    struct SocketNode *next;
};

//error function **DONE
void error_exit(const char *msg) {
    perror(msg);
    //exit(1);
}

//create tcp sockets and get socket port **DONE
int getsocketname(int *sd){
    socklen_t alen= sizeof (struct sockaddr_in);
    struct sockaddr_in reg_addr;

    *sd = socket(AF_INET, SOCK_STREAM, 0);
    reg_addr.sin_family = AF_INET;
    reg_addr.sin_port = htons(0); //requests TCP to choose unique port number for server
    reg_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(*sd, (struct sockaddr *)&reg_addr, sizeof(reg_addr));

    listen(*sd, 5);

    getsockname(*sd, (struct sockaddr *) &reg_addr, &alen);
    return ntohs(reg_addr.sin_port);
}

//handle client download (TCP) **DONE
int handle_client(int sd) 
{
	char	*bp;
	int 	n, bytes_to_read;
    struct pdu send_pdu, receive_pdu;
    FILE *file;

    //printf("Client connected, handling request...\n"); //testing

	// Read filename from the TCP connection
    n = read(sd, &receive_pdu, 20);
    if (n <= 0) {
        error_exit("Failed to read data from client");
    }

    //printf("Received PDU type: %c\n", receive_pdu.type); //testing
    //printf("Received PDU data: %s\n", receive_pdu.data); //testing

    char *filename = strtok(receive_pdu.data, " \n");
    printf("Valid data: %s\n", filename); //testing

	// Open the file
	int fd = open(filename, O_RDONLY);
	//char error_check_byte;
	if (fd < 0) {
		error_exit("Failed to open file");
	}

    memset(&send_pdu, 0, sizeof(send_pdu)); // Clear the struct

    if(receive_pdu.type == 'D'){
        printf("Client requested file: %s\n", filename); //testing

        //send pdu type
        send_pdu.type = 'C';
        if(write(sd, &send_pdu.type, sizeof(send_pdu.type)) != sizeof(send_pdu.type)){
            error_exit("Failed to send pdu type");
        }

        //send file content
        while ((n = read(fd, send_pdu.data, sizeof(send_pdu.data))) > 0) {
            if (write(sd, &send_pdu.data, n) != n) {
                error_exit("Failed to send file content");
                close(fd);
                close(sd);
                return -1;
            }
        }
    }

    // Close the file and the socket
    close(fd);
	close(sd);

	return(0);
}

//Register file **DONE
struct SocketNode* register_file(char* fileinfo, struct SocketNode** registered_tcp_sockets){
    int tcp_port, tcp_sock;

    //setup tcp socket for each new file
    tcp_port = getsocketname(&tcp_sock);
    //printf("TCP port for content hosting: %d\n", tcp_port); //testing


    struct SocketNode *new_node = malloc(sizeof(struct SocketNode));
    strncpy(new_node->peer_name, fileinfo, 10);
    new_node->peer_name[10] = '\0';
    strncpy(new_node->file_name, fileinfo + 10, 10);
    new_node->file_name[10] = '\0';
    new_node->socket = tcp_sock;
    new_node->tcp_port = tcp_port;
    new_node->next = *registered_tcp_sockets;
    *registered_tcp_sockets = new_node;
    
    return new_node;
}

//reset pdu char arrays **DONE
void resetPDUs(struct pdu *send_pdu, struct pdu *receive_pdu){
    memset(send_pdu->data, 0, sizeof(send_pdu->data));
    memset(receive_pdu->data, 0, sizeof(receive_pdu->data));
}

//request download from another server **DONE
char * request_download(struct pdu *send_pdu, int *udp_sock, struct sockaddr_in *sin, int alen, char *requested_file){
    char temp[20];
    send_pdu->type = 'S';

    //get peer name
    while (1) {
        printf("Enter your peer name:\n");

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
        if (fgets(requested_file, 20, stdin) != NULL) {
            requested_file[strcspn(requested_file, "\n")] = '\0'; //removes newline character
            if (strlen(requested_file) > 10) {
                printf("File name is too long. Please enter a name with a maximum of 10 characters.\n");
            } 
            else {
                snprintf(send_pdu->data + 10, 11, "%-10s", requested_file); //copy to send_pdu data
                break;
            }
        }
    }

    if(sendto(*udp_sock, send_pdu, sizeof(*send_pdu), 0, (struct sockaddr*)sin, alen) == -1){
        error_exit("Failed to send PDU");
    }

    //printf("Sent download request to index server.\n"); //testing
    printf("The file you have requested is %s.\n", requested_file); //testing
    return requested_file;
}

//delete node from linked list **DONE
void deleteNode(struct SocketNode** head, const char* target_peer_name, const char* target_file_name) {
    struct SocketNode* current = *head;
    struct SocketNode* previous = NULL;
    char buffer[11]; //temp buffer for peer name

    strncpy(buffer, target_peer_name, 10);

    // Traverse the list to find the target node
    while (current != NULL) {
        if (strcmp(current->peer_name, buffer) == 0 &&
            strcmp(current->file_name, target_file_name) == 0) {
            // Target node found
            if (previous == NULL) {
                // Target node is the head of the list
                *head = current->next;
            } else {
                // Bypass the target node
                previous->next = current->next;
            }

            // Free the target node's memory
            free(current);
            return;
        }

        // Move to the next node
        previous = current;
        current = current->next;
    }

    // Target node not found
    printf("Node with peer: %s and file: %s not found.\n", target_peer_name, target_file_name);
}

//deregister content **DONE
char* deregister_file(struct SocketNode** registered_tcp_sockets, struct pdu *send_pdu, struct pdu *receive_pdu, int *udp_sock, struct sockaddr_in *sin, int alen){
    char temp[11];

    //print registered sockets
    struct SocketNode *current = *registered_tcp_sockets;
    printf("Registered files:\n");
    while(current != NULL){
        printf("Peer name: %s\n", current->peer_name);
        printf("File name: %s\n", current->file_name);
        printf("TCP port: %d\n", current->tcp_port);
        printf("\n");
        current = current->next;
    }

    printf("Enter the peer name of the file you would like to deregister:\n");
    if (fgets(temp, sizeof(temp), stdin) != NULL) {
        temp[strcspn(temp, "\n")] = '\0'; // Remove newline character
        if (strlen(temp) > 10) {
            printf("Peer name is too long. Please enter a name with a maximum of 10 characters.\n");
        } else {
            snprintf(send_pdu->data, 11, "%-10s", temp);
        }
    }

    printf("Enter the name of the file you would like to deregister:\n");
    if (fgets(temp, sizeof(temp), stdin) != NULL) {
        temp[strcspn(temp, "\n")] = '\0'; // Remove newline character
        if (strlen(temp) > 10) {
            printf("File name is too long. Please enter a name with a maximum of 10 characters.\n");
        } else {
            snprintf(send_pdu->data + 10, 11, "%-10s", temp);
        }
    }

    send_pdu->type = 'T';
    if (sendto(*udp_sock, send_pdu, sizeof(*send_pdu), 0, (struct sockaddr*)sin, alen) == -1) {
        error_exit("Failed to send PDU");
    }

    return send_pdu->data;
}

//handle content download **DONE
void content_download(int sd, char* requested_file){
    int i;
    struct pdu receive_pdu, send_pdu;
    char *filename = strtok(requested_file, " \n");
    
    // Open local file to write received content
		int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644); 
		if (fd < 0) {
			error_exit("Failed to open file");
		}

        sleep(3); //delay to allow client to transmit
        memset(&receive_pdu, 0, sizeof(receive_pdu)); // Clear the struct

        if (read(sd, &receive_pdu.type, 1) != 1) {
            error_exit("Failed to read the PDU Type");
        }
        printf("Received PDU type: %c\n", receive_pdu.type); //testing

        switch(receive_pdu.type){
            case 'E':
                //Error
                if (read(sd, &receive_pdu.data, sizeof(receive_pdu.data)) != sizeof(receive_pdu.data)) {
                    error_exit("Failed to read the PDU Data");
                }
                printf("Error message from server: %s\n", receive_pdu.data);
                break;
            case 'C':
                //Receiving file content
                printf("Receiving file content...\n");
                while ((i = read(sd, receive_pdu.data, sizeof(receive_pdu.data))) > 0) {
                    write(fd, receive_pdu.data, i);
                }
                break;
        }
        printf("File received and saved as %s\n", filename); //testing

        // close file and socket
        close(fd);
        close(sd);
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
    int udp_sock, tcp_download_port, tcp_download_socket;
    struct pdu send_pdu, receive_pdu;
    struct sockaddr_in sin;
    int alen = sizeof(sin);
    struct hostent	*phe, *hp;
    int port = 3000;
    char *host = "localhost";
    fd_set afds, rfds;
    struct SocketNode *registered_tcp_sockets = NULL, *add_tcp_sock;
    char requested_file[11];


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
    
    

    printf("Welcome to the P2P network.\n");
    display_menu();

    //main loop
    while(1){

        //Select attributes initialization
        FD_ZERO(&afds);
        FD_SET(udp_sock, &afds);  // Add UDP socket
        FD_SET(0, &afds);         // Add stdin
        struct SocketNode *current = registered_tcp_sockets;
        while(current != NULL){
            FD_SET(current->socket, &afds); // Add registered TCP sockets to select
            current = current->next;
        }
        memcpy(&rfds, &afds, sizeof(rfds));

        if (select(FD_SETSIZE, &rfds, NULL, NULL, NULL) < 0) {
            error_exit("Select error");
        }

        //printf("Checking for udp connections...\n"); //testing
        //check incoming udp packets
        if(FD_ISSET(udp_sock, &rfds)){
            resetPDUs(&send_pdu, &receive_pdu); //clear pdus

            if (recvfrom(udp_sock, &receive_pdu, sizeof(receive_pdu), 0, (struct sockaddr *)&sin, &alen) == -1) {
                   error_exit("Failed to receive response");
                }

            char rtype = receive_pdu.type;
            char temp[11], temp_host[11];
            char *content_server_host;
            int content_server_port, content_udp_sock;

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
                    //recieve content download information
                    strncpy(temp_host, receive_pdu.data, 10);
                    temp_host[10] = '\0';
                    content_server_host = temp_host;

				    strncpy(temp, receive_pdu.data + 10, 10);
				    temp[10] = '\0';
                    content_server_port = atoi(temp);

                    //printf("Content server host: %s\n", content_server_host); //testing
                    //printf("Content server port: %d\n", content_server_port); //testing

                    //trigger download response from content server
                    //clear pdu char arrays
                    resetPDUs(&send_pdu, &receive_pdu);

                    //setup tcp port for content downloading
                    struct sockaddr_in reg_addr;

                    tcp_download_socket = socket(AF_INET, SOCK_STREAM, 0);
                    if (tcp_download_socket < 0) {
                        error_exit("Failed to create TCP socket");
                    }

                    memset(&reg_addr, 0, sizeof(reg_addr));
                    reg_addr.sin_family = AF_INET;
                    reg_addr.sin_port = htons(content_server_port);
                    
                    //printf("attempting to connect to %s...\n", content_server_host); //testing
                    if (hp = gethostbyname(content_server_host)) {
                        bcopy(hp->h_addr, (char *)&reg_addr.sin_addr, hp->h_length);
                    } else if (inet_aton(content_server_host, (struct in_addr *)&reg_addr.sin_addr)) {
                        error_exit("Can't get server's address");
                    }

                    //connect to content server
                    if (connect(tcp_download_socket, (struct sockaddr *)&reg_addr, sizeof(reg_addr)) == -1) {
                        error_exit("Failed to connect to content server");
                    }
                    else{
                        printf("Connected to content server.\n"); //testing
                    }

                    //send requested file name to content server
                    send_pdu.type = 'D';
                    snprintf(send_pdu.data, 11, "%-10s", requested_file);
                    if (write(tcp_download_socket, &send_pdu.type, sizeof(send_pdu.type)) != sizeof(send_pdu.type)) {
                        error_exit("Failed to send PDU type");
                    }
                    if (write(tcp_download_socket, &send_pdu.data, sizeof(send_pdu.data)) != sizeof(send_pdu.data)) {
                        error_exit("Failed to send PDU data");
                    }

                
                    //content downlaod
                    content_download(tcp_download_socket, requested_file);

                    // printf("Received PDU type: %c\n", receive_pdu.type); //testing
                    // printf("Received PDU data: %s\n", receive_pdu.data); //testing

                    //register new file with index server
                    struct SocketNode* new_tcp_sock;
                    char temp[20]; 
                    memset(&send_pdu, 0, sizeof(send_pdu)); // Clear the struct
                    send_pdu.type = 'R';

                    //copy data to register file with index server
                    while (1) {
                        printf("Enter the peer name you would like to assign to this file:\n");

                        if (fgets(temp, sizeof(temp), stdin) != NULL) {
                            temp[strcspn(temp, "\n")] = '\0';
                            if (strlen(temp) > 10) {
                                printf("Peer name is too long. Please enter a name with a maximum of 10 characters.\n");
                            } else {
                                snprintf(send_pdu.data, 11, "%-10s", temp);
                                break;
                            }
                        }
                    }
                    snprintf(send_pdu.data + 10, 11, "%-10s", requested_file);
                    new_tcp_sock = register_file(send_pdu.data, &registered_tcp_sockets);
                    snprintf(send_pdu.data + 20, 11, "%-10d", new_tcp_sock->tcp_port);

                    printf("registering to index server: %s\n", send_pdu.data); //testing

                    if (sendto(udp_sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr*)&sin, alen) == -1) { 
                        error_exit("Failed to send PDU");
                    }
                    else{
                        printf("Sent registration request to index server.\n");
                    }

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

        //printf("Checking for tcp connections...\n"); //testing
        // Check if there’s an incoming TCP connection (handle client content download)
        current = registered_tcp_sockets;
        while(current != NULL){
            //printf("checking tcp connection on socket: %d\n", current->socket); // testing
            if (FD_ISSET(current->socket, &rfds)) {
                //printf("Incoming TCP connection...\n"); //testing
                struct sockaddr_in client;
                int client_len = sizeof(client);
                int new_tcp_sock;

                new_tcp_sock = accept(current->socket, (struct sockaddr *)&client, &client_len);
                if (new_tcp_sock < 0) {
                    error_exit("Failed to accept client connection");
                }

                switch(fork()){
                    case 0:
                        close(current->socket);
                        handle_client(new_tcp_sock);
                        exit(0);
                    default:
                        close(new_tcp_sock);
                        break;
                    case -1:
                        error_exit("fork: error");
                }

                break;
            }
            current = current->next;
        }

        //printf("Checking for user input...\n"); //testing
        // Check stdin for user input
        if (FD_ISSET(0, &rfds)) {
            
            char option;
            option = getchar();
            while (getchar() != '\n');
            char temp[20];

            switch(option){
            case 'R':
                resetPDUs(&send_pdu, &receive_pdu);
                
                send_pdu.type = 'R';
                
                //get peer name
                while (1) {
                    printf("Enter the peer name you would like to assign to this file:\n");

                    if (fgets(temp, sizeof(temp), stdin) != NULL) {
                        temp[strcspn(temp, "\n")] = '\0';
                        if (strlen(temp) > 10) {
                            printf("Peer name is too long. Please enter a name with a maximum of 10 characters.\n");
                        } else {
                            snprintf(send_pdu.data, 11, "%-10s", temp);
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
                            snprintf(send_pdu.data + 10, 11, "%-10s", temp);
                            break;
                        }
                    }
                }  
                
                //get new tcp socket connection
                add_tcp_sock = register_file(send_pdu.data, &registered_tcp_sockets);
                
                snprintf(send_pdu.data + 20, 11, "%-10d", add_tcp_sock->tcp_port);

                if (sendto(udp_sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr*)&sin, alen) == -1) {
                   error_exit("Failed to send PDU");
                }
                //printf("Sent registration request to index server.\n"); //testing

                break;

            case 'D':
                resetPDUs(&send_pdu, &receive_pdu);
                
                //request download from another server
                request_download(&send_pdu, &udp_sock, &sin, alen, &requested_file);
                printf("Requested file: %s\n", requested_file); //testing

                break;
            case 'T':
                resetPDUs(&send_pdu, &receive_pdu);
                char *deregistered_file;

                //if registered content exists, deregister content
                if(registered_tcp_sockets == NULL){
                    printf("No content to deregister.\n");
                    break;
                }
                else{
                    deregistered_file = deregister_file(&registered_tcp_sockets, &send_pdu, &receive_pdu, &udp_sock, &sin, alen);
                }

                deleteNode(&registered_tcp_sockets, deregistered_file, deregistered_file + 10);

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
                resetPDUs(&send_pdu, &receive_pdu);

                //close registered tcp sockets
                current = registered_tcp_sockets;
                while(current != NULL){
                    //deregister all files from index server
                    resetPDUs(&send_pdu, &receive_pdu);

                    send_pdu.type = 'T';
                    snprintf(send_pdu.data, 11, "%-10s", current->peer_name);
                    snprintf(send_pdu.data + 10, 11, "%-10s", current->file_name);

                    if (sendto(udp_sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr*)&sin, alen) == -1) {
                        error_exit("Failed to send PDU");
                    }

                    if (recvfrom(udp_sock, &receive_pdu, sizeof(receive_pdu), 0, (struct sockaddr *)&sin, &alen) == -1) {
                        error_exit("Failed to receive response");
                    }
                    printf("%s\n", receive_pdu.data);

                    //close registered tcp sockets
                    close(current->socket);
                    current = current->next;
                }

                close(udp_sock);
                close(tcp_download_socket);
                printf("Exiting P2P network.\n");
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

