#include <stdio.h> // This brings in basic input/output tools, like printing to the screen or reading files.
#include <stdlib.h> // This adds tools for memory management, random numbers, and exiting the program.
#include <string.h> // This includes functions to handle strings, like copying or finding their length.
#include <unistd.h> // This gives access to system functions, like creating processes or closing files.
#include <sys/socket.h> // This provides tools for network connections, like sockets for sending data online.
#include <sys/types.h> // This defines basic data types used in system programming, like process IDs.
#include <sys/stat.h> // This includes functions to check file details, like if a file exists or its size.
#include <sys/wait.h> // This provides tools to manage child processes, like waiting for them to finish.
#include <netinet/in.h> // This defines structures for internet addresses used in networking.
#include <arpa/inet.h> // This helps convert network addresses, like turning an IP address into a number.
#include <fcntl.h> // This includes tools for file control, like opening or closing files.
#include <dirent.h> // This lets the program work with directories, like listing files in a folder.
#include <errno.h> // This provides error codes to understand what went wrong if something fails.
#include <libgen.h> // This includes functions to work with file paths, like getting a file's name.
#include <signal.h> // This lets the program handle system signals, like when a process ends.

#define PORT 8083              // S4 server port for ZIP files // This sets the port number to 8083 for the server to listen on.
#define BUFFER_SIZE 1024 // This sets a buffer size of 1024 bytes for reading and writing data.
#define MAX_PATH_LENGTH 1024 // This limits file path lengths to 1024 characters.
#define MAX_FILE_SIZE 104857600 // 100MB max file size // This sets the maximum file size to 100 megabytes.

int fileExists(const char *path) { // This starts a function to check if a file exists at a given path.
    struct stat buffer; // This creates a structure to hold file information.
    return (stat(path, &buffer) == 0); // This checks if the file exists and returns true if it does, false if not.
}

const char *getFileExtension(const char *filename) { // This starts a function to find a file's extension, like "zip".
    const char *dot = strrchr(filename, '.'); // This looks for the last dot in the filename to find the extension.
    if (!dot || dot == filename) return ""; // If there's no dot or the dot is at the start, it returns an empty string.
    return dot + 1; // This returns the extension (everything after the dot).
}

int createDirectories(const char *path) { // This starts a function to create folders, even nested ones, if they don't exist.
    char tmp[MAX_PATH_LENGTH]; // This creates a temporary array to hold a copy of the path.
    char *p = NULL; // This sets up a pointer to help process the path.
    size_t len; // This will store the length of the path.

    snprintf(tmp, sizeof(tmp), "%s", path); // This safely copies the path into the temporary array.
    len = strlen(tmp); // This gets the length of the copied path.
    if (len > 0 && tmp[len - 1] == '/') { // This checks if the path ends with a slash.
        tmp[len - 1] = '\0'; // If it does, this removes the slash to clean up the path.
    }

    for (p = tmp + 1; *p; p++) { // This loop goes through each character in the path, starting after the first one.
        if (*p == '/') { // This checks if the current character is a folder separator (slash).
            *p = '\0'; // This temporarily replaces the slash to isolate a folder name.
            printf("Attempting to create directory: %s\n", tmp); // This prints a message about trying to create a folder.
            if (mkdir(tmp, 0755) == -1 && errno != EEXIST) { // This tries to create the folder and checks if it failed (unless it already exists).
                printf("Failed to create directory %s: %s\n", tmp, strerror(errno)); // If it failed, this prints an error with the reason.
                return -1; // This exits the function with an error code.
            }
            *p = '/'; // This puts the slash back to continue processing the path.
        }
    }

    printf("Attempting to create final directory: %s\n", tmp); // This prints a message about creating the last folder.
    if (mkdir(tmp, 0755) == -1 && errno != EEXIST) { // This tries to create the final folder and checks for failure.
        printf("Failed to create final directory %s: %s\n", tmp, strerror(errno)); // If it failed, this prints an error message.
        return -1; // This exits the function with an error code.
    }

    return 0; // This means the folders were created successfully.
}

