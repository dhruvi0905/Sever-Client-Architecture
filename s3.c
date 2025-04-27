#include <stdio.h> // This brings in tools for printing messages to the screen
#include <stdlib.h> // This gives us ways to manage memory and exit the program
#include <string.h> // This helps us work with text, like copying or comparing words
#include <unistd.h> // This provides tools for system tasks like closing files
#include <sys/socket.h> // This lets us set up network connections
#include <sys/types.h> // This defines some basic system data types
#include <sys/stat.h> // This helps us check if files or folders exist
#include <sys/wait.h> // This lets us handle processes that finish running
#include <netinet/in.h> // This gives us structures for internet addresses
#include <arpa/inet.h> // This helps us work with IP addresses
#include <fcntl.h> // This provides tools for controlling files
#include <dirent.h> // This lets us read folders and list files
#include <errno.h> // This helps us figure out what went wrong when errors happen
#include <libgen.h> // This gives us tools to work with file paths
#include <signal.h> // This lets us handle signals, like when a process ends

#define PORT 8082              // S3 server port for TXT files // This sets the port number for the S3 server to handle text files
#define BUFFER_SIZE 1024 // This decides how much data we can handle at once
#define MAX_PATH_LENGTH 1024 // This limits how long file paths can be
#define MAX_FILE_SIZE 104857600 // 100MB max file size // This sets the biggest file size we allow to 100MB

// Function to check if a file exists
int fileExists(const char *path) { // This starts a function to check if a file is there
    struct stat buffer; // This creates a space to hold file info
    return (stat(path, &buffer) == 0); // This checks the file and returns true if it exists, false if not
}

// Function to get file extension
const char *getFileExtension(const char *filename) { // This starts a function to find a file’s extension
    const char *dot = strrchr(filename, '.'); // This looks for the last dot in the filename
    if (!dot || dot == filename) return ""; // If there’s no dot or it’s at the start, return nothing
    return dot + 1; // This gives back the extension after the dot
}

// Function to create directories recursively
int createDirectories(const char *path) { // This starts a function to create folders, even nested ones
    char tmp[MAX_PATH_LENGTH]; // This makes a temporary space for the path
    char *p = NULL; // This will point to parts of the path
    size_t len; // This will hold the path’s length

    snprintf(tmp, sizeof(tmp), "%s", path); // This copies the path into our temporary space
    len = strlen(tmp); // This gets how long the path is
    if (tmp[len - 1] == '/') { // If the path ends with a slash...
        tmp[len - 1] = '\0'; // This removes the slash
    }

    for (p = tmp + 1; *p; p++) { // This loops through the path starting after the first character
        if (*p == '/') { // If we find a slash...
            *p = '\0'; // This cuts the path here temporarily
            printf("Attempting to create directory: %s\n", tmp); // This says which folder we’re trying to make
            if (mkdir(tmp, 0755) == -1 && errno != EEXIST) { // This tries to make the folder
                printf("Failed to create directory %s: %s\n", tmp, strerror(errno)); // This prints why it failed
                return -1; // This stops and says it didn’t work
            }
            *p = '/'; // This puts the slash back
        }
    }

    // Create the final directory
    printf("Attempting to create final directory: %s\n", tmp); // This says we’re making the final folder
    if (mkdir(tmp, 0755) == -1 && errno != EEXIST) { // This tries to make the last folder
        printf("Failed to create final directory %s: %s\n", tmp, strerror(errno)); // This prints why it failed
        return -1; // This stops and says it didn’t work
    }

    return 0; // This says everything worked fine
}

