#include <string>
#include <iostream>

#include "..\nanomsg\nn.h"
#include "..\nanomsg\reqrep.h"
#include "..\nanomsg\utils\win.h"

int main(int argc, char* argv[]){
	int rc;
	int workerSock;
	char request[4096];
	char sendData[] = "HERE'S SOME DATA!!!";
	int nSendDataLen = strlen(sendData);

	char address[] = "tcp://127.0.0.1:80";

	workerSock = nn_socket(AF_SP, NN_REQ);
	//assert(workerSock >= 0);
	if (workerSock < 0){
		std::cerr << "nn_socket failure." << std::endl;
	}
	rc = nn_connect(workerSock, address);
	//assert(workerSock >= 0);
	if (rc < 0){
		std::cerr << "nn_connect failure." << std::endl;
	}

	int linger = 1000;
	nn_setsockopt(workerSock, NN_SOL_SOCKET, NN_SNDTIMEO, &linger, sizeof(linger));
	nn_setsockopt(workerSock, NN_SOL_SOCKET, NN_RCVTIMEO, &linger, sizeof(linger));

	int i = 0;
	while (1)
	{
		std::cout << "Press a key to send...";
		getchar();
		std::cout << "Sending " << i << " on " << address << std::endl;
		rc = nn_send(workerSock, sendData, nSendDataLen + 1, 0);
		//assert(rc >= 0);
		if (rc < 0){
			std::cerr << "nn_send failure." << std::endl;
		}

		request[0] = '\0';

		rc = nn_recv(workerSock, request, sizeof(request), 0);
		std::cout << "got:" << request << std::endl;
		//assert(rc >= 0);
		if (rc < 0){
			std::cerr << "nn_recv failure." << std::endl;
		}

		i++;
	}
}