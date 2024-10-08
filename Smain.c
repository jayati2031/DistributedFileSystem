//smain.c
//this is the main server program for a file management system, it handles various file operations like uploading, downloading, removing files,
//and creating tar archives. The server can handle .c files directly and communicates with other servers (Stext and Spdf) for .txt and .pdf files.
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <ftw.h>
#include <stdbool.h>
#include <limits.h>

//port numbers for different servers
#define PORT 3001
#define SPDF_PORT 3002
#define STEXT_PORT 3003
#define BUFFER_SIZE 1024
#define MAX_FILENAME 256

//global variables to store directory paths
char SMAIN_DIR[256];
char SPDF_DIR[256];
char STEXT_DIR[256];

//function prototypes
void prcclient(int client_socket);
void expand_path_for_home(const char* path, char* expanded_path);
int open_file_for_writing(int client_socket, char* filename, char* expanded_path);
int transfer_file_from_client(int source_fd, int dest_fd);
int transfer_file_to_from_txt_pdf(int source_fd, int dest_fd);
int transfer_data_from_fd(int source_fd, int dest_fd);
void function_to_process_ufile(int client_socket, char* filename, char* destination_path);
void function_to_process_dfile(int client_socket, char* filename);
void function_to_process_rmfile(int client_socket, char* filename);
void function_to_process_dtar(int client_socket, char* filetype);
void function_to_process_display(int client_socket, char* pathname);
int function_for_server_communications(int port, char* request, char* response);

//main function: Sets up the server, creates necessary directories,
//and enters an infinite loop to accept client connections.
int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    //get home directory
    const char *homedir = getenv("HOME");

    //set up directories
    snprintf(SMAIN_DIR, sizeof(SMAIN_DIR), "%s/smain", homedir);
    snprintf(SPDF_DIR, sizeof(SPDF_DIR), "%s/spdf", homedir);
    snprintf(STEXT_DIR, sizeof(STEXT_DIR), "%s/stext", homedir);

    //create directories if they don't exist
    mkdir(SMAIN_DIR, 0755);
    mkdir(SPDF_DIR, 0755);
    mkdir(STEXT_DIR, 0755);

    //create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set socket options to reuse address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    //set up server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    //start listening for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Smain server is running on port %d\n", PORT);

    //main server loop
    while(1) {
        //accept incoming connection
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        //fork a new process to handle the client
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            close(client_socket);
            continue;
        } else if (pid == 0) {
            //child process
            close(server_fd);
            prcclient(client_socket);
            exit(0);
        } else {
            //parent process
            close(client_socket);
            //wait for child processes to avoid zombies
            waitpid(-1, NULL, WNOHANG);
        }
    }

    close(server_fd);
    return 0;
}

//prcclient: Processes client requests in a loop until the client disconnects.
//it reads commands from the client and calls appropriate functions to handle them.
void prcclient(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    int valread;

    while (1) {
        //clear buffer and read client request
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(client_socket, buffer, BUFFER_SIZE);
        if (valread <= 0) {
            //client disconnected or error occurred
            break;
        }

        printf("Received command: %s\n", buffer);
        //parse command and arguments
        char command[10] = {0};
        char arg1[256] = {0};
        char arg2[256] = {0};
        sscanf(buffer, "%s %s %s", command, arg1, arg2);

        //process different commands
        switch(command[0]) {
            case 'u':
                if (strcmp(command, "ufile") == 0) {
                    function_to_process_ufile(client_socket, arg1, arg2);
                } else {
                    send(client_socket, "Invalid command", 15, 0);
                }
                break;
            case 'd':
                if (strcmp(command, "dfile") == 0) {
                    function_to_process_dfile(client_socket, arg1);
                } else if (strcmp(command, "dtar") == 0) {
                    function_to_process_dtar(client_socket, arg1);
                } else if (strcmp(command, "display") == 0) {
                    function_to_process_display(client_socket, arg1);
                } else {
                    send(client_socket, "Invalid command", 15, 0);
                }
                break;
            case 'r':
                if (strcmp(command, "rmfile") == 0) {
                    function_to_process_rmfile(client_socket, arg1);
                } else {
                    send(client_socket, "Invalid command", 15, 0);
                }
                break;
            default:
                send(client_socket, "Invalid command", 15, 0);
        }
    }
    close(client_socket);
}

