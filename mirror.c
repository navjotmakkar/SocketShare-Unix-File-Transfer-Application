#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <ftw.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define BUFFR_LEN_BYTES 2048
#define OUT_ZIP_NAME "resultM.tar.gz"
#define OUT_FLIST_NAME "outFilePathListM.txt"
#define S_PORTNO 40002
#define ROOT_PATH getenv("HOME")

typedef struct 
{
    char ipAddr[INET_ADDRSTRLEN];
    int portNo;
} mirrorStruct;

//store the count of how many client has requested to make the connection
int numClientConnected = 0;

//!! by below function a text output is sent to the client!!
int processTextOutputResp(int server_sd, char* txtoutput){
    long typeoutput = 1;

    // sending the type of output to the client's connect sock
    send(server_sd, &typeoutput, sizeof(typeoutput), 0);

    printf("Text response written to the client %d connection socket: %s\n", numClientConnected, txtoutput);
    // !!actual text output being sent to the client's connection sock !! 
    send(server_sd, txtoutput, strlen(txtoutput), 0);

    return 0;
}

//!! by below function last newline character is being replaced by null character !!
 void processNewLineChar(char buffrStr[]){

    int buffrStrLen = strlen(buffrStr);
    if(buffrStrLen > 0 && buffrStr[buffrStrLen - 1] == '\n'){
        buffrStr[buffrStrLen - 1] = '\0';
    }
 } 

// !! below function processes the zip file : content is sent back via descriptor 
int processZipFileResp(int server_sd){

   //open the tar file
   int tarZipFd = open(OUT_ZIP_NAME, O_RDONLY);

   if(tarZipFd < 0){
    printf("Not able to open the zip file to send the data to the client\n");
    return 1;
   }

   //declare buffer to read and store the content of the tar file
   char buffrResp[BUFFR_LEN_BYTES];
   ssize_t numBytesRead;

   //read the tar file with 2048 bytes at a time
   while((numBytesRead = read(tarZipFd, buffrResp, 2048)) > 0){
    send(server_sd, buffrResp, numBytesRead, 0);
   }

   //closing the tar file descriptor
   close(tarZipFd);

   return 0;
} 

// !! below function searches a file within specified directory
int fileSearch_tarfgetz(char *rootDirPath, int sizeArg1, int sizeArg2, int *numResFiles)
{
    // !! DIrectory path buffer variable 
    char buffrDirPath[BUFFR_LEN_BYTES];
    // !! clearing buffer
    memset(buffrDirPath, 0, sizeof(buffrDirPath));

    printf("Searching files based on size...\n");
    
    // !!open root directory and store it in DIR structure object
    DIR *dirObj;
    struct dirent *dirEntrStruct;
    if ((dirObj = opendir(rootDirPath)) == NULL) {
        printf("Error: Could not open the directory %s", rootDirPath);
        return 1;
    }
    
    FILE *flist_fp;
    // !!open temporary file list using fopen in append mode 
    flist_fp = fopen(OUT_FLIST_NAME, "a");
    if (flist_fp == NULL) {
        printf("Error: Could not open the temporary file to store the list of files\n");
        return 1;
    }
    // !! Iterate through the direcotry 
     while ((dirEntrStruct = readdir(dirObj)) != NULL)
    {
        // !!skip . and ..
        if (strcmp(dirEntrStruct->d_name, "..") == 0 || strcmp(dirEntrStruct->d_name, ".") == 0)
            continue;

        //create the full path for the directory/fil by concatenating the root directory path
        sprintf(buffrDirPath, "%s/%s", rootDirPath, dirEntrStruct->d_name);
        printf("Processing the entry %s\n", buffrDirPath);
        
        // get file/directory stats
        struct stat statObj;
        if (stat(buffrDirPath, &statObj) == -1)
            continue;
         
        // !!if entry is directory, recursively search it
        if (dirEntrStruct->d_type == DT_DIR)
        {
            fileSearch_tarfgetz(buffrDirPath, sizeArg1, sizeArg2, numResFiles);
        }
        // !!if file, check if it's between size 1 and size 2 and print name
        else if (S_ISREG(statObj.st_mode) && statObj.st_size >= sizeArg1 && statObj.st_size <= sizeArg2 && sizeArg1 <= sizeArg2) 
        {
         
            if (rootDirPath[strlen(rootDirPath) - 1] == '/') {
                sprintf(buffrDirPath, "%s%s", rootDirPath, dirEntrStruct->d_name);
            } else {
                sprintf(buffrDirPath, "%s/%s", rootDirPath, dirEntrStruct->d_name);
            }
            // Write the file path to the file list and print a message
            fprintf(flist_fp, "%s\n", buffrDirPath);
            printf("File %s matched the criteria \n", buffrDirPath);
            *numResFiles+=1;
        }
    }
    closedir(dirObj);
    fclose(flist_fp);
    return 0;

}

