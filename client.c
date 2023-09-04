#define _XOPEN_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define S_PORTNO 40001
#define ARGS_MAX 7
#define BUFF_LEN 2048
#define RESP_TYPE_TXT 1
#define RESP_TYPE_STRUCT 2

//to store ip address and port no. of mirror
typedef struct {
    char ipAddr[INET_ADDRSTRLEN];
    int portNo;
} mirrorStruct;

//flags for unzip option, quit option, and if file found matching the option
int isUnzipFlag = 0, isQuitFlag = 0, isFileRespFlag = 0;

//check that valid command was entered by the user
bool isValidCommand(const char* cmdArg) {
    //list of supported commands
    const char* validCmdList[] = {
        "filesrch",
        "tarfgetz",
        "getdirf",
        "fgets",
        "targzf",
        "quit"
    };

    //compare the cmdArg with each element of above valid command lists
    for (int i = 0; i < 6; i++) {
        if (strcmp(cmdArg, validCmdList[i]) == 0) {
            //return true if it matched that means that this command is supported
            return true;
        }
    }

    //return false if its an invalid command
    return false;
}

//checks if the string argument provided contains any non-digit character or not
bool checkIsDigit(char* arg) {
	//loop through wach characters in the string
    for (int i = 0; arg[i] != '\0'; i++) {
    	//check if it is a digit
        if (!isdigit(arg[i])) {
        	//return 0 if any character is not a digit
            return false;  
        }
    }
    //return 1 if all characters are digit
    return true; 
}

//check if the year is leap year as per Georgian calendar
bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

//check if the date provided is valid or not
bool checkDateFormat(char inputDate[]) {
    //check that 10 characters are provided as per the format YYYY-MM-DD
	if (strlen(inputDate) != 10 ||
	    inputDate[4] != '-' ||
	    inputDate[7] != '-') {
	    return false;
	}

	//extract the year, month and day value and convert it into integer as well
	int inYear, inMonth, inDay;
	if (sscanf(inputDate, "%4d-%2d-%2d", &inYear, &inMonth, &inDay) != 3) {
	    return false;
	}

    //check that values of year, month and day are within limits
    if (inYear < 1 || inMonth < 1 || inMonth > 12 || inDay < 1 || inDay > 31) {
        return false;
    }

    //handling number of days in february for leap and non-leap year
    if (inMonth == 2) {
        if ((isLeapYear(inYear) && inDay > 29) || (!isLeapYear(inYear) && inDay > 28)) {
            return false;
        }
    } 
    //handling other months where number of days are 30
    else if ((inMonth == 4 || inMonth == 6 || inMonth == 9 || inMonth == 11) && inDay > 30) {
        return false;
    }

    return true;
}

