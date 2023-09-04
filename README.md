# SocketShare-Unix-File-Transfer-Application
This application is developed for COMP-8567 course at an University of Windsor during May 2023 semester.

This client-server application enables file transfer and data retrieval between multiple clients and a central server, all operating on different machines. Clients can request files from the server using specific commands. The server, along with its mirror, listens for client requests and responds accordingly. Upon a connection request from a client, the server forks a child process, allowing it to exclusively serve that client's request. After processing the client's command, the result is returned to the client, and the function exits upon receiving a "quit" command.

Clients operate within an infinite loop, waiting for user-entered commands. Once a command is entered, the client verifies its syntax and sends it to the server for processing, displaying an appropriate error message if the command syntax is incorrect. The available commands include fetching specific files, retrieving files within size constraints, searching for files by name, obtaining files based on file types, and retrieving files created within specified date ranges.

List of commands - 
1. fgets: The server must search the files (file 1 ..up to file4) in its directory tree rooted at ~ and return temp.tar.gz that contains at least one (or more of the listed files) if they are present 
2. tarfgetz: The server must return to the client temp.tar.gz that contains all the files in the directory tree rooted at its ~ whose file-size in bytes is >=size1 and <=size2 
3. filesrch: If the file filename is found in its file directory tree rooted at ~, the server must return the filename, size(in bytes), and date created to the client and the client prints the received information on its terminal. 
4. targzf1: the server must return temp.tar.gz that contains all the files in its directory tree rooted at ~ belonging to the file type/s listed in the extension list, else the server sends the message “No file found” to the client (which is printed on the client terminal by the client) 
5. getdirf: The server must return to the client temp.tar.gz that contains all the files in the directory tree rooted at ~ whose date of creation is <=date2 and >=date1
6. quit: End the client session

How to run-
1. run the server.c file
2. Run the mirror.c file
3. Run the client.c file and provide the ip address of server as an argument eg ./client 0.0.0.0  