int process_tarfgetz(int server_sd, char** inArgs){

    size_t sizeArg1 = atoi(inArgs[0]);
    size_t sizeArg2 = atoi(inArgs[1]);
    
    char* rootDirPath = ROOT_PATH;
    char buffrTarCmd[BUFFR_LEN_BYTES];

    char* createTarCmd = "tar -cz -f %s -T %s";

    int cmdResultStatus, numResFiles = 0;

    //!! discover the files matching the criteria of size in the specified directory
    fileSearch_tarfgetz(rootDirPath, sizeArg1, sizeArg2, &numResFiles);

    printf("%d number of files which matched the size criteria \n", numResFiles);

    // formatting of tar command 
    sprintf(buffrTarCmd, createTarCmd, OUT_ZIP_NAME, OUT_FLIST_NAME);

    if(numResFiles > 0)
    {
        // execution of tar command 
        cmdResultStatus = system(buffrTarCmd);

        //!!check if the tar command execution was succesfful!!
        if(cmdResultStatus == 0)
        {
            printf("Tar file created using bash command: %s\n", buffrTarCmd);

            FILE* ptrFile;
            char* buffrFileData;
            long tarFileSize;
            
            // !!FILE is opened in binary mode for reading
            ptrFile = fopen(OUT_ZIP_NAME, "rb");
            
            if(ptrFile == NULL)
            {
                printf("Error: Couldn't open the tar file to send data to client\n");
                return 1;
            }
            //!! getting the size of file
            fseek(ptrFile, 0, SEEK_END);
            tarFileSize = ftell(ptrFile);
            fseek(ptrFile, 0, SEEK_SET);
            // alloacation of memory
            buffrFileData = (char* )malloc((tarFileSize + 1) * sizeof(char));
            
            // entire file is read into the buffer
            fread(buffrFileData, tarFileSize, 1, ptrFile);
            
            // send the size of the tar file over socket
            send(server_sd, &tarFileSize, sizeof(tarFileSize), 0);

            // response for file transmission is processed 
            int transmissionOutput = processZipFileResp(server_sd);

            // whether file transmission was successfull
            if(transmissionOutput != 0)
                printf("Socket file transfer failed in tarfgetz\n");
            else
                printf("Socket file transfer completed in tarfgetz\n");

            fclose(ptrFile);
        } else {
            printf("Error: Could not create the tar file\n");
            return 1;
        }
    } else {
       
       // appropriate message is sent if no matching file is found
        processTextOutputResp(server_sd,"No file found matching the size criteria provided");
    }

   // removal of temporary files and tar filess 
   remove(OUT_FLIST_NAME);
   remove(OUT_ZIP_NAME);

   return 0;
} 

// !! to convert a date string into unix timestamp
//!! whether the timestamp should represent the start or the end of the day
time_t convertTimeUnixFormat(const char* strTime, int typeDate){

    // !!structure to hold the time 
    struct tm tmStructObj;

    int yy, mm, dd;
    //!! reading from input string and parsing into year, month and day component
    if(sscanf(strTime,"%d-%d-%d", &yy, &mm, &dd) != 3)
    {
        return(time_t)-1;
    }
   
     //!! setting the year, month and day component -> in structure
    tmStructObj.tm_year = yy-1900;
    tmStructObj.tm_mon = mm - 1;
    tmStructObj.tm_mday = dd;

    if(typeDate == 1){
        //!! setting to the start of the day
        tmStructObj.tm_hour = 0;
        tmStructObj.tm_min = 0;
        tmStructObj.tm_sec = 0;
    }
    else {
        //!! setting to the end of the day
        tmStructObj.tm_hour = 23;
        tmStructObj.tm_min = 59;
        tmStructObj.tm_sec = 59;

    }

    // !!daylight saving information not available 
    tmStructObj.tm_isdst = -1;

    // !! conversion to unix timestamp
    time_t timeValConverted = mktime(&tmStructObj);

    //!! whether conversion was succesfull or not 
    if(timeValConverted == (time_t)-1)
    {
        return (time_t)-1;
    } 
    return timeValConverted;
}

