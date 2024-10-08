//client24s.c
//this program implements a client for interacting with the Smain server.
//it allows users to send various file-related commands and handle the responses.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#define PORT 3001
#define BUFFER_SIZE 1024

//function prototypes
int function_for_server_connection();
int function_to_send_socket_command(int sockfd, const char* command);
int function_to_validate_command(const char* command);
void function_to_handle_ufile(int sockfd, const char* filename);
void function_to_handle_dfile(int sockfd, const char* filename);
void function_to_handle_remove(int sockfd);
void function_to_handle_dtar(int sockfd, const char* filetype);
void function_to_handle_display(int sockfd);

//main function: Handles user input and directs program flow
int main() {
    //establish connection to the server
    int sockfd = function_for_server_connection();
    if (sockfd < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        return 1;
    }

    printf("Connected to Smain server. Enter commands:\n");

    //main loop for handling user commands
    char command[BUFFER_SIZE];
    while (1) {
        printf("> ");
        if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        //remove newline
        command[strcspn(command, "\n")] = 0;

        //check for exit command
        if (strcmp(command, "exit") == 0) {
            break;
        }

        //validate and process the command
        if (function_to_validate_command(command)) {
            char cmd[10], arg1[256], arg2[256];
            sscanf(command, "%s %s %s", cmd, arg1, arg2);

            //send command to server
            if (function_to_send_socket_command(sockfd, command) < 0) {
                fprintf(stderr, "Failed to send command\n");
                close(sockfd);
                sockfd = function_for_server_connection();
                if (sockfd < 0) {
                    fprintf(stderr, "Failed to reconnect to server\n");
                    exit(1);
                }
                continue;
            }

            //wait for server response
            char response[BUFFER_SIZE];
            int bytes_received = recv(sockfd, response, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                fprintf(stderr, "Failed to receive server response\n");
                continue;
            }
            response[bytes_received] = '\0';

            //check if server accepted the file
            if (strstr(response, "Invalid file type") != NULL) {
                printf("Server rejected file: %s\n", response);
                continue;
            }
            
            //handle different types of commands
            if (strcmp(cmd, "ufile") == 0) {
                function_to_handle_ufile(sockfd, arg1);
            } else if (strcmp(cmd, "dfile") == 0) {
                function_to_handle_dfile(sockfd, arg1);
            } else if (strcmp(cmd, "dtar") == 0) {
                function_to_handle_dtar(sockfd, arg1);
            } else if (strcmp(cmd, "rmfile") == 0) {
                function_to_handle_remove(sockfd);
            } else {
                function_to_handle_display(sockfd);
            }
        } else {
            printf("Invalid command. Please try again.\n");
        }
    }

    close(sockfd);
    return 0;
}

//function to establish a connection with the server
int function_for_server_connection() {
    int sockfd;
    struct sockaddr_in servaddr;

    //create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        return -1;
    }

    //clear buffer
    memset(&servaddr, 0, sizeof(servaddr));

    //assign IP and PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);

    //convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    //connect to the server
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    return sockfd;
}

//function to send a command to the server
int function_to_send_socket_command(int sockfd, const char* command) {
    if (send(sockfd, command, strlen(command), 0) < 0) {
        perror("Send failed");
        return -1;
    }
    return 0;
}

//function to validate user input commands
int function_to_validate_command(const char* command) {
    char cmd[10];
    char arg1[256];
    char arg2[256];

    int parsed = sscanf(command, "%s %s %s", cmd, arg1, arg2);

    if (parsed < 1) {
        return 0;
    }

    //check for valid commands and their required number of arguments
    if (strcmp(cmd, "ufile") == 0) {
        return (parsed == 3 && (strncmp(arg2, "~/smain", 7) == 0));
    } else if (strcmp(cmd, "dfile") == 0 || strcmp(cmd, "rmfile") == 0 || strcmp(cmd, "display") == 0) {
        return (parsed == 2 && (strncmp(arg1, "~/smain", 7) == 0));
    } else if (strcmp(cmd, "dtar") == 0) {
        return (parsed == 2);
    }

    return 0;
}

