Distributed File System using Sockets
Overview
This project implements a distributed file system using socket programming. The system is composed of three servers: Smain, Spdf, and Stext, and allows multiple client connections. Clients can upload, download, and manage files of three types (.c, .pdf, .txt), but they interact solely with Smain. Behind the scenes, Smain distributes files to the appropriate servers (Spdf for .pdf files and Stext for .txt files) without client awareness.

Project Structure
The project consists of four main components:

Smain.c - Main server handling all client requests and file distribution.
Spdf.c - Server dedicated to handling .pdf files.
Stext.c - Server dedicated to handling .txt files.
client24s.c - Client-side code that sends commands to Smain to perform file operations.
Servers
Smain: The main server that handles all client communications. It stores .c files locally but routes .pdf and .txt files to Spdf and Stext, respectively.
Spdf: A server that stores and processes all .pdf files sent from Smain.
Stext: A server that stores and processes all .txt files sent from Smain.
Client Commands
Clients communicate only with Smain and issue the following commands:

ufile <filename> <destination_path>: Uploads the specified file to Smain. Depending on the file type, Smain either stores the file locally (for .c files) or transfers it to Spdf or Stext (for .pdf and .txt files respectively).

dfile <filename>: Downloads the specified file from Smain to the client. Smain retrieves .pdf and .txt files from the appropriate servers (Spdf, Stext) before sending them to the client.

rmfile <filename>: Deletes the specified file from Smain. Depending on the file type, Smain either deletes the file locally (.c files) or sends a deletion request to Spdf or Stext.

dtar <filetype>: Creates a tar archive of the specified file type (.c, .pdf, .txt) and sends it to the client. For .pdf and .txt archives, Smain retrieves them from Spdf and Stext.

display <pathname>: Lists all files (.c, .pdf, .txt) in the specified directory on Smain. The list includes files stored in both Spdf and Stext.

Key Features
Multiple Client Support: Smain can handle multiple client connections by forking a new process for each client using the prcclient() function.

Transparent File Management: Clients interact only with Smain and are unaware of the file distribution across different servers.

Error Handling: Commands include basic error checking, such as file existence and path validation.
