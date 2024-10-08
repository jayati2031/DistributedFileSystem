//spdf.c
//this program implements a server for storing, retrieving, and managing pdf files.
//it uses socket programming to handle client requests and performs various file operations.
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <ftw.h>
#include <stdbool.h>
#include <errno.h>

//define constants for server configuration
#define PORT 3002
#define BUFFER_SIZE 1024
#define SO_REUSEPORT 15

//global variable to store the path of the spdf directory
char SPDF_DIR[PATH_MAX];

//function prototypes
void handle_client_request(int client_socket);
void function_for_ufile_dfile_rmfile(int client_socket, char* filename, char* destination_path, int operation);
void function_to_create_tar(int client_socket);
void function_to_display_all_files(int client_socket, char* pathname);
char* get_home_directory();
void expand_path_for_home(char* expanded_path, const char* path);
void replace_smain_with_spdf(char* path);
void create_path_directories(const char* path);
void send_response_to_client(int client_socket, const char* message);
int open_file_with_flag(const char* filepath, int flags);
void send_file_content(int client_socket, int fd);
void receive_and_write_file(int client_socket, int fd);

//enum to represent different file operations
enum FileOperation {
    STORE_PDF,
    RETRIEVE_PDF,
    REMOVE_PDF
};

//main function: Sets up the server socket, initializes the SPDF directory,
//and enters an infinite loop to accept and handle client connections.
int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    //set up the SPDF directory in the user's home folder
    snprintf(SPDF_DIR, sizeof(SPDF_DIR), "%s/spdf", get_home_directory());
    mkdir(SPDF_DIR, 0755);

    //create a socket for the server
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set socket options to reuse address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //set up the server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //bind the socket to the specified address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //start listening for client connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Spdf server is running on port %d\n", PORT);

    //main server loop
    while(1) {
        //accept a client connection
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        //handle the client request
        handle_client_request(client_socket);
        close(client_socket);
    }

    return 0;
}

//handle client request: Reads the client's command and arguments,
//then calls the appropriate function based on the command.
void handle_client_request(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    int valread = read(client_socket, buffer, BUFFER_SIZE);
    if (valread < 0) {
        perror("read failed");
        return;
    }

    printf("Received request: %s\n", buffer);

    //parse the command and arguments
    char command[10] = {0};
    char arg1[256] = {0};
    char arg2[256] = {0};
    sscanf(buffer, "%s %s %s", command, arg1, arg2);

    //determine which operation to perform based on the command
    switch(command[0]) {
        case 'u':
            if (strcmp(command, "ufile") == 0) {
                function_for_ufile_dfile_rmfile(client_socket, arg1, arg2, STORE_PDF);
            }
            break;
        case 'd':
            switch(command[1]) {
                case 'f':
                    if (strcmp(command, "dfile") == 0) {
                        function_for_ufile_dfile_rmfile(client_socket, arg1, NULL, RETRIEVE_PDF);
                    }
                    break;
                case 't':
                    if (strcmp(command, "dtar") == 0) {
                        function_to_create_tar(client_socket);
                    }
                    break;
                case 'i':
                    if (strcmp(command, "display") == 0) {
                        function_to_display_all_files(client_socket, arg1);
                    }
                    break;
            }
            break;
        case 'r':
            if (strcmp(command, "rmfile") == 0) {
                function_for_ufile_dfile_rmfile(client_socket, arg1, NULL, REMOVE_PDF);
            }
            break;
        default:
            send_response_to_client(client_socket, "Invalid command");
    }
}

//function to handle uploading, downloading, and removing PDF files.
//it expands the file path, replaces 'smain' with 'spdf' in the path,cand performs the requested operation.
void function_for_ufile_dfile_rmfile(int client_socket, char* filename, char* destination_path, int operation) {
    char expanded_path[PATH_MAX];
    expand_path_for_home(expanded_path, destination_path ? destination_path : filename);
    replace_smain_with_spdf(expanded_path);

    switch(operation) {
        case STORE_PDF: {
            //create necessary directories and open the file for writing
            create_path_directories(expanded_path);
            char filepath[PATH_MAX];
            snprintf(filepath, sizeof(filepath), "%s/%s", expanded_path, filename);
            
            int fd = open_file_with_flag(filepath, O_WRONLY | O_CREAT | O_TRUNC);
            if (fd < 0) {
                send_response_to_client(client_socket, "Failed to store PDF");
                return;
            }
            
            //receive the file content from the client and write it to the file
            receive_and_write_file(client_socket, fd);
            close(fd);
            break;
        }
        case RETRIEVE_PDF: {
            //open the file for reading
            int fd = open_file_with_flag(expanded_path, O_RDONLY);
            if (fd < 0) {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, BUFFER_SIZE, "Failed to open file: %s", strerror(errno));
                send_response_to_client(client_socket, error_msg);
                return;
            }

            //get the file size for logging
            struct stat file_stat;
            if (fstat(fd, &file_stat) < 0) {
                perror("Failed to get file size");
                close(fd);
                send(client_socket, "Failed to get file size", 23, 0);
                return;
            }
            printf("File size: %ld bytes\n", file_stat.st_size);

            //send the file content to the client
            send_file_content(client_socket, fd);
            close(fd);
            break;
        }
        case REMOVE_PDF: {
            //remove the specified PDF file
            if (remove(expanded_path) == 0) {
                char response[BUFFER_SIZE];
                snprintf(response, BUFFER_SIZE, "Pdf file %s removed successfully", expanded_path);
                send_response_to_client(client_socket, response);
            } else {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, BUFFER_SIZE, "Failed to remove PDF: %s", strerror(errno));
                send_response_to_client(client_socket, error_msg);
            }
            break;
        }
    }
}