// !!A function for locating files within a directory using date criteria.!!
// !!rootDirPath: The path of the root directory to search within.!!
// !!startDate: The starting date in Unix timestamp format.!!
// !!endDate: The ending date in Unix timestamp format.!!
// !!resultFileCount: A pointer to an integer to keep track of the matched file count.!!
int fileSearch_getdirf(char *rootDirPath, time_t dateArg1, time_t dateArg2, int *numResFiles)
{
    // !!object of directory and dirent stucture for directory traversal 
    DIR * dirObj;
    struct dirent *dirEntrStruct;
    char buffrDirPath[BUFFR_LEN_BYTES];
    FILE *flist_fp;
    
    //!! clearing buffer!! 
    memset(buffrDirPath, 0, sizeof(buffrDirPath));
    
    printf("Searching files based on date...\n");
    
    // !!open root directory
    if ((dirObj = opendir(rootDirPath)) == NULL) {
        printf("Error: Could not open the directory %s", rootDirPath);
        return 1;
    }
    
    // !!open temporary file list in append mode
    flist_fp = fopen(OUT_FLIST_NAME, "a");
    if (flist_fp == NULL) {
         printf("Error: Could not open the temporary file to store the list of files\n");
        return 1;
    }
    // !!traversing through each entry in the directory!!
     while ((dirEntrStruct = readdir(dirObj)) != NULL)
    {
        // skip . and .. entries
        if (strcmp(dirEntrStruct->d_name, ".") == 0 || strcmp(dirEntrStruct->d_name, "..") == 0)
        {
            continue;
        }
        
        //create the full path for the directory/fil by concatenating the root directory path
        sprintf(buffrDirPath, "%s/%s", rootDirPath, dirEntrStruct->d_name);
        printf("Processing the entry %s\n", buffrDirPath);

        // if directory, recursively search it
        if (dirEntrStruct->d_type == DT_DIR)
        {
            fileSearch_getdirf(buffrDirPath, dateArg1, dateArg2, numResFiles);
        }
        // if file, check if it's between range of date 
        else 
        {
            // get file/directory stats
            struct stat statObj;
            if (stat(buffrDirPath, &statObj) == -1)
                continue;
            
            //get the modification time of the file through stat
            time_t time_file;
            time_file = statObj.st_mtime;
            
            // !! test whether timestamp is within specified date range
            if (time_file >= dateArg1 && time_file <= dateArg2)
            {
                // !! construct the full path 
                if (rootDirPath[strlen(rootDirPath) - 1] == '/') {
                    sprintf(buffrDirPath, "%s%s", rootDirPath, dirEntrStruct->d_name);
                } else {
                    sprintf(buffrDirPath, "%s/%s", rootDirPath, dirEntrStruct->d_name);
                }
                
                // !! writing the file path to teporary file list
                fprintf(flist_fp, "%s\n", buffrDirPath);


                printf("File %s matched the criteria \n", buffrDirPath);
                
                // !! increasing the count of matching files
                *numResFiles+=1;
            }
        }
    }
    closedir(dirObj);
    fclose(flist_fp);
    return 0;

}