// Function to handle UPLOAD command
void handleUpload(int client_socket, char *path) { // This starts a function to handle file uploads
    char buffer[BUFFER_SIZE]; // This makes a space for messages
    char actual_path[MAX_PATH_LENGTH]; // This will hold the real file path
    
    // Expect ~/S3 prefix
    if (strncmp(path, "~/S3", 4) != 0) { // This checks if the path starts with "~/S3"
        send(client_socket, "ERROR: Path must start with ~/S3", 32, 0); // This tells the client the path is wrong
        return; // This stops since the path isn’t right
    }
    
    // Validate file extension
    const char *ext = getFileExtension(path); // This gets the file’s extension
    if (strcmp(ext, "txt") != 0) { // This checks if it’s a text file
        send(client_socket, "ERROR: File must have .txt extension", 36, 0); // This says only text files are allowed
        return; // This stops since it’s not a text file
    }
    
    // Resolve path
    sprintf(actual_path, "%s%s", getenv("HOME"), path + 1); // This builds the full path from the home folder
    printf("Resolved path: %s\n", actual_path); // This shows the path we’re using
    
    // Create directories if they don't exist
    char dir_path[MAX_PATH_LENGTH]; // This makes a space for the folder path
    strcpy(dir_path, actual_path); // This copies the full path
    char *last_slash = strrchr(dir_path, '/'); // This finds the last slash
    
    if (last_slash) { // If there’s a slash...
        *last_slash = '\0'; // This removes the filename to get the folder
        if (strlen(dir_path) > 0) { // If there’s a folder path...
            printf("Creating directories for: %s\n", dir_path); // This says we’re making folders
            if (createDirectories(dir_path) == -1) { // This tries to create the folders
                char error_msg[BUFFER_SIZE]; // This makes space for an error message
                snprintf(error_msg, BUFFER_SIZE, "ERROR: Could not create directory %s (%s)", dir_path, strerror(errno)); // This builds the error message
                send(client_socket, error_msg, strlen(error_msg), 0); // This sends the error to the client
                return; // This stops since folders couldn’t be made
            }
        }
    }
    
    // Tell client we're ready to receive the file
    send(client_socket, "READY", 5, 0); // This says we’re ready for the file
    
    // Receive file size
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the file size
    if (recv(client_socket, buffer, BUFFER_SIZE, 0) <= 0) { // This waits for the client to send the size
        send(client_socket, "ERROR: Could not receive file size", 33, 0); // This says we didn’t get the size
        return; // This stops since we need the size
    }
    
    long file_size = atol(buffer); // This turns the size into a number
    if (file_size <= 0 || file_size > MAX_FILE_SIZE) { // If the size is bad or too big...
        send(client_socket, "ERROR: Invalid file size", 24, 0); // This tells the client the size isn’t okay
        return; // This stops here
    }
    
    // Send acknowledgment
    send(client_socket, "SIZE_ACK", 8, 0); // This says we got the size
    // Prepare to receive file
    printf("Opening file for writing: %s\n", actual_path); // This says we’re creating the file
    FILE *file = fopen(actual_path, "wb"); // This opens the file to write data
    if (!file) { // If we couldn’t open it...
        char error_msg[BUFFER_SIZE]; // This makes space for an error
        snprintf(error_msg, BUFFER_SIZE, "ERROR: Could not create file at %s (%s)", actual_path, strerror(errno)); // This explains why it failed
        printf("Upload error: %s\n", error_msg); // This shows the error
        send(client_socket, error_msg, strlen(error_msg), 0); // This tells the client
        return; // This stops here
    }
    
    // Receive file content
    long total_bytes = 0; // This tracks how much we’ve received
    int bytes_received; // This holds the size of each chunk
    
    while (total_bytes < file_size) { // This keeps going until we get the whole file
        memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the next chunk
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0); // This gets a chunk of the file
        
        if (bytes_received <= 0) { // If we didn’t get data...
            fclose(file); // This closes the file
            remove(actual_path); // This deletes the partial file
            send(client_socket, "ERROR: File receive error", 25, 0); // This tells the client we failed
            return; // This stops here
        }
        
        fwrite(buffer, 1, bytes_received, file); // This writes the chunk to the file
        total_bytes += bytes_received; // This updates how much we’ve got
    }
    
    fclose(file); // This closes the file
    
    // Send success message
    send(client_socket, "SUCCESS", 7, 0); // This tells the client it worked
    printf("File successfully saved to %s\n", actual_path); // This says the file is saved
}

