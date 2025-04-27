#include <stdio.h> // This brings in tools for printing and reading stuff on the screen
#include <stdlib.h> // This gives us basic memory management and other handy utilities
#include <string.h> // This helps us work with text strings, like copying or comparing them
#include <unistd.h> // This provides access to system functions like closing files or pausing
#include <sys/socket.h> // This is for creating network connections, like sockets for chatting over the internet
#include <sys/types.h> // This defines some basic data types used in system programming
#include <sys/stat.h> // This helps us check info about files, like if they exist or their size
#include <sys/wait.h> // This lets us manage child processes, like waiting for them to finish
#include <netinet/in.h> // This has internet address structures for networking
#include <arpa/inet.h> // This helps convert IP addresses between text and binary forms
#include <fcntl.h> // This gives us tools for working with files, like opening or closing them
#include <dirent.h> // This lets us read directories, like listing files in a folder
#include <errno.h> // This provides error codes to understand what went wrong when something fails
#include <signal.h> // This helps handle signals, like stopping a program gracefully

#define PORT 8080 // This sets the main server port number to 8080
#define S2_PORT 8081 // This sets a second server port to 8081 for specific file types
#define S3_PORT 8082 // This sets a third server port to 8082 for other file types
#define S4_PORT 8083 // This sets a fourth server port to 8083 for yet another file type
#define BUFFER_SIZE 1024 // This defines how much data we can handle at once (1KB)
#define MAX_PATH_LENGTH 1024 // This sets the maximum length for file paths
#define MAX_FILE_SIZE 104857600 // This limits file sizes to 100MB to avoid huge uploads

// This creates a structure to hold a command and its arguments, like a to-do list
typedef struct {
    char command[20]; // This stores the command name, like "upload" or "download"
    char arg1[MAX_PATH_LENGTH]; // This holds the first argument, like a file path
    char arg2[MAX_PATH_LENGTH]; // This holds a second argument, if needed
} Command;

// This function takes a text input and breaks it into command and arguments
void parseCommand(char *input, Command *cmd) {
    char *token; // This will hold each piece of the input as we split it
    memset(cmd, 0, sizeof(Command)); // This clears the command structure to start fresh
    token = strtok(input, " \n"); // This grabs the first word (command) by splitting on spaces or newlines
    if (token != NULL) { // If we found a command...
        strncpy(cmd->command, token, sizeof(cmd->command) - 1); // Copy the command into the structure safely
        token = strtok(NULL, " \n"); // Get the next piece (first argument)
        if (token != NULL) { // If there’s an argument...
            strncpy(cmd->arg1, token, sizeof(cmd->arg1) - 1); // Copy the first argument safely
            token = strtok(NULL, " \n"); // Get the next piece (second argument)
            if (token != NULL) { // If there’s another argument...
                strncpy(cmd->arg2, token, sizeof(cmd->arg2) - 1); // Copy the second argument safely
            }
        }
    }
}

// This function checks if a file exists at the given path
int fileExists(const char *path) {
    struct stat buffer; // This will hold info about the file
    return (stat(path, &buffer) == 0); // Returns true (1) if the file exists, false (0) if it doesn’t
}

// This function finds the file extension, like "txt" or "pdf"
const char *getFileExtension(const char *filename) {
    const char *dot = strrchr(filename, '.'); // Look for the last dot in the filename
    if (!dot || dot == filename) return ""; // If no dot or dot is at the start, return empty string
    return dot + 1; // Return the extension (everything after the dot)
}

// This function picks the right server port based on the file extension
int getServerPort(const char *filename) {
    const char *ext = getFileExtension(filename); // Get the file’s extension
    if (strcmp(ext, "pdf") == 0) return S2_PORT; // If it’s a PDF, use port 8081
    if (strcmp(ext, "txt") == 0) return S3_PORT; // If it’s a text file, use port 8082
    if (strcmp(ext, "zip") == 0) return S4_PORT; // If it’s a zip file, use port 8083
    return -1; // If no match, return -1 to show no valid port
}

// This function creates directories if they don’t exist, like making folders for a file path
int createDirectories(const char *path) {
    if (!path || strlen(path) == 0) { // Check if the path is empty or invalid
        printf("Invalid path for directory creation\n"); // Print an error if it is
        return -1; // Return failure
    }
    char tmp[MAX_PATH_LENGTH]; // This will hold a copy of the path to work with
    snprintf(tmp, sizeof(tmp), "%s", path); // Copy the path into tmp safely
    size_t len = strlen(tmp); // Get the length of the path
    if (len > 0 && tmp[len - 1] == '/') { // If the path ends with a slash...
        tmp[len - 1] = '\0'; // Remove the trailing slash
    }
    char *p; // This will point to parts of the path as we process it
    for (p = tmp + 1; *p; p++) { // Loop through the path starting after the first character
        if (*p == '/') { // If we find a slash (directory separator)...
            *p = '\0'; // Temporarily end the string here
            printf("Creating intermediate directory: %s\n", tmp); // Show which folder we’re making
            if (mkdir(tmp, 0755) == -1 && errno != EEXIST) { // Try to create the folder
                printf("Failed to create %s: %s\n", tmp, strerror(errno)); // Print error if it fails
                return -1; // Return failure
            }
            *p = '/'; // Put the slash back to continue processing
        }
    }
    printf("Creating final directory: %s\n", tmp); // Show the final folder we’re making
    if (mkdir(tmp, 0755) == -1 && errno != EEXIST) { // Try to create the final folder
        printf("Failed to create %s: %s\n", tmp, strerror(errno)); // Print error if it fails
        return -1; // Return failure
    }
    return 0; // Return success
}

// This function sends a file to another server based on its port
int transferFileToServer(int client_socket, const char *dest_path, int server_port, long file_size) {
    int sock = 0; // This will hold the socket for connecting to the other server
    struct sockaddr_in serv_addr; // This holds the server’s address details
    char buffer[BUFFER_SIZE]; // This is a temporary space for sending/receiving data
    printf("Initiating transfer to port %d for path %s\n", server_port, dest_path); // Announce the transfer
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Try to create a socket
        printf("Socket creation error for transfer: %s\n", strerror(errno)); // Print error if it fails
        send(client_socket, "ERROR: Could not create socket", 29, 0); // Tell the client it failed
        return -1; // Return failure
    }
    memset(&serv_addr, 0, sizeof(serv_addr)); // Clear the server address structure
    serv_addr.sin_family = AF_INET; // Set the address family to IPv4
    serv_addr.sin_port = htons(server_port); // Set the target port (convert to network format)
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { // Convert IP address to binary
        printf("Invalid address for transfer\n"); // Print error if the address is wrong
        send(client_socket, "ERROR: Invalid server address", 28, 0); // Tell the client it failed
        close(sock); // Close the socket
        return -1; // Return failure
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // Try to connect to the server
        printf("Connection failed to port %d: %s\n", server_port, strerror(errno)); // Print error if it fails
        send(client_socket, "ERROR: Could not connect to target server", 40, 0); // Tell the client it failed
        close(sock); // Close the socket
        return -1; // Return failure
    }
    snprintf(buffer, BUFFER_SIZE, "UPLOAD %s", dest_path); // Prepare an "UPLOAD" command with the file path
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // Send the command to the server
        printf("Failed to send UPLOAD command: %s\n", strerror(errno)); // Print error if it fails
        send(client_socket, "ERROR: Failed to send to target server", 38, 0); // Tell the client it failed
        close(sock); // Close the socket
        return -1; // Return failure
    }
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer for receiving data
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Receive a response from the server
    if (bytes_received <= 0 || strncmp(buffer, "READY", 5) != 0) { // Check if the server said "READY"
        buffer[bytes_received > 0 ? bytes_received : 0] = '\0'; // Null-terminate the buffer
        printf("Target server not ready: %s\n", bytes_received > 0 ? buffer : "No response"); // Print error
        send(client_socket, "ERROR: Target server not ready", 30, 0); // Tell the client it failed
        close(sock); // Close the socket
        return -1; // Return failure
    }
    snprintf(buffer, BUFFER_SIZE, "%ld", file_size); // Prepare the file size as a string
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // Send the file size to the server
        printf("Failed to send file size: %s\n", strerror(errno)); // Print error if it fails
        send(client_socket, "ERROR: Failed to send file size", 31, 0); // Tell the client it failed
        close(sock); // Close the socket
        return -1; // Return failure
    }
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer again
    bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Receive acknowledgment from the server
    if (bytes_received <= 0 || strcmp(buffer, "SIZE_ACK") != 0) { // Check if the server acknowledged the size
        buffer[bytes_received > 0 ? bytes_received : 0] = '\0'; // Null-terminate the buffer
        printf("Size acknowledgment failed: %s\n", bytes_received > 0 ? buffer : "No response"); // Print error
        send(client_socket, "ERROR: Size acknowledgment failed", 33, 0); // Tell the client it failed
        close(sock); // Close the socket
        return -1; // Return failure
    }
    if (send(client_socket, "SIZE_ACK", 8, 0) < 0) { // Send acknowledgment back to the client
        printf("Failed to send SIZE_ACK to client: %s\n", strerror(errno)); // Print error if it fails
        close(sock); // Close the socket
        return -1; // Return failure
    }
        long total_bytes = 0; // This keeps track of how many bytes we've received so far
    int bytes_received_client; // This stores how many bytes we get from the client in each chunk
    while (total_bytes < file_size) { // Keep going until we've received all the file's bytes
        memset(buffer, 0, BUFFER_SIZE); // Clear the buffer to make sure it's empty before we use it
        bytes_received_client = recv(client_socket, buffer, BUFFER_SIZE, 0); // Get a chunk of data from the client
        if (bytes_received_client <= 0) { // If we got no data or there was an error...
            printf("Error receiving file from client: %s\n", strerror(errno)); // Print what went wrong
            send(sock, "ERROR: File receive error", 25, 0); // Tell the other server something broke
            close(sock); // Close the connection to the other server
            return -1; // Say the transfer failed
        }
        if (send(sock, buffer, bytes_received_client, 0) < 0) { // Try to send the chunk to the other server
            printf("Error sending file to port %d: %s\n", server_port, strerror(errno)); // Print the problem if it fails
            send(client_socket, "ERROR: File transfer error", 26, 0); // Tell the client it didn’t work
            close(sock); // Close the connection to the other server
            return -1; // Say the transfer failed
        }
        total_bytes += bytes_received_client; // Add the chunk size to our total
        printf("Transferred %d bytes, total: %ld\n", bytes_received_client, total_bytes); // Show progress
    }
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer again for the next step
    bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Wait for a status message from the other server
    if (bytes_received <= 0) { // If we didn’t get a status or there was an error...
        printf("No status from target server: %s\n", strerror(errno)); // Print what went wrong
        send(client_socket, "ERROR: No status from target server", 35, 0); // Tell the client something broke
        close(sock); // Close the connection to the other server
        return -1; // Say the transfer failed
    }
    buffer[bytes_received] = '\0'; // Add an end marker to the status message so it’s a proper string
    if (send(client_socket, buffer, strlen(buffer), 0) < 0) { // Send the status back to the client
        printf("Failed to send status to client: %s\n", strerror(errno)); // Print the problem if it fails
        close(sock); // Close the connection to the other server
        return -1; // Say the transfer failed
    }
    close(sock); // Close the connection to the other server since we’re done
    printf("Transfer complete, status: %s\n", buffer); // Show the final status
    return (strncmp(buffer, "SUCCESS", 7) == 0) ? 0 : -1; // Return success (0) if the status is "SUCCESS", else failure (-1)
}