void handleUpload(int client_socket, char *path) { // This starts a function to handle file uploads from a client.
    char buffer[BUFFER_SIZE]; // This creates a buffer to hold data during the upload.
    char actual_path[MAX_PATH_LENGTH]; // This will store the full path of the file.
    
    if (strncmp(path, "~/S4", 4) != 0) { // This checks if the path starts with "~/S4".
        printf("S4: Invalid path prefix: %s\n", path); // If not, this prints an error message.
        send(client_socket, "ERROR: Path must start with ~/S4", 32, 0); // This tells the client the path is wrong.
        return; // This stops the function.
    }
    
    const char *ext = getFileExtension(path); // This gets the file's extension.
    if (strcmp(ext, "zip") != 0) { // This checks if the extension is "zip".
        printf("S4: File must have .zip extension: %s\n", path); // If not, this prints an error.
        send(client_socket, "ERROR: File must have .zip extension", 36, 0); // This tells the client only zip files are allowed.
        return; // This stops the function.
    }
    
    sprintf(actual_path, "%s%s", getenv("HOME"), path + 1); // This builds the full path using the user's home directory.
    printf("S4: Resolved path: %s\n", actual_path); // This prints the full path for logging.
    
    char dir_path[MAX_PATH_LENGTH]; // This will hold the directory part of the path.
    strcpy(dir_path, actual_path); // This copies the full path to work with.
    char *last_slash = strrchr(dir_path, '/'); // This finds the last slash in the path.
    
    if (last_slash) { // This checks if there’s a slash (meaning there’s a directory).
        *last_slash = '\0'; // This cuts off the filename to get just the directory.
        if (strlen(dir_path) > 0) { // This ensures the directory path isn’t empty.
            printf("S4: Creating directories for: %s\n", dir_path); // This prints a message about creating folders.
            if (createDirectories(dir_path) == -1) { // This tries to create the directories.
                char error_msg[BUFFER_SIZE]; // This creates a buffer for an error message.
                snprintf(error_msg, BUFFER_SIZE, "ERROR: Could not create directory %s (%s)", dir_path, strerror(errno)); // This builds the error message.
                printf("S4: %s\n", error_msg); // This prints the error.
                send(client_socket, error_msg, strlen(error_msg), 0); // This sends the error to the client.
                return; // This stops the function.
            }
        }
    }
    
    printf("S4: Sending READY\n"); // This logs that the server is ready to receive the file.
    send(client_socket, "READY", 5, 0); // This tells the client to start sending the file.
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer to prepare for new data.
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0); // This receives the file size from the client.
    if (bytes_received <= 0) { // This checks if the size was received correctly.
        printf("S4: Failed to receive file size: %s\n", strerror(errno)); // If not, this logs an error.
        send(client_socket, "ERROR: Could not receive file size", 33, 0); // This tells the client there was a problem.
        return; // This stops the function.
    }
    
    long file_size = atol(buffer); // This converts the received size to a number.
    if (file_size <= 0 || file_size > MAX_FILE_SIZE) { // This checks if the size is valid and not too big.
        printf("S4: Invalid file size: %ld\n", file_size); // If invalid, this logs an error.
        send(client_socket, "ERROR: Invalid file size", 24, 0); // This tells the client the size is wrong.
        return; // This stops the function.
    }
    
    printf("S4: Sending SIZE_ACK for file size %ld\n", file_size); // This logs that the size was accepted.
    send(client_socket, "SIZE_ACK", 8, 0); // This confirms to the client that the size is okay.
    
    printf("S4: Opening file for writing: %s\n", actual_path); // This logs that the file is being opened.
    FILE *file = fopen(actual_path, "wb"); // This opens the file for writing in binary mode.
    if (!file) { // This checks if the file was opened successfully.
        char error_msg[BUFFER_SIZE]; // This creates a buffer for an error message.
        snprintf(error_msg, BUFFER_SIZE, "ERROR: Could not create file at %s (%s)", actual_path, strerror(errno)); // This builds the error message.
        printf("S4: %s\n", error_msg); // This prints the error.
        send(client_socket, error_msg, strlen(error_msg), 0); // This sends the error to the client.
        return; // This stops the function.
    }
    
    long total_bytes = 0; // This keeps track of how many bytes have been received.
    int bytes_read; // This will store the number of bytes read in each loop.
    
    while (total_bytes < file_size) { // This loops until all bytes of the file are received.
        memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for new data.
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0); // This receives a chunk of the file.
        
        if (bytes_read <= 0) { // This checks if there was an error receiving data.
            printf("S4: Error receiving file data: %s\n", strerror(errno)); // This logs the error.
            fclose(file); // This closes the file.
            remove(actual_path); // This deletes the incomplete file.
            send(client_socket, "ERROR: File receive error", 25, 0); // This tells the client there was a problem.
            return; // This stops the function.
        }
        
        size_t bytes_written = fwrite(buffer, 1, bytes_read, file); // This writes the received chunk to the file.
        if (bytes_written != bytes_read) { // This checks if all bytes were written correctly.
            printf("S4: Error writing to file: wrote %zu of %d bytes\n", bytes_written, bytes_read); // This logs the error.
            fclose(file); // This closes the file.
            remove(actual_path); // This deletes the incomplete file.
            send(client_socket, "ERROR: File write error", 23, 0); // This tells the client there was a problem.
            return; // This stops the function.
        }
        
        total_bytes += bytes_read; // This updates the total bytes received.
        printf("S4: Received %d bytes, total: %ld of %ld\n", bytes_read, total_bytes, file_size); // This logs progress.
    }
    
    if (fflush(file) != 0 || fclose(file) != 0) { // This ensures the file is saved and closed properly.
        printf("S4: Error finalizing file: %s\n", strerror(errno)); // If not, this logs an error.
        remove(actual_path); // This deletes the problematic file.
        send(client_socket, "ERROR: Could not finalize file", 30, 0); // This tells the client there was a problem.
        return; // This stops the function.
    }
    
    if (!fileExists(actual_path)) { // This checks if the file exists after saving.
        printf("S4: File does not exist after write: %s\n", actual_path); // If not, this logs an error.
        send(client_socket, "ERROR: File creation failed", 27, 0); // This tells the client the file wasn’t created.
        return; // This stops the function.
    }
    
    printf("S4: File successfully saved to %s\n", actual_path); // This logs that the file was saved.
    send(client_socket, "SUCCESS", 7, 0); // This tells the client the upload worked.
}

