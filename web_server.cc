
#include <regex>
#include "web_server.h"
bool VERBOSE;


// **************************************************************************************
// * processConnection()
// * - Handles reading the line from the network and sending it back to the client.
// * - Returns true if the client sends "QUIT" command, false if the client sends "CLOSE".
// **************************************************************************************
#define BUFFER_SIZE 1024

// **************************************************************************************
// * Read the header. Return the appropriate status code and filename
// **************************************************************************************

int readRequest(int connFd, std::string& filename) {
	int returnCode = 400; //Default return code
	
	//read everything up to and including end of the header:
	std::string requestHeader;
	char buffer[BUFFER_SIZE];
	ssize_t bytesRead;

	while(true) {
		bytesRead = read(connFd, buffer, sizeof(buffer));
		
		//check if there is a client connection
		if (bytesRead <= 0) {
			return returnCode;
		}

		//append the data from client to requestHeader
		requestHeader.append(buffer, bytesRead);

		//this will check if the end of requestHeader is reached
		size_t endOfHeader = requestHeader.find("\r\n\r\n");
		if (endOfHeader != std::string::npos) {
			break;
		}
	}

	//output the request header
	if (VERBOSE) {
		std::cout << "----------------------- Start of header ----------------------" << std::endl;
		std::cout << requestHeader << std::endl;
		std::cout << "----------------------- End of header ------------------------" << std::endl;
	}

	//split header into different lines
	std::istringstream headerStream(requestHeader);
	std::string line;

	//Look at the first line of the header
	if (std::getline(headerStream, line)) {
		//check for valid GET
		if (line.find("GET ") == 0) {
			//find the filename
			size_t firstSpaceIndex = line.find(' ', 4);
			if (firstSpaceIndex != std::string::npos) {
				std::string requestedFile = line.substr(4, firstSpaceIndex - 4);

				//check for valid filename
				if (std::regex_match(requestedFile, std::regex("^/(file[0-9].html|image[0-9].jpg)$"))) {
					returnCode = 200; //if filename is valid, set return code to 200
					filename = requestedFile.substr(1);
				} else {
					returnCode = 404; //if filename is invalid, set return code to 404

				}
			}
		}
	}

	return returnCode;
}
//The above section of the code is base on assistance from ChatGPT, a language model by OpenAI.
//Reference: https://openai.com/chatgpt

int sendLine(int connFd, const std::string& stringToSend) {
	char buffer[stringToSend.size() + 2]; //char array that is 2 bytes longer than the string

	//copy string to bugger
	std::copy(stringToSend.begin(), stringToSend.end(), buffer);

	//replace the last two bytes of the array with the <CR> and <LF>
	buffer[stringToSend.size()] = '\r';
	buffer[stringToSend.size() + 1] = '\n';

	//call write function to send that array
	ssize_t bytesWritten = write(connFd, buffer, sizeof(buffer));

	//check for errors
	if (bytesWritten == -1) {
		return -1;
	} else if (bytesWritten != sizeof(buffer)) {
		return -2;
	}

	return 0;
}

int send404(int connFd) {

	int result;

	//debug
	//std::cout << "Sending File Not Found (404) response." << std::endl;
	
	//using sendLine():
	if ((result = sendLine(connFd, "HTTP/1.1 400 Not Found")) != 0) return result; //send properly formmated http response w/ the error code 404
	if ((result = sendLine (connFd, "Content-Type: text/html")) != 0) return result; //send content type message
	if ((result = sendLine (connFd, "")) != 0) return result; //blank line
	if ((result = sendLine (connFd, "<html><body><h1>404 Not Found</h1></body></html>")) != 0) return result; //friendly message
	if ((result = sendLine (connFd, "")) != 0) return result; //black line indicating end of message
	
	return 0;
}

