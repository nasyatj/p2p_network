#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>

#define PORT 9002
#define BUFLEN 100
#define PEERLENGTH 20
#define CONTENTLENGTH 20
#define ADDRLENGTH 16
#define PORTLENGTH 6
#define MAXCONTENT 100

//PDU Structure
typedef struct udpPDU
{
	char type;
	char data[BUFLEN];
} pdu;

//Function Prototyping:
//Print Options available on the network
void printOptions();
//Setting up all local variables for first time registration
void initialization(struct udpPDU* sendData, struct udpPDU* response, bool * registrationStatus, char peerName[], char address[], char contentList[][CONTENTLENGTH], char port[], int * numContent);
//Regsitration function
void registration(struct udpPDU* sendData, struct udpPDU* response, bool * registrationStatus, bool * finishSocket, char peerName[], char address[], char contentList[][CONTENTLENGTH], char port[], int * numContent, ssize_t * recvLength, struct sockaddr_in* server_address, socklen_t * addrSize, int network_socket);
//Reset the send data and response data PDUs
void resetStructs(struct udpPDU * sendData, struct udpPDU * response);
//DeRegistration Function
void deRegistration(struct udpPDU* sendData, struct udpPDU* response, bool * registrationStatus, char peerName[], char address[], char contentList[][CONTENTLENGTH], char port[], int * numContent, ssize_t * recvLength, struct sockaddr_in* server_address, socklen_t * addrSize, int network_socket);
//Content Hosting Function
void hosting (char address[], char port[]);
//Content Download Function
bool download(char contentTemp[], char clientPort[], char contentAddr[]);