// Function to handle DOWNLOAD command
void handleDownload(int client_socket, char *path) { // This starts a function to send a file to the client
    char buffer[BUFFER_SIZE]; // This holds data we send or receive
    char actual_path[MAX_PATH_LENGTH]; // This stores the real file path
    
    printf("S3 received download request for path: %s\n", path); // This says what file the client wants
    
    // Check if path starts with ~/S3 or ~S3
    if (strncmp(path, "~/S3", 4) != 0 || (path[4] != '/' && path[4] != '\0')) { // This checks if the path starts correctly
        printf("Invalid path prefix: %s\n", path); // This says the path is wrong
        send(client_socket, "ERROR: Path must start with ~/S3/", 32, 0); // This tells the client
        return; // This stops here
    }
    
    // Resolve the full path
    sprintf(actual_path, "%s%s", getenv("HOME"), path + 1); // This builds the full path
    printf("Resolved to actual path: %s\n", actual_path); // This shows the path
    
    // Check if file exists
    if (!fileExists(actual_path)) { // This checks if the file is there
        printf("File not found: %s\n", actual_path); // This says we couldn’t find it
        send(client_socket, "ERROR: File not found", 21, 0); // This tells the client
        return; // This stops here
    }
    
    // Try to open the file
    FILE *file = fopen(actual_path, "rb"); // This opens the file for reading
    if (!file) { // If we couldn’t open it...
        printf("Failed to open file: %s\n", strerror(errno)); // This says why it failed
        send(client_socket, "ERROR: Could not open file", 26, 0); // This tells the client
        return; // This stops here
    }
    
    // Get file size
    fseek(file, 0, SEEK_END); // This jumps to the end of the file
    long file_size = ftell(file); // This gets the file’s size
    fseek(file, 0, SEEK_SET); // This goes back to the start
    
    // Send file size
    sprintf(buffer, "%ld", file_size); // This puts the size in the buffer
    printf("Sending file size: %s\n", buffer); // This shows what we’re sending
    send(client_socket, buffer, strlen(buffer), 0); // This sends the size
    
    // Wait for acknowledgment
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the response
    if (recv(client_socket, buffer, BUFFER_SIZE, 0) <= 0 || strcmp(buffer, "SIZE_ACK") != 0) { // This waits for the client to say “SIZE_ACK”
        printf("Failed to receive SIZE_ACK\n"); // This says we didn’t get confirmation
        fclose(file); // This closes the file
        send(client_socket, "ERROR: Size acknowledgment failed", 33, 0); // This tells the client
        return; // This stops here
    }
    
    // Send file data
    size_t bytes_read; // This tracks how much we read
    long total_sent = 0; // This tracks how much we’ve sent
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) { // This reads chunks of the file
        if (send(client_socket, buffer, bytes_read, 0) < 0) { // This sends each chunk
            printf("Failed to send file data\n"); // This says sending failed
            fclose(file); // This closes the file
            send(client_socket, "ERROR: Failed to send file", 26, 0); // This tells the client
            return; // This stops here
        }
        total_sent += bytes_read; // This updates our total
        printf("Sent %zu bytes, total: %ld\n", bytes_read, total_sent); // This shows our progress
    }
    
    fclose(file); // This closes the file
    
    // Wait for success confirmation
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the final response
    if (recv(client_socket, buffer, BUFFER_SIZE, 0) <= 0 || strcmp(buffer, "SUCCESS") != 0) { // This waits for “SUCCESS” from the client
        printf("Did not receive SUCCESS confirmation: %s\n", buffer); // This says what we got instead
    } else { // If we got the right confirmation...
        printf("File %s successfully sent\n", actual_path); // This says it worked
    }
}
// Function to handle REMOVE command
void handleRemove(int client_socket, char *path) { // This starts a function to delete a file
    char actual_path[MAX_PATH_LENGTH]; // This holds the real file path
    
    printf("S3: REMOVE command processing path: %s\n", path); // This says what file we’re deleting
    
    if (strncmp(path, "~/S3", 3) != 0) { // This checks if the path starts with "~/S3"
        printf("S3: Invalid path prefix (not ~/S3): %s\n", path); // This says the path is wrong
        send(client_socket, "ERROR: Path must start with ~/S3", 32, 0); // This tells the client
        return; // This stops here
    }
    
    sprintf(actual_path, "%s%s", getenv("HOME"), path + 1); // This builds the full path
    printf("S3: Resolved actual path: %s\n", actual_path); // This shows the path
    
    if (!fileExists(actual_path)) { // This checks if the file exists
        printf("S3: File not found: %s\n", actual_path); // This says we couldn’t find it
        send(client_socket, "ERROR: File not found", 21, 0); // This tells the client
        return; // This stops here
    }
    
    if (remove(actual_path) == 0) { // This tries to delete the file
        printf("S3: File %s successfully removed\n", actual_path); // This says it worked
        send(client_socket, "SUCCESS", 7, 0); // This tells the client
    } else { // If deleting failed...
        printf("S3: Failed to remove file %s: %s\n", actual_path, strerror(errno)); // This says why it failed
        send(client_socket, "ERROR: Failed to remove file", 28, 0); // This tells the client
    }
}
// Function to handle LIST command
// Function to handle LIST command // This is a duplicate comment for the LIST function
void handleList(int client_socket, char *path) { // This starts a function to list text files
    char actual_path[MAX_PATH_LENGTH]; // This holds the real folder path
    char response[BUFFER_SIZE * 10] = {0}; // This stores the list of files
    
    // Add debug output
    printf("S3: LIST received path: '%s'\n", path); // This says what folder we’re listing
    
    // Fix: Change 3 to 4 in the strncmp
    if (strncmp(path, "~/S3", 4) != 0) { // This checks if the path starts with "~/S3"
        printf("S3: LIST rejected path (invalid prefix): '%s'\n", path); // This says the path is wrong
        send(client_socket, "ERROR: Path must start with ~/S3", 32, 0); // This tells the client
        return; // This stops here
    }
    
    sprintf(actual_path, "%s%s", getenv("HOME"), path + 1); // This builds the full path
    printf("S3: LIST using actual path: '%s'\n", actual_path); // This shows the path
    
    DIR *dir; // This will hold the folder we’re looking at
    struct dirent *ent; // This will hold each file’s info
    
    struct stat st; // This checks if the path is a folder
    if (stat(actual_path, &st) != 0 || !S_ISDIR(st.st_mode)) { // If it’s not a folder...
        printf("S3: LIST directory does not exist: '%s'\n", actual_path); // This says it’s not there
        send(client_socket, "", 0, 0); // This sends an empty response
        return; // This stops here
    }
    
    if ((dir = opendir(actual_path)) != NULL) { // This tries to open the folder
        char **files = NULL; // This will hold the list of text files
        int file_count = 0; // This counts how many files we find
        
        while ((ent = readdir(dir)) != NULL) { // This loops through each file
            if (strstr(ent->d_name, ".txt") != NULL) { // If it’s a text file...
                files = realloc(files, (file_count + 1) * sizeof(char *)); // This makes space for the filename
                files[file_count] = strdup(ent->d_name); // This copies the filename
                file_count++; // This adds one to our count
            }
        }
        closedir(dir); // This closes the folder
        
        for (int i = 0; i < file_count; i++) { // This loops through the files
            for (int j = i + 1; j < file_count; j++) { // This compares each file with later ones
                if (strcmp(files[i], files[j]) > 0) { // If they’re out of order...
                    char *temp = files[i]; // This holds one filename
                    files[i] = files[j]; // This swaps them
                    files[j] = temp; // This finishes the swap
                }
            }
        }
        
        for (int i = 0; i < file_count; i++) { // This loops through the sorted files
            strcat(response, files[i]); // This adds the filename to the list
            if (i < file_count - 1) { // If it’s not the last file...
                strcat(response, "\n"); // This adds a new line
            }
            free(files[i]); // This frees the filename’s memory
        }
        free(files); // This frees the list itself
    }
    
    printf("S3: LIST sending response (%zu bytes): '%s'\n", strlen(response), response); // This shows what we’re sending
    send(client_socket, response, strlen(response), 0); // This sends the file list
    printf("S3: LIST completed for directory %s\n", actual_path); // This says we’re done
}