// !! below function processes and searches for files within a date range!!
int process_getdirf(int server_sd, char** inArgs){
  
  //!! conversion to time_t format!!
    time_t dateArg1 = convertTimeUnixFormat(inArgs[0],1);
    time_t dateArg2 = convertTimeUnixFormat(inArgs[1],2);

    // !!rroot directory path!!
    char* rootDirPath = ROOT_PATH;
    //!!buff for tar command
    char buffrTarCmd[BUFFR_LEN_BYTES];

    // !!tar file creation command!!
    char* createTarCmd = "tar -cz -f %s -T %s";
    
    //!! var to hold number of matched files
    int cmdResultStatus, numResFiles = 0;

    //!!Search for files within a specific date range!!
    fileSearch_getdirf(rootDirPath, dateArg1, dateArg2, &numResFiles);

    printf("%d number of files which matched the size criteria \n", numResFiles);

    //!! creation of tar command, with output tar file and list of files
    sprintf(buffrTarCmd, createTarCmd, OUT_ZIP_NAME, OUT_FLIST_NAME);

    //!!condition for Matching files
    if(numResFiles > 0){

        // !! execution of tar command to compress files!!
       int cmdResultStatus = system(buffrTarCmd);
      
       if(cmdResultStatus == 0){
            printf("Tar file created using bash command: %s\n", buffrTarCmd);

        
            FILE* ptrFile;
            char* buffrFileData;
            long tarFileSize;
            
            // !!tar file opening in binary mode 
            ptrFile = fopen(OUT_ZIP_NAME, "rb");

            // test whether tar file is successfully opened            
            if(ptrFile == NULL)
            {
                printf("Error: Could not open the temporary file to store the list of files\n");
                return 1;
            }

            // !!tar file size
            fseek(ptrFile, 0, SEEK_END);
            tarFileSize = ftell(ptrFile);
            fseek(ptrFile, 0, SEEK_SET);

            buffrFileData = (char* )malloc((tarFileSize + 1) * sizeof(char));
            
            // !!entire tar read file into the buffer
            fread(buffrFileData, tarFileSize, 1, ptrFile);

            // !!tar file size sent to the client 
            send(server_sd, &tarFileSize, sizeof(tarFileSize), 0);
            
            // !!processing of tar file initaited
            int transmissionOutput = processZipFileResp(server_sd);

            // !! check if the transmission was successful
            if(transmissionOutput != 0)
            {
                printf("Socket file transfer failed in getdirf\n");
            }
            else
            {
                printf("Socket file transfer completed in getdirf\n");
            }

            fclose(ptrFile);

       } else {
        printf("Error: Could not create the tar file\n");
        return 1;
       }

    } else {
        //!! response is sent to client and cleaning up
        processTextOutputResp(server_sd,"No file found matching the date criteria provided");
        remove(OUT_FLIST_NAME);
        return 0;
    }

    remove(OUT_FLIST_NAME);
    remove(OUT_ZIP_NAME);

    return 0;

}

//!! search files based on extension type
int fileSearch_Ext(char* rootDirPath,char** fNameExtFormatArr, int numFileExts, int *numResFiles)
{
    DIR * dirObj;
    struct dirent *dirEntrStruct; // directory entry structure
    char buffrDirPath[BUFFR_LEN_BYTES]; // buffer to store directory path
    int cmdResultStatus;
    FILE *flist_fp; 
    int dd;

    printf("Searching files based on extension type...\n");
    
    // !!open root directory!!
    if ((dirObj = opendir(rootDirPath)) == NULL) {
        printf("Error: Could not open the directory %s", rootDirPath);
        return 1;
    }
    
    // !!open temporary file list!!
    flist_fp = fopen(OUT_FLIST_NAME, "a");
    if (flist_fp == NULL) {
        printf("Error: Could not open the temporary file to store the list of files\n");
        return 1;
    }
    // !!Iterate through the direcotry entry
     while ((dirEntrStruct = readdir(dirObj)) != NULL)
    {
       // !! if entry is a directory and not'.', '..'
       if (dirEntrStruct->d_type == DT_DIR && strcmp(dirEntrStruct->d_name, ".") != 0 && strcmp(dirEntrStruct->d_name, "..") != 0) {
            //create the full path for the directory/file by concatenating the root directory path
            sprintf(buffrDirPath, "%s/%s", rootDirPath, dirEntrStruct->d_name);
            // Iterating through subdirectories
            fileSearch_Ext(buffrDirPath, fNameExtFormatArr, numFileExts, numResFiles);
        } else {
            //!! skipping parent and current entries
            if(strcmp(dirEntrStruct->d_name, ".") == 0 || strcmp(dirEntrStruct->d_name, "..") == 0)
                continue;
            
            printf("Processing the entry %s\n", dirEntrStruct->d_name);

            // !!check if the file extension matches any extension in the array!!
            for (dd = 0; dd < numFileExts; dd++) {
                if (fnmatch(fNameExtFormatArr[dd], dirEntrStruct->d_name, FNM_PATHNAME) == 0) {
                    //create the full path for the file by concatenating the root directory path
                    sprintf(buffrDirPath, "%s/%s", rootDirPath, dirEntrStruct->d_name);
                    
                    // appending file path to the result file
                    fprintf(flist_fp, "%s\n", buffrDirPath);
                    printf("File %s matched the criteria \n", buffrDirPath);
                    
                    *numResFiles+=1;
                    
                    break;
                }
            }
        }
    }
    closedir(dirObj);
    fclose(flist_fp);
    return 0;
}