void handleDownload(int client_socket, char *path) { // This starts a function to handle file downloads.
    char buffer[BUFFER_SIZE]; // This creates a buffer for sending data.
    char actual_path[MAX_PATH_LENGTH]; // This will store the full path of the file.
    
    printf("S4: Received download request for path: %s\n", path); // This logs the download request.
    
    if (strncmp(path, "~/S4", 4) != 0 || (path[4] != '/' && path[4] != '\0')) { // This checks if the path starts with "~/S4/".
        printf("S4: Invalid path prefix: %s\n", path); // If not, this logs an error.
        send(client_socket, "ERROR: Path must start with ~/S4/", 32, 0); // This tells the client the path is wrong.
        return; // This stops the function.
    }
    
    sprintf(actual_path, "%s%s", getenv("HOME"), path + 1); // This builds the full path using the home directory.
    printf("S4: Resolved to actual path: %s\n", actual_path); // This logs the full path.
    
    if (!fileExists(actual_path)) { // This checks if the file exists.
        printf("S4: File not found: %s\n", actual_path); // If not, this logs an error.
        send(client_socket, "ERROR: File not found", 21, 0); // This tells the client the file doesn’t exist.
        return; // This stops the function.
    }
    
    FILE *file = fopen(actual_path, "rb"); // This opens the file for reading in binary mode.
    if (!file) { // This checks if the file was opened successfully.
        printf("S4: Failed to open file: %s (%s)\n", actual_path, strerror(errno)); // If not, this logs an error.
        send(client_socket, "ERROR: Could not open file", 26, 0); // This tells the client there was a problem.
        return; // This stops the function.
    }
    
    fseek(file, 0, SEEK_END); // This moves to the end of the file to find its size.
    long file_size = ftell(file); // This gets the file size.
    fseek(file, 0, SEEK_SET); // This moves back to the start of the file.
    
    sprintf(buffer, "%ld", file_size); // This puts the file size into the buffer as text.
    printf("S4: Sending file size: %s\n", buffer); // This logs the file size being sent.
    send(client_socket, buffer, strlen(buffer), 0); // This sends the file size to the client.
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for new data.
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0); // This waits for the client to confirm the size.
    if (bytes_received <= 0 || strcmp(buffer, "SIZE_ACK") != 0) { // This checks if the confirmation was received correctly.
        printf("S4: Failed to receive SIZE_ACK: %s\n", bytes_received > 0 ? buffer : strerror(errno)); // If not, this logs an error.
        fclose(file); // This closes the file.
        send(client_socket, "ERROR: Size acknowledgment failed", 33, 0); // This tells the client there was a problem.
        return; // This stops the function.
    }
    
    size_t bytes_read; // This will store the number of bytes read from the file.
    long total_sent = 0; // This tracks how many bytes have been sent.
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) { // This loops to read and send the file in chunks.
        if (send(client_socket, buffer, bytes_read, 0) < 0) { // This sends a chunk to the client.
            printf("S4: Failed to send file data: %s\n", strerror(errno)); // If it fails, this logs an error.
            fclose(file); // This closes the file.
            send(client_socket, "ERROR: Failed to send file", 26, 0); // This tells the client there was a problem.
            return; // This stops the function.
        }
        total_sent += bytes_read; // This updates the total bytes sent.
        printf("S4: Sent %zu bytes, total: %ld\n", bytes_read, total_sent); // This logs progress.
    }
    
    fclose(file); // This closes the file after sending.
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for new data.
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0); // This waits for the client to confirm success.
    if (bytes_received <= 0 || strcmp(buffer, "SUCCESS") != 0) { // This checks if the confirmation was received.
        printf("S4: Did not receive SUCCESS confirmation: %s\n", bytes_received > 0 ? buffer : "No response"); // If not, this logs an error.
    } else { // This runs if the confirmation was received.
        printf("S4: File %s successfully sent\n", actual_path); // This logs that the file was sent successfully.
    }
}