void handleUploadLocal(int client_socket, const char *path, long file_size) { // This function saves a file locally
    char buffer[BUFFER_SIZE]; // This is a temporary space to hold data chunks
    char actual_path[MAX_PATH_LENGTH]; // This will hold the full path where we’ll save the file
    // Resolve ~ to home directory
    if (strncmp(path, "~/S1", 3) == 0) { // Check if the path starts with "~/S1"
        snprintf(actual_path, MAX_PATH_LENGTH, "%s%s", getenv("HOME"), path + 1); // Replace ~ with the user’s home folder and add the rest
    } else if (strncmp(path, "~S1", 2) == 0) { // Check if the path starts with "~S1"
        snprintf(actual_path, MAX_PATH_LENGTH, "%s/%s", getenv("HOME"), path + 2); // Add home folder and the rest of the path
    } else { // If no special prefix...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s", path); // Just use the path as is
    }
    printf("Attempting to save to: %s\n", actual_path); // Show where we plan to save the file
    // Verify directory exists
    char dir_path[MAX_PATH_LENGTH]; // This will hold just the folder part of the path
    snprintf(dir_path, MAX_PATH_LENGTH, "%s", actual_path); // Copy the full path to work with
    char *last_slash = strrchr(dir_path, '/'); // Find the last slash to separate folder from filename
    if (last_slash) { // If we found a slash...
        *last_slash = '\0'; // Cut off the filename to get just the folder path
        if (strlen(dir_path) > 0) { // If the folder path isn’t empty...
            struct stat st; // This will hold info about the folder
            if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) { // Check if the folder exists and is actually a folder
                printf("Directory does not exist or is not a directory: %s\n", dir_path); // Complain if it’s not right
                send(client_socket, "ERROR: Directory does not exist", 31, 0); // Tell the client it failed
                return; // Stop here
            }
            printf("Verified directory exists: %s\n", dir_path); // Confirm the folder is good
        }
        *last_slash = '/'; // Put the slash back for later
    } else { // If no slash was found...
        printf("Invalid path: %s\n", actual_path); // Say the path doesn’t make sense
        send(client_socket, "ERROR: Invalid file path", 24, 0); // Tell the client it’s wrong
        return; // Stop here
    }
    printf("Opening file: %s\n", actual_path); // Show we’re about to create the file
    FILE *file = fopen(actual_path, "wb"); // Open the file for writing in binary mode
    if (!file) { // If we couldn’t open the file...
        char error_msg[BUFFER_SIZE]; // This will hold a custom error message
        snprintf(error_msg, BUFFER_SIZE, "ERROR: Could not create file at %s (%s)", actual_path, strerror(errno)); // Describe the problem
        printf("File creation failed: %s\n", error_msg); // Print the error
        send(client_socket, error_msg, strlen(error_msg), 0); // Tell the client what happened
        return; // Stop here
    }
    printf("Receiving file data (size: %ld bytes)\n", file_size); // Announce we’re starting to get the file
    long total_bytes = 0; // This tracks how many bytes we’ve received
    int bytes_received; // This holds the size of each chunk we get
    while (total_bytes < file_size) { // Keep going until we’ve got the whole file
        memset(buffer, 0, BUFFER_SIZE); // Clear the buffer for the next chunk
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0); // Get a chunk from the client
        if (bytes_received <= 0) { // If we got no data or an error...
            printf("Receive error: %s\n", strerror(errno)); // Print what went wrong
            fclose(file); // Close the file
            remove(actual_path); // Delete the partial file to avoid junk
            send(client_socket, "ERROR: File receive error", 25, 0); // Tell the client it failed
            return; // Stop here
        }
        size_t bytes_written = fwrite(buffer, 1, bytes_received, file); // Write the chunk to the file
        if (bytes_written != bytes_received) { // If we didn’t write all the data...
            printf("Write error: wrote %zu of %d bytes\n", bytes_written, bytes_received); // Show the problem
            fclose(file); // Close the file
            remove(actual_path); // Delete the partial file
            send(client_socket, "ERROR: File write error", 24, 0); // Tell the client it failed
            return; // Stop here
        }
        total_bytes += bytes_received; // Add the chunk size to our total
        printf("Received %d bytes, total: %ld\n", bytes_received, total_bytes); // Show progress
    }
    if (fflush(file) != 0 || fclose(file) != 0) { // Make sure the file is fully saved and closed
        printf("Error finalizing file: %s\n", strerror(errno)); // Print any problem
        remove(actual_path); // Delete the file if something went wrong
        send(client_socket, "ERROR: Could not finalize file", 31, 0); // Tell the client it failed
        return; // Stop here
    }
    // Verify file exists
    if (!fileExists(actual_path)) { // Check if the file is really there
        printf("File does not exist after write: %s\n", actual_path); // Complain if it’s missing
        send(client_socket, "ERROR: File creation failed", 28, 0); // Tell the client it failed
        return; // Stop here
    }
    if (send(client_socket, "SUCCESS", 7, 0) < 0) { // Send a success message to the client
        printf("Failed to send SUCCESS: %s\n", strerror(errno)); // Print if it didn’t send
        return; // Stop here
    }
    printf("File successfully saved to %s\n", actual_path); // Celebrate that the file was saved
}

void forwardDownloadRequest(int client_socket, const char *path, int server_port) { // This sends a download request to another server
    char buffer[BUFFER_SIZE]; // This is a space to hold messages we send or receive
    int sock = 0; // This will hold the connection to the other server
    struct sockaddr_in serv_addr; // This stores the other server’s address details
    
    printf("### DEBUG FWD: Forwarding download request to port %d for %s\n", server_port, path); // Show what we’re trying to do
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Try to make a new connection
        printf("### DEBUG FWD: Socket creation error: %s\n", strerror(errno)); // Print if it fails
        send(client_socket, "ERROR: Could not create socket", 29, 0); // Tell the client it didn’t work
        return; // Stop here
    }
    
    // Configure server address
    memset(&serv_addr, 0, sizeof(serv_addr)); // Clear the address structure
    serv_addr.sin_family = AF_INET; // Say we’re using IPv4
    serv_addr.sin_port = htons(server_port); // Set the port we’re connecting to
    
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { // Convert the IP address to binary
        printf("### DEBUG FWD: Invalid address\n"); // Print if the address is wrong
        send(client_socket, "ERROR: Invalid server address", 28, 0); // Tell the client it failed
        close(sock); // Close the connection
        return; // Stop here
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // Try to connect to the other server
        printf("### DEBUG FWD: Connection failed to port %d: %s\n", server_port, strerror(errno)); // Print if it fails
        send(client_socket, "ERROR: Could not connect to target server", 40, 0); // Tell the client it didn’t work
        close(sock); // Close the connection
        return; // Stop here
    }
    
    // Send download command
    snprintf(buffer, BUFFER_SIZE, "DOWNLOAD %s", path); // Create a "DOWNLOAD" command with the file path
    printf("### DEBUG FWD: Sending command: %s\n", buffer); // Show what we’re sending
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // Try to send the command
        printf("### DEBUG FWD: Failed to send command\n"); // Print if it didn’t send
        send(client_socket, "ERROR: Failed to send to target server", 38, 0); // Tell the client it failed
        close(sock); // Close the connection
        return; // Stop here
    }
    // Get file size from target server
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer so it’s empty and ready for new data
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This grabs the file size from the other server
    if (bytes_received <= 0) { // If we didn’t get any data or there was a problem...
        printf("### DEBUG FWD: Failed to receive file size\n"); // Print that something went wrong
        send(client_socket, "ERROR: Target server error", 26, 0); // Tell the client the server had an issue
        close(sock); // Close the connection to the other server
        return; // Stop here since we can’t continue
    }
    buffer[bytes_received] = '\0'; // Add an end marker to the data so it’s a proper string
    
    // Check if target server reported an error
    if (strncmp(buffer, "ERROR", 5) == 0) { // If the server sent an error message...
        printf("### DEBUG FWD: Target server reported error: %s\n", buffer); // Show the error details
        send(client_socket, buffer, strlen(buffer), 0); // Pass the error to the client
        close(sock); // Close the connection to the other server
        return; // Stop here since there’s an issue
    }
    
    printf("### DEBUG FWD: Received file size: %s\n", buffer); // Show the file size we got
    
    // Forward file size to client
    if (send(client_socket, buffer, strlen(buffer), 0) < 0) { // Try to send the file size to the client
        printf("### DEBUG FWD: Failed to send file size to client\n"); // Print if it didn’t work
        close(sock); // Close the connection to the other server
        return; // Stop here since we can’t proceed
    }
    
    // Wait for client's SIZE_ACK
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer again for the next message
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // Wait for the client to say “SIZE_ACK”
    if (bytes_received <= 0 || strcmp(buffer, "SIZE_ACK") != 0) { // If we didn’t get the right response...
        printf("### DEBUG FWD: Client SIZE_ACK failed\n"); // Print that the client didn’t confirm properly
        close(sock); // Close the connection to the other server
        return; // Stop here since something’s off
    }
    
    // Forward SIZE_ACK to target server
    printf("### DEBUG FWD: Sending SIZE_ACK to target server\n"); // Announce we’re confirming to the server
    if (send(sock, "SIZE_ACK", 8, 0) < 0) { // Send the “SIZE_ACK” to the other server
        printf("### DEBUG FWD: Failed to send SIZE_ACK to target\n"); // Print if it didn’t send
        close(sock); // Close the connection to the other server
        return; // Stop here since we hit a snag
    }
    
    // Forward file data from target server to client
    long total_bytes = 0; // This tracks how many bytes we’ve sent so far
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) { // Keep getting chunks of the file
        printf("### DEBUG FWD: Received %zd bytes from target\n", bytes_received); // Show how much we got
        if (send(client_socket, buffer, bytes_received, 0) < 0) { // Try to send the chunk to the client
            printf("### DEBUG FWD: Failed to forward data to client\n"); // Print if it didn’t work
            close(sock); // Close the connection to the other server
            return; // Stop here since we can’t continue
        }
        total_bytes += bytes_received; // Add the chunk size to our total
        printf("### DEBUG FWD: Forwarded total: %ld bytes\n", total_bytes); // Show our progress
    }
    
    printf("### DEBUG FWD: File transfer complete, sending SUCCESS\n"); // Announce we’re done with the file
    
    // Send SUCCESS to target server
    if (send(sock, "SUCCESS", 7, 0) < 0) { // Tell the server we finished successfully
        printf("### DEBUG FWD: Failed to send SUCCESS to target\n"); // Print if it didn’t send
    }
    
    close(sock); // Close the connection to the other server
    printf("### DEBUG FWD: Forwarding completed\n"); // Say we’re all done
}

