Overview

This project is a multithreaded HTTP server implemented in C, designed to handle concurrent client requests efficiently. The server supports basic HTTP methods GET and PUT, and uses synchronization primitives to ensure thread-safe operations on shared resources.

Features

• Multithreaded Architecture: Utilizes worker threads to handle multiple client requests simultaneously.

• Thread-Safe File Access: Implements reader-writer locks for synchronized file operations.

• HTTP Methods Supported:

	• GET: Fetches and serves files from the server.
 
	• PUT: Accepts and writes files to the server.
 
	• UNSUPPORTED: Handles unsupported HTTP methods with a 501 Not Implemented response.
• Logging: Audits HTTP requests and responses with details such as method, URI, status code, and request ID.

• Dynamic Thread Management: The number of worker threads can be configured via command-line arguments.

 Requirements
 
  	• POSIX-compliant operating system
   
  	• Libraries: pthread, unistd, fcntl, stdio, stdlib, string.h
   
  	• Run the make command in the project directory to compile the httpserver binary.

 Usage
  	
	./http_server -t <thread_count> <port>
  
 	Arguments
	• -t <thread_count>: (Optional) Number of worker threads. Default is 4.
	• <port>: Port number to listen for incoming connections (1–65535).