//!! search and process for files with specified extensions, compress to a tar, transffered over a socket
int process_targzf(int server_sd, char** fileExtsTypeArr, int numFileExts)
{
    DIR* dirObj;
    struct dirent *dirEntrStruct;
    char buffrTarCmd[BUFFR_LEN_BYTES];

    // !! command to create a tar file
    char* createTarCmd = "tar -cz -f %s -T %s";
    char* rootDirPath = ROOT_PATH;
    int dd, cmdResultStatus;

    // !!array for storing foramtted file ext
    char* fNameExtFormatArr[numFileExts];  

    printf("Searching files based on file extensions: ");
    
    //!! printing file extensions
    for (dd = 0; dd < numFileExts; dd++) {
        printf("%s ", fileExtsTypeArr[dd]);
    }
    printf("\n");
    
    //opening of root directorey
    if ((dirObj = opendir(rootDirPath)) == NULL) {
        printf("Error: Could not open the directory %s", rootDirPath);
        return 1;
    }

    // !!format file extension patterns 
    for (dd = 0; dd < numFileExts; dd++) {
        fNameExtFormatArr[dd] = malloc(strlen(fileExtsTypeArr[dd]) + 2);
        sprintf(fNameExtFormatArr[dd], "*.%s", fileExtsTypeArr[dd]);
    }

    // !!counter for the numbr of resulting files
    int numResFiles = 0;

    //!! files for specified extensions
    if (fileSearch_Ext(rootDirPath, fNameExtFormatArr, numFileExts, &numResFiles) != 0) {
        printf("Error: Unexpected error occurred during searching of files!");
        return 1;
    }
    
    //!! no files match criteria
    if (numResFiles == 0) {
        
        // !!response text to client 
        processTextOutputResp(server_sd, "No file found matching the list of file extensions provided");
        remove(OUT_FLIST_NAME); // !!removal of temp filelist 
        return 0;
    }

    //!!Creation of command tar -> output tar file + list of files
    sprintf(buffrTarCmd, createTarCmd, OUT_ZIP_NAME, OUT_FLIST_NAME);

    //!!execution of tar cmd!!
    cmdResultStatus = system(buffrTarCmd);
    if (cmdResultStatus != 0) {
        printf("Error: Could not create the tar file\n");
        return 1;
    }

    printf("Tar file of the resulted files created using bash command: %s\n", buffrTarCmd);

    //!! Get the size of the tar file(opening in binary mode)
    FILE *tarfd;
    long int tarFileSize;
    tarfd = fopen(OUT_ZIP_NAME, "rb");
    if (tarfd == NULL) {
        printf("Error: Couldn't open the tar file to send data to client\n");
        return 1;
    }
    fseek(tarfd, 0, SEEK_END);
    tarFileSize = ftell(tarfd);
    fseek(tarfd, 0, SEEK_SET);
    
    // !!Send the size of the file as a long int(to clinet)
    send(server_sd, &tarFileSize, sizeof(tarFileSize), 0);
    
    //!!Transfer the tar(file) over the socket!!
    if (processZipFileResp(server_sd) != 0) {
        printf("Socket file transfer failed in targzf\n");
        remove(OUT_FLIST_NAME);
        return 1;
    }
    else {
        printf("Socket file transfer completed in targzf\n");
    }
    
    fclose(tarfd);
    
    // !!Delete outFilePathListM.txt
    remove(OUT_FLIST_NAME);
    // !!Delete resultM.tar.gz
    remove(OUT_ZIP_NAME);

    return 0;
}