void handleDownloadLocal(int client_socket, const char *path) { // This function handles downloading a file locally
    char buffer[BUFFER_SIZE]; // This is a space to hold data chunks
    char actual_path[MAX_PATH_LENGTH]; // This will store the full file path
    char filename[MAX_PATH_LENGTH] = {0}; // This holds just the file’s name
    char dir_path[MAX_PATH_LENGTH] = {0}; // This holds the folder path
    
    printf("### DEBUG: Original request path: %s\n", path); // Show the path we’re working with
    
    // Extract filename and directory path
    char *last_slash = strrchr(path, '/'); // Find the last slash to split folder and file
    if (last_slash) { // If there’s a slash...
        strncpy(filename, last_slash + 1, MAX_PATH_LENGTH - 1); // Copy the filename after the slash
        size_t dir_len = last_slash - path; // Figure out how long the folder part is
        strncpy(dir_path, path, dir_len); // Copy the folder part
        dir_path[dir_len] = '\0'; // End the folder string properly
    } else { // If no slash...
        strncpy(filename, path, MAX_PATH_LENGTH - 1); // The whole path is just the filename
    }
    
    printf("### DEBUG: Extracted filename: %s\n", filename); // Show the filename we found
    printf("### DEBUG: Extracted directory: %s\n", dir_path); // Show the folder path
    
    // Check file extension and get corresponding server port
    const char *ext = getFileExtension(filename); // Get the file’s extension, like “txt”
    int server_port = getServerPort(filename); // Figure out which server port matches the extension
    printf("### DEBUG: File extension: %s, Server port: %d\n", ext, server_port); // Show the extension and port
    
    char modified_path[MAX_PATH_LENGTH]; // This will hold a new path if we need to redirect
    
    // Handle redirection based on file extension
    if (server_port != -1) { // If the file needs to go to another server...
        printf("### DEBUG: File needs to be handled by server on port %d\n", server_port); // Say where it’s going
        
        // Handle different path formats
        if (strncmp(path, "~/S1/", 5) == 0) { // Check if the path starts with "~/S1/"
            printf("### DEBUG: Found ~/S1/ prefix\n"); // Note the special prefix
            const char* remaining_path = path + 5; // Skip the "~/S1/" part
            if (strcmp(ext, "txt") == 0) { // If it’s a text file...
                snprintf(modified_path, MAX_PATH_LENGTH, "~/S3/%s", remaining_path); // Change the path for server S3
                printf("### DEBUG: Redirecting to S3 with path: %s\n", modified_path); // Show the new path
                forwardDownloadRequest(client_socket, modified_path, server_port); // Send the request to S3
                return; // Stop here
            } else if (strcmp(ext, "pdf") == 0) { // If it’s a PDF...
                snprintf(modified_path, MAX_PATH_LENGTH, "~/S2/%s", remaining_path); // Change the path for server S2
                forwardDownloadRequest(client_socket, modified_path, server_port); // Send to S2
                return; // Stop here
            } else if (strcmp(ext, "zip") == 0) { // If it’s a zip file...
                snprintf(modified_path, MAX_PATH_LENGTH, "~/S4/%s", remaining_path); // Change the path for server S4
                forwardDownloadRequest(client_socket, modified_path, server_port); // Send to S4
                return; // Stop here
            }
        } else if (strncmp(path, "~S1/", 4) == 0) { // Check if the path starts with "~S1/"
            printf("### DEBUG: Found ~S1/ prefix\n"); // Note the different prefix
            const char* remaining_path = path + 4; // Skip the "~S1/" part
            if (strcmp(ext, "txt") == 0) { // If it’s a text file...
                snprintf(modified_path, MAX_PATH_LENGTH, "~S3/%s", remaining_path); // Change the path for S3
                printf("### DEBUG: Redirecting to S3 with path: %s\n", modified_path); // Show the new path
                forwardDownloadRequest(client_socket, modified_path, server_port); // Send to S3
                return; // Stop here
            } else if (strcmp(ext, "pdf") == 0) { // If it’s a PDF...
                snprintf(modified_path, MAX_PATH_LENGTH, "~S2/%s", remaining_path); // Change the path for S2
                forwardDownloadRequest(client_socket, modified_path, server_port); // Send to S2
                return; // Stop here
            } else if (strcmp(ext, "zip") == 0) { // If it’s a zip file...
                snprintf(modified_path, MAX_PATH_LENGTH, "~S4/%s", remaining_path); // Change the path for S4
                forwardDownloadRequest(client_socket, modified_path, server_port); // Send to S4
                return; // Stop here
            }
        }
    }
    
    // Handle local files (non-pdf, non-txt, non-zip files)
    printf("### DEBUG: Handling file locally\n"); // Say we’re dealing with the file ourselves
    
    // Convert path formats to actual path
    if (strncmp(path, "~/S1/", 5) == 0) { // If the path starts with "~/S1/"...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s%s", getenv("HOME"), path + 1); // Replace ~ with the home folder
    } else if (strncmp(path, "~S1/", 4) == 0) { // If it starts with "~S1/"...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s/%s", getenv("HOME"), path + 2); // Add home folder and rest of path
    } else { // Otherwise...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s", path); // Use the path as is
    }
    
    printf("### DEBUG: Resolved local path: %s\n", actual_path); // Show the final path we’re using
    
    if (!fileExists(actual_path)) { // Check if the file is actually there
        printf("### DEBUG: File not found: %s\n", actual_path); // Say we couldn’t find it
        send(client_socket, "ERROR: File not found", 21, 0); // Tell the client it’s missing
        return; // Stop here
    }
    
    // Open and read the file
    FILE *file = fopen(actual_path, "rb"); // Open the file for reading in binary mode
    if (!file) { // If we couldn’t open it...
        printf("### DEBUG: Could not open file: %s (%s)\n", actual_path, strerror(errno)); // Print why it failed
        send(client_socket, "ERROR: Could not open file", 26, 0); // Tell the client we had trouble
        return; // Stop here
    }
    
    // Get file size
    fseek(file, 0, SEEK_END); // Jump to the end of the file
    long file_size = ftell(file); // See how big the file is
    fseek(file, 0, SEEK_SET); // Go back to the start
    printf("### DEBUG: File size: %ld bytes\n", file_size); // Show the file size
    
    // Send file size to client
    snprintf(buffer, BUFFER_SIZE, "%ld", file_size); // Put the file size into the buffer as text
    printf("### DEBUG: Sending file size: %s\n", buffer); // Show what we’re sending
    if (send(client_socket, buffer, strlen(buffer), 0) < 0) { // Try to send the size
        printf("### DEBUG: Failed to send file size\n"); // Print if it didn’t work
        fclose(file); // Close the file
        return; // Stop here
    }
    
    // Set a timeout for receiving SIZE_ACK
    struct timeval tv; // This sets up a timer
    tv.tv_sec = 5; // Wait up to 5 seconds
    tv.tv_usec = 0; // No extra microseconds
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)); // Apply the timeout to the client connection
    
    // Wait for client's SIZE_ACK
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer for the client’s response
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // Wait for the client to confirm
    
    // Reset socket to blocking mode
    tv.tv_sec = 0; // Turn off the timeout
    tv.tv_usec = 0; // No microseconds
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)); // Remove the timeout setting
    
    if (bytes_received <= 0 || strcmp(buffer, "SIZE_ACK") != 0) { // If the client didn’t confirm properly...
        printf("### DEBUG: Client SIZE_ACK failed or not received: %s\n", 
               bytes_received > 0 ? buffer : "no response"); // Print what went wrong
        fclose(file); // Close the file
        if (bytes_received <= 0) { // If we got no response...
            printf("### DEBUG: Client SIZE_ACK timeout or error: %s\n", strerror(errno)); // Say it timed out
        }
        return; // Stop here
    }
    
    printf("### DEBUG: Received SIZE_ACK from client\n"); // Confirm the client said okay
    
    // Send file data
    size_t bytes_read; // This tracks how much we read from the file
    long total_sent = 0; // This tracks how much we’ve sent
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) { // Keep reading chunks of the file
        printf("### DEBUG: Sending chunk of %zu bytes\n", bytes_read); // Show the chunk size
        if (send(client_socket, buffer, bytes_read, 0) < 0) { // Try to send the chunk
            printf("### DEBUG: Failed to send file data: %s\n", strerror(errno)); // Print if it failed
            fclose(file); // Close the file
            return; // Stop here
        }
        total_sent += bytes_read; // Add the chunk to our total
        printf("### DEBUG: Sent %zu bytes, total: %ld\n", bytes_read, total_sent); // Show progress
    }
    
    if (ferror(file)) { // Check if there was a problem reading the file
        printf("### DEBUG: Error reading file: %s\n", strerror(errno)); // Print the issue
        fclose(file); // Close the file
        return; // Stop here
    }
    
    fclose(file); // Close the file since we’re done
    printf("### DEBUG: File transfer complete\n"); // Say we finished sending the file
    
    // Try to receive SUCCESS but don't wait too long
    tv.tv_sec = 1; // Set a 1-second timeout
    tv.tv_usec = 0; // No extra microseconds
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)); // Apply the timeout
    
    memset(buffer, 0, BUFFER_SIZE); // Clear the buffer for the client’s response
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // Wait for the client to say “SUCCESS”
    
    // Reset socket to blocking mode
    tv.tv_sec = 0; // Turn off the timeout
    tv.tv_usec = 0; // No microseconds
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)); // Remove the timeout
    
    if (bytes_received > 0 && strcmp(buffer, "SUCCESS") == 0) { // If the client confirmed success...
        printf("### DEBUG: Received SUCCESS from client\n"); // Say we got it
    } else { // If we didn’t get “SUCCESS”...
        printf("### DEBUG: Did not receive SUCCESS from client, continuing anyway\n"); // Note it but keep going
    }
    
    printf("### DEBUG: Local download of %s completed successfully\n", actual_path); // Celebrate finishing the download
}

