#define _POSIX_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#define h_addr h_addr_list[0]

int recieveMessage(int sock, char* msgBuf, int bufSize);
int sendMessage(int sock, char* msg, int msgSize);
int selectOperation(int sock, struct timeval);
int resetSock(int* sock, struct sockaddr_in, struct timeval);
char* find(int sock, int max, int cur, char* string);
int main(int argc, char **argv){
	char *hostName;
	char* string;
	int sock;
	char buf[512];
	struct sockaddr_in server;
	struct sockaddr_in local;
	socklen_t addrlen = sizeof(local);
	struct hostent *h;

	int portNum = 1025, finalPort = 0;
	unsigned short clientPort = 0;
	int i = 0, retVal = 0, num = 0, skip = 0;
	struct timeval timeVal;
	struct timeval timeOut;
	timeVal.tv_sec = 0;
	timeVal.tv_usec = 0;

	timeOut.tv_sec = 3;
	timeOut.tv_usec = 0;

	time_t curTime;
	char softwareVersion = 0;
	char serverIdentification[7];
	char identifcationString[7] = {67, 65, 80, 83, 32, -1, 0};
	char clientIntroduction[5] = {67, 65, 80, 83, 0};

	int found = 0;
	int err = 0;

	if(argc < 2){
		exit(-1);
	}
	memset(serverIdentification, 0, 7);
	/*Get IP / symbolic hostname */
	hostName = argv[1];

	server.sin_family = (short) AF_INET;

	/*Get server information*/
	h = gethostbyname(hostName);
	if(h == NULL){
		exit(-2);
	}
	/*Check that server information was valid*/
	if(h == NULL){
		snprintf(buf, 512, "Host error\n");
		write(1, buf, strlen(buf));
		exit(-1);
	}

	bcopy((char *)h->h_addr, (char *)&server.sin_addr.s_addr, h->h_length);
	/*Look through each port between 1025 and 65536*/
	for(portNum = 1025; portNum <= 65536; portNum++){
		skip = 0;
		printf("checking port %d\n", portNum);
		server.sin_port = htons(portNum);
		/*connect to server*/		
		/*Create Socket*/
		err = resetSock(&sock, server, timeOut);
		if(err < 0){
			skip = 1;
		}
		/*Call Select*/
		/*Check if port will allow read*/
		if(skip != 1){
			retVal = selectOperation(sock, timeVal);
			if(retVal < 1){
				skip = 1;
			}
			else if(retVal > 0){
				/*can operate on socket*/
				/*Send introduction message*/
				num = sendMessage(sock, clientIntroduction, 5);
				if(num < 0){
					/*send failed*/
					skip = 1;
				}
			}
		}
		/*recieve return message*/
		if(skip != 1){
			retVal = recieveMessage(sock, serverIdentification, 7);
			if(retVal <= 0){
				skip = 1;
			}
		}
		/*Verify response*/
		if(skip != 1){
			for(i = 0; i < 5; i++){
				if(identifcationString[0] != serverIdentification[0]){
					skip = 1;
				}
			}

			/*get software version value*/
			/*won't work with this
			if(identifcationString[5] < 48 || identifcationString[5] > 57){
				skip = 1;
			}
			else{
			}*/
			softwareVersion = serverIdentification[5];
			if(identifcationString[6] != 0){
				skip = 1;
			}
		}
		if(skip != 1){
			found = 1;
			break;
		}


		/*close socket*/
		close(sock);
	}

	if(found == 1){
		/*find FOUND value*/
		getsockname(sock, (struct sockaddr *)&local, &addrlen);
		clientPort = local.sin_port;
		finalPort = portNum;
		curTime = time(NULL);
		string = find(sock, 3, 0, "");
		printf("%s\nClient: %d\nServer: %d\nTime:%s\nSoftware Version:%c\n", 
			string,
			clientPort, finalPort,
			asctime(gmtime(&curTime))
			,softwareVersion);
		free(string);
		exit(0);

	}
	else{
		exit(0);
	}
}
char* find(int sock, int max, int cur, char* string){
	char* returnedString = NULL;
	char* newString;
	char msg[10];
	int num1 = 0, num2 = 0;
	memset(msg, 0, 10);
	/*allocate space form string*/
	newString = malloc(cur + 2); //cur is 0 indexed, and need byte for /0
	if(newString == NULL){
		exit(-1);
	}
	/*copy string data to new string so as to not change previous strings*/
	memset(newString, 0, cur + 2);
	strncpy(newString, string, cur);

	for(int i = 32; i <= 126; i++){
		if(returnedString != NULL){
			free(newString);
			return returnedString;
		}
		else{
			memset(msg, 0, 10);
			newString[cur] = i;

			/*Send string to server*/
			num1 = send(sock, newString, cur + 2, 0);
			if(num1 < 0){
				free(newString);
				printf("Send: %s\n", strerror(errno));
				exit(-1);
			}
			/*Server response*/
			num2 = recv(sock, msg, 10, 0);
			if(num2 < 0){
				printf("Recv: %s\n", strerror(errno));
				exit(-1);
			}
			else{
				printf("%s (%d bytes)changed to %s (%d bytes)\n", newString, num1, msg, num2);
			}
		

			/*compare strings*/
			if(strncmp(msg, "Found", strlen("Found")) == 0){
				return newString;
			}
			else if(cur < max){
				returnedString = find(sock, max, cur + 1, newString);
			}
			else if(i >= 122){
				free(newString);
				return NULL;
			}
		}
	}
	if(returnedString != NULL){
		if(strncmp(msg, "Found", strlen("Found")) == 0 &&
				strlen("Found") == strlen(returnedString)){				
			return newString;
		}
	}
	free(newString);
	return NULL;

}
int resetSock(int* sock, struct sockaddr_in server, struct timeval timeVal){
	int err = 0;
	*sock = socket(AF_INET, SOCK_STREAM, 0);	
	if(*sock < 0){
		printf("%d: %s\n", errno, strerror(errno));
		return -1;
	}
	err = connect(*sock, (struct sockaddr *)&server, sizeof(server));
	if(err < 0){
		printf("%d: %s\n", errno, strerror(errno));;
		return -1;
	}
	err = setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&timeVal, sizeof(struct timeval));
	if(err < 0){
		printf("%d: %s\n", errno, strerror(errno));
		return -1;
	}
	return 1;
}