//expand_path_for_home: Expands the '~' in a path to the user's home directory.
//this function is used to convert relative paths to absolute paths.
void expand_path_for_home(const char* path, char* expanded_path) {
    if (path[0] == '~') {
        //if path starts with '~', replace it with the home directory
        const char *homedir = getenv("HOME");
        snprintf(expanded_path, PATH_MAX, "%s%s", homedir, path + 1);
    } else {
        //otherwise, just copy the path as is
        strncpy(expanded_path, path, PATH_MAX);
    }
}

//open_file_for_writing: Opens a file for writing in the specified path.
//it creates the file if it doesn't exist and truncates it if it does.
int open_file_for_writing(int client_socket, char* filename, char* expanded_path) {
    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", expanded_path, filename);

    //open file with write, create, and truncate flags
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to create file");
        send(client_socket, "Failed to upload file", 21, 0);
        return -1; 
    }
    return fd;
}

//transfer_file_from_client: Transfers file data from the client to a file descriptor.
//it reads data in chunks and writes it to the destination file descriptor.
int transfer_file_from_client(int source_fd, int dest_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read, bytes_written;
    int total_bytes = 0;

    //read data from source and write to destination
    while ((bytes_read = recv(source_fd, buffer, BUFFER_SIZE, 0)) >= 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            send(source_fd, "Failed to write file", 20, 0);
            return -1;
        }
        //break the loop if we've received the entire file
        if (bytes_read < BUFFER_SIZE) {
            break;
        }
        total_bytes += bytes_written;
    }

    //check for receive error
    if (bytes_read < 0) {
        send(source_fd, "Failed to receive file", 22, 0);
        return -1;
    }
    return total_bytes;
}

//transfer_file_to_from_txt_pdf: Transfers file data between sockets.
//this function is used to forward data between the client and Stext/Spdf servers.
int transfer_file_to_from_txt_pdf(int source_fd, int dest_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    int total_bytes_forwarded = 0;

    //continue receiving and forwarding data until connection closes
    while (1) {
        bytes_received = recv(source_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            //forward received data to destination
            int bytes_sent = send(dest_fd, buffer, bytes_received, 0);
            if (bytes_sent != bytes_received) {
                printf("Failed to forward all data to Spdf\n");
                break;
            }
            total_bytes_forwarded += bytes_sent;
        } else if (bytes_received == 0) {
            //connection closed by client
            printf("File was empty\n");
            break;
        } else {
            //error in receiving data
            perror("recv failed");
            break;
        }
    }
    return total_bytes_forwarded;
}

//transfer_data_from_fd: Transfers data from a file descriptor to a socket.
//this function is used to send file contents to the client.
int transfer_data_from_fd(int source_fd, int dest_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    int total_bytes_sent = 0;

    //read from source and send to destination
    while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
        int sent = send(dest_fd, buffer, bytes_read, 0);
        if (sent < 0) {
            perror("Failed to send data");
            close(source_fd);
            return -1;
        }
        total_bytes_sent += sent;
    }
    return total_bytes_sent;
}