void forwardRemoveRequest(int client_socket, const char *path, int server_port) { // This sends a file deletion request to another server
    char buffer[BUFFER_SIZE]; // This holds messages we send or receive
    int sock = 0; // This will hold the connection to the other server
    struct sockaddr_in serv_addr; // This stores the server’s address details
    
    printf("### REMOVE-FORWARDING: Starting for path %s to port %d\n", path, server_port); // Say what we’re trying to do
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Try to make a new connection
        printf("### REMOVE-FORWARDING: Socket creation error: %s\n", strerror(errno)); // Print if it didn’t work
        send(client_socket, "ERROR: Could not create socket", 29, 0); // Tell the client we had trouble
        return; // Stop here
    }
    
    // Configure server address
    memset(&serv_addr, 0, sizeof(serv_addr)); // Clear the address structure
    serv_addr.sin_family = AF_INET; // Say we’re using IPv4
    serv_addr.sin_port = htons(server_port); // Set the port we’re connecting to
    
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { // Convert the IP address to binary
        printf("### REMOVE-FORWARDING: Invalid address\n"); // Print if the address is wrong
        send(client_socket, "ERROR: Invalid server address", 28, 0); // Tell the client it failed
        close(sock); // Close the connection
        return; // Stop here
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // Try to connect to the server
        printf("### REMOVE-FORWARDING: Connection failed to port %d: %s\n", server_port, strerror(errno)); // Print if it failed
        send(client_socket, "ERROR: Could not connect to target server", 40, 0); // Tell the client we couldn’t connect
        close(sock); // Close the connection
        return; // Stop here
    }
    // Send remove command - ensure we follow the exact format expected by S3
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer to make sure it’s empty before we add anything
    snprintf(buffer, BUFFER_SIZE, "REMOVE %s", path); // This creates a command like "REMOVE file.txt" with the file path
    printf("### REMOVE-FORWARDING: Sending command: '%s'\n", buffer); // This shows the command we’re about to send
    
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This tries to send the command to the other server
        printf("### REMOVE-FORWARDING: Failed to send command: %s\n", strerror(errno)); // This prints an error if sending fails
        send(client_socket, "ERROR: Failed to send to target server", 38, 0); // This tells the client we couldn’t send the command
        close(sock); // This closes the connection to the other server
        return; // This stops the function since we hit a problem
    }
    
    // Get response from target server
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer again to prepare for the server’s reply
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the server to send back a response
    if (bytes_received <= 0) { // If we didn’t get a response or there was an error...
        printf("### REMOVE-FORWARDING: Failed to receive response: %s\n", strerror(errno)); // This prints what went wrong
        send(client_socket, "ERROR: Target server error", 26, 0); // This tells the client the server didn’t respond
        close(sock); // This closes the connection to the other server
        return; // This stops the function since we can’t continue
    }
    buffer[bytes_received] = '\0'; // This adds an end marker to the response to make it a proper string
    
    // Forward the response to client
    printf("### REMOVE-FORWARDING: Received response: '%s'\n", buffer); // This shows the response we got from the server
    if (send(client_socket, buffer, strlen(buffer), 0) < 0) { // This tries to send the response to the client
        printf("### REMOVE-FORWARDING: Failed to forward response to client: %s\n", strerror(errno)); // This prints an error if sending fails
    }
    
    close(sock); // This closes the connection to the other server since we’re done
    printf("### REMOVE-FORWARDING: Completed for path %s\n", path); // This says we finished the delete request
}
// Function to handle REMOVE command
void handleRemoveLocal(int client_socket, const char *path) { // This function deletes a file locally
    char actual_path[MAX_PATH_LENGTH]; // This will hold the full path to the file
    char filename[MAX_PATH_LENGTH] = {0}; // This will store just the file’s name
    char modified_path[MAX_PATH_LENGTH] = {0}; // This will hold a new path if we need to redirect
    
    printf("### REMOVE LOCAL: Processing path: %s\n", path); // This shows the file path we’re working with
    
    // Extract filename
    char *last_slash = strrchr(path, '/'); // This looks for the last slash to separate folder and file
    if (last_slash) { // If we found a slash...
        strncpy(filename, last_slash + 1, MAX_PATH_LENGTH - 1); // This copies the filename after the slash
    } else { // If there’s no slash...
        strncpy(filename, path, MAX_PATH_LENGTH - 1); // This uses the whole path as the filename
    }
    
    // Check file extension and get corresponding server port
    const char *ext = getFileExtension(filename); // This gets the file’s extension, like “txt” or “pdf”
    int server_port = getServerPort(filename); // This finds which server handles that extension
    
    printf("### REMOVE LOCAL: File %s has extension %s, server port: %d\n", 
           filename, ext, server_port); // This shows the file, its extension, and the server port
    
    // Handle redirection based on file extension
    if (server_port != -1) { // If the file belongs to another server...
        // Modify path based on the server that should handle it
        if (strncmp(path, "~/S1/", 5) == 0) { // If the path starts with "~/S1/"...
            const char* remaining_path = path + 5; // This skips the "~/S1/" part
            if (strcmp(ext, "txt") == 0) { // If it’s a text file...
                snprintf(modified_path, MAX_PATH_LENGTH, "~/S3/%s", remaining_path); // This changes the path for server S3
                forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request to S3
                return; // This stops since another server is handling it
            } else if (strcmp(ext, "pdf") == 0) { // If it’s a PDF...
                snprintf(modified_path, MAX_PATH_LENGTH, "~/S2/%s", remaining_path); // This changes the path for server S2
                forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request to S2
                return; // This stops here
            } else if (strcmp(ext, "zip") == 0) { // If it’s a zip file...
                snprintf(modified_path, MAX_PATH_LENGTH, "~/S4/%s", remaining_path); // This changes the path for server S4
                forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request to S4
                return; // This stops here
            }
        } else if (strncmp(path, "~S1/", 4) == 0) { // If the path starts with "~S1/"...
            const char* remaining_path = path + 4; // This skips the "~S1/" part
            if (strcmp(ext, "txt") == 0) { // If it’s a text file...
                snprintf(modified_path, MAX_PATH_LENGTH, "~S3/%s", remaining_path); // This changes the path for server S3
                forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request to S3
                return; // This stops here
            } else if (strcmp(ext, "pdf") == 0) { // If it’s a PDF...
                snprintf(modified_path, MAX_PATH_LENGTH, "~S2/%s", remaining_path); // This changes the path for server S2
                forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request to S2
                return; // This stops here
            } else if (strcmp(ext, "zip") == 0) { // If it’s a zip file...
                snprintf(modified_path, MAX_PATH_LENGTH, "~S4/%s", remaining_path); // This changes the path for server S4
                forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request to S4
                return; // This stops here
            }
        }
    }
    
    // For S1 files or any other files, handle locally
    if (strncmp(path, "~/", 2) == 0) { // If the path starts with "~/"...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s%s", getenv("HOME"), path + 1); // This replaces ~ with the home folder
    } else if (path[0] == '~') { // If the path starts with just “~”...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s/%s", getenv("HOME"), path + 1); // This adds the home folder and the rest
    } else { // If it’s a regular path...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s", path); // This uses the path as is
    }
    
    printf("### REMOVE LOCAL: Resolved path for local removal: %s\n", actual_path); // This shows the final path we’ll use
    
    if (!fileExists(actual_path)) { // This checks if the file is actually there
        printf("### REMOVE LOCAL: File not found: %s\n", actual_path); // This says we couldn’t find the file
        send(client_socket, "ERROR: File not found", 21, 0); // This tells the client the file doesn’t exist
        return; // This stops since there’s nothing to delete
    }
    
    if (remove(actual_path) == 0) { // This tries to delete the file
        printf("### REMOVE LOCAL: Successfully removed file: %s\n", actual_path); // This says the file was deleted
        send(client_socket, "SUCCESS", 7, 0); // This tells the client it worked
    } else { // If deleting didn’t work...
        printf("### REMOVE LOCAL: Failed to remove file: %s (%s)\n", 
               actual_path, strerror(errno)); // This prints why it failed
        send(client_socket, "ERROR: Failed to remove file", 28, 0); // This tells the client we had trouble
    }
}
// New function to forward list request to other servers
// Enhanced function to forward list request to other servers
void forwardListRequest(int client_socket, const char *path, int server_port, char *response, size_t response_size) { // This asks another server for a list of files
    char buffer[BUFFER_SIZE]; // This holds messages we send or receive
    int sock = 0; // This will hold the connection to the other server
    struct sockaddr_in serv_addr; // This stores the other server’s address details
    
    printf("### LIST FWD: Forwarding list request to port %d for path %s\n", server_port, path); // This says what we’re doing
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // This tries to make a new connection
        printf("### LIST FWD: Socket creation error: %s\n", strerror(errno)); // This prints if we couldn’t make it
        return; // This stops since we can’t connect
    }
    
    // Configure server address
    memset(&serv_addr, 0, sizeof(serv_addr)); // This clears the address structure
    serv_addr.sin_family = AF_INET; // This says we’re using IPv4
    serv_addr.sin_port = htons(server_port); // This sets the port we’re connecting to
    
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { // This converts the IP address to binary
        printf("### LIST FWD: Invalid address\n"); // This prints if the address is wrong
        close(sock); // This closes the connection
        return; // This stops since the address is bad
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // This tries to connect to the server
        printf("### LIST FWD: Connection failed to port %d: %s\n", server_port, strerror(errno)); // This prints if we couldn’t connect
        close(sock); // This closes the connection
        return; // This stops since we can’t proceed
    }
    
    // Send LIST command
    snprintf(buffer, BUFFER_SIZE, "LIST %s", path); // This creates a command like “LIST folder”
    printf("### LIST FWD: Sending command: '%s'\n", buffer); // This shows the command we’re sending
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This tries to send the command
        printf("### LIST FWD: Failed to send command: %s\n", strerror(errno)); // This prints if sending failed
        close(sock); // This closes the connection
        return; // This stops since we hit a problem
    }
    
    // Get response from target server
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the server’s reply
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the list of files
    if (bytes_received <= 0) { // If we got no response or an error...
        printf("### LIST FWD: Failed to receive response: %s\n", 
               bytes_received == 0 ? "Connection closed" : strerror(errno)); // This prints what happened
        close(sock); // This closes the connection
        return; // This stops since we didn’t get a list
    }
    buffer[bytes_received] = '\0'; // This adds an end marker to make the response a proper string
    
    // Check if target server reported an error
    if (strncmp(buffer, "ERROR", 5) == 0) { // If the server sent an error...
        printf("### LIST FWD: Target server reported error: '%s'\n", buffer); // This shows the error message
        // Don't forward error to client
        close(sock); // This closes the connection
        return; // This stops without bothering the client
    }
    
    // Append the response to our collection
    if (strlen(buffer) > 0) { // If the server sent us some files...
        printf("### LIST FWD: Received file list from port %d (%zu bytes): '%s'\n", 
               server_port, strlen(buffer), buffer); // This shows the list we got
        strncat(response, buffer, response_size - strlen(response) - 1); // This adds the list to our collection
        if (strlen(response) < response_size - 1) // If there’s still room...
            strcat(response, "\n"); // This adds a new line to separate lists
    } else { // If the server sent nothing...
        printf("### LIST FWD: Received empty response from port %d\n", server_port); // This notes the empty reply
    }
    
    close(sock); // This closes the connection to the server
    printf("### LIST FWD: Listing completed from server on port %d\n", server_port); // This says we’re done with this server
}

