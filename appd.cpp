#include <string>

#include <sstream>
#include <iostream>
#include <fstream>

#include <vector>

#include <atomic>

#include <chrono>
#include <thread>
//std::this_thread::sleep_for(std::chrono::milliseconds(ms));

#include "..\nanomsg\nn.h"
#include "..\nanomsg\reqrep.h"
#include "..\nanomsg\utils\win.h"

const char* APPD_ADDRESS;
std::atomic<bool> IsMaster = FALSE;
bool HasRequestedMaster = FALSE;
unsigned __int64 MasterRequestMoment;

HANDLE HeartbeatThread;
DWORD HeartbeatThreadID;

char MasterHeartbeat[] = "Master Heartbeat";
int MasterHeartbeatLen = strlen(MasterHeartbeat);

char SlaveHeartbeat[] = "Slave Heartbeat";
int SlaveHeartbeatLen = strlen(SlaveHeartbeat);

std::string MasterRequest;
char MasterRequestTemplate[] = "Master Request";
int MasterRequestTemplateLen = strlen(MasterRequestTemplate);

char SlaveVoteYes[] = "Slave Vote Yes";
int SlaveVoteYesLen = strlen(SlaveVoteYes);

char SlaveVoteNo[] = "Slave Vote No";
int SlaveVoteNoLen = strlen(SlaveVoteNo);

int numDaemons;
std::atomic<int> numDaemonsDone;
std::atomic<int> numDaemonsVoted;

struct BROADCAST_ARGS {
	std::string address;
};

DWORD CALLBACK MasterHeartbeatThread(LPVOID lpParameter)
{
	BROADCAST_ARGS* parameters = reinterpret_cast<BROADCAST_ARGS*>(lpParameter);

	int rc;
	int workerSock;
	char request[4096];
	bool success = true;

	workerSock = nn_socket(AF_SP, NN_REQ);
	if (workerSock < 0){
		std::cerr << "Unhealthy App Daemon at: " << parameters->address << " : nn_socket failure." << std::endl;
		success = false;
	}

	rc = nn_connect(workerSock, (parameters->address).c_str());
	if (rc < 0){
		std::cerr << "Unhealthy App Daemon at: " << parameters->address << " : nn_connect failure." << std::endl;
		success = false;
	}

	int linger = 1000;
	nn_setsockopt(workerSock, NN_SOL_SOCKET, NN_RCVTIMEO, &linger, sizeof(linger));

	rc = nn_send(workerSock, MasterHeartbeat, MasterHeartbeatLen + 1, 0);
	if (rc < 0){
		std::cerr << "Unhealthy App Daemon at: " << parameters->address << " : nn_send failure." << std::endl;
		success = false;
	}

	request[0] = '\0';
	rc = nn_recv(workerSock, request, sizeof(request), 0);
	if (rc < 0){
		std::cerr << "Unhealthy App Daemon at: " << parameters->address << " : nn_recv failure." << std::endl;
		success = false;
	}

	if (success){
		std::cout << "Healthy App Daemon at: " << parameters->address << std::endl;
	}

	numDaemonsDone++;

	return TRUE;
}