int send400(int connFd) {
	int result;

	std::cout << "Sending Bad Request (400) response." << std::endl;

	if ((result = sendLine(connFd, "HTTP/1.1 400 Bad Request")) != 0) return result; //send a properly formatted HTTP response with error code 400
	if ((result = sendLine(connFd, "")) != 0) return result; //blank line

	return 0;
}

int send200(int connFd, const std::string& filename) {
	struct stat fileStat; //use stat() function to find size of file
	std::string fullPath = "./" + filename;

	if (stat(fullPath.c_str(), &fileStat) == -1 || S_ISDIR(fileStat.st_mode)) { //check if stat function fails, dont read permission or check if file doesn't exist. send 404 and exit
		send404(connFd);
		return -1;
	}

	//debug
	//std::cout << "Attempting to open file: " << fullPath << std::endl;

	//call open() to open file in binary
	int fileFd = open(fullPath.c_str(), O_RDONLY);

	if (fileFd == -1) {
		std::cout << "Failed to open file: " << strerror(errno) << std::endl;
		send404(connFd);
		return -1;
	}

	//debug
	//std::cout << "Sending file: " << filename << std::endl;

	//use sendLine() to send the header
	std::string contentType;
	if (filename.find(".html") != std::string::npos) {
		contentType = "text/html";
	} else if (filename.find(".jpg") != std::string::npos || filename.find(".jpeg") != std::string::npos) {
		contentType = "image/jpeg";
	} else {
		contentType = "application/octet-stream";
	} //this handles other file types

	//find content length
	off_t contentLength = fileStat.st_size;

	//send a properly formatted HTTP response with the code 200
	std::string response = "HTTP/1.1 200 OK\r\n";
	response += "Content-Type: " + contentType + "\r\n"; //send content type depending on file type
	response += "Content-Length: " + std::to_string(contentLength) + "\r\n"; //send length type

	if (sendLine(connFd, response) != 0) {
		close(fileFd);
		return -2;
	}

	//send file data using read and write
	const int bufferSize = 1024;
	char buffer[bufferSize];
	ssize_t bytesRead;

	while ((bytesRead = read(fileFd, buffer, sizeof(buffer))) > 0) {
		ssize_t bytesWritten = write(connFd, buffer, bytesRead);

		if (bytesWritten == -1) {
			close(fileFd);
			return -3;
		}
		
	}

	close(fileFd);
	return 0;
}

//The above section of the code for function send200 was based on the assistance received from Chat GPT, a language model by Open AI.
//Reference: https://chat.openai.com/chatgpt


int processConnection(int connFd) {
	std::string filename;
	int returnCode = readRequest(connFd, filename);

	//this checks for the favicon request so it does not freeze up when it is accessing the file
	if (filename == "favicon.ico") {
		sendLine(connFd, "HTTP/1.1 204 No Content");
		close(connFd);
		return 0;
	}

	// this section of the if statement code was based on the assistance I received from Chat GPT, a language model by Open AI.
	// Reference: https://chat.openai.com/chatgpt

	//checks the correct return code
	if (returnCode == 400) {
		//std::cout << "Bad Request (400) received." << std::endl;
		send400(connFd);
	
	} else if (returnCode == 404) {
		//std::cout << "File Not Found (404) for: " << filename << std::endl;
		send404(connFd);

	} else {
		//std::cout << "Processing successful request for: " << filename << std::endl;
		send200(connFd, filename);
	}

	close(connFd);

	return 0;
}



// **************************************************************************************
// * main()
// * - Sets up the sockets and accepts new connection until processConnection() returns 1
// **************************************************************************************