int verifyInputCmdArgs(char cmdBuff[]){
	//declare argument array to store the command arguments and their count
	char *argsArr[ARGS_MAX];
	int argCount = 0;

	//tokenize the input command populate the argument array and count
	char* cmdTok = strtok(cmdBuff, " "); 
	while (cmdTok != NULL) {
		argsArr[argCount++] = cmdTok; 
		cmdTok = strtok(NULL, " "); 
	}
	
	//set the last element of argument array as NULL to mark the end of arguments
	argsArr[argCount] = NULL; 

	//check that first argument is an actual valid command or not
	if(!isValidCommand(argsArr[0])){
		printf("This command is invalid. Supported commands are- targzf, fgets, filesrch, tarfgetz, getdirf, and quit.\n");
		return 1;
	}

	//check if arguments provided with the command or not except for quit command
	if(argCount > 1 && strcmp(argsArr[0], "quit") == 0){
		printf("Invalid Input: 'quit' command does not require any arguments!\n");
		return 1;
	}

	//verify the command arguments in case of 'targzf' command
	if( strcmp(argsArr[0], "targzf") == 0 && (argCount > 5 || argCount < 2)){
		printf("Invalid Input: 'targzf' command requires atleast one file extension as an argument and maximum upto 4 file extensions!\n");
		return 1;
	}

	//verify the command arguments in case of 'fgets' command
	if( strcmp(argsArr[0], "fgets") == 0 && (argCount > 5 || argCount < 2)){
		printf("Invalid Input: 'fgets' command requires atleast one filename as an argument and maximum upto 4 filenames!\n");
		return 1;
	}

	//verify the command arguments in case of 'filesrch' command
	if( strcmp(argsArr[0], "filesrch") == 0 && argCount != 2){
		printf("Invalid Input: 'filesrch' command only takes filename as input!\n");
		return 1;
	}

	//verify the command arguments in case of 'tarfgetz' command
	if (strcmp(argsArr[0], "tarfgetz") == 0) {
		//check that 2 size arguments are entered by the user
		if(argCount != 3){
			printf("Invalid Input: 'tarfgetz' command requires two arguments size1 and size2!\n");
			return 1;
		}
		//check that size arguments are numbers not characters
        if(!checkIsDigit(argsArr[1]) || !checkIsDigit(argsArr[2])) {
            printf("Invalid Input: size1 and size2 should be an integer!\n");
            return 1;
        }
        //check that size1 should be less than size2
        if (atoi(argsArr[1]) > atoi(argsArr[2])) {
            printf("Invalid Input: size1 should be less than size2!\n");
            return 1;
        }
    }

    //verify the command arguments in case of 'getdirf' command
	if (strcmp(argsArr[0], "getdirf") == 0) {
		//check that 2 date arguments are entered by the user
		if(argCount != 3){
			printf("Invalid Input: 'getdirf' command requires two arguments date1 and date2!\n");
			return 1;
		}
		//check that date arguments are valid or not
        if (!checkDateFormat(argsArr[1]) || !checkDateFormat(argsArr[2])) {
            printf("Invalid Input: date1 and date2 should be a valid date with format 'YYYY-MM-DD'!\n");
            return 1;
        }

        //initialise the the structure tm to store date time
		struct tm date_tm1 = { 0 };
	    struct tm date_tm2 = { 0 };

	    //parse the first date string into the tm struct using strptime
	    if (strptime(argsArr[1], "%Y-%m-%d", &date_tm1) == NULL) {
	        printf("Error: Not able to parse the date 1 string! %s\n", argsArr[1]);
	        return 1;
	    }

	    //parse the second date string into the tm struct using strptime
	    if (strptime(argsArr[2], "%Y-%m-%d", &date_tm2) == NULL) {
	        printf("Error: Not able to parse the date 1 string! %s\n", argsArr[2]);
	        return 1;
	    }

	    //convert the tm struct into the unix timestamp for further processing
	    time_t unixTsdate1 = mktime(&date_tm1);
	    time_t unixTsdate2 = mktime(&date_tm2);
		
		//date 1 should be less than date 2
		if (unixTsdate1 > unixTsdate2) {
			printf("Invalid Input: Date 1 cannot be greater than Date 2!\n");
			return 1;
		}
    }

	//unzip option not allowed with filesrch and fgets command
	if((strcmp(argsArr[0], "filesrch") == 0 || strcmp(argsArr[0], "fgets") == 0) && isUnzipFlag == 1){
		printf("Error: unzip option not supported with filesrch and fgets command!\n");
		isUnzipFlag=0;
		return 1;
	}
	
	//process the quit command by enabling the isQuitFlag flag
	if (strcmp(argsArr[0], "quit") == 0) {
		isQuitFlag = 1;
	}

	return 0;	
}

//checks for the empty input - new line or spaces only
bool checkEmptyInput(const char* inCmdStr) {
    for (int c = 0; inCmdStr[c] != '\0'; c++) {
        if (inCmdStr[c] != ' ' && inCmdStr[c] != '\n') {
            return true;
        }
    }
    return false;
}