// Function to handle TAR command
void handleTar(int client_socket, char *filetype) { // This starts a function to bundle text files
    char tar_command[MAX_PATH_LENGTH * 2]; // This holds the command to make the bundle
    char tar_path[MAX_PATH_LENGTH]; // This stores the bundle’s path
    char buffer[BUFFER_SIZE]; // This holds messages we send or receive
    
    if (strcmp(filetype, "txt") != 0) { // This checks if the request is for text files
        send(client_socket, "ERROR: S3 only handles txt files", 31, 0); // This says we only do text files
        return; // This stops here
    }
    
    sprintf(tar_path, "/tmp/text.tar"); // This sets where we’ll save the bundle
    
    sprintf(tar_command, "find %s/S3 -name \"*.txt\" -type f | tar -cf %s -T -", getenv("HOME"), tar_path); // This builds a command to bundle text files
    
    int result = system(tar_command); // This runs the command to make the bundle
    
    if (result != 0 || !fileExists(tar_path)) { // If it failed or the bundle isn’t there...
        send(client_socket, "ERROR: Failed to create tar file", 32, 0); // This tells the client we failed
        return; // This stops here
    }
    
    FILE *file = fopen(tar_path, "rb"); // This opens the bundle file
    if (!file) { // If we couldn’t open it...
        send(client_socket, "ERROR: Could not open tar file", 30, 0); // This tells the client
        return; // This stops here
    }
    
    fseek(file, 0, SEEK_END); // This jumps to the end of the file
    long file_size = ftell(file); // This gets the file’s size
    fseek(file, 0, SEEK_SET); // This goes back to the start
    
    sprintf(buffer, "%ld", file_size); // This puts the size in the buffer
    send(client_socket, buffer, strlen(buffer), 0); // This sends the size to the client
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer
    if (recv(client_socket, buffer, BUFFER_SIZE, 0) <= 0 || strcmp(buffer, "SIZE_ACK") != 0) { // This waits for “SIZE_ACK”
        fclose(file); // This closes the file
        remove(tar_path); // This deletes the bundle
        send(client_socket, "ERROR: Size acknowledgment failed", 33, 0); // This tells the client
        return; // This stops here
    }
    
    size_t bytes_read; // This tracks how much we read
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) { // This reads chunks of the bundle
        if (send(client_socket, buffer, bytes_read, 0) < 0) { // This sends each chunk
            fclose(file); // This closes the file
            remove(tar_path); // This deletes the bundle
            send(client_socket, "ERROR: Failed to send file", 26, 0); // This tells the client
            return; // This stops here
        }
    }
    
    fclose(file); // This closes the file
    remove(tar_path); // This deletes the bundle
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer
    if (recv(client_socket, buffer, BUFFER_SIZE, 0) <= 0 || strcmp(buffer, "SUCCESS") != 0) { // This waits for “SUCCESS”
        printf("Tar file transfer status: %s\n", buffer); // This shows what we got
    } else { // If we got “SUCCESS”...
        printf("Tar file successfully sent\n"); // This says it worked
    }
}