//function_to_process_ufile: Handles the 'ufile' command to upload a file.
//it determines the file type and processes it accordingly.
void function_to_process_ufile(int client_socket, char* filename, char* destination_path) {
    char expanded_path[PATH_MAX];
    expand_path_for_home(destination_path, expanded_path);

    //get file extension
    char *file_extension = strrchr(filename, '.');
    if (!file_extension || (strcmp(file_extension, ".c") != 0 && strcmp(file_extension, ".txt") != 0 && strcmp(file_extension, ".pdf") != 0)) {
        send(client_socket, "Invalid file type", 17, 0);
        return;
    }

    //send acceptance message to client
    send(client_socket, "File type accepted", 18, 0);

    //process .c files
    if (file_extension && strcmp(file_extension, ".c") == 0) {
        //create directories if they don't exist
        char *p = strchr(expanded_path + 1, '/');
        while (p) {
            *p = '\0';
            if (mkdir(expanded_path, 0755) == -1 && errno != EEXIST) {
                send(client_socket, "Failed to create directory", 26, 0);
                return;
            }
            *p = '/';
            p = strchr(p + 1, '/');
        }
        if (mkdir(expanded_path, 0755) == -1 && errno != EEXIST) {
            send(client_socket, "Failed to create directory", 26, 0);
            return;
        }

        //open file and transfer data
        int fd = open_file_for_writing(client_socket, filename, expanded_path);
        int bytes_transferred = transfer_file_from_client(client_socket, fd);
        close(fd);

        //send response to client
        if (bytes_transferred < 0) {
            send(client_socket, "Failed to upload file", 21, 0);
        } else {
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "File %s uploaded successfully.", filename);
            send(client_socket, response, strlen(response), 0);
        }
    } 
    //process .txt files
    else if (file_extension && strcmp(file_extension, ".txt") == 0) {
        //transfer .txt files to Stext
        char request[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        snprintf(request, sizeof(request), "ufile %s %s", filename, expanded_path);
        
        //connect to Stext server
        int stext_sock = function_for_server_communications(STEXT_PORT, NULL, NULL);
        if (stext_sock < 0) {
            send(client_socket, "Failed to connect to Stext server", 33, 0);
            return;
        }

        //send the request to Stext and forward data
        send(stext_sock, request, strlen(request), 0);
        int total_bytes_forwarded = transfer_file_to_from_txt_pdf(client_socket, stext_sock);
        printf("Total bytes forwarded to Stext: %d\n", total_bytes_forwarded);

        //signal end of file to Stext
        shutdown(stext_sock, SHUT_WR);

        //get response from Stext and send to client
        memset(response, 0, BUFFER_SIZE);
        int response_len = recv(stext_sock, response, BUFFER_SIZE - 1, 0);
        if (response_len > 0) {
            response[response_len] = '\0';
            printf("Response from Stext: %s\n", response);
            send(client_socket, response, response_len, 0);
        } else {
            send(client_socket, "No response from Stext server", 30, 0);
        }
        close(stext_sock);
    } 
    //process .pdf files
    else if (file_extension && strcmp(file_extension, ".pdf") == 0) {
        //transfer .pdf files to Spdf
        char request[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        snprintf(request, sizeof(request), "ufile %s %s", filename, expanded_path);
        
        //connect to Spdf server
        int spdf_sock = function_for_server_communications(SPDF_PORT, NULL, NULL);
        if (spdf_sock < 0) {
            send(client_socket, "Failed to connect to Spdf server", 32, 0);
            return;
        }

        //send the request to Spdf
        send(spdf_sock, request, strlen(request), 0);

        //forward the file content from client to Spdf
        int total_bytes_forwarded = transfer_file_to_from_txt_pdf(client_socket, spdf_sock);
        printf("Total bytes forwarded to Spdf: %d\n", total_bytes_forwarded);

        //signal end of file to Spdf
        shutdown(spdf_sock, SHUT_WR);

        //get response from Spdf and send to client
        memset(response, 0, BUFFER_SIZE);
        int response_len = recv(spdf_sock, response, BUFFER_SIZE - 1, 0);
        if (response_len > 0) {
            response[response_len] = '\0';
            printf("Response from Spdf: %s\n", response);
            send(client_socket, response, response_len, 0);
        } else {
            send(client_socket, "No response from Spdf server", 29, 0);
        }

        close(spdf_sock);
        return;
    }
    //add this line to ensure a response is always sent
    send(client_socket, "Upload processed", 16, 0);
}

//function_to_process_dfile: Handles the 'dfile' command to download a file.
//it determines the file type and processes the download accordingly.
void function_to_process_dfile(int client_socket, char* filename) {
    //get file extension
    char *file_extension = strrchr(filename, '.');
    if (!file_extension || (strcmp(file_extension, ".c") != 0 && strcmp(file_extension, ".txt") != 0 && strcmp(file_extension, ".pdf") != 0)) {
        send(client_socket, "Invalid file type", 17, 0);
        return;
    }

    //send acceptance message to client
    send(client_socket, "File type accepted", 18, 0);

    //process .c files
    if (file_extension && strcmp(file_extension, ".c") == 0) {
        char filepath[PATH_MAX];
        expand_path_for_home(filename, filepath);

        //open the file
        int fd = open(filepath, O_RDONLY);
        if (fd < 0) {
            char error_msg[BUFFER_SIZE];
            snprintf(error_msg, BUFFER_SIZE, "Failed to open file: %s", strerror(errno));
            send(client_socket, error_msg, strlen(error_msg), 0);
            return;
        }

        //transfer file data to client
        int total_data_sent = transfer_data_from_fd(fd, client_socket);
        close(fd);
        send(client_socket, "", 0, 0);
        printf("Total data sent to client: %d\n", total_data_sent);
    } 
    //process .txt and .pdf files
    else if (file_extension && (strcmp(file_extension, ".txt") == 0 || strcmp(file_extension, ".pdf") == 0)) {
        //determine appropriate server
        int port = (strcmp(file_extension, ".txt") == 0) ? STEXT_PORT : SPDF_PORT;
        char request[BUFFER_SIZE];
        char response[BUFFER_SIZE];

        //prepare request for Stext/Spdf server
        snprintf(request, sizeof(request), "dfile %s", filename);
        int sock = function_for_server_communications(port, request, response);
        if (sock < 0) {
            printf("Failed to communicate with server\n");
            send(client_socket, "Failed to retrieve file from server", 35, 0);
            return;
        }
        send(client_socket, response, strlen(response), 0);

        //forward the file content from Stext/Spdf to client
        int total_bytes_sent = transfer_file_to_from_txt_pdf(sock, client_socket);
        printf("Total file bytes sent to client: %d\n", total_bytes_sent);
        close(sock);
    }
}

//function_to_process_rmfile: Handles the 'rmfile' command to remove a file.
//it determines the file type and processes the removal accordingly.
void function_to_process_rmfile(int client_socket, char* filename) {
    //get file extension
    char *file_extension = strrchr(filename, '.');
    if (!file_extension || (strcmp(file_extension, ".c") != 0 && strcmp(file_extension, ".txt") != 0 && strcmp(file_extension, ".pdf") != 0)) {
        send(client_socket, "Invalid file type", 17, 0);
        return;
    }

    //send acceptance message to client
    send(client_socket, "File type accepted", 18, 0);

    //process .c files
    if (file_extension && strcmp(file_extension, ".c") == 0) {
        //remove .c files locally
        char filepath[PATH_MAX];
        expand_path_for_home(filename, filepath);

        if (remove(filepath) == 0) {
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "File %s removed\n", filename);
            send(client_socket, response, strlen(response), 0);
        } else {
            perror("Failed to remove file");
            send(client_socket, "Failed to remove file", 21, 0);
        }
    } 
    //process .txt and .pdf files
    else if (file_extension && (strcmp(file_extension, ".txt") == 0 || strcmp(file_extension, ".pdf") == 0)) {
        //request file removal from appropriate server
        int server_port = (strcmp(file_extension, ".txt") == 0) ? STEXT_PORT : SPDF_PORT;
        char request[BUFFER_SIZE];
        char response[BUFFER_SIZE];
        snprintf(request, sizeof(request), "rmfile %s", filename);
        int server_sock = function_for_server_communications(server_port, request, response);
        if (server_sock < 0) {
            send(client_socket, "Failed to connect to server", 27, 0);
        } else {
            send(client_socket, response, strlen(response), 0);
            close(server_sock);
        }
    }
    //add this line to ensure a response is always sent
    send(client_socket, "Remove processed", 16, 0);
}

