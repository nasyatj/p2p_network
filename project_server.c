/* project_indexserver.c - main */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <arpa/inet.h>

#define MAX_DATA_SIZE 100

/*------------------------------------------------------------------------
 * main - Iterative UDP server for index service
 *------------------------------------------------------------------------
 */
struct pdu {
		char type;
		char data[MAX_DATA_SIZE];
};  

struct file_info { // Also keep track of how many times the file has been sent and send the least sent for load balancing
    char filename[MAX_DATA_SIZE];
    char peer_name[MAX_DATA_SIZE];
    struct sockaddr_in host_address;
	int request_count;
};

struct node {
    struct file_info data;
    struct node *next;
};

// Function prototypes
struct node* add_file_info(struct node *head, struct file_info new_info, int sock, struct sockaddr_in *client_addr, socklen_t alen);
void send_error_pdu(int sock, struct sockaddr_in *client_addr, socklen_t alen, const char *error_message);
struct node* remove_file_info(struct node *head, const char *filename, const char *peer_name, struct sockaddr_in host_address);
void print_file_info_list(int sock, struct node *head, struct sockaddr_in *client_addr, socklen_t alen);
void free_file_info_list(struct node *head);

struct node* add_file_info(struct node *head, struct file_info new_info, int sock, struct sockaddr_in *client_addr, socklen_t alen) {
    // Search for matching file name and same peer name in the linked list
    struct node *current = head;
    while (current != NULL) {
        if (strcmp(current->data.filename, new_info.filename) == 0 && strcmp(current->data.peer_name, new_info.peer_name) == 0) {
            // Send error message to client using send_error_pdu
            send_error_pdu(sock, client_addr, alen, "Error: Host name already in use");
            return head; // Do not add the new entry
        }
        current = current->next;
    }

    // Allocate memory for the new node
    struct node *new_node = (struct node *)malloc(sizeof(struct node));
    if (new_node == NULL) {
        perror("Failed to allocate memory");
        return head;
    }

    // Initialize the new node
    new_node->data = new_info;
    new_node->next = head;

    return new_node;
}

// Requires the filename, peer name, and host address to match
struct node* remove_file_info(struct node *head, const char *filename, const char *peer_name, struct sockaddr_in host_address) {
    struct node *current = head;
    struct node *previous = NULL;

    while (current != NULL) {
        // Check if the current node's filename, peer name, and host address match the target values
        if (strcmp(current->data.filename, filename) == 0 &&
            strcmp(current->data.peer_name, peer_name) == 0 &&
            memcmp(&current->data.host_address, &host_address, sizeof(struct sockaddr_in)) == 0) {
            // If it's the head node
            if (previous == NULL) {
                head = current->next;
            } else {
                previous->next = current->next;
            }
            free(current);
            return head;
        }
        previous = current;
        current = current->next;
    }

    // If the filename was not found, return the original head
    return head;
}

void print_file_info_list(int sock, struct node *head, struct sockaddr_in *client_addr, socklen_t alen) {
    struct node *current = head;
    char buffer[MAX_DATA_SIZE * 3]; // Buffer to hold the formatted string
    struct pdu send_pdu; // PDU to send to the client

    while (current != NULL) {
        // Format the file_info data into a string
        snprintf(buffer, sizeof(buffer), "Filename: %s, Peer Name: %s, Host Address: %s\n",
                 current->data.filename,
                 current->data.peer_name,
                 inet_ntoa(current->data.host_address.sin_addr));

        // Create a PDU with type 'O' and the formatted string as the data field
        send_pdu.type = 'O';
        strncpy(send_pdu.data, buffer, sizeof(send_pdu.data) - 1);
        send_pdu.data[sizeof(send_pdu.data) - 1] = '\0'; // Ensure null-termination

        // Send the PDU to the client
        if (sendto(sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr *)client_addr, alen) < 0) {
            perror("sendto");
            return;
        }

        current = current->next;
    }
}

void free_file_info_list(struct node *head) {
    struct node *current = head;
    struct node *next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
}