// Helper function to parse directory path from S1 to other servers
void modifyPathForServer(const char *s1_path, char *modified_path, int server_num) { // This changes a path to match another server
    const char* remaining_path = NULL; // This will hold the part of the path we keep
    
    // Check for absolute path with HOME first
    if (strncmp(s1_path, getenv("HOME"), strlen(getenv("HOME"))) == 0) { // If the path starts with the home folder...
        // This is an absolute path like /home/ishmeet/S1/folder_1
        char *s1_dir_marker = strstr(s1_path, "/S1/"); // This looks for “/S1/” in the path
        if (s1_dir_marker) { // If we found “/S1/”...
            remaining_path = s1_dir_marker + 4; // This skips past “/S1/” to get the rest
            snprintf(modified_path, MAX_PATH_LENGTH, "~/S%d/%s", server_num, remaining_path); // This makes a new path like "~/S2/rest"
        }
    }
    // Otherwise check for the tilde formats
    else if (strncmp(s1_path, "~/S1/", 5) == 0) { // If the path starts with "~/S1/"...
        remaining_path = s1_path + 5; // This skips past “~/S1/”
        snprintf(modified_path, MAX_PATH_LENGTH, "~/S%d/%s", server_num, remaining_path); // This makes a path like “~/S3/rest”
    } 
    else if (strncmp(s1_path, "~S1/", 4) == 0) { // If the path starts with “~S1/”...
        remaining_path = s1_path + 4; // This skips past “~S1/”
        snprintf(modified_path, MAX_PATH_LENGTH, "~S%d/%s", server_num, remaining_path); // This makes a path like “~S4/rest”
    }
    else if (strstr(s1_path, "/S1/") != NULL) { // If “/S1/” appears anywhere in the path...
        const char* s1_marker = strstr(s1_path, "/S1/"); // This finds where “/S1/” is
        remaining_path = s1_marker + 4; // This skips past “/S1/”
        // Use ~/ format for other servers
        snprintf(modified_path, MAX_PATH_LENGTH, "~/S%d/%s", server_num, remaining_path); // This makes a path like “~/S2/rest”
    }
}
// Update handleListLocal function to get files from all servers
void handleListLocal(int client_socket, const char *path) { // This function lists files from different servers
    char actual_path[MAX_PATH_LENGTH]; // This will hold the full path for the local server (S1)
    char s2_path[MAX_PATH_LENGTH] = {0}; // This will hold the path for server S2
    char s3_path[MAX_PATH_LENGTH] = {0}; // This will hold the path for server S3
    char s4_path[MAX_PATH_LENGTH] = {0}; // This will hold the path for server S4
    char response[BUFFER_SIZE * 10] = {0}; // This will store the final list of all files
    char temp_response[BUFFER_SIZE * 10] = {0}; // This is a temporary space for file lists from other servers
    char *all_files[1000] = {NULL}; // This is an array to store up to 1000 file names
    int file_count = 0; // This keeps track of how many files we’ve found
    
    printf("### LIST: Original request path: %s\n", path); // This shows the path we’re working with
    
    // Convert the S1 path to actual path
    if (strncmp(path, "~/S1", 3) == 0) { // If the path starts with "~/S1"...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s%s", getenv("HOME"), path + 1); // This replaces "~" with the home folder and adds the rest
    } else if (strncmp(path, "~S1", 2) == 0) { // If the path starts with "~S1"...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s/%s", getenv("HOME"), path + 2); // This adds the home folder and the rest of the path
    } else { // If it’s a regular path...
        snprintf(actual_path, MAX_PATH_LENGTH, "%s", path); // This just uses the path as is
    }
    
    printf("### LIST: Listing files in: %s\n", actual_path); // This shows the final path for S1
    
    // Convert S1 path for other servers
    modifyPathForServer(path, s2_path, 2); // This changes the path to work for server S2
    modifyPathForServer(path, s3_path, 3); // This changes the path to work for server S3
    modifyPathForServer(path, s4_path, 4); // This changes the path to work for server S4
    
    printf("### LIST: Equivalent paths:\n"); // This announces we’re showing paths for other servers
    printf("###   S2: %s\n", s2_path); // This shows the path for S2
    printf("###   S3: %s\n", s3_path); // This shows the path for S3
    printf("###   S4: %s\n", s4_path); // This shows the path for S4
    
    // Get files from S1 (.c files)
    DIR *dir; // This will hold the folder we’re looking at
    struct dirent *ent; // This will hold info about each file in the folder
    struct stat st; // This will hold details about the folder itself
    
    if (stat(actual_path, &st) == 0 && S_ISDIR(st.st_mode)) { // This checks if the path is a real folder
        if ((dir = opendir(actual_path)) != NULL) { // This tries to open the folder
            while ((ent = readdir(dir)) != NULL) { // This loops through each file in the folder
                if (strstr(ent->d_name, ".c") != NULL) { // If the file ends with ".c"...
                    all_files[file_count] = strdup(ent->d_name); // This copies the filename to our list
                    file_count++; // This adds 1 to our file count
                }
            }
            closedir(dir); // This closes the folder since we’re done
        }
    }
    
    // Get files from S2 (.pdf files)
    memset(temp_response, 0, sizeof(temp_response)); // This clears the temporary space for S2’s files
    forwardListRequest(client_socket, s2_path, S2_PORT, temp_response, sizeof(temp_response)); // This asks S2 for its file list
    
    if (strlen(temp_response) > 0) { // If S2 sent us some files...
        char *token = strtok(temp_response, "\n"); // This splits the list into individual filenames
        while (token != NULL) { // This loops through each filename
            if (strstr(token, ".pdf") != NULL) { // If the file ends with ".pdf"...
                all_files[file_count] = strdup(token); // This adds the filename to our list
                file_count++; // This adds 1 to our file count
            }
            token = strtok(NULL, "\n"); // This gets the next filename
        }
    }
    
    // Get files from S3 (.txt files)
    memset(temp_response, 0, sizeof(temp_response)); // This clears the temporary space for S3’s files
    forwardListRequest(client_socket, s3_path, S3_PORT, temp_response, sizeof(temp_response)); // This asks S3 for its file list
    
    if (strlen(temp_response) > 0) { // If S3 sent us some files...
        char *token = strtok(temp_response, "\n"); // This splits the list into individual filenames
        while (token != NULL) { // This loops through each filename
            if (strstr(token, ".txt") != NULL) { // If the file ends with ".txt"...
                all_files[file_count] = strdup(token); // This adds the filename to our list
                file_count++; // This adds 1 to our file count
            }
            token = strtok(NULL, "\n"); // This gets the next filename
        }
    }
    
    // Get files from S4 (.zip files)
    memset(temp_response, 0, sizeof(temp_response)); // This clears the temporary space for S4’s files
    forwardListRequest(client_socket, s4_path, S4_PORT, temp_response, sizeof(temp_response)); // This asks S4 for its file list
    
    if (strlen(temp_response) > 0) { // If S4 sent us some files...
        char *token = strtok(temp_response, "\n"); // This splits the list into individual filenames
        while (token != NULL) { // This loops through each filename
            if (strstr(token, ".zip") != NULL) { // If the file ends with ".zip"...
                all_files[file_count] = strdup(token); // This adds the filename to our list
                file_count++; // This adds 1 to our file count
            }
            token = strtok(NULL, "\n"); // This gets the next filename
        }
    }
    
    // Sort files alphabetically within each file type
    for (int i = 0; i < file_count; i++) { // This loops through all files
        for (int j = i + 1; j < file_count; j++) { // This compares each file with the ones after it
            // Extract file extensions
            const char *ext1 = getFileExtension(all_files[i]); // This gets the extension of the first file
            const char *ext2 = getFileExtension(all_files[j]); // This gets the extension of the second file
            
            // If same extension, sort alphabetically
            if (strcmp(ext1, ext2) == 0 && strcmp(all_files[i], all_files[j]) > 0) { // If extensions match and first file comes after second...
                char *temp = all_files[i]; // This holds the first file temporarily
                all_files[i] = all_files[j]; // This swaps the first file with the second
                all_files[j] = temp; // This puts the first file in the second’s place
            }
        }
    }
    
    // First display all .c files
    for (int i = 0; i < file_count; i++) { // This loops through all files
        if (strstr(all_files[i], ".c") != NULL) { // If the file ends with ".c"...
            if (strlen(response) > 0) // If we already have some files in the list...
                strcat(response, "\n"); // This adds a new line to separate files
            strcat(response, all_files[i]); // This adds the .c file to the final list
        }
    }
    
    // Then display all .pdf files
    for (int i = 0; i < file_count; i++) { // This loops through all files again
        if (strstr(all_files[i], ".pdf") != NULL) { // If the file ends with ".pdf"...
            if (strlen(response) > 0) // If the list isn’t empty...
                strcat(response, "\n"); // This adds a new line
            strcat(response, all_files[i]); // This adds the .pdf file to the list
        }
    }
    
    // Then display all .txt files
    for (int i = 0; i < file_count; i++) { // This loops through all files again
        if (strstr(all_files[i], ".txt") != NULL) { // If the file ends with ".txt"...
            if (strlen(response) > 0) // If we have files already...
                strcat(response, "\n"); // This adds a new line
            strcat(response, all_files[i]); // This adds the .txt file to the list
        }
    }
    
    // Finally display all .zip files
    for (int i = 0; i < file_count; i++) { // This loops through all files one last time
        if (strstr(all_files[i], ".zip") != NULL) { // If the file ends with ".zip"...
            if (strlen(response) > 0) // If the list has files...
                strcat(response, "\n"); // This adds a new line
            strcat(response, all_files[i]); // This adds the .zip file to the list
        }
    }
    
    // Free memory
    for (int i = 0; i < file_count; i++) { // This loops through all files we stored
        if (all_files[i] != NULL) { // If the file entry exists...
            free(all_files[i]); // This frees the memory we used for the filename
        }
    }
    
    printf("### LIST: Sending file list:\n%s\n", response); // This shows the final list we’re sending
    if (send(client_socket, response, strlen(response), 0) < 0) { // This tries to send the list to the client
        printf("Failed to send file list: %s\n", strerror(errno)); // This prints an error if sending fails
    }
}
void forwardTarRequest(int client_socket, const char *filetype, int server_port) { // This sends a request to bundle files
    char buffer[BUFFER_SIZE]; // This holds messages we send or receive
    int sock = 0; // This will hold the connection to the other server
    struct sockaddr_in serv_addr; // This stores the server’s address details
    
    printf("### TAR FWD: Forwarding tar request to port %d for filetype %s\n", server_port, filetype); // This says what we’re doing
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // This tries to make a new connection
        printf("### TAR FWD: Socket creation error: %s\n", strerror(errno)); // This prints if we couldn’t make it
        send(client_socket, "ERROR: Could not create socket", 29, 0); // This tells the client we had trouble
        return; // This stops since we can’t connect
    }
    
    // Configure server address
    memset(&serv_addr, 0, sizeof(serv_addr)); // This clears the address structure
    serv_addr.sin_family = AF_INET; // This says we’re using IPv4
    serv_addr.sin_port = htons(server_port); // This sets the port we’re connecting to
    
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) { // This converts the IP address to binary
        printf("### TAR FWD: Invalid address\n"); // This prints if the address is wrong
        send(client_socket, "ERROR: Invalid server address", 28, 0); // This tells the client it’s bad
        close(sock); // This closes the connection
        return; // This stops since we can’t proceed
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // This tries to connect to the server
        printf("### TAR FWD: Connection failed to port %d: %s\n", server_port, strerror(errno)); // This prints if we couldn’t connect
        send(client_socket, "ERROR: Could not connect to target server", 40, 0); // This tells the client we failed
        close(sock); // This closes the connection
        return; // This stops here
    }
    
    // Send TAR command
    snprintf(buffer, BUFFER_SIZE, "TAR %s", filetype); // This creates a command like “TAR pdf”
    printf("### TAR FWD: Sending command: %s\n", buffer); // This shows the command we’re sending
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This tries to send the command
        printf("### TAR FWD: Failed to send command\n"); // This prints if it didn’t send
        send(client_socket, "ERROR: Failed to send to target server", 38, 0); // This tells the client we had an issue
        close(sock); // This closes the connection
        return; // This stops since we hit a problem
    }
    
    // Get file size from target server
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the server’s reply
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the size of the bundled file
    if (bytes_received <= 0) { // If we didn’t get a response...
        printf("### TAR FWD: Failed to receive file size\n"); // This prints that something went wrong
        send(client_socket, "ERROR: Target server error", 26, 0); // This tells the client the server didn’t respond
        close(sock); // This closes the connection
        return; // This stops since we can’t continue
    }
    buffer[bytes_received] = '\0'; // This adds an end marker to make it a proper string
    
    // Check if target server reported an error
    if (strncmp(buffer, "ERROR", 5) == 0) { // If the server sent an error...
        printf("### TAR FWD: Target server reported error: %s\n", buffer); // This shows the error message
        send(client_socket, buffer, strlen(buffer), 0); // This passes the error to the client
        close(sock); // This closes the connection
        return; // This stops since there’s an issue
    }
    
    printf("### TAR FWD: Received file size: %s\n", buffer); // This shows the size of the bundled file
    
    // Forward file size to client
    if (send(client_socket, buffer, strlen(buffer), 0) < 0) { // This tries to send the size to the client
        printf("### TAR FWD: Failed to send file size to client\n"); // This prints if it didn’t work
        close(sock); // This closes the connection
        return; // This stops since we can’t proceed
    }
    
    // Wait for client's SIZE_ACK
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the client’s response
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // This waits for the client to say “SIZE_ACK”
    if (bytes_received <= 0 || strcmp(buffer, "SIZE_ACK") != 0) { // If the client didn’t confirm properly...
        printf("### TAR FWD: Client SIZE_ACK failed\n"); // This prints that the client didn’t respond right
        close(sock); // This closes the connection
        return; // This stops since something’s wrong
    }
    
    // Forward SIZE_ACK to target server
    printf("### TAR FWD: Sending SIZE_ACK to target server\n"); // This says we’re confirming to the server
    if (send(sock, "SIZE_ACK", 8, 0) < 0) { // This sends “SIZE_ACK” to the server
        printf("### TAR FWD: Failed to send SIZE_ACK to target\n"); // This prints if it didn’t send
        close(sock); // This closes the connection
        return; // This stops since we hit a snag
    }
    
    // Forward file data from target server to client
    long total_bytes = 0; // This tracks how many bytes we’ve sent
    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) { // This keeps getting chunks of the bundled file
        printf("### TAR FWD: Received %zd bytes from target\n", bytes_received); // This shows how much we got
        if (send(client_socket, buffer, bytes_received, 0) < 0) { // This tries to send the chunk to the client
            printf("### TAR FWD: Failed to forward data to client\n"); // This prints if it didn’t work
            close(sock); // This closes the connection
            return; // This stops since we can’t continue
        }
        total_bytes += bytes_received; // This adds the chunk size to our total
        printf("### TAR FWD: Forwarded total: %ld bytes\n", total_bytes); // This shows our progress
    }
    
    printf("### TAR FWD: File transfer complete, waiting for SUCCESS from client\n"); // This says we finished sending the file
    
    // Forward SUCCESS from client to target server
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the client’s response
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // This waits for the client to say “SUCCESS”
    if (bytes_received > 0 && strcmp(buffer, "SUCCESS") == 0) { // If the client confirmed success...
        printf("### TAR FWD: Received SUCCESS from client, forwarding to target\n"); // This says we got the confirmation
        if (send(sock, "SUCCESS", 7, 0) < 0) { // This tries to tell the server we’re done
            printf("### TAR FWD: Failed to send SUCCESS to target\n"); // This prints if it didn’t send
        }
    } else { // If we didn’t get “SUCCESS”...
        printf("### TAR FWD: Did not receive SUCCESS from client\n"); // This notes we didn’t get confirmation
    }
    
    close(sock); // This closes the connection to the server
    printf("### TAR FWD: Tar forwarding completed\n"); // This says we’re all done
}
void handleTarLocal(int client_socket, const char *filetype) { // This function handles requests to bundle files of a certain type
    char tar_command[MAX_PATH_LENGTH * 2]; // This will hold the command to create a tar file
    char tar_path[MAX_PATH_LENGTH]; // This will store the path where the tar file is saved
    char buffer[BUFFER_SIZE]; // This is a space for sending and receiving messages
    
    printf("Handling tar request for filetype: %s\n", filetype); // This says what type of files we’re bundling
    
    // Handle C files locally
    if (strcmp(filetype, "c") == 0) { // If the request is for .c files...
        snprintf(tar_path, MAX_PATH_LENGTH, "/tmp/c_files.tar"); // This sets the tar file’s name and location
        snprintf(tar_command, MAX_PATH_LENGTH * 2, "find %s/S1 -name \"*.c\" -type f | tar -cf %s -T -", getenv("HOME"), tar_path); // This builds a command to find all .c files and bundle them
        printf("Executing tar command: %s\n", tar_command); // This shows the command we’re about to run
        int result = system(tar_command); // This runs the command to create the tar file
        if (result != 0 || !fileExists(tar_path)) { // If the command failed or the file isn’t there...
            printf("Failed to create tar file: result=%d\n", result); // This says something went wrong
            send(client_socket, "ERROR: Failed to create tar file", 32, 0); // This tells the client we couldn’t make the file
            return; // This stops since we can’t continue
        }
        
        // Send tar file to client
        FILE *file = fopen(tar_path, "rb"); // This opens the tar file for reading
        if (!file) { // If we couldn’t open it...
            printf("Could not open tar file %s: %s\n", tar_path, strerror(errno)); // This says why it failed
            send(client_socket, "ERROR: Could not open tar file", 30, 0); // This tells the client we had trouble
            return; // This stops here
        }
        
        fseek(file, 0, SEEK_END); // This jumps to the end of the file
        long file_size = ftell(file); // This gets the file’s size
        fseek(file, 0, SEEK_SET); // This goes back to the start
        
        snprintf(buffer, BUFFER_SIZE, "%ld", file_size); // This puts the file size into the buffer as text
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) { // This tries to send the size to the client
            printf("Failed to send tar file size: %s\n", strerror(errno)); // This prints if sending failed
            fclose(file); // This closes the file
            remove(tar_path); // This deletes the tar file to clean up
            return; // This stops since we hit a problem
        }
        
        memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the client’s response
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // This waits for the client to say “SIZE_ACK”
        if (bytes_received <= 0 || strcmp(buffer, "SIZE_ACK") != 0) { // If the client didn’t confirm properly...
            buffer[bytes_received > 0 ? bytes_received : 0] = '\0'; // This ensures the buffer is a proper string
            printf("Size acknowledgment failed: %s\n", bytes_received > 0 ? buffer : "No response"); // This says what went wrong
            fclose(file); // This closes the file
            remove(tar_path); // This deletes the tar file
            send(client_socket, "ERROR: Size acknowledgment failed", 33, 0); // This tells the client it failed
            return; // This stops here
        }
        
        size_t bytes_read; // This tracks how much we read from the file
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) { // This reads chunks of the tar file
            if (send(client_socket, buffer, bytes_read, 0) < 0) { // This tries to send each chunk to the client
                printf("Failed to send tar file data: %s\n", strerror(errno)); // This prints if sending failed
                fclose(file); // This closes the file
                remove(tar_path); // This deletes the tar file
                send(client_socket, "ERROR: Failed to send file", 26, 0); // This tells the client we had trouble
                return; // This stops here
            }
        }
        
        if (ferror(file)) { // This checks if there was a problem reading the file
            printf("Error reading tar file: %s\n", strerror(errno)); // This prints the issue
            fclose(file); // This closes the file
            remove(tar_path); // This deletes the tar file
            send(client_socket, "ERROR: File read error", 23, 0); // This tells the client it failed
            return; // This stops here
        }
        
        fclose(file); // This closes the file since we’re done
        remove(tar_path); // This deletes the tar file to clean up
        
        memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the client’s final response
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // This waits for the client to say “SUCCESS”
        if (bytes_received <= 0 || strcmp(buffer, "SUCCESS") != 0) { // If we didn’t get “SUCCESS”...
            buffer[bytes_received > 0 ? bytes_received : 0] = '\0'; // This ensures the buffer is a string
            printf("Tar file transfer status: %s\n", bytes_received > 0 ? buffer : "No response"); // This notes what we got
        } else { // If we got “SUCCESS”...
            printf("Tar file successfully sent\n"); // This says everything worked
        }
    }
    // Forward txt files to S3
    else if (strcmp(filetype, "txt") == 0) { // If the request is for .txt files...
        printf("### TAR: Forwarding tar request for TXT files to S3\n"); // This says we’re sending it to server S3
        forwardTarRequest(client_socket, filetype, S3_PORT); // This passes the request to S3
    }
    // Forward pdf files to S2
    else if (strcmp(filetype, "pdf") == 0) { // If the request is for .pdf files...
        printf("### TAR: Forwarding tar request for PDF files to S2\n"); // This says we’re sending it to server S2
        forwardTarRequest(client_socket, filetype, S2_PORT); // This passes the request to S2
    }
    // Explicitly reject ZIP files
    else if (strcmp(filetype, "zip") == 0) { // If the request is for .zip files...
        printf("### TAR: ZIP file archiving not supported\n"); // This says we don’t support zipping zip files
        send(client_socket, "ERROR: ZIP file archiving not supported", 38, 0); // This tells the client it’s not allowed
    }
    // Reject any other file types
    else { // If it’s some other file type...
        printf("Invalid filetype: %s\n", filetype); // This says the type isn’t recognized
        send(client_socket, "ERROR: Only .c, .txt, and .pdf file types are supported", 54, 0); // This tells the client what we support
    }
}