//function_to_process_dtar: Handles the 'dtar' command to create and download a tar archive.
//it creates a tar archive of files based on the specified file type.
void function_to_process_dtar(int client_socket, char* filetype) {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    //get file extension
    if (!filetype || (strcmp(filetype, ".c") != 0 && strcmp(filetype, ".txt") != 0 && strcmp(filetype, ".pdf") != 0)) {
        send(client_socket, "Invalid file type", 17, 0);
        return;
    }

    //send acceptance message to client
    send(client_socket, "File type accepted", 18, 0);

    //process .c files
    if (strcmp(filetype, ".c") == 0) {
        char tar_file_path[MAX_FILENAME];
        //create tar of .c files locally
        snprintf(tar_file_path, sizeof(tar_file_path), "%s/cfiles.tar", SMAIN_DIR);
        //remove existing tar file if any
        remove(tar_file_path);
        char command[BUFFER_SIZE];
        snprintf(command, BUFFER_SIZE, "tar -cf %s -C %s .", tar_file_path, SMAIN_DIR);
        system(command);

        //send tar file to client
        int fd = open(tar_file_path, O_RDONLY);
        if (fd < 0) {
            perror("Failed to open tar file");
            send(client_socket, "Failed to create tar file", 25, 0);
            return;
        }

        //get file size for logging
        struct stat file_stat;
        if (fstat(fd, &file_stat) < 0) {
            perror("Failed to get file size");
            close(fd);
            send(client_socket, "Failed to get file size", 23, 0);
            return;
        }

        int total_data_sent = transfer_data_from_fd(fd, client_socket);
        printf("Total data sent to client: %d\n", total_data_sent);

        //send end-of-file marker
        char eof_marker[8] = "EOF\n";
        send(client_socket, eof_marker, strlen(eof_marker), 0);

        close(fd);
        //clean up the tar file after sending
        remove(tar_file_path);
    } 
    //process .txt files
    else if (strcmp(filetype, ".txt") == 0) {
        snprintf(request, sizeof(request), "dtar %s", filetype);
        int stext_sock = function_for_server_communications(STEXT_PORT, request, response);
        if (stext_sock < 0) {
            send(client_socket, "Failed to communicate with Stext server", 39, 0);
            return;
        }
        //transfer data from stext to client
        int total_bytes_sent = transfer_file_to_from_txt_pdf(stext_sock, client_socket);
        printf("Total bytes sent to client: %zu\n", total_bytes_sent);
        close(stext_sock);
    } 
    //process .pdf files
    else if (strcmp(filetype, ".pdf") == 0) {
        snprintf(request, sizeof(request), "dtar %s", filetype);
        int spdf_sock = function_for_server_communications(SPDF_PORT, request, response);
        if (spdf_sock < 0) {
            send(client_socket, "Failed to communicate with Spdf server", 38, 0);
            return;
        }
        //transfer data from spdf to client
        int total_bytes_sent = transfer_file_to_from_txt_pdf(spdf_sock, client_socket);
        printf("Total bytes sent to client: %zu\n", total_bytes_sent);
        close(spdf_sock);
    }
}