int main(int argc, char **argv)
{
	
	bool registrationStatus = false, finishSocket = false;
	char peerName[PEERLENGTH];
	char contentList [MAXCONTENT][CONTENTLENGTH];
	char address [ADDRLENGTH];
	char port [PORTLENGTH];
	int numContent;
	pdu sendData, response;
	char choice;
	bool check;
	pid_t pid;
	
	initialization(&sendData, &response, &registrationStatus, peerName, address, contentList, port, &numContent);
	
	socklen_t addrSize;
	
	//Get server address from command line parameters
	char host [100];
	strcpy(host, argv[1]);
	
	//UDP SOCKET
	//Define the host
	struct hostent *server;
	//Specify an address for the socket
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address)); // set 0's
	server_address.sin_family = AF_INET; // Specifies that this is an internet server
	server_address.sin_port = htons(PORT); // Port number of the server
	server = gethostbyname (host);
	bcopy((char *)server -> h_addr, (char *) &server_address.sin_addr.s_addr, server -> h_length);
	addrSize = sizeof(server_address);
	ssize_t recvLength;
	
	//Create a UDP socket
	int network_socket;
	network_socket = socket(PF_INET, SOCK_DGRAM, 0); // SOCK_DGRAM for UDP, AF_INET for internet server IPv4
	
	if (network_socket < 0)
	{
		printf("There was an error creating the socket...");
		exit(1);
	}
	
	printf("You have entered the P2P Network!\n");
	printOptions();
	
	//Start infinite loop
	while (1)
	{
		//Perform Content Consistency check
		int i;
		for (i = 0 ; i < numContent ; i++)
		{
			resetStructs(&sendData, &response);
			sendData.type = 'H';
			strcpy(sendData.data, contentList[i]);
			sendto(network_socket, (struct udpPDU *) &sendData, sizeof(sendData), 0, (struct sockaddr*) &server_address, addrSize);
			recvLength = recvfrom(network_socket, (struct udpPDU*) &response, sizeof(response), 0, (struct sockaddr*) &server_address, &addrSize);
			if (response.type != 'H')
			{
				if (i == (MAXCONTENT - 1))
				{
					memset(contentList[i], '\0', sizeof(contentList[i]));
					numContent = numContent - 1;
					break;
				}
				else
				{
					int j = i;
					for (j = i ; j < numContent ; j++)
					{
						memset(contentList[j], '\0', sizeof(contentList[j]));
						strcpy(contentList[j], contentList[j + 1]);
					}
					numContent = numContent - 1;
					memset(contentList[numContent], '\0', sizeof(contentList[numContent]));
					break;
				}
			}
		}
		//Get choide input from user
		choice = getchar();
		
		//Indicates registration request
		if (choice == 'R')
		{
			registration(&sendData, &response, &registrationStatus, &finishSocket, peerName, address, contentList, port, &numContent, &recvLength, (struct sockaddr_in *) &server_address, &addrSize, network_socket);
			if (finishSocket == true)
			{
				finishSocket = false;
				//Fork which spawns a second process to run hosting in the background
				pid = fork();
				if (pid == 0)
				{
					hosting(address, port);
				}
			}
			printOptions();
		}
		//Indicates Download Request
		else if (choice == 'S')
		{
			bool downloadStatus = false;
			resetStructs(&sendData, &response);
			check = false;
			char contentTemp[CONTENTLENGTH];
			int contLength;
			printf("Enter the name of the content you would like to download:\n");
			scanf("%s", contentTemp);
			contLength = strlen(contentTemp);
			contLength++;
			//20 Char max check
			if (strlen(contentTemp) >= 20)
			{
				printf("Error! Content name must be less than 20 characters!\nPlease Try Again...");
				check = true;
			}
			if (check == false)
			{
				//Send download request to the index server
				sendData.type = 'S';
				strcpy(sendData.data, contentTemp);
				sendto(network_socket, (struct udpPDU *) &sendData, sizeof(sendData), 0, (struct sockaddr*) &server_address, addrSize);
				recvLength = recvfrom(network_socket, (struct udpPDU*) &response, sizeof(response), 0, (struct sockaddr*) &server_address, &addrSize);
				if (response.type == 'S')
				{
					//Retrieve IP address and port from index server response
					char contentAddr [ADDRLENGTH];
					char clientPort [PORTLENGTH];
					int i = 0, j = 0;
					for (i = 0 ; i < ADDRLENGTH ; i++)
					{
						if (response.data[i] == '\0')
						{
							break;
						}
						contentAddr[i] = response.data[i];
					}
					contentAddr[i] = '\0';
					i++;
					for (i = i ; i < ADDRLENGTH + PORTLENGTH ; i++)
					{
						if (response.data[i] == '\0')
						{
							break;
						}
						clientPort[j] = response.data[i];
						j++;
					}
					clientPort[j] = '\0';
					//Call download function and pass IP address and Port number
					downloadStatus = download(contentTemp, clientPort, contentAddr);
					//If download successful trigeer registration of content with index server
					if (downloadStatus == true)
					{
						bool regAlready = false;
						for (i = 0 ; i < MAXCONTENT ; i++)
						{
							if (strcmp(contentTemp, contentList[i]) == 0)
							{
								regAlready = true;
								printf("You have already registered this content with the server");
								printf("If a re-registration is required, please de-register the content first, then re-register");
								break;
							}
						}
						if (regAlready == false)
						{
							resetStructs(&sendData, &response);
							sendData.type = 'T';
							sprintf(sendData.data, "Successful Download");
							sendto(network_socket, (struct udpPDU *) &sendData, sizeof(sendData), 0, (struct sockaddr*) &server_address, addrSize);
							strcpy(contentList[numContent], contentTemp);
							numContent++;
						}
						else
						{
							resetStructs(&sendData, &response);
							sendData.type = 'E';
							sprintf(sendData.data, "UnSuccessful Download");
							sendto(network_socket, (struct udpPDU *) &sendData, sizeof(sendData), 0, (struct sockaddr*) &server_address, addrSize);
						}
					}
				}
			}
			printOptions();
		}
		//Content listing request start
		else if (choice == 'O')
		{
			resetStructs(&sendData, &response);
			sendData.type = 'O';
			sendto(network_socket, (struct udpPDU *) &sendData, sizeof(sendData), 0, (const struct sockaddr*) &server_address, addrSize);
			response.type = 'O';
			printf("Available Files on the P2P Network:\n");
			//Begin loop receiving packets until final packet is reached
			while (response.type != 'F')
			{
				recvLength = recvfrom(network_socket, (struct udpPDU*) &response, sizeof(response), 0, (struct sockaddr*) &server_address, &addrSize);
				if (response.type == 'F' && response.data[0] == '\0')
				{
					printf("No Files available on the P2P Network...\n");
					break;
				}
				int i;
				char buffer [BUFLEN];
				memset(buffer, '\0', sizeof(buffer));
				//Print out names of content
				for (i = 0 ; i < BUFLEN ; i++)
				{
					if (response.data [i] == '\0')
					{
						buffer [i] = '\n';
						if (i == (BUFLEN - 1))
						{
							printf("%s", buffer);
							break;
						}
						if (response.data[i + 1] == '\0')
						{
							printf("%s", buffer);
							break;
						}
					}
					else
					{
						buffer [i] = response.data [i];
					}
				}
			}
			printOptions();
		}
		//Begin deregistration request
		else if (choice == 'T')
		{
			deRegistration(&sendData, &response, &registrationStatus, peerName, address, contentList, port, &numContent, &recvLength, (struct sockaddr_in *) &server_address, &addrSize, network_socket);
			printOptions();
		}
		//Being Quit request
		else if (choice == 'Q')
		{
			bool status = false;
			if (registrationStatus == false)
			{
				printf("Error! You are not registered with the P2P Network\n");
			}
			else
			{
				int i;
				resetStructs(&sendData, &response);
				//Send a series of T PDUs to deregister all content
				for (i = 0 ; i < numContent ; i++)
				{
					resetStructs(&sendData, &response);
					strcpy(sendData.data, contentList[i]);
					sendData.type = 'T';
					sendto(network_socket, (struct udpPDU *) &sendData, sizeof(sendData), 0, (struct sockaddr*) &server_address, addrSize);
					recvLength = recvfrom(network_socket, (struct udpPDU*) &response, sizeof(response), 0, (struct sockaddr*) &server_address, &addrSize);
					//Reset local content list
					if (response.type == 'T')
					{
						memset(contentList[i], '\0', sizeof(contentList[i]));
						printf("%s\n", response.data);
					}
					else
					{
						printf("%s\n", response.data);
						status = true;
					}
				}
				//If no error occured, reset all values and kill the hosting process
				if (status == false)
				{
					resetStructs(&sendData, &response);
					recvLength = recvfrom(network_socket, (struct udpPDU*) &response, sizeof(response), 0, (struct sockaddr*) &server_address, &addrSize);
					if (response.type == 'T')
					{
						printf("%s\n", response.data);
						initialization(&sendData, &response, &registrationStatus, peerName, address, contentList, port, &numContent);
						finishSocket = false;
						kill (pid, SIGKILL);
					}
				}
			}
			printOptions();
		}
	}
	
	return 0;
}