int selectOperation(int sock, struct timeval timeVal){
	fd_set writeSet, readSet;
	int err = 0;

	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_SET(sock, &readSet);
	FD_SET(sock, &writeSet);
	/*Check if port will allow read*/
	err = select(sock + 1, &readSet, &writeSet, NULL, &timeVal);
	if(err < 0){
		printf("%d: %s\n", errno, strerror(errno));
		return -1;
	}
	return err;
}
int sendMessage(int sock, char* msg, int msgSize){
	int num = 0;

	/*Send message on socket*/
	num = send(sock, msg, msgSize, 0);
	if(num != msgSize){
		/*Problem has occured*/
		if(num < 0){
			printf("%d: %s\n", errno, strerror(errno));
		}
		/*return error value*/
		return -1;
	}

	/*return # of bytes sent*/
	return num;
}

int recieveMessage(int sock, char* msgBuf, int bufSize){
	int err = 0;
	err = recv(sock, msgBuf, bufSize, 0);
	if(err < 0){
		if(errno == 9){
			sleep(0); //do nothing
		}
		if(errno == EAGAIN){
			err = recv(sock, msgBuf, bufSize, 0);
		}
		if(err < 1){
			printf("%d: %s\n", errno, strerror(errno));
			/*return negative errno*/
			err = errno * -1;
			return err;
		}
	}

	return err;

}