void handleRemove(int client_socket, char *path) { // This starts a function to handle file deletion.
    char actual_path[MAX_PATH_LENGTH]; // This will store the full path of the file.
    
    printf("S4: REMOVE command processing path: %s\n", path); // This logs the delete request.
    
    if (strncmp(path, "~/S4", 4) != 0) { // This checks if the path starts with "~/S4".
        printf("S4: Invalid path prefix (not ~/S4): %s\n", path); // If not, this logs an error.
        send(client_socket, "ERROR: Path must start with ~/S4", 32, 0); // This tells the client the path is wrong.
        return; // This stops the function.
    }
    
    sprintf(actual_path, "%s%s", getenv("HOME"), path + 1); // This builds the full path using the home directory.
    printf("S4: Resolved actual path: %s\n", actual_path); // This logs the full path.
    
    if (!fileExists(actual_path)) { // This checks if the file exists.
        printf("S4: File not found: %s\n", actual_path); // If not, this logs an error.
        send(client_socket, "ERROR: File not found", 21, 0); // This tells the client the file doesn’t exist.
        return; // This stops the function.
    }
    
    if (remove(actual_path) == 0) { // This tries to delete the file and checks if it worked.
        printf("S4: File %s successfully removed\n", actual_path); // If it worked, this logs success.
        send(client_socket, "SUCCESS", 7, 0); // This tells the client the file was deleted.
    } else { // This runs if deletion failed.
        printf("S4: Failed to remove file %s: %s\n", actual_path, strerror(errno)); // This logs the error.
        send(client_socket, "ERROR: Failed to remove file", 28, 0); // This tells the client there was a problem.
    }
}