void send_error_pdu(int sock, struct sockaddr_in *client_addr, socklen_t alen, const char *error_message) {
    struct pdu error_pdu;
    error_pdu.type = 'E';
    strncpy(error_pdu.data, error_message, sizeof(error_pdu.data) - 1);
    error_pdu.data[sizeof(error_pdu.data) - 1] = '\0'; // Ensure null-termination
    sendto(sock, &error_pdu, sizeof(error_pdu), 0, (struct sockaddr *)client_addr, alen);
}

int main(int argc, char *argv[])
{
	struct  sockaddr_in fsin;	/* the from address of a client	*/
	char	buf[MAX_DATA_SIZE];		/* "input" buffer; any size > 0	*/
	int		sock;			/* server socket		*/
	int		alen;			/* from-address length		*/
	struct  sockaddr_in sin; /* an Internet endpoint address         */
    int     s, type;        /* socket descriptor and socket type    */
	int 	port=3000;
	struct node *file_info_list = NULL; /* Initialize the head of the list */
	struct file_info new_info;
	FILE *file;

	/*Define PDU structure*/
	struct pdu recv_pdu, send_pdu;                                   

	switch(argc){
		case 1:
			break;
		case 2:
			port = atoi(argv[1]);
			break;
		default:
			fprintf(stderr, "Usage: %s [port]\n", argv[0]);
			exit(1);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
                                                                                                 
    /* Allocate a socket */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0){
		fprintf(stderr, "can't create socket\n");
		exit(1);
	}
                                                                                
    /* Bind the socket */
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		fprintf(stderr, "can't bind to %d port\n",port);
		listen(s, 5);	
	}
	alen = sizeof(fsin);

	/* Main loop to handle incoming requests */
	while (1) {
		printf("Waiting for requests\n");
		if(recvfrom(s, &recv_pdu, sizeof(recv_pdu), 0, (struct sockaddr *)&fsin, &alen) < 0){
			fprintf(stderr, "Error receiving request");
			continue; // Skip the rest of the loop due to error
		}
		printf("Received request\n");
		// Prepare the strings for extraction if needed
		char peer_name[11]; // 10 bytes for peer name + 1 for null terminator
		char file_name[11]; // 10 bytes for file name + 1 for null terminator

		switch(recv_pdu.type){
			case 'R':
				// Client is registering a file
				// Ensure the strings are null-terminated
				strncpy(peer_name, recv_pdu.data, 10);
				peer_name[10] = '\0';
				strncpy(file_name, recv_pdu.data + 10, 10);
				file_name[10] = '\0';

				// Print the extracted peer name and file name for debugging
				printf("Registering file: %s from peer: %s\n", file_name, peer_name);

				// Extract the sender IP address from fsin
				struct sockaddr_in sender_addr;
				memset(&sender_addr, 0, sizeof(sender_addr));
				sender_addr.sin_family = AF_INET;
				sender_addr.sin_addr = fsin.sin_addr;

				// Print the extracted sender IP address for debugging
				printf("Received sender IP: %s\n", inet_ntoa(sender_addr.sin_addr));

				// Print the raw data for debugging
				printf("Raw data: ");
				for (int i = 0; i < sizeof(recv_pdu.data); i++) {
					printf("%02x ", (unsigned char)recv_pdu.data[i]);
				}
				printf("\n");

				// Extract the sender file TCP port from recv_pdu.data (assuming it's stored after the file name)
				uint16_t sender_port;
				memcpy(&sender_port, recv_pdu.data + 20, sizeof(sender_port));
				sender_addr.sin_port = sender_port;

				// Print the extracted sender port for debugging
				printf("Received sender port: %u\n", ntohs(sender_port));

				// Create a new file_info structure
				struct file_info new_file_info;
				strncpy(new_file_info.filename, file_name, sizeof(new_file_info.filename) - 1);
				new_file_info.filename[sizeof(new_file_info.filename) - 1] = '\0';
				strncpy(new_file_info.peer_name, peer_name, sizeof(new_file_info.peer_name) - 1);
				new_file_info.peer_name[sizeof(new_file_info.peer_name) - 1] = '\0';
				new_file_info.host_address = sender_addr;
				new_file_info.request_count = 0; // Initialize request_count to 0

				// Add the new file_info to the linked list
				struct node *new_head = add_file_info(file_info_list, new_file_info, sock, &fsin, alen);

				if (new_head != file_info_list) {
					// Update the head of the list
					file_info_list = new_head;

					// Send a success PDU to the client
					send_pdu.type = 'A';
					strncpy(send_pdu.data, "File successfully registered", sizeof(send_pdu.data) - 1);
					send_pdu.data[sizeof(send_pdu.data) - 1] = '\0'; // Ensure null-termination
					sendto(sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr *)&fsin, alen);

					// Print acknowledgment sent message
    				printf("Acknowledgment sent: File successfully registered\n");
				} // Otherwise, an error PDU was already sent by add_file_info

				break;
			case 'S':
				// Client is searching for a file
				// Ensure the strings are null-terminated
				strncpy(peer_name, recv_pdu.data, 10);
				peer_name[10] = '\0';
				strncpy(file_name, recv_pdu.data + 10, 10);
				file_name[10] = '\0';

				// Print the extracted peer name and file name for debugging
				printf("Searching for file: %s Request from peer: %s\n", file_name, peer_name);

				// Find the file_info in the linked list with the lowest request_count
				struct node *current = file_info_list;
				struct node *found_node = NULL;
				int min_request_count = INT_MAX;

				// Iterate through the linked list to find the file with the lowest request_count
				// that matches the file_name and does not have the same peer_name as the requestor
				while (current != NULL) {
					printf("Checking node: filename=%s, peer_name=%s, request_count=%d\n", current->data.filename, current->data.peer_name, current->data.request_count);
					if (strcmp(current->data.filename, file_name) == 0 && strcmp(current->data.peer_name, peer_name) != 0) {
						if (current->data.request_count < min_request_count) {
							min_request_count = current->data.request_count;
							found_node = current;
						}
					}
					current = current->next;
				}

				if (found_node != NULL) {
					// File found, return the address associated with the file_info
					struct sockaddr_in *file_address = &found_node->data.host_address;
					printf("File found. Address: %s\n", inet_ntoa(file_address->sin_addr));

					// Increment the request_count for the found file
					found_node->data.request_count++;

					// Send the address back to the client
					send_pdu.type = 'S';
					snprintf(send_pdu.data, sizeof(send_pdu.data), "Address: %s", inet_ntoa(file_address->sin_addr));
					sendto(sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr *)&fsin, alen);
				} else {
					// File not found, send an error message to the client
					send_error_pdu(sock, &fsin, alen, "Error: File not found");
				}

				break;
			case 'T':
				// Client is deregistering a file
				// Ensure the strings are null-terminated
				strncpy(peer_name, recv_pdu.data, 10);
				peer_name[10] = '\0';
				strncpy(file_name, recv_pdu.data + 10, 10);
				file_name[10] = '\0';

				// Print the extracted peer name and file name for debugging
				printf("Deregistering file: %s from peer: %s\n", file_name, peer_name);

				// Remove the file_info from the linked list
				file_info_list = remove_file_info(file_info_list, file_name, peer_name, fsin);

				// Send a success PDU to the client
				send_pdu.type = 'A';
				strncpy(send_pdu.data, "File successfully deregistered", sizeof(send_pdu.data) - 1);
				send_pdu.data[sizeof(send_pdu.data) - 1] = '\0'; // Ensure null-termination
				if (sendto(sock, &send_pdu, sizeof(send_pdu), 0, (struct sockaddr *)&fsin, alen) < 0) {
					perror("sendto");
				} else {
					printf("Acknowledgment sent: File successfully deregistered\n");
				}

				break;
			case 'O':
				// Client is requesting the list of files
				printf("Client is requesting the list of files\n");
				print_file_info_list(sock, file_info_list, &fsin, alen);
				break;
			default:
				// Invalid PDU type
				printf("Error: Invalid PDU type\n");
				send_error_pdu(sock, &fsin, alen, "Error: Invalid PDU type");
				break;
		}
	}
	free_file_info_list(file_info_list);
}