//function to create a tar archive of all PDF files in the SPDF directory
//and send it to the client.
void function_to_create_tar(int client_socket) {
    char tar_file_path[PATH_MAX];
    snprintf(tar_file_path, sizeof(tar_file_path), "%s/pdf.tar", SPDF_DIR);

    //remove existing tar file if it exists
    remove(tar_file_path);

    //prepare the tar command
    char tar_command[BUFFER_SIZE];
    snprintf(tar_command, sizeof(tar_command), "tar -cf %s -C %s .", tar_file_path, SPDF_DIR);
    
    //execute the tar command to create the archive
    int result = system(tar_command);
    if (result != 0) {
        perror("Failed to create tar file");
        send(client_socket, "Failed to create tar file", 25, 0);
        return;
    }

    //open the created tar file for reading
    int fd = open(tar_file_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open tar file");
        send(client_socket, "Failed to open tar file", 23, 0);
        return;
    }

    //get the tar file size for logging
    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        perror("Failed to get file size");
        close(fd);
        send(client_socket, "Failed to get file size", 23, 0);
        return;
    }

    //send the tar file content to the client
    char buffer[BUFFER_SIZE];
    int bytes_read;
    int total_bytes_sent = 0;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        int bytes_sent = send(client_socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("Failed to send tar data");
            break;
        }
        total_bytes_sent += bytes_sent;
    }

    printf("Total bytes sent: %zu\n", total_bytes_sent);

    //send end-of-file marker
    char eof_marker[8] = "EOF\n";
    send(client_socket, eof_marker, strlen(eof_marker), 0);

    //clean up
    close(fd);
    //remove the temporary tar file
    remove(tar_file_path);
}

//function to display all PDF files in a specified directory.
//it lists all files with a .pdf extension in the given path.
void function_to_display_all_files(int client_socket, char* pathname) {
    char dirpath[512];
    
    //construct the full directory path
    if (strncmp(pathname, "~/smain", 7) == 0) {
        snprintf(dirpath, sizeof(dirpath), "%s%s", SPDF_DIR, pathname + 7);
    } else {
        snprintf(dirpath, sizeof(dirpath), "%s/%s", SPDF_DIR, pathname);
    }

    DIR *dir;
    struct dirent *ent;
    struct stat st;
    char response[BUFFER_SIZE] = "";
    char fullpath[PATH_MAX];

    //open the directory and iterate through its contents
    if ((dir = opendir(dirpath)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, ent->d_name);
            if (stat(fullpath, &st) == 0) {
                //check if the file is a regular file and has a .pdf extension
                if (S_ISREG(st.st_mode) && strstr(ent->d_name, ".pdf") != NULL) {
                    strcat(response, ent->d_name);
                    strcat(response, "\n");
                }
            }
        }
        closedir(dir);
    }

    //send the list of PDF files to the client
    send(client_socket, response, strlen(response), 0);
}

//function to get the user's home directory path.
//it uses the HOME environment variable.
char* get_home_directory() {
    const char *homedir = getenv("HOME");
    return (char*)homedir;
}

//function to expand the '~' symbol in a path to the full home directory path.
//if the path doesn't start with '~', it returns the original path.
void expand_path_for_home(char* expanded_path, const char* path) {
    if (path[0] == '~') {
        snprintf(expanded_path, PATH_MAX, "%s%s", get_home_directory(), path + 1);
    } else {
        strncpy(expanded_path, path, PATH_MAX);
    }
}


//function to replace 'smain' with 'spdf' in a given path.
//this is used to convert client-side paths to server-side paths.
void replace_smain_with_spdf(char* path) {
    char *smain_pos = strstr(path, "/smain");
    if (smain_pos != NULL) {
        int remaining_length = strlen(smain_pos + strlen("/smain"));
        memmove(smain_pos + strlen("/spdf"), smain_pos + strlen("/smain"), remaining_length + 1);
        memcpy(smain_pos, "/spdf", strlen("/spdf"));
    }
}

//bunction to create all directories in a given path.
//it creates each directory in the path if it doesn't exist.
void create_path_directories(const char* path) {
    char temp[PATH_MAX];
    char *p = NULL;
    int len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    if (temp[len - 1] == '/')
        temp[len - 1] = 0;
    for (p = temp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(temp, 0755);
            *p = '/';
        }
    mkdir(temp, 0755);
}

//function to send a response message to the client.
//it sends the message through the provided client socket.
void send_response_to_client(int client_socket, const char* message) {
    send(client_socket, message, strlen(message), 0);
}

//function to open a file with specified flags.
//it returns the file descriptor or -1 if the operation fails.
int open_file_with_flag(const char* filepath, int flags) {
    int fd = open(filepath, flags, 0644);
    if (fd < 0) {
        perror("Failed to open file");
    }
    return fd;
}

//function to send file content to the client.
//it reads the file in chunks and sends each chunk to the client socket.
void send_file_content(int client_socket, int fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    int total_bytes_sent = 0;

    //read from file and send to client in chunks
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        int sent = send(client_socket, buffer, bytes_read, 0);
        if (sent < 0) {
            perror("Failed to send data");
            break;
        }
        total_bytes_sent += sent;
    }

    //print the total number of bytes sent for logging
    printf("Total file bytes sent: %zu\n", total_bytes_sent);
}

//function to receive file content from the client and write it to a file.
//it receives data in chunks and writes each chunk to the file.
void receive_and_write_file(int client_socket, int fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    int total_bytes_received = 0;

    //receive data from client and write to file in chunks
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (write(fd, buffer, bytes_received) != bytes_received) {
            perror("Failed to write file");
            return;
        }
        total_bytes_received += bytes_received;
    }

    //print the total number of bytes received and written for logging
    printf("Total bytes received and written: %d\n", total_bytes_received);
}