void handleList(int client_socket, char *path) { // This starts a function to list zip files in a directory.
    char actual_path[MAX_PATH_LENGTH]; // This will store the full path of the directory.
    char response[BUFFER_SIZE * 10] = {0}; // This will hold the list of files to send to the client.
    
    printf("S4: LIST received path: '%s'\n", path); // This logs the list request.
    
    if (strncmp(path, "~/S4", 4) != 0) { // This checks if the path starts with "~/S4".
        printf("S4: LIST rejected path (invalid prefix): '%s'\n", path); // If not, this logs an error.
        send(client_socket, "ERROR: Path must start with ~/S4", 32, 0); // This tells the client the path is wrong.
        return; // This stops the function.
    }
    
    sprintf(actual_path, "%s%s", getenv("HOME"), path + 1); // This builds the full path using the home directory.
    printf("S4: LIST using actual path: '%s'\n", actual_path); // This logs the full path.
    
    DIR *dir; // This will hold the directory being read.
    struct dirent *ent; // This will hold information about each file in the directory.
    
    struct stat st; // This will hold information about the path.
    if (stat(actual_path, &st) != 0 || !S_ISDIR(st.st_mode)) { // This checks if the path exists and is a directory.
        printf("S4: LIST directory does not exist: '%s'\n", actual_path); // If not, this logs an error.
        send(client_socket, "", 0, 0); // This sends an empty response to the client.
        return; // This stops the function.
    }
    
    if ((dir = opendir(actual_path)) != NULL) { // This tries to open the directory.
        char **files = NULL; // This will store the list of zip file names.
        int file_count = 0; // This tracks how many zip files are found.
        
        while ((ent = readdir(dir)) != NULL) { // This loops through each file in the directory.
            if (strstr(ent->d_name, ".zip") != NULL) { // This checks if the file is a zip file.
                files = realloc(files, (file_count + 1) * sizeof(char *)); // This makes space for the new file name.
                files[file_count] = strdup(ent->d_name); // This copies the file name to the list.
                file_count++; // This increases the count of files.
            }
        }
        closedir(dir); // This closes the directory.
        
        for (int i = 0; i < file_count; i++) { // This starts a loop to sort the file names.
            for (int j = i + 1; j < file_count; j++) { // This compares each file with the ones after it.
                if (strcmp(files[i], files[j]) > 0) { // This checks if the files are out of order alphabetically.
                    char *temp = files[i]; // This temporarily holds one file name.
                    files[i] = files[j]; // This swaps the file names.
                    files[j] = temp; // This completes the swap.
                }
            }
        }
        
        for (int i = 0; i < file_count; i++) { // This loops to build the response with file names.
            strcat(response, files[i]); // This adds a file name to the response.
            if (i < file_count - 1) { // This checks if it’s not the last file.
                strcat(response, "\n"); // If not, this adds a newline to separate file names.
            }
            free(files[i]); // This frees the memory used for the file name.
        }
        free(files); // This frees the memory used for the file list.
    }
    
    printf("S4: LIST sending response (%zu bytes): '%s'\n", strlen(response), response); // This logs the response being sent.
    send(client_socket, response, strlen(response), 0); // This sends the list of files to the client.
    printf("S4: LIST completed for directory %s\n", actual_path); // This logs that the list was sent.
}

void handleTar(int client_socket, char *filetype) { // This starts a function to handle tar commands (but it’s not supported).
    printf("S4: TAR command received for filetype: %s\n", filetype); // This logs the tar request.
    
    if (strcmp(filetype, "zip") != 0) { // This checks if the filetype is "zip".
        send(client_socket, "ERROR: S4 only handles zip files", 31, 0); // If not, this tells the client only zip files are allowed.
        return; // This stops the function.
    }
    
    send(client_socket, "ERROR: ZIP file archiving not supported", 38, 0); // This tells the client tar isn’t supported for zip files.
}