void MasterLoop(){
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	std::vector<HANDLE> broadcastThreads;
	std::vector<DWORD> broadcastThreadIDs;

	HANDLE newThread;
	DWORD newThreadID;

	std::cout << "Broadcasting " << MasterHeartbeat << std::endl;

	std::ifstream infile("daemons.list");
	std::string line;
	numDaemons = 0;
	numDaemonsDone = 0;
	numDaemonsVoted = 0;
	while (std::getline(infile, line))
	{
		BROADCAST_ARGS* parameters = new BROADCAST_ARGS;
		parameters->address = line;

		//std::cout << "Master Heartbeat sent to: " << line << std::endl;

		newThread = CreateThread(NULL, 0, MasterHeartbeatThread, parameters, 0, &newThreadID);

		broadcastThreads.push_back(newThread);
		broadcastThreadIDs.push_back(newThreadID);

		numDaemons++;
	}

	while (numDaemonsDone < numDaemons){
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	std::cout << "Finished Master Heartbeat Broadcast!" << std::endl;
}

DWORD CALLBACK MasterRequestThread(LPVOID lpParameter)
{
	BROADCAST_ARGS* parameters = reinterpret_cast<BROADCAST_ARGS*>(lpParameter);

	int rc;
	int workerSock;
	char request[4096];

	workerSock = nn_socket(AF_SP, NN_REQ);
	if (workerSock < 0){
		std::cerr << "nn_socket failure." << std::endl;
	}

	rc = nn_connect(workerSock, (parameters->address).c_str());
	if (rc < 0){
		std::cerr << "nn_connect failure." << std::endl;
	}

	int linger = 1000;
	nn_setsockopt(workerSock, NN_SOL_SOCKET, NN_RCVTIMEO, &linger, sizeof(linger));

	const char* sendData = MasterRequest.c_str();
	int sendDataLen = strlen(sendData);

	rc = nn_send(workerSock, sendData, sendDataLen + 1, 0);
	if (rc < 0){
		std::cerr << "nn_send failure." << std::endl;
	}

	request[0] = '\0';
	rc = nn_recv(workerSock, request, sizeof(request), 0);
	if (rc < 0){
		std::cerr << "nn_recv failure." << std::endl;
		numDaemonsVoted++;
	}
	else{
		std::cout << "Got: " << request << std::endl;
	}

	numDaemonsDone++;

	return TRUE;
}

DWORD CALLBACK MasterRequestBroadcast(LPVOID lpParameter){
	UNREFERENCED_PARAMETER(lpParameter);

	std::vector<HANDLE> broadcastThreads;
	std::vector<DWORD> broadcastThreadIDs;

	HANDLE newThread;
	DWORD newThreadID;

	MasterRequest = MasterRequestTemplate + ' ' + std::to_string(MasterRequestMoment);

	std::cout << "Broadcasting " << MasterRequest << std::endl;

	std::ifstream infile("daemons.list");
	std::string line;
	numDaemons = 0;
	numDaemonsDone = 0;
	numDaemonsVoted = 0;
	while (std::getline(infile, line))
	{
		BROADCAST_ARGS* parameters = new BROADCAST_ARGS;
		parameters->address = line;

		//std::cout << "Master Request sent to: " << line << std::endl;

		newThread = CreateThread(NULL, 0, MasterRequestThread, parameters, 0, &newThreadID);

		broadcastThreads.push_back(newThread);
		broadcastThreadIDs.push_back(newThreadID);

		numDaemons++;
	}

	while (numDaemonsDone < numDaemons){
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	std::cout << "Finished Master Request Broadcast!" << std::endl;
	//std::cout << "Received " << numDaemonsVoted << " Slave Votes." << std::endl;
	if (numDaemonsVoted > numDaemons / 2){
		std::cout << "This is now the Master Daemon. (" << numDaemonsVoted << "/" << numDaemons << " voted)" << std::endl;
		IsMaster = TRUE;
	}
	else{
		std::cout << "This is still a Slave Daemon. (" << numDaemonsVoted << "/" << numDaemons << " voted)" << std::endl;
	}

	return TRUE;
}

void SlaveLoop(){
	int rc;
	int workerSock;
	char request[4096];

	workerSock = nn_socket(AF_SP, NN_REP);
	if (workerSock < 0){
		std::cerr << "nn_socket failure." << std::endl;
	}

	rc = nn_bind(workerSock, APPD_ADDRESS);
	if (rc < 0){
		std::cerr << "nn_bind failure." << std::endl;
	}

	int linger = 4000;
	nn_setsockopt(workerSock, NN_SOL_SOCKET, NN_RCVTIMEO, &linger, sizeof(linger));

	while (!IsMaster)
	{
		std::cout << "Expecting Master Heartbeat on: " << APPD_ADDRESS << std::endl;

		request[0] = '\0';
		rc = nn_recv(workerSock, request, sizeof(request), 0);
		if (rc < 0){
			std::cerr << "No Master Heartbeat!" << std::endl;
			if (!HasRequestedMaster){
				MasterRequestMoment = __rdtsc();
				HasRequestedMaster = TRUE;
				CreateThread(0, 0, MasterRequestBroadcast, 0, 0, 0);
			}
		}
		else{
			//std::cout << "Is master?:" << request << "|" << MasterHeartbeat << std::endl;
			if (strcmp(MasterHeartbeat, request) != 0){
				if (strncmp(request, MasterRequestTemplate, MasterRequestTemplateLen) == 0){
					std::istringstream iss(request);
					std::string requestPart;
					iss >> requestPart;
					iss >> requestPart;
					iss >> requestPart;
					std::cout << "Got Master Request: " << requestPart << std::endl;
				}
			}
			else{
				std::cout << "Got Master Heartbeat. ";

				rc = nn_send(workerSock, SlaveHeartbeat, SlaveHeartbeatLen + 1, 0);
				if (rc < 0){
					std::cerr << "nn_send failure." << std::endl;
				}

				std::cout << "Sent Slave Heartbeat." << std::endl;
			}
		}
	}
}

char* getline(void) {
	char * line = (char *)malloc(100), *linep = line;
	size_t lenmax = 100, len = lenmax;
	int c;

	if (line == NULL)
		return NULL;

	while(TRUE) {
		c = fgetc(stdin);
		if (c == EOF)
			break;

		if (--len == 0) {
			len = lenmax;
			char * linen = (char *)realloc(linep, lenmax *= 2);

			if (linen == NULL) {
				free(linep);
				return NULL;
			}
			line = linen + (line - linep);
			linep = linen;
		}

		if ((*line++ = c) == '\n')
			break;
	}
	*(line - 1) = '\0';
	return linep;
}

int main(int argc, char* argv[]){
	std::cout << "Enter this App Daemon's address: ";
	APPD_ADDRESS = getline();
	std::cout << "This App Daemon will be communicated with at " << APPD_ADDRESS << std::endl;

	while (TRUE){
		if (IsMaster){
			MasterLoop();
		}
		else{
			SlaveLoop();
		}
	}
}