// !!below func searching files based on file names
int fileSearch_FName(char* rootDirPath, char** fNamesArr, int numFNames, int *numResFiles)
{
    DIR * dirObj;
    struct dirent *dirEntrStruct;
    char buffrDirPath[BUFFR_LEN_BYTES];
    int cmdResultStatus;
    FILE *flist_fp;

    printf("Searching files based on file name...\n");
    
    // !!open root directory!!
    if ((dirObj = opendir(rootDirPath)) == NULL) {
        printf("Error: Could not open the directory %s", rootDirPath);
        return 1;
    }
    
    // !!open temporary file to store list of matching files!!
    flist_fp = fopen(OUT_FLIST_NAME, "a");
    if (flist_fp == NULL) {
        printf("Error: Could not open the temporary file to store the list of files\n");
        return 1;
    }
    //!! iterate through all entries!!
     while ((dirEntrStruct = readdir(dirObj)) != NULL)
    {
       
       if (dirEntrStruct->d_type == DT_DIR && strcmp(dirEntrStruct->d_name, ".") != 0 && strcmp(dirEntrStruct->d_name, "..") != 0) {
            //create the full path for the directory/file by concatenating the root directory path
            sprintf(buffrDirPath, "%s/%s", rootDirPath, dirEntrStruct->d_name);
            
            // recursively search with-subdirectories
            fileSearch_Ext(buffrDirPath, fNamesArr, numFNames, numResFiles);
        } else {
            //skipp current and parent directory
            if(strcmp(dirEntrStruct->d_name, ".") == 0 || strcmp(dirEntrStruct->d_name, "..") == 0)
                continue;

            printf("Processing the directory %s\n", dirEntrStruct->d_name);
            // !!iterate through the array of target filenames!!
            for (int dd = 0; dd < numFNames; dd++) {
                // !!test if the current file name is same as in the array
                if (strcmp(fNamesArr[dd], dirEntrStruct->d_name) == 0) {
                    //create the full path for the file by concatenating the root directory path
                    sprintf(buffrDirPath, "%s/%s", rootDirPath, dirEntrStruct->d_name);
                    // !!Appending the path to the result list
                    fprintf(flist_fp, "%s\n", buffrDirPath);

                    printf("File %s matched the criteria \n", buffrDirPath);
                    
                    //!! Incrementing count of matching filles 
                    *numResFiles+=1;
                    break;
                }
            }
        }
       
    }
    closedir(dirObj);
    fclose(flist_fp);
    return 0;

}

//!!below func gathers file information and sens to client 
void process_filesrch(int server_sd, char **inArgs){

    // From below input args it extracts filename
    char* nameFile = inArgs[0];

    //!!buff to hold file info response
    char fileInfoResp[BUFFR_LEN_BYTES];
    
    printf("Searching for the information for: %s\n", nameFile);
    
    // !!root directory path
    char* rootDirPath = ROOT_PATH;
    char* findCmdFormat = (char*) malloc(strlen(rootDirPath) + strlen(nameFile) +27);
    
    // !!create the find command format string 
    sprintf(findCmdFormat,"find %s -name '%s' -print -quit", rootDirPath, nameFile);
    
    printf("Executing command for finding the file: %s\n", findCmdFormat);
   
    //!! pipe opening - for the execution of find cmd & reading it's output
    FILE* fileInfoPipe = popen(findCmdFormat,"r");

    // !!Test whether pipe was successfully created 
    if(fileInfoPipe != NULL){
        char findCmdOutput[256];

        //!!reading the output of find cmd
        if(fgets(findCmdOutput, sizeof(findCmdOutput), fileInfoPipe) != NULL){
           
           // REMOVAL of newline charc
           findCmdOutput[strcspn(findCmdOutput,"\n")] = '\0';
           
           //structure - for file info
           struct stat statInfo;

           //getting file info using stat
           if(stat(findCmdOutput, &statInfo) == 0){
            //get the modification time of the file through stat 
            time_t fileCreateTimeUnix;
            fileCreateTimeUnix = statInfo.st_mtime;

            //convert the unix timestamp to human readable format
            char* fileCreateTime = ctime(&fileCreateTimeUnix);
           
            //!! from time string, newline character to be removed
            processNewLineChar(fileCreateTime);

            // file information response will be created
            sprintf(fileInfoResp,"%s , Size: %lld byytes, Creation Date: %s)", findCmdOutput, (long long) statInfo.st_size, fileCreateTime);
           } else {

            // response for file information cannot be retrived
            sprintf(fileInfoResp,"File information not found for %s", findCmdOutput);
           }
        }else {

            // whether file with the given name was found
            sprintf(fileInfoResp,"No file found in the root directory with name %s", nameFile);
        }

        // close the pipe
        pclose(fileInfoPipe);
    } else {
        printf("Error: Not able to create pipe for running find command!\n");
        
        //
        sprintf(fileInfoResp,"Unexpected error occurred in finding the file %s", nameFile);
    }
    free(findCmdFormat);
    
    // !!Sending the file info repsonse to client!!
    processTextOutputResp(server_sd, fileInfoResp);
} 