//Print all options for the network
void printOptions()
{
	printf("Your Options Are: \n");
	printf("R: Register content with the network\n");
	printf("D: Download a content from the network\n");
	printf("O: List available content on the network\n");
	printf("T: De-Register content\n");
	printf("Q: De-Register all content from the network and be removed from the Peer list\n");
}

//Function to download a piece of content from another peer
bool download(char contentTemp[], char clientPort[], char contentAddr[])
{
	pdu sendData, response;
	bool fileRecvdErrStatus = false;
	
	//TCP SOCKET
	//This is for downloading content
	struct hostent *contentServer;
	struct sockaddr_in contentServerAddress;
	memset(&contentServerAddress, 0, sizeof(contentServerAddress)); // set 0's
	contentServerAddress.sin_family = AF_INET;
	contentServerAddress.sin_port = htons(atoi(clientPort));
	contentServer = gethostbyname(contentAddr);
	bcopy((char *)contentServer -> h_addr, (char *) &contentServerAddress.sin_addr.s_addr, contentServer -> h_length);
	int downloadConnect;
	
	//TCP Download Socket
	int fileClientSocket;
	fileClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (fileClientSocket < 0)
	{
		printf("There was an error creating the socket...");
		exit(1);
	}
	downloadConnect = connect(fileClientSocket, (struct sockaddr*) &contentServerAddress, sizeof(contentServerAddress));
	if (downloadConnect == -1)
	{
		printf("There was an error making a connection to the remote socket \n");
	}
	memset(sendData.data, '\0', sizeof(sendData.data));
	sendData.type = 'D';
	strcpy(sendData.data, contentTemp);
	send(fileClientSocket, (struct udpPDU *) &sendData, sizeof(sendData), 0);
	ssize_t numBytes;
	memset(response.data, '\0', sizeof(response.data));
	int fp;
	//Create a file to write to
	fp = open(contentTemp, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
	while((numBytes = recv(fileClientSocket, (struct udpPDU *) &response, sizeof(response), 0)) > 0)
	{
		if (response.type == 'C' || response.type == 'F')
		{
			//Write to the file
			write(fp, response.data, sizeof(response.data));
			if (response.type == 'F')
			{
				break;
			}
		}
		else
		{
			printf("%s\n", response.data);
			fileRecvdErrStatus = true;
			break;
		}
	}
	//Close the file pointer and the TCP connection to the server peer
	close(fp);
	close(fileClientSocket);
	if (fileRecvdErrStatus == false)
	{
		printf("\nFile Successfully Received!\n");
		return true;
	}
	return false;
}

//Hosting function to be run by the child process
void hosting (char address[], char port[])
{
	printf("Content Hosting Running in the Background...\n");
	pdu sendInfo, resp;
	//TCP SOCKET
	//This is for hosting content
	struct sockaddr_in hosting_address;
	struct sockaddr_in client_address;
	char clientAddrReadable [ADDRLENGTH];
	socklen_t clientSize;
	memset(&hosting_address, 0, sizeof(hosting_address)); // set 0's
	memset(&client_address, 0, sizeof(client_address)); // set 0's
	hosting_address.sin_family = AF_INET; // Specifies that this is an internet server
	hosting_address.sin_port = htons(atoi(port)); // Port number of the server
	hosting_address.sin_addr.s_addr = htonl(INADDR_ANY);
	clientSize = sizeof(client_address);
	
	//TCP Hosting Socket
	int fileSocket;
	fileSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (fileSocket < 0)
	{
		printf("There was an error creating the socket...");
		exit(1);
	}
	
	int bindErr;
	bindErr = bind(fileSocket, (struct sockaddr*) &hosting_address, sizeof(hosting_address));
	if (bindErr == -1)
	{
		printf("Socket binding error\n");
		exit(1);
	}
	listen(fileSocket, 5);
	int clientSocket;
	while (1)
	{
		clientSocket = accept(fileSocket, (struct sockaddr*)&client_address, &clientSize);
		
		memset(resp.data, '\0', sizeof(resp.data));
		recv(clientSocket, &resp, sizeof(resp), 0);
		printf("\nIncoming Download Request: \n");
		inet_ntop(AF_INET, &client_address.sin_addr, (char *)clientAddrReadable, sizeof(clientAddrReadable));
		printf("Client IP: %s\n", clientAddrReadable);
		printf("Content Requested: %s\n", resp.data);
		memset(sendInfo.data, '\0', sizeof(sendInfo.data));
		int readFile, numBytesRead;
		//Open file for reading
		readFile = open(resp.data, O_RDONLY);
		if (readFile > 0)
		{
			while ((numBytesRead = read(readFile, sendInfo.data, sizeof(sendInfo.data))) > 0)
			{
				if (numBytesRead < 100)
				{
					sendInfo.type = 'F';
				}
				else
				{
					sendInfo.type = 'C';
				}
				//Send the file to the client 100 bytes at a time
				send (clientSocket, (struct udpPDU *)&sendInfo, sizeof(sendInfo), 0);
			}
			close (readFile);
			printf("\nContent Successfully Sent To Client!\n");
		}
		else
		{
			//if error occured
			sendInfo.type = 'E';
			strcpy(sendInfo.data, "Error! File could not be found...\0");
			send (clientSocket, (struct udpPDU *)&sendInfo, sizeof(sendInfo), 0);
			close (readFile);
			printf("\nRequest Failed! Content does not exist...\n");
		}
		close(clientSocket);
		printOptions();
	}
}

//Deregistration function
void deRegistration(struct udpPDU* sendData, struct udpPDU* response, bool * registrationStatus, char peerName[], char address[], char contentList[][CONTENTLENGTH], char port[], int * numContent, ssize_t * recvLength, struct sockaddr_in* server_address, socklen_t * addrSize, int network_socket)
{
	if (*registrationStatus == false)
	{
		printf("Error! You are not registered with the P2P Network\n");
		return;
	}
	char contentTemp[CONTENTLENGTH];
	bool found = false;
	int i;
	printf("Enter the name of the content you would like to de-register:\n");
	scanf("%s", contentTemp);
	if (strlen(contentTemp) >= 20)
	{
		printf("Error! Content name must be less than 20 characters!\nPlease Try Again...");
		return;
	}
	for (i = 0 ; i < MAXCONTENT ; i++)
	{
		if (strcmp(contentTemp, contentList[i]) == 0)
		{
			found = true;
			break;
		}
	}
	if (found == false)
	{
		printf("Error! You are not hosting this content on the P2P Network\n");
		return;
	}
	else
	{
		//Send T PDU to the index server with the content name
		resetStructs(sendData, response);
		sendData -> type = 'T';
		strcpy(sendData -> data, contentList[i]);
		sendto(network_socket, (struct udpPDU *) sendData, sizeof(*sendData), 0, (struct sockaddr*) server_address, *addrSize);
		*recvLength = recvfrom(network_socket, (struct udpPDU*) response, sizeof(*response), 0, (struct sockaddr*) server_address, addrSize);
		if (response -> type == 'T')
		{
			//if at max content
			if (i == (MAXCONTENT - 1))
			{
				memset(contentList[i], '\0', sizeof(*contentList[i]));
				*numContent = *numContent - 1;
			}
			else
			{
				int j = i;
				for (j = i ; j < *numContent ; j++)
				{
					memset(contentList[j], '\0', sizeof(*contentList[j]));
					strcpy(contentList[j], contentList[j + 1]);
				}
				*numContent = *numContent - 1;
				memset(contentList[*numContent], '\0', sizeof(*contentList[*numContent]));
			}
			printf("%s\n", response -> data);
			//If number of content is now 0, reset data as client has been quit automatically from the network
			if (*numContent == 0)
			{
				initialization(sendData, response, registrationStatus, peerName, address, contentList, port, numContent);
			}
		}
		else
		{
			printf("%s\n", response -> data);
		}
	}
}

//Registration function
void registration(struct udpPDU* sendData, struct udpPDU* response, bool * registrationStatus, bool * finishSocket, char peerName[], char address[], char contentList[][CONTENTLENGTH], char port[], int * numContent, ssize_t * recvLength, struct sockaddr_in* server_address, socklen_t * addrSize, int network_socket)
{
	int peerSize, contentSize, portSize;
	char contentTemp [CONTENTLENGTH];
	int i;
	resetStructs(sendData, response);
	//If first time registration
	if (*registrationStatus == false)
	{
		//Client Reg inputs
		bool portExit = false;
		printf("Enter Peer Name:\n");

		scanf("%s", peerName);
		if (strlen(peerName) >= 20)
		{
			printf("Error! Peer name must be less than 20 characters!\nPlease Try Again...");
			return;
		}
		printf("Enter File Name:\n");
		scanf("%s", contentTemp);
		if (strlen(contentTemp) >= 20)
		{
			printf("Error! Content name must be less than 20 characters!\nPlease Try Again...");
			return;
		}
		strcpy(contentList[*numContent], contentTemp);
		while (portExit == false)
		{
			printf("Enter Your Port Number:\n");
			scanf("%s", port);
			portExit = true;
			for (i = 0 ; i < strlen(port) ; i++)
			{
				if (((int) port [i] < 48 || (int) port[i] > 57) && (int) port[i] != 0)
				{
					printf("Error! Port number must be all numerical values, please try again...\n");
					portExit = false;
					memset(port, '\0', sizeof(*port));
					break;
				}
			}
		}
		peerSize = strlen(peerName);
		contentSize = strlen(contentList[*numContent]);
		portSize = strlen(port);
		peerSize++;
		contentSize++;
		portSize++;
		memcpy(sendData -> data, peerName, peerSize);
		memcpy(sendData -> data + peerSize, contentList[*numContent], contentSize);
		memcpy(sendData -> data + peerSize + contentSize, port, portSize);
		*numContent = *numContent + 1;
		sendData -> type = 'R';
		//Send R type PDU to the index server
		sendto(network_socket, (struct udpPDU *) sendData, sizeof(*sendData), 0, (struct sockaddr*) server_address, *addrSize);
		*recvLength = recvfrom(network_socket, (struct udpPDU*) response, sizeof(*response), 0, (struct sockaddr*) server_address, addrSize);
		printf("%s\n", response -> data);
		//If error occured
		if (response -> type != 'E')
		{
			*registrationStatus = true;
			*finishSocket = true;
		}
		//If not start data structures
		else
		{
			printf("%s\n", response -> data);
			memset(peerName, '\0', sizeof(*peerName));
			memset(port, '\0', sizeof(*port));
			*numContent = *numContent - 1;
			memset(contentList[*numContent], '\0', sizeof(*contentList[*numContent]));
		}
	}
	//If already registered, registering a second piece of content
	else
	{
		printf("Enter File Name:\n");
		scanf("%s", contentTemp);
		if (strlen(contentTemp) >= 20)
		{
			printf("Error! Content name is too long, keep it less than 20 chars!\nPlease Try Again...");
			return;
		}
		for (i = 0 ; i < MAXCONTENT ; i++)
		{
			if (strcmp(contentTemp, contentList[i]) == 0)
			{
				printf("Error, You have already registered this content with the server");
				printf("If a re-registration is required, please de-register the content first, then re-register");
				return;
			}
		}
		strcpy(contentList[*numContent], contentTemp);
		peerSize = strlen(peerName);
		contentSize = strlen(contentList[*numContent]);
		peerSize++;
		contentSize++;
		memcpy(sendData -> data, peerName, peerSize);
		memcpy(sendData -> data + peerSize, contentList[*numContent], contentSize);
		*numContent = *numContent + 1;
		sendData -> type = 'R';
		//Send R type PDU to the index server
		sendto(network_socket, (struct udpPDU *) sendData, sizeof(*sendData), 0, (const struct sockaddr*) server_address, *addrSize);
		*recvLength = recvfrom(network_socket, (struct udpPDU*) response, sizeof(*response), 0, (struct sockaddr*) server_address, addrSize);
		//If error occured
		if (response -> type == 'E')
		{
			printf("%s\n", response -> data);
			*numContent = *numContent - 1;
			memset(contentList[*numContent], '\0', sizeof(*contentList[*numContent]));
			return;
		}
		printf("%s\n", response -> data);
	}
	
}

//Set data structures to default values to prepare for registration
void initialization(struct udpPDU* sendData, struct udpPDU * response, bool * registrationStatus, char peerName[], char address[], char contentList[][CONTENTLENGTH], char port[], int * numContent)
{
	memset(sendData -> data, '\0', sizeof(sendData -> data));
	memset(response -> data, '\0', sizeof(response -> data));
	*registrationStatus = false;
	memset(peerName, '\0', sizeof(*peerName));
	memset(address, '\0', sizeof(*address));
	memset(port, '\0', sizeof(*port));
	int i;
	for (i = 0 ; i < MAXCONTENT ; i++)
	{
		memset(contentList[i], '\0', sizeof(*contentList[i]));
	}
	*numContent = 0;
}

//Reset send and response data structures used in transmission and receiving of data
void resetStructs(struct udpPDU * sendData, struct udpPDU * response)
{
	memset(sendData -> data, '\0', sizeof(sendData -> data));
	memset(response -> data, '\0', sizeof(response -> data));
}