// Function to process client requests
void processRequest(int client_socket) { // This starts a function to figure out what the client wants
    char buffer[BUFFER_SIZE]; // This holds the client’s message
    ssize_t bytes_received; // This tracks how much we got
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer
    printf("S3: Waiting for client request...\n"); // This says we’re ready
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0); // This waits for a message
    
    if (bytes_received <= 0) { // If we got nothing...
        printf("S3: No data received or connection closed\n"); // This says the client is gone
        close(client_socket); // This closes the connection
        return; // This stops here
    }
    
    buffer[bytes_received] = '\0'; // This makes the message a proper string
    printf("S3: Raw received data: '%s', length: %zd\n", buffer, bytes_received); // This shows what we got
    
    char command[20] = {0}; // This will hold the command
    char path[MAX_PATH_LENGTH] = {0}; // This will hold the path or argument
    int items = sscanf(buffer, "%19s %1023[^\n]", command, path); // This splits the message into parts
    printf("S3: Parsed %d items - Command: '%s', Path: '%s'\n", items, command, path); // This shows the pieces
    
    if (strcmp(command, "UPLOAD") == 0) { // If the client wants to upload...
        printf("S3: Processing UPLOAD command\n"); // This says what we’re doing
        handleUpload(client_socket, path); // This starts the upload
    } else if (strcmp(command, "DOWNLOAD") == 0) { // If they want to download...
        printf("S3: Processing DOWNLOAD command\n"); // This says what’s happening
        handleDownload(client_socket, path); // This starts the download
    } else if (strcmp(command, "REMOVE") == 0) { // If they want to delete...
        printf("S3: Processing REMOVE command for path: '%s'\n", path); // This says what file
        handleRemove(client_socket, path); // This starts the deletion
    } else if (strcmp(command, "LIST") == 0) { // If they want a file list...
        printf("S3: Processing LIST command\n"); // This says we’re listing
        handleList(client_socket, path); // This starts the listing
    } else if (strcmp(command, "TAR") == 0) { // If they want a bundle...
        printf("S3: Processing TAR command\n"); // This says we’re bundling
        handleTar(client_socket, path); // This starts the bundling
    } else { // If it’s something else...
        printf("S3: Unknown command: '%s'\n", command); // This says we don’t know it
        send(client_socket, "ERROR: Unknown command", 22, 0); // This tells the client
    }
}
// Signal handler for child processes
void sig_child(int signo) { // This starts a function to handle finished processes
    pid_t pid; // This holds the process ID
    int stat; // This holds the exit status
    
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0); // This cleans up any finished processes
    return; // This says we’re done
}