//function_to_process_display: Handles the 'display' command to list files in a directory.
//it gathers file lists from local storage and remote servers.
void function_to_process_display(int client_socket, char* pathname) {
    //send acceptance message to client
    send(client_socket, "In display function", 19, 0);

    char response[BUFFER_SIZE] = "";
    char c_files[BUFFER_SIZE] = "";
    char pdf_files[BUFFER_SIZE] = "";
    char txt_files[BUFFER_SIZE] = "";
    char full_path[PATH_MAX];
    
    expand_path_for_home(pathname, full_path);

    //get list of .c files locally
    DIR *dir;
    struct dirent *ent;
    struct stat st;
    if ((dir = opendir(full_path)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            char file_path[PATH_MAX];
            snprintf(file_path, PATH_MAX, "%s/%s", full_path, ent->d_name);
            if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode) && strstr(ent->d_name, ".c") != NULL) {
                strcat(c_files, ent->d_name);
                strcat(c_files, "\n");
            }
        }
        closedir(dir);
    }

    //get list of .pdf files from Spdf
    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "display %s", pathname);
    int spdf_sock = function_for_server_communications(SPDF_PORT, request, pdf_files);
    if (spdf_sock < 0) {
        strcat(pdf_files, "Failed to get PDF files\n");
    }
    close(spdf_sock);

    //get list of .txt files from Stext
    int stext_sock = function_for_server_communications(STEXT_PORT, request, txt_files);
    if (stext_sock < 0) {
        strcat(txt_files, "Failed to get TXT files\n");
    }
    close(stext_sock);

    //combine all lists
    strcat(response, c_files);
    strcat(response, pdf_files);
    strcat(response, txt_files);

    if (response == 0) {
        strcpy(response, "No files found in the directory");
    }

    send(client_socket, response, strlen(response), 0);
    printf("Display request processed\n");
}

//function_for_server_communications: Establishes a connection with a server and sends/receives data.
//it's used for communicating with Stext and Spdf servers.
int function_for_server_communications(int port, char* request, char* response) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    //create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    //set up server address structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    //convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        close(sock);
        return -1;
    }

    //connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        close(sock);
        return -1;
    }

    //send request if provided
    if (request && strlen(request) > 0) {
        send(sock, request, strlen(request), 0);
    }

    //read response if buffer provided
    if (response) {
        read(sock, response, BUFFER_SIZE);
    }
    return sock;
}