// Main Function
int main(int argc, char *argv[]) {
	//declaring structure to store server and mirror IP address and port number
    struct sockaddr_in ipAddrServerStruct;
	struct sockaddr_in ipAddrMirrorStruct;

    char ipAddr[16];
    char *outFName = "temp.tar.gz";

    //check if ip address provided or not
    if (argc < 2) {
        printf("Error: Server IP address not provided!\n");
        exit(1);
    }

    //copy the server ip address into ipAdrr character array
    strcpy(ipAddr, argv[1]);

    //create the socket to connect to the server
    int serverSd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSd < 0) {
        printf("Error: Failed to create socket!\n");
        exit(1);
    }

    //fill the server values in the structure
    ipAddrServerStruct.sin_family = AF_INET; //AF_INET denotes that IP address use will be of IPv4 family
    ipAddrServerStruct.sin_port = htons(S_PORTNO); //convert the human readable port number to network byte order form

    //convert the server ip address to binary form and storing it in the struct to connect to the server
    if (inet_pton(AF_INET, ipAddr, &ipAddrServerStruct.sin_addr) != 1) {
        printf("Error: Server IP address must belong to IPv4 family, please enter correct IP address.\n");
        exit(1);
    }

    //connect to the server using connect() system call
    if (connect(serverSd, (struct sockaddr *)&ipAddrServerStruct, sizeof(ipAddr)) < 0) {
        printf("Error: Failed to connect to the server using socket!\n");
        return 1;
    }

    //declaring character arrays to store command and responses from server
    char cmdOptionBuff[BUFF_LEN] = {0};
	char textBuffResp[BUFF_LEN];
	char fileBuffResp[BUFF_LEN];
	char valCmdBuff[BUFF_LEN];

	//recieve the response from server on connection to determine if connection confirmed or redirection to mirror is required
   	long servRespType = 0;
	recv(serverSd, &servRespType, sizeof(servRespType), 0); 

	//if the resposnse type is text means that connection with server is successful
	if(servRespType == RESP_TYPE_TXT){
		printf("Sucessfully connected to server\n");
		
		//empty the text resposne buffer
		memset(textBuffResp, 0, sizeof(textBuffResp));
		//read the response send by the server into the textBuffResp
		read(serverSd, textBuffResp, sizeof(textBuffResp));

		printf("Server Response: %s\n", textBuffResp);
	}
	//if server response is a structure means mirror redirection
	else if(servRespType == RESP_TYPE_STRUCT)
	{	
		printf("Redirecting to the mirror server.\n");
		
		mirrorStruct mirrorObj;
		
		//recieve the mirror ip address and port number in the struct
		recv(serverSd, &mirrorObj, sizeof(mirrorStruct), 0);
		
		//close the socket connection with server
		close(serverSd);

		//debug logs
		printf("Mirror IP Address: %s\n", mirrorObj.ipAddr);
		printf("Mirror Port No.: %d\n", mirrorObj.portNo);
		
		//create new socket to connect with mirror
		serverSd = socket(AF_INET, SOCK_STREAM, 0);
		if (serverSd < 0) {
			printf("Error: Failed to create socket to connect with mirror!\n");
			exit(1);
		}
		
		//define the structure for mirror
		ipAddrMirrorStruct.sin_family = AF_INET; //ipv4 family
		ipAddrMirrorStruct.sin_port = htons(mirrorObj.portNo); //convert portno into binary form
		
		//convert ip address into binary form and storing it in the struct to connect to the server
		if (inet_pton(AF_INET, mirrorObj.ipAddr, &ipAddrMirrorStruct.sin_addr) <= 0) {
			printf("Invalid mirror address.\n");
			exit(1);
		}
		
		// Connect to Mirror Server
		if (connect(serverSd, (struct sockaddr *)&ipAddrMirrorStruct, sizeof(ipAddrMirrorStruct)) < 0) {
			printf("Connection to mirror failed.\n");
			exit(1);
		}

		recv(serverSd, &servRespType, sizeof(servRespType), 0); 
		
		printf("Sucessfully connected to mirror\n");
		
		//empty the text resposne buffer
		memset(textBuffResp, 0, sizeof(textBuffResp));
		//read the response send by the mirror server into the textBuffResp
		read(serverSd, textBuffResp, sizeof(textBuffResp));

		printf("Server Response: %s\n", textBuffResp);
	}


	//infinite loop to exchange info between client and server/mirror
	while(1){
		//empty the buffer for storing command option entered by user
		memset(cmdOptionBuff, 0, sizeof(cmdOptionBuff)); 
		isFileRespFlag = 0;
		
        printf("\nC$ ");
        fgets(cmdOptionBuff, 2048, stdin);

		size_t cmdInputLen = strlen(cmdOptionBuff);
		
		//remove the trailing spaces and new line characters if any
    	while (cmdInputLen > 0 && (cmdOptionBuff[cmdInputLen - 1] == ' ' || cmdOptionBuff[cmdInputLen - 1] == '\n')) {
        	cmdInputLen--;
    	}
    	cmdOptionBuff[cmdInputLen] = '\0';

    	//replace the -u option with null character and enable unzip flag
    	if (cmdInputLen >= 3 && strcmp(cmdOptionBuff + cmdInputLen - 3, " -u") == 0) {
        	isUnzipFlag = 1;
        	cmdOptionBuff[cmdInputLen - 3] = '\0';
    	}

    	if (!checkEmptyInput(cmdOptionBuff)){
    		printf("Please provide some input command!\n");
    		continue;
    	}

		//copy the command into valCmdBuff for validation
		strcpy(valCmdBuff, cmdOptionBuff);
		
		//validate the input command and its arguments
		if(verifyInputCmdArgs(valCmdBuff))
			continue;

        //send the input command to the server/mirror
        send(serverSd, cmdOptionBuff, strlen(cmdOptionBuff), 0);

        printf("\nRecieving response from server.....\n");

		//read the response sent by the server/mirror 
		long serverResp;
		int readBytesServer = read(serverSd, &serverResp, sizeof(serverResp));	

		if(readBytesServer > 0){
			//if the resposnse type is text
			if(serverResp == RESP_TYPE_TXT){
				//empty the text response buffer
				memset(textBuffResp, 0, sizeof(textBuffResp));
				//read the response send by the mirror server into the textBuffResp
				read(serverSd, textBuffResp, sizeof(textBuffResp));

				printf("Server Response: %s\n", textBuffResp);
			}
			//server sent the file in response
			else{
				isFileRespFlag=1;
				
				//get the size of file from the header of the response recieved from header
				long fSize = serverResp;
				 
				//empty the file response buffer
				memset(fileBuffResp, 0, sizeof(fileBuffResp));
				
				//create a new file with the name 'temp.tar.gz'
				FILE* filePointer = fopen(outFName, "wb");
				if (filePointer == NULL) {
					printf("Error: Not able to create file in the client!\n");
					exit(1);
				}

				//recieve the file data from the server
				long totalBytes = 0;

				//run the loop until whole data is recieved
				while(totalBytes<fSize){
					//calculate the number of that bytes that still need to be recieved as per the file size with a limit of 2048bytes
					int numBytesNotRecieved = BUFF_LEN;
					if (totalBytes + BUFF_LEN > fSize) {
						numBytesNotRecieved = fSize - totalBytes;
					}
					//recieve data from server
					int numBytesRecieved = recv(serverSd, fileBuffResp, numBytesNotRecieved, 0);
					if (numBytesRecieved < 0) {
						printf("Error: File data not recieved properly!\n");
						exit(1);
					}

					//write the file data recieved from server into the file on client side
					fwrite(fileBuffResp, sizeof(char), numBytesRecieved, filePointer);
					
					//adding bytes to totalBytes 
					totalBytes += numBytesRecieved;

					//check if total bytes has reached to size of file
					if (totalBytes >= fSize) {
						break;
					}
					
				}

				printf("\nFile Response: %s recieved from server and saved in the current working directory successfully.\n", outFName);
				fflush(filePointer);
				fclose(filePointer);
			}
		}
		//empty response from server ie 0 bytes which means server is disconnected
		else{
            printf("Error: Server is no longer connected\n");
            break;
		}

		//process the 'Quit' command
		if(isQuitFlag){
			printf("Terminating the session!\n");
			break;
		}
		
		//process the unzip option if entered by the user
		if (isUnzipFlag && isFileRespFlag) {
			//create the bash command for unzipping
			char bashCmd[BUFF_LEN];
			snprintf(bashCmd, BUFF_LEN, "tar -xz -f %s", outFName);
			
			//run the bash command
			system(bashCmd);
			
			printf("File is unzipped in the current working directory.\n");
			
			//reset the unzip flag
			isUnzipFlag = 0;
		}
	}

	//close the connection socket with server
    close(serverSd);

	return 0;
}