// !!Below func searches and processes files based on filenaems!!
int process_fgets(int server_sd, char** fNamesArr, int numFNames){

    DIR* dirObj;
    char* buffrTarCmd = malloc(sizeof(char)*BUFFR_LEN_BYTES);
    char* createTarCmd = "tar -cz -f %s -T %s"; // !!tar file creation command 
    char* rootDirPath = ROOT_PATH;
    int dd, cmdResultStatus;

    printf("Searching files based on file names: ");

    // !!target filename(which are being searched) printing 
    for(dd = 0; dd<numFNames; dd++){
        printf("%s ", fNamesArr[dd]);
    }
    printf("\n");

    // !!opening of the root directory
    if((dirObj = opendir(rootDirPath)) == NULL){
        printf("Error: Could not open the directory %s", rootDirPath);
        return 1;
    }

    int numResFiles = 0;

    // Searching for files based on filenames and counting of matching files
    if(fileSearch_FName(rootDirPath, fNamesArr, numFNames, &numResFiles) != 0)
    {
        printf("Error: Unexpected error occurred during searching of files!");
        return 1;
    }

    // found no matching file
    if(numResFiles == 0)
    {
        processTextOutputResp(server_sd, "No file found matching the list of file names provided");
        remove(OUT_FLIST_NAME);// temporary file list emptied
        return 0;
    }

    // Create tar command to compress files into resultM.tar.gz
    sprintf(buffrTarCmd, createTarCmd, OUT_ZIP_NAME, OUT_FLIST_NAME);
    
    // !!execution of the tar command!!
    cmdResultStatus = system(buffrTarCmd);
    if (cmdResultStatus != 0) {
        printf("Error: Could not create the tar file\n");
        return 1;
    }

    printf("Tar file of the resulted files created using bash command: %s\n", buffrTarCmd);

    FILE * ptrFile;
    char* buffrFileData;
    long tarFileSize;

    // opening the tar file in binary mode
    ptrFile = fopen(OUT_ZIP_NAME, "rb");

    if(ptrFile == NULL)
    {
        printf("Error: Couldn't open the tar file to send data to client\n");
        return 1;
    }
    
    // !!getting the size of the file!!
    fseek(ptrFile, 0, SEEK_END);
    tarFileSize = ftell(ptrFile);
    fseek(ptrFile, 0, SEEK_SET);

    buffrFileData = (char*)malloc((tarFileSize + 1) * sizeof(char));
    // !!reading tar file data!!
    fread(buffrFileData,tarFileSize, 1, ptrFile);

    // !!sending the tar file size to the client!!
    send(server_sd, &tarFileSize, sizeof(tarFileSize), 0);

    //!!processing the transmission of the tar file!!
    int transmissionOutput = processZipFileResp(server_sd);
    if(transmissionOutput != 0){
        printf("Socket file transfer failed in fgets\n");
        return 1;
    }
    else
    {
        printf("Socket file transfer completed in fgets\n");
    }

    // Removing temporary files
    remove(OUT_FLIST_NAME);
    remove(OUT_ZIP_NAME);

    fclose(ptrFile);

    return 0;
}

