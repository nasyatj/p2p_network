#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define INDEX_SERVER_IP "127.0.0.1"   // IP address of index server
#define INDEX_SERVER_PORT 9000        // Port of index server for UDP communication
#define MAX_PDU_SIZE 100              // Maximum PDU data size
#define PEER_NAME "Peer1"             // Peer name
#define CONTENT_NAME "File1"          // Content name

// Define PDU types
#define PDU_TYPE_REGISTER 'R'
#define PDU_TYPE_ACK 'A'
#define PDU_TYPE_ERROR 'E'

// Helper function to create TCP socket, bind it, and return assigned port
int getsocketname(int sd, int psd){
    socklen_t alen;
    struct sockaddr_in reg_addr;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    reg_addr.sin_family = AF_INET;
    reg_addr.sin_port = htons(0); // Requests TCP to choose unique port number
    reg_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sd, (struct sockaddr *)&reg_addr, sizeof(reg_addr));

    alen = sizeof(struct sockaddr_in);
    getsockname(psd, (struct sockaddr *) &reg_addr, &alen);
    return ntohs(reg_addr.sin_port);
}

void error_exit(const char *msg) {
    perror(msg);
    exit(1);
}

int main() {
    int udp_sock, tcp_sock = 0;
    struct sockaddr_in udp_server_addr;
    char pdu[MAX_PDU_SIZE];
    char response[2];   // To hold response type (A-type or E-type)

    // Step 1: Create TCP socket and get assigned port using helper function
    int tcp_port = getsocketname(tcp_sock, tcp_sock);
    if (tcp_port < 0) {
        error_exit("Failed to create TCP socket and get port");
    }
    printf("Assigned TCP port: %d\n", tcp_port);

    // Step 2: Create UDP socket for communication with index server
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        error_exit("UDP socket creation failed");
    }

    // Set up UDP server address (index server)
    memset(&udp_server_addr, 0, sizeof(udp_server_addr));
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(INDEX_SERVER_PORT);
    if (inet_pton(AF_INET, INDEX_SERVER_IP, &udp_server_addr.sin_addr) <= 0) {
        error_exit("Invalid index server IP address");
    }

    // Step 3: Prepare R-type PDU (Peer Name, Content Name, TCP Port)
    snprintf(pdu, MAX_PDU_SIZE, "%c%-10s%-10s%05d", PDU_TYPE_REGISTER, PEER_NAME, CONTENT_NAME, tcp_port);

    // Step 4: Send PDU to index server
    if (sendto(udp_sock, pdu, strlen(pdu), 0, (struct sockaddr*)&udp_server_addr, sizeof(udp_server_addr)) < 0) {
        error_exit("Failed to send PDU");
    }
    printf("Sent registration request to index server.\n");

    // Step 5: Receive response from index server
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    if (recvfrom(udp_sock, response, sizeof(response), 0, (struct sockaddr*)&from_addr, &from_len) < 0) {
        error_exit("Failed to receive response");
    }

    // Step 6: Handle response
    if (response[0] == PDU_TYPE_ACK) {
        printf("Registration successful.\n");
    } else if (response[0] == PDU_TYPE_ERROR) {
        printf("Registration failed: Name conflict or error.\n");
    }