int main() { // This is where the program starts
    int server_fd, client_socket; // These hold the server and client connections
    struct sockaddr_in address; // This stores the server’s address
    int opt = 1; // This is a setting to reuse the port
    socklen_t addrlen = sizeof(address); // This is the size of the address
    pid_t child_pid; // This holds the ID of a child process
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // This tries to create a server socket
        perror("Socket creation failed"); // This prints if it failed
        exit(EXIT_FAILURE); // This stops the program
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { // This sets options for the socket
        perror("Setsockopt failed"); // This prints if it didn’t work
        exit(EXIT_FAILURE); // This stops the program
    }
    
    address.sin_family = AF_INET; // This says we’re using IPv4
    address.sin_addr.s_addr = INADDR_ANY; // This lets us accept any network address
    address.sin_port = htons(PORT); // This sets our port number
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { // This attaches the socket to the port
        perror("Bind failed"); // This prints if it failed
        exit(EXIT_FAILURE); // This stops the program
    }
    
    if (listen(server_fd, 10) < 0) { // This starts listening for up to 10 clients
        perror("Listen failed"); // This prints if it didn’t work
        exit(EXIT_FAILURE); // This stops the program
    }
    
    signal(SIGCHLD, sig_child); // This sets up handling for child processes
    
    char s3_path[MAX_PATH_LENGTH]; // This holds the S3 folder path
    sprintf(s3_path, "%s/S3", getenv("HOME")); // This builds the path for S3
    printf("Creating S3 directory: %s\n", s3_path); // This says we’re making the folder
    if (mkdir(s3_path, 0755) == -1 && errno != EEXIST) { // This tries to create S3
        perror("Failed to create S3 directory"); // This prints if it failed
    }
    
    printf("S3 server started on port %d...\n", PORT); // This interpolates the port number into the string
    while (1) { // This keeps the server running forever
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) { // This waits for a client
            perror("Accept failed"); // This prints if it failed
            continue; // This tries again
        }
        
        if ((child_pid = fork()) == 0) { // This creates a new process for the client
            close(server_fd); // This closes the server socket in the child
            processRequest(client_socket); // This handles the client’s request
            close(client_socket); // This closes the client connection
            exit(0); // This ends the child process
        } else { // This is the parent process
            close(client_socket); // This closes the client socket in the parent
        }
    }
    
    return 0; // This says the program ended (though we never reach here)
}