// !!Below func process the client requests
void processclient(int server_sd){
    char clientRequestCmd[BUFFR_LEN_BYTES];
    int  clientReqBytes = 0;

    // !! sends the message i.e server is ready to process the client request
    processTextOutputResp(server_sd, "Server is now ready to process the client request");

    //!! infinite loop for processing of client requests
    while(1)
    {
        // !!eraseing the buff comd
        memset(clientRequestCmd, 0, sizeof(clientRequestCmd));

        //!! reading of client requests from socket
        clientReqBytes = read(server_sd, clientRequestCmd, BUFFR_LEN_BYTES);
        processNewLineChar(clientRequestCmd); 
         
        // whether client is still connectedd
        if(clientReqBytes <= 0)
        {
            printf("Connection Status: Client %d is no longer connected\n", numClientConnected);
            break;
        }

        printf("Client request: %s\n", clientRequestCmd);

        // !!array to hold input of args from client
        char* inArgs[10];
        int numInArgs = 0;

        //!! tokenization the client request cmd to extract base comd and args 
        char* clientReqTok = strtok(clientRequestCmd, " ");  //basic tokenization
        char* inCmd = clientReqTok; 

        printf("Base Command recieved from client: %s \n", inCmd);

        //get all the arguments received from the client after the command
        while(clientReqTok != NULL)
        {
            clientReqTok = strtok(NULL, " ");
            if(clientReqTok != NULL){
                inArgs[numInArgs++] = clientReqTok;
            }
        }
        inArgs[numInArgs] = NULL; // Setting of last element of array

        //!!commands process as per input comd 
        if(strcmp(inCmd, "filesrch") == 0)
        {
            process_filesrch(server_sd, inArgs);
        }
        else if(strcmp(inCmd, "tarfgetz") == 0)
        {
            int filesgetresult = process_tarfgetz(server_sd, inArgs);
            
            if(filesgetresult == 1){
                processTextOutputResp(server_sd, "Command failed on server side");
                printf("Error : tarfgetz failed\n");
            }
        }else if (strcmp(inCmd, "getdirf") == 0){

            int filesgetresult = process_getdirf(server_sd, inArgs);
          
            if(filesgetresult == 1){
                processTextOutputResp(server_sd, "Command failed on server side");
                printf("Error : getdirf failed\n");
            }   

        }
        else if (strcmp(inCmd, "fgets") == 0)
        {
            int filesgetresult = process_fgets(server_sd, inArgs, numInArgs);

            if(filesgetresult == 1){
                processTextOutputResp(server_sd, "Command failed on server side");
                printf("Error : fgets failed\n");
            }
        }
        else if (strcmp(inCmd, "targzf") == 0)
        {
            int filesgetresult= process_targzf(server_sd, inArgs, numInArgs);
            
            if (filesgetresult== 1) {
                processTextOutputResp(server_sd, "Command failed on server side");
                printf("Error : targzf failed\n");
            }
        }
        else if (strcmp(inCmd, "quit") == 0)
        {       
            processTextOutputResp(server_sd, "Socket connection is closed as requested");
            close(server_sd); 
            printf("Socket connection close request by client %d is processed\n", numClientConnected);
            break;
        }  
        else
        {   
            processTextOutputResp(server_sd, "Please enter the valid command\n");
            continue;   
        }
    }
} 

int main(int argc, char *argv[])
{
    // variables for sockets and wait status
    int server_sd, conn_sd, clientWaitStat;
    
    // structure for server address info
    struct sockaddr_in serverIPSocketInfo;
   
    // socket creation in reliable byte stream connection - TCP
    if((server_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Error: Server socket could not be created\n");
        exit(1);
    }
   
    // adding server ip infromation to structure
    serverIPSocketInfo.sin_family = AF_INET; // ipv4
    serverIPSocketInfo.sin_addr.s_addr = htonl(INADDR_ANY); // all availables interfaces are listened
    serverIPSocketInfo.sin_port = htons(S_PORTNO); // port number defined above
   
    //server socket is binded to specified port and address; server socket in listening mode 
    bind(server_sd, (struct sockaddr *) &serverIPSocketInfo, sizeof(serverIPSocketInfo));
    listen(server_sd, 9);
   
    printf("Server socket is now on listen mode at port number %d\n", S_PORTNO);
  
    // infinite loop for client connections
    while(1)
    {
        // count for connected clints
        numClientConnected++;

        // accepting the request from the client
        conn_sd = accept(server_sd,(struct sockaddr *) NULL, NULL);
        printf("Connection request accepted for client %d\n", numClientConnected);
      
        // forking a new process to handle the client 
        if(!fork()){
            // client request processing 
            processclient(conn_sd);
            close(conn_sd);
            exit(0);
        }

        // waiting for child prcoesses to finish without blocking the loop
        waitpid(0, &clientWaitStat, WNOHANG);
    }

}