void processClientCommand(int client_socket, Command *cmd) { // This figures out what the client wants to do
    char buffer[BUFFER_SIZE]; // This holds messages we send or receive
    printf("Processing command: %s\n", cmd->command); // This shows the command we got
    
    if (strcmp(cmd->command, "downlf") == 0) { // If the client wants to download a file...
        // For download, pass the original path directly without transforming
        handleDownloadLocal(client_socket, cmd->arg1); // This starts the download process
    }
    else if (strcmp(cmd->command, "uploadf") == 0) { // If the client wants to upload a file...
        char client_file[MAX_PATH_LENGTH] = {0}; // This will hold the file’s name on the client’s side
        char server_path[MAX_PATH_LENGTH] = {0}; // This will hold the folder path on the server
        char full_server_path[MAX_PATH_LENGTH] = {0}; // This will hold the complete path with filename
        char filename[MAX_PATH_LENGTH] = {0}; // This will store just the filename
        
        snprintf(client_file, MAX_PATH_LENGTH, "%s", cmd->arg1); // This copies the client’s file path
        snprintf(server_path, MAX_PATH_LENGTH, "%s", cmd->arg2); // This copies the server’s folder path
        printf("Client file: %s, Server path: %s\n", client_file, server_path); // This shows both paths
        
        // Extract filename safely
        char *slash = strrchr(client_file, '/'); // This looks for the last slash in the client’s path
        if (slash && *(slash + 1)) { // If we found a slash and there’s a filename after it...
            snprintf(filename, MAX_PATH_LENGTH, "%s", slash + 1); // This copies just the filename
        } else { // If no slash...
            snprintf(filename, MAX_PATH_LENGTH, "%s", client_file); // This uses the whole path as the filename
        }
        printf("Extracted filename: %s\n", filename); // This shows the filename we got
        
        snprintf(full_server_path, MAX_PATH_LENGTH, "%s/%s", server_path, filename); // This combines the folder and filename
        printf("Processing upload: %s to %s\n", client_file, full_server_path); // This shows the full upload plan
        
        char actual_dir[MAX_PATH_LENGTH]; // This will hold the real folder path
        if (strncmp(server_path, "~/S1", 3) == 0) { // If the path starts with "~/S1"...
            snprintf(actual_dir, MAX_PATH_LENGTH, "%s%s", getenv("HOME"), server_path + 1); // This replaces "~" with the home folder
        } else if (strncmp(server_path, "~S1", 2) == 0) { // If it starts with "~S1"...
            snprintf(actual_dir, MAX_PATH_LENGTH, "%s/%s", getenv("HOME"), server_path + 2); // This adds the home folder and rest
        } else { // If it’s not a valid path...
            printf("Invalid path: %s\n", server_path); // This says the path is wrong
            send(client_socket, "ERROR: Invalid path, must start with ~/S1 or ~S1", 47, 0); // This tells the client what’s wrong
            return; // This stops here
        }
        
        printf("Creating directory for: %s\n", actual_dir); // This says we’re making the folder
        if (createDirectories(actual_dir) == -1 && errno != EEXIST) { // This tries to create the folder, unless it already exists
            printf("Initial directory creation failed: %s\n", strerror(errno)); // This prints why it failed
            send(client_socket, "ERROR: Could not create directory", 33, 0); // This tells the client we couldn’t make the folder
            return; // This stops here
        }
        
        printf("Sending READY\n"); // This says we’re ready to get the file
        if (send(client_socket, "READY", 5, 0) < 0) { // This sends “READY” to the client
            printf("Failed to send READY: %s\n", strerror(errno)); // This prints if it didn’t send
            return; // This stops since we hit a problem
        }
        
        memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for the file size
        printf("Waiting for file size\n"); // This says we’re waiting for the size
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // This waits for the client to send the size
        if (bytes_received <= 0) { // If we didn’t get anything...
            printf("Failed to receive file size: %s\n", strerror(errno)); // This prints what went wrong
            send(client_socket, "ERROR: Could not receive file size", 33, 0); // This tells the client we had trouble
            return; // This stops here
        }
        
        buffer[bytes_received] = '\0'; // This makes the buffer a proper string
        printf("Received file size: %s\n", buffer); // This shows the size we got
        long file_size = atol(buffer); // This converts the size to a number
        if (file_size <= 0 || file_size > MAX_FILE_SIZE) { // If the size is invalid or too big...
            printf("Invalid file size: %ld\n", file_size); // This says the size is no good
            send(client_socket, "ERROR: Invalid file size", 24, 0); // This tells the client it’s wrong
            return; // This stops here
        }
        
        const char *ext = getFileExtension(filename); // This gets the file’s extension
        int server_port = getServerPort(filename); // This finds which server handles that extension
        printf("File extension: %s, Server port: %d\n", ext, server_port); // This shows the extension and server
        
        if (server_port != -1) { // If another server should handle it...
            char dest_path[MAX_PATH_LENGTH]; // This will hold the path for the other server
            int offset = (strncmp(server_path, "~/S1", 3) == 0) ? 4 : 3; // This figures out how much to skip
            if (strcmp(ext, "pdf") == 0) { // If it’s a PDF...
                snprintf(dest_path, MAX_PATH_LENGTH, "~/S2%s/%s", server_path + offset, filename); // This sets the path for S2
            } else if (strcmp(ext, "txt") == 0) { // If it’s a text file...
                snprintf(dest_path, MAX_PATH_LENGTH, "~/S3%s/%s", server_path + offset, filename); // This sets the path for S3
            } else if (strcmp(ext, "zip") == 0) { // If it’s a zip file...
                snprintf(dest_path, MAX_PATH_LENGTH, "~/S4%s/%s", server_path + offset, filename); // This sets the path for S4
            }
            printf("Forwarding to port %d: %s\n", server_port, dest_path); // This says where we’re sending it
            transferFileToServer(client_socket, dest_path, server_port, file_size); // This sends the file to the other server
        } else { // If we handle it locally...
            printf("Handling locally\n"); // This says we’ll deal with it here
            if (send(client_socket, "SIZE_ACK", 8, 0) < 0) { // This tells the client we’re ready for the file
                printf("Failed to send SIZE_ACK: %s\n", strerror(errno)); // This prints if it didn’t send
                return; // This stops here
            }
            handleUploadLocal(client_socket, full_server_path, file_size); // This starts the local upload
        }
    }
    else if (strcmp(cmd->command, "removef") == 0) { // If the client wants to delete a file...
        char path[MAX_PATH_LENGTH] = {0}; // This will hold the file’s path
        strncpy(path, cmd->arg1, MAX_PATH_LENGTH - 1); // This copies the path from the command
        
        // Extract filename
        char filename[MAX_PATH_LENGTH] = {0}; // This will store just the filename
        char *last_slash = strrchr(path, '/'); // This looks for the last slash
        if (last_slash) { // If we found a slash...
            strncpy(filename, last_slash + 1, MAX_PATH_LENGTH - 1); // This copies the filename
        } else { // If no slash...
            strncpy(filename, path, MAX_PATH_LENGTH - 1); // This uses the whole path
        }
        
        // Check file extension and get corresponding server port
        const char *ext = getFileExtension(filename); // This gets the file’s extension
        int server_port = getServerPort(filename); // This finds the right server for the extension
        
        printf("REMOVE: File %s has extension %s, server port: %d\n", 
               filename, ext, server_port); // This shows the file details
        
        // Handle redirection based on file extension
        if (server_port != -1) { // If another server handles this file...
            char modified_path[MAX_PATH_LENGTH] = {0}; // This will hold the new path for the other server
            
            // Check for tilde paths
            if (strncmp(path, "~/S1/", 5) == 0) { // If the path starts with "~/S1/"...
                const char* remaining_path = path + 5; // This skips past "~/S1/"
                if (strcmp(ext, "txt") == 0) { // If it’s a text file...
                    snprintf(modified_path, MAX_PATH_LENGTH, "~/S3/%s", remaining_path); // This sets the path for S3
                    printf("REMOVE: Forwarding to S3 with path: %s\n", modified_path); // This says we’re sending it to S3
                    forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request
                    return; // This stops here
                } else if (strcmp(ext, "pdf") == 0) { // If it’s a PDF...
                    snprintf(modified_path, MAX_PATH_LENGTH, "~/S2/%s", remaining_path); // This sets the path for S2
                    printf("REMOVE: Forwarding to S2 with path: %s\n", modified_path); // This says we’re sending it to S2
                    forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request
                    return; // This stops here
                } else if (strcmp(ext, "zip") == 0) { // If it’s a zip file...
                    snprintf(modified_path, MAX_PATH_LENGTH, "~/S4/%s", remaining_path); // This sets the path for S4
                    printf("REMOVE: Forwarding to S4 with path: %s\n", modified_path); // This says we’re sending it to S4
                    forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request
                    return; // This stops here
                }
            } else if (strncmp(path, "~S1/", 4) == 0) { // If the path starts with "~S1/"...
                const char* remaining_path = path + 4; // This skips past "~S1/"
                if (strcmp(ext, "txt") == 0) { // If it’s a text file...
                    snprintf(modified_path, MAX_PATH_LENGTH, "~S3/%s", remaining_path); // This sets the path for S3
                    printf("REMOVE: Forwarding to S3 with path: %s\n", modified_path); // This says we’re sending it to S3
                    forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request
                    return; // This stops here
                } else if (strcmp(ext, "pdf") == 0) { // If it’s a PDF...
                    snprintf(modified_path, MAX_PATH_LENGTH, "~S2/%s", remaining_path); // This sets the path for S2
                    printf("REMOVE: Forwarding to S2 with path: %s\n", modified_path); // This says we’re sending it to S2
                    forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request
                    return; // This stops here
                } else if (strcmp(ext, "zip") == 0) { // If it’s a zip file...
                    snprintf(modified_path, MAX_PATH_LENGTH, "~S4/%s", remaining_path); // This sets the path for S4
                    printf("REMOVE: Forwarding to S4 with path: %s\n", modified_path); // This says we’re sending it to S4
                    forwardRemoveRequest(client_socket, modified_path, server_port); // This sends the delete request
                    return; // This stops here
                }
            }
        }
        
        // If file type doesn't need forwarding or path format not recognized, 
        // handle locally
        char actual_path[MAX_PATH_LENGTH]; // This will hold the real file path
        if (strncmp(path, "~/", 2) == 0) { // If the path starts with "~/"...
            snprintf(actual_path, MAX_PATH_LENGTH, "%s%s", getenv("HOME"), path + 1); // This replaces "~" with the home folder
        } else if (path[0] == '~') { // If it starts with just "~"...
            snprintf(actual_path, MAX_PATH_LENGTH, "%s/%s", getenv("HOME"), path + 1); // This adds the home folder and rest
        } else { // If it’s a regular path...
            snprintf(actual_path, MAX_PATH_LENGTH, "%s", path); // This uses the path as is
        }
        
        printf("REMOVE: Handling locally at path: %s\n", actual_path); // This says we’re deleting it here
        
        // For local removal
        if (!fileExists(actual_path)) { // This checks if the file is there
            printf("REMOVE: File not found: %s\n", actual_path); // This says we couldn’t find it
            send(client_socket, "ERROR: File not found", 21, 0); // This tells the client it’s missing
            return; // This stops here
        }
        
        if (remove(actual_path) == 0) { // This tries to delete the file
            printf("REMOVE: Successfully removed file: %s\n", actual_path); // This says it worked
            send(client_socket, "SUCCESS", 7, 0); // This tells the client it’s done
        } else { // If deleting failed...
            printf("REMOVE: Failed to remove file: %s (%s)\n", actual_path, strerror(errno)); // This prints why
            send(client_socket, "ERROR: Failed to remove file", 28, 0); // This tells the client we had trouble
        }
    }
    else if (strcmp(cmd->command, "dispfnames") == 0) { // If the client wants a list of files...
        char actual_path[MAX_PATH_LENGTH]; // This will hold the folder path
        snprintf(actual_path, MAX_PATH_LENGTH, "%s", cmd->arg1); // This copies the path
        handleListLocal(client_socket, actual_path); // This starts the file listing process
    }
    else if (strcmp(cmd->command, "downltar") == 0) { // If the client wants to bundle files...
        handleTarLocal(client_socket, cmd->arg1); // This starts the bundling process
    }
    else { // If we don’t know the command...
        printf("Unknown command: %s\n", cmd->command); // This says it’s not recognized
        send(client_socket, "ERROR: Unknown command", 22, 0); // This tells the client it’s invalid
    }
}
void processRequest(int client_socket) { // This handles a single client request
    char buffer[BUFFER_SIZE]; // This holds the client’s message
    ssize_t bytes_received; // This tracks how much data we got
    memset(buffer, 0, BUFFER_SIZE); // This clears the buffer to start fresh
    printf("Waiting for client command\n"); // This says we’re ready for a command
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // This waits for the client’s message
    if (bytes_received <= 0) { // If we got nothing or there was an error...
        printf("No data received from client: %s\n", strerror(errno)); // This prints what went wrong
        close(client_socket); // This closes the connection
        return; // This stops here
    }
    buffer[bytes_received] = '\0'; // This makes the message a proper string
    printf("Raw command: %s\n", buffer); // This shows the exact message
    Command cmd; // This will hold the broken-down command
    parseCommand(buffer, &cmd); // This splits the message into parts
    printf("Parsed command: %s, Arg1: %s, Arg2: %s\n", cmd.command, cmd.arg1, cmd.arg2); // This shows the command and its pieces
    processClientCommand(client_socket, &cmd); // This figures out what to do with the command
    printf("Closing client socket\n"); // This says we’re done with the client
    close(client_socket); // This closes the connection
}