void processRequest(int client_socket) { // This starts a function to process client requests.
    char buffer[BUFFER_SIZE]; // This creates a buffer to hold the client’s command.
    ssize_t bytes_received; // This will store how many bytes were received.
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for new data.
    printf("S4: Waiting for client request...\n"); // This logs that the server is waiting.
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0); // This receives the client’s command.
    
    if (bytes_received <= 0) { // This checks if data was received correctly.
        printf("S4: No data received or connection closed\n"); // If not, this logs an error.
        close(client_socket); // This closes the connection.
        return; // This stops the function.
    }
    
    buffer[bytes_received] = '\0'; // This adds an end marker to the received data.
    printf("S4: Raw received data: '%s', length: %zd\n", buffer, bytes_received); // This logs what was received.
    
    char command[20] = {0}; // This will hold the command (like "UPLOAD").
    char path[MAX_PATH_LENGTH] = {0}; // This will hold the path or argument.
    int items = sscanf(buffer, "%19s %1023[^\n]", command, path); // This splits the data into command and path.
    printf("S4: Parsed %d items - Command: '%s', Path: '%s'\n", items, command, path); // This logs the parsed command.
    
    if (strcmp(command, "UPLOAD") == 0) { // This checks if the command is "UPLOAD".
        printf("S4: Processing UPLOAD command\n"); // This logs that an upload is being handled.
        handleUpload(client_socket, path); // This calls the upload function.
    } else if (strcmp(command, "DOWNLOAD") == 0) { // This checks if the command is "DOWNLOAD".
        printf("S4: Processing DOWNLOAD command\n"); // This logs that a download is being handled.
        handleDownload(client_socket, path); // This calls the download function.
    } else if (strcmp(command, "REMOVE") == 0) { // This checks if the command is "REMOVE".
        printf("S4: Processing REMOVE command for path: '%s'\n", path); // This logs that a delete is being handled.
        handleRemove(client_socket, path); // This calls the remove function.
    } else if (strcmp(command, "LIST") == 0) { // This checks if the command is "LIST".
        printf("S4: Processing LIST command\n"); // This logs that a list is being handled.
        handleList(client_socket, path); // This calls the list function.
    } else if (strcmp(command, "TAR") == 0) { // This checks if the command is "TAR".
        printf("S4: Processing TAR command\n"); // This logs that a tar is being handled.
        handleTar(client_socket, path); // This calls the tar function.
    } else { // This runs if the command isn’t recognized.
        printf("S4: Unknown command: '%s'\n", command); // This logs the unknown command.
        send(client_socket, "ERROR: Unknown command", 22, 0); // This tells the client the command isn’t valid.
    }
}

void sig_child(int signo) { // This starts a function to handle signals from child processes.
    pid_t pid; // This will store the process ID of a child.
    int stat; // This will store the child’s status.
    
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0); // This cleans up finished child processes without waiting.
    return; // This exits the function.
}

int main() { // This starts the main program.
    int server_fd, client_socket; // These will hold the server and client socket numbers.
    struct sockaddr_in address; // This will hold the server’s address details.
    int opt = 1; // This is an option for socket settings.
    socklen_t addrlen = sizeof(address); // This stores the size of the address structure.
    pid_t child_pid; // This will store the ID of a child process.
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // This creates a socket for network connections.
        perror("Socket creation failed"); // If it fails, this prints an error.
        exit(EXIT_FAILURE); // This stops the program.
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { // This sets options to reuse the port.
        perror("Setsockopt failed"); // If it fails, this prints an error.
        exit(EXIT_FAILURE); // This stops the program.
    }
    
    address.sin_family = AF_INET; // This sets the address type to internet (IPv4).
    address.sin_addr.s_addr = INADDR_ANY; // This allows connections from any IP address.
    address.sin_port = htons(PORT); // This sets the port number (converting it for network use).
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { // This ties the socket to the port.
        perror("Bind failed"); // If it fails, this prints an error.
        exit(EXIT_FAILURE); // This stops the program.
    }
    
    if (listen(server_fd, 10) < 0) { // This starts listening for up to 10 connections.
        perror("Listen failed"); // If it fails, this prints an error.
        exit(EXIT_FAILURE); // This stops the program.
    }
    
    signal(SIGCHLD, sig_child); // This sets up the signal handler for child processes.
    
    char s4_path[MAX_PATH_LENGTH]; // This will hold the path to the S4 directory.
    sprintf(s4_path, "%s/S4", getenv("HOME")); // This builds the path to ~/S4.
    printf("Creating S4 directory: %s\n", s4_path); // This logs that the directory is being created.
    if (mkdir(s4_path, 0755) == -1 && errno != EEXIST) { // This tries to create the S4 directory.
        perror("Failed to create S4 directory"); // If it fails (and not because it exists), this prints an error.
    }
    
    printf("S4 server started on port %d...\n", PORT); // This logs that the server is running.
    
    while (1) { // This starts an infinite loop to handle clients.
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) { // This waits for a client to connect.
            perror("Accept failed"); // If it fails, this prints an error.
            continue; // This tries again for the next client.
        }
        
        if ((child_pid = fork()) == 0) { // This creates a new process for the client.
            close(server_fd); // This closes the server socket in the child process.
            processRequest(client_socket); // This handles the client’s request.
            close(client_socket); // This closes the client connection.
            exit(0); // This ends the child process.
        } else { // This runs in the parent process.
            close(client_socket); // This closes the client socket in the parent.
        }
    }
    
    return 0; // This ends the program (though it never reaches here due to the loop).
}