int main (int argc, char *argv[]) {

  // ********************************************************************
  // * Process the command line arguments
  // ********************************************************************
  int opt = 0;
  while ((opt = getopt(argc,argv,"v")) != -1) {
    
    switch (opt) {
    case 'v':
      VERBOSE = true;
      break;
    case ':':
    case '?':
    default:
      std::cout << "useage: " << argv[0] << " -v" << std::endl;
      exit(-1);
    }
  }

  // *******************************************************************
  // * Creating the inital socket is the same as in a client.
  // ********************************************************************
  int listenFd = socket(AF_INET, SOCK_STREAM, 0); // Call socket() to create the socket you will use for lisening.

  //check if the socket is made correctly
  if (listenFd == -1) {
	  std::cout << "Failed to create listening socket " << strerror(errno) << std::endl;
	  exit(-1); // if socket() failed, abort program (can't recover). 
  }


  //DEBUG << "Calling Socket() assigned file descriptor " << listenFd << ENDL;

  
  // ********************************************************************
  // * The bind() and calls take a structure that specifies the
  // * address to be used for the connection. On the cient it contains
  // * the address of the server to connect to. On the server it specifies
  // * which IP address and port to lisen for connections.
  // ********************************************************************
  struct sockaddr_in servaddr; //define the structure
  srand(time(NULL));
  u_int16_t port = (rand() % 10000) + 1024; //port number
  bzero(&servaddr, sizeof(servaddr)); //zero the whole thing
  servaddr.sin_family = AF_INET; //IPv4 protocal family
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //let the system pick the IP address
  servaddr.sin_port = htons(port); //pick random high-number port

  // ********************************************************************
  // * Binding configures the socket with the parameters we have
  // * specified in the servaddr structure.  This step is implicit in
  // * the connect() call, but must be explicitly listed for servers.
  // ********************************************************************
  //
  // Call bind() multiple times if another person is using the port already

  //DEBUG << "Calling bind(" << listenFd << "," << &servaddr << "," << sizeof(servaddr) << ")" << ENDL;

  bool bindSuccessful = false;
  while (!bindSuccessful && port >= 1024) {
	if (bind(listenFd, (struct sockaddr *) &servaddr, sizeof(servaddr)) <0) {
		std::cout << "bind() failed: " << strerror(errno) << std::endl;
		port--; //Try another port
		servaddr.sin_port = htons(port); //Update port in servaddr struct
	
	//This part of the code is based on assistance from ChatGPT, a language model by OpenAI
	//Reference: https://openai.com/chatgpt
			
  } else {
	  bindSuccessful = true; //Bind works
	  std::cout << "Using port: " << port << std::endl;
  	}
  } 

  if (!bindSuccessful) {
	  std::cout << "bind() failed in all ports." << std::endl;
	  exit(-1);
  } 


  // ********************************************************************
  // * Setting the socket to the listening state is the second step
  // * needed to being accepting connections.  This creates a queue for
  // * connections and starts the kernel listening for connections.
  // ********************************************************************
  int listenQueueLength = 1;
  if( listen(listenFd, listenQueueLength) <0) {
	  std::cout << "listen() failed: " << strerror(errno) << std::endl;
	  exit(-1);
  }
  //DEBUG << "Calling listen(" << listenFd << "," << listenQueueLength << ")" << ENDL;

  // ********************************************************************
  // * The accept call will sleep, waiting for a connection.  When 
  // * a connection request comes in the accept() call creates a NEW
  // * socket with a new fd that will be used for the communication.
  // ********************************************************************

  bool quitProgram = false;
  while (!quitProgram) {
    int connFd = accept(listenFd, (struct sockaddr *) NULL, NULL);
    if (connFd < 0) {
	    std::cout << "accept() failed: " << strerror(errno) << std::endl;
	    exit(-1);
    }

    //DEBUG << "Calling accept(" << listenFd << "NULL,NULL)." << ENDL;

    // The accept() call checks the listening queue for connection requests.
    // If a client has already tried to connect accept() will complete the
    // connection and return a file descriptor that you can read from and
    // write to. If there is no connection waiting accept() will block and
    // not return until there is a connection.
    
    //DEBUG << "We have recieved a connection on " << connFd << ENDL;

    
    quitProgram = processConnection(connFd);
   
    close(connFd);
  }

  close(listenFd);

}