void sig_child(int signo) { // This handles when a child process finishes
    pid_t pid; // This will hold the ID of the child process
    int stat; // This will hold the child’s exit status
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) { // This checks for any finished child processes
        printf("Child process %d terminated\n", pid); // This says which process ended
    }
}

int main() { // This is where the program starts
    int server_fd, client_socket; // These will hold the server and client connections
    struct sockaddr_in address; // This stores the server’s address details
    int opt = 1; // This is a setting to reuse the port
    socklen_t addrlen = sizeof(address); // This is the size of the address structure
    pid_t child_pid; // This will hold the ID of a child process
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { // This tries to create a server socket
        perror("Socket creation failed"); // This prints if it didn’t work
        exit(EXIT_FAILURE); // This stops the program
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { // This tries to set options for the server socket
        perror("Setsockopt failed"); // This prints an error if setting the options didn’t work
        exit(EXIT_FAILURE); // This stops the program if we couldn’t set the options
    }
    
    address.sin_family = AF_INET; // This says we’re using IPv4 for the server’s address
    address.sin_addr.s_addr = INADDR_ANY; // This allows the server to accept connections from any network address
    address.sin_port = htons(PORT); // This sets the port number the server will use, converting it to the right format
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { // This tries to attach the server socket to the address and port
        perror("Bind failed"); // This prints an error if binding didn’t work
        exit(EXIT_FAILURE); // This stops the program if binding failed
    }
    
    if (listen(server_fd, 10) < 0) { // This tells the server to start listening for up to 10 clients at a time
        perror("Listen failed"); // This prints an error if listening couldn’t start
        exit(EXIT_FAILURE); // This stops the program if listening failed
    }
    
    signal(SIGCHLD, sig_child); // This sets up a way to handle when child processes finish, calling sig_child
    
    char s1_path[MAX_PATH_LENGTH]; // This will hold the path for the S1 folder
    snprintf(s1_path, MAX_PATH_LENGTH, "%s/S1", getenv("HOME")); // This creates the path by adding “/S1” to the home folder
    printf("Creating S1 directory: %s\n", s1_path); // This shows the folder we’re trying to create
    if (mkdir(s1_path, 0755) == -1 && errno != EEXIST) { // This tries to make the S1 folder with specific permissions
        printf("Failed to create S1: %s\n", strerror(errno)); // This prints why folder creation failed, unless it already exists
    }
    
    printf("S1 server started on port %d...\n", PORT); // This says the server is up and running on the chosen port
    
    while (1) { // This starts an endless loop to keep the server running
        printf("Waiting for client connection\n"); // This says we’re ready for a new client
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) { // This waits for a client to connect
            perror("Accept failed"); // This prints an error if accepting the connection didn’t work
            continue; // This skips to the next loop to try again
        }
        
        printf("New client connected\n"); // This says a client has successfully connected
        
        if ((child_pid = fork()) == 0) { // This creates a new process to handle the client
            // Child process
            close(server_fd); // This closes the main server socket since the child doesn’t need it
            
            // Handle multiple requests from this client
            char buffer[BUFFER_SIZE]; // This will hold the client’s messages
            ssize_t bytes_received; // This tracks how much data we get
            
            while (1) { // This starts a loop to handle multiple commands from the client
                memset(buffer, 0, BUFFER_SIZE); // This clears the buffer for a new message
                printf("Waiting for client command\n"); // This says we’re ready for the client’s next command
                bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0); // This waits for the client to send something
                
                if (bytes_received <= 0) { // If we got no data or there was an error...
                    printf("Client disconnected: %s\n", strerror(errno)); // This says the client left or something went wrong
                    break; // This stops the loop since the client is gone
                }
                
                buffer[bytes_received] = '\0'; // This turns the message into a proper string
                printf("Raw command: %s\n", buffer); // This shows the exact message we got
                
                Command cmd; // This will hold the broken-down command
                parseCommand(buffer, &cmd); // This splits the message into command and arguments
                printf("Parsed command: %s, Arg1: %s, Arg2: %s\n", cmd.command, cmd.arg1, cmd.arg2); // This shows the command pieces
                
                if (strcmp(cmd.command, "exit") == 0) { // If the client wants to quit...
                    printf("Client requested exit\n"); // This says they’re done
                    break; // This stops the loop to end the session
                }
                
                processClientCommand(client_socket, &cmd); // This handles whatever the client asked for
            }
            
            printf("Closing client socket\n"); // This says we’re done with this client
            close(client_socket); // This closes the connection to the client
            exit(0); // This ends the child process
        } else if (child_pid < 0) { // If creating the child process failed...
            perror("Fork failed"); // This prints why it didn’t work
            close(client_socket); // This closes the client connection
        } else { // This is the parent process
            // Parent process
            close(client_socket); // This closes the client socket since the parent doesn’t need it
        }
    }
    
    close(server_fd); // This closes the main server socket (though we never reach here because of the loop)
    return 0; // This says the program ended successfully (also never reached)
}