//function to handle uploading a file to the server
void function_to_handle_ufile(int sockfd, const char* filename) {
    //open and send the file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t total_bytes_sent = 0;
    do {
        //read from file and send to server
        bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        if (bytes_read > 0) {
            if (send(sockfd, buffer, bytes_read, 0) < 0) {
                perror("Failed to send file data");
                fclose(file);
                return;
            }
            total_bytes_sent += bytes_read;
        }
    } while (bytes_read > 0);

    fclose(file);
    printf("File sent successfully. Total bytes sent: %zu\n", total_bytes_sent);

    //send an empty packet to signal end of file
    send(sockfd, "", 0, 0);
}

//function to handle downloading a file from the server
void function_to_handle_dfile(int sockfd, const char* filename) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    //extract the base filename
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;

    //open file for writing
    FILE *file = fopen(basename, "wb");
    if (!file) {
        perror("Failed to create file");
        return;
    }

    int total_bytes = 0;
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        //write received data to file
        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written < bytes_received) {
            perror("Failed to write to file");
            fclose(file);
            return;
        }
        total_bytes += bytes_written;
        
        //check if this is the end of the file
        if (bytes_received < BUFFER_SIZE) {
            break;
        }
    }

    fclose(file);

    //print appropriate message based on the response
    if (bytes_received < 0) {
        perror("Error receiving file data");
    } else if (total_bytes > 0) {
        printf("File received and saved as: %s\n", basename);
    } else {
        printf("No data received.\n");
    }
}

//function to handle removing a file from the server
void function_to_handle_remove(int sockfd) {
    char buffer[BUFFER_SIZE];
    //get response from server regarding the working of command
    int bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    
    //print appropriate message based on the server response
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);
    } else if (bytes_received == 0) {
        printf("Server closed the connection\n");
    } else {
        perror("Error receiving response from server");
    }
}

//function to handle downloading a tar file from the server
void function_to_handle_dtar(int sockfd, const char* filetype) {
    printf("Receiving tar file...\n");
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    //create filename for the tar file
    char full_filename[256];
    snprintf(full_filename, sizeof(full_filename), "%sfiles.tar", filetype + 1);
    
    //open file for writing
    FILE* file = fopen(full_filename, "wb");
    if (file == NULL) {
        perror("Failed to create tar file");
        return;
    }
    
    size_t total_bytes = 0;
    while (1) {
        bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
        //error or connection closed
        if (bytes_received <= 0) {
            break;
        }
        
        //check for EOF marker
        if (bytes_received >= 4 && memcmp(buffer + bytes_received - 4, "EOF\n", 4) == 0) {
            //dont write the EOF marker to the file
            bytes_received -= 4;
            fwrite(buffer, 1, bytes_received, file);
            total_bytes += bytes_received;
            //end of file reached
            break;
        }
        
        //write received data to file
        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written < bytes_received) {
            perror("Failed to write to tar file");
            fclose(file);
            return;
        }
        total_bytes += bytes_written;
        printf("Received %d bytes, total: %zu bytes\n", bytes_received, total_bytes);
    }
    
    fclose(file);
    
    if (bytes_received < 0) {
        perror("Error receiving data");
    }
    
    //print appropriate message
    if (total_bytes > 0) {
        printf("Tar file received and saved as: %s (Total bytes: %zu)\n", full_filename, total_bytes);
    } else {
        printf("No data received or error occurred.\n");
        //remove empty file
        remove(full_filename);
    }
}

//function to handle displaying files from the server
void function_to_handle_display(int sockfd) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    //list the file names from the directories that match
    printf("Files in the directory:\n");
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        if (bytes_received < BUFFER_SIZE - 1) {
            break;
        }
    }

    //if there was any error
    if (bytes_received < 0) {
        perror("Error receiving response from server");
    }
    printf("\n"); 
}