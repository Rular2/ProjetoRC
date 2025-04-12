#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>

#define SERVER_PORT     9000
#define BUF_SIZE        1024
#define DEFAULT_BACKLOG 5
#define MAX_USERS       100
#define MAX_USERNAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 50
#define DATABASE_FILE "credentials.txt"
#define ENGINEERS_FILE "engineers.txt"
#define ORGANIZATIONS_FILE "organizations.txt"

// Structure to store user information
typedef struct {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    int user_type;  // 1 for Engineer, 2 for Organization
    int socket_fd;  // Socket file descriptor for this user
    int is_online;  // 1 if user is online, 0 otherwise
} User;

// Array to store online users
User users[MAX_USERS];
int numUsers = 0;

// Function prototypes
void handle_sigchld(int sig);
void process_client(int client_fd);
void send_message(int client_socket, const char *message);
int show_login_menu(int client_socket);
void process_login(int client_socket);
void process_registration(int client_socket);
int check_credentials(int client_socket, char *username, char *password);
void add_user_to_file(char *username, char *password, int user_type);
void show_main_menu(int client_socket, char *username, int user_type);
void register_engineer(int client_socket, char *username);
void register_organization(int client_socket, char *username);
void list_engineers(int client_socket);
void list_organizations(int client_socket);
void view_profile(int client_socket, char *username, int user_type);
int username_exists(const char *username);
void erro(const char *msg);
void add_user_to_online_list(char *username, char *password, int user_type, int client_socket);
char *receive_string(int client_socket);

// Handler to prevent zombie processes
void handle_sigchld(int sig) {
    // Reap all dead processes
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Message sending with error handling
void send_message(int client_socket, const char *message) {
    ssize_t bytes_sent = send(client_socket, message, strlen(message), 0);
    if (bytes_sent < 0) {
        perror("Error sending message");
    } else if (bytes_sent < strlen(message)) {
        fprintf(stderr, "Warning: Message sent partially\n");
    }
}

char *receive_string(int client_socket) {
    static char buffer[BUF_SIZE];
    int nread = recv(client_socket, buffer, BUF_SIZE - 1, 0);
    
    if (nread <= 0) {
        buffer[0] = '\0';
        return buffer;
    }
    
    // Remove trailing newline if present
    if (buffer[nread-1] == '\n') {
        buffer[nread-1] = '\0';
    } else {
        buffer[nread] = '\0';
    }
    
    return buffer;
}

int show_login_menu(int client_socket) {
    char buffer[BUF_SIZE];
    
    send_message(client_socket, "\n===== Welcome to Engineering Platform =====\n");
    send_message(client_socket, "1. Login\n");
    send_message(client_socket, "2. Register\n");
    send_message(client_socket, "3. Exit\n");
    send_message(client_socket, "=======================================\n");
    send_message(client_socket, "Enter your choice: ");
    
    int nread = recv(client_socket, buffer, BUF_SIZE - 1, 0);
    if (nread <= 0) {
        return -1;
    }
    
    buffer[nread] = '\0';
    return atoi(buffer);
}

void process_login(int client_socket) {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    
    send_message(client_socket, "Enter username: ");
    strcpy(username, receive_string(client_socket));
    
    send_message(client_socket, "Enter password: ");
    strcpy(password, receive_string(client_socket));
    
    int user_type = check_credentials(client_socket, username, password);
    if (user_type > 0) {
        char success_msg[BUF_SIZE];
        snprintf(success_msg, BUF_SIZE, "Login successful! Welcome, %s!\n", username);
        send_message(client_socket, success_msg);
        
        // Add user to online list
        add_user_to_online_list(username, password, user_type, client_socket);
        
        // Show main menu based on user type
        show_main_menu(client_socket, username, user_type);
    } else {
        send_message(client_socket, "Invalid username or password. Please try again.\n");
    }
}

int check_credentials(int client_socket, char *username, char *password) {
    FILE *file = fopen(DATABASE_FILE, "r");
    if (file == NULL) {
        send_message(client_socket, "Error opening credentials file!\n");
        return 0;
    }
    
    char line[BUF_SIZE];
    char stored_username[MAX_USERNAME_LENGTH];
    char stored_password[MAX_PASSWORD_LENGTH];
    int user_type;
    
    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "%s %s %d", stored_username, stored_password, &user_type) == 3) {
            if (strcmp(username, stored_username) == 0 && strcmp(password, stored_password) == 0) {
                fclose(file);
                return user_type;  // Return user type (1 for Engineer, 2 for Organization)
            }
        }
    }
    
    fclose(file);
    return 0;  // Authentication failed
}

void add_user_to_online_list(char *username, char *password, int user_type, int client_socket) {
    // Check if we've reached the maximum number of users
    if (numUsers >= MAX_USERS) {
        return;
    }
    
    // Check if user is already in the list
    for (int i = 0; i < numUsers; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].is_online = 1;
            users[i].socket_fd = client_socket;
            return;
        }
    }
    
    // Add new user to the list
    strcpy(users[numUsers].username, username);
    strcpy(users[numUsers].password, password);
    users[numUsers].user_type = user_type;
    users[numUsers].socket_fd = client_socket;
    users[numUsers].is_online = 1;
    numUsers++;
    
    printf("User '%s' added to online users list. Total users: %d\n", username, numUsers);
}

void process_registration(int client_socket) {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    int user_type;
    
    send_message(client_socket, "Enter new username: ");
    strcpy(username, receive_string(client_socket));
    
    // Check if username already exists
    if (username_exists(username)) {
        send_message(client_socket, "Username already exists. Please choose another one.\n");
        return;
    }
    
    send_message(client_socket, "Enter new password: ");
    strcpy(password, receive_string(client_socket));
    
    send_message(client_socket, "Select user type:\n");
    send_message(client_socket, "1. Engineer\n");
    send_message(client_socket, "2. Organization\n");
    send_message(client_socket, "Enter your choice: ");
    
    char choice[BUF_SIZE];
    strcpy(choice, receive_string(client_socket));
    user_type = atoi(choice);
    
    if (user_type != 1 && user_type != 2) {
        send_message(client_socket, "Invalid user type. Registration failed.\n");
        return;
    }
    
    // Add user to the file
    add_user_to_file(username, password, user_type);
    
    send_message(client_socket, "Registration successful!\n");
    
    // If it's an engineer, collect additional info
    if (user_type == 1) {
        send_message(client_socket, "Complete your engineer profile:\n");
        register_engineer(client_socket, username);
    } else if (user_type == 2) {
        send_message(client_socket, "Complete your organization profile:\n");
        register_organization(client_socket, username);
    }
}

void add_user_to_file(char *username, char *password, int user_type) {
    FILE *file = fopen(DATABASE_FILE, "a");
    if (file == NULL) {
        printf("Error opening file!\n");
        return;
    }
    
    fprintf(file, "%s %s %d\n", username, password, user_type);
    fclose(file);
    
    // Create blocked file
    char blocked_filename[100];
    snprintf(blocked_filename, sizeof(blocked_filename), "blocked_%s.txt", username);
    FILE *blocked_file = fopen(blocked_filename, "w");
    if (blocked_file != NULL) {
        fclose(blocked_file);
    }
    
    // Create access file
    char access_filename[100];
    snprintf(access_filename, sizeof(access_filename), "access_%s.txt", username);
    FILE *access_file = fopen(access_filename, "w");
    if (access_file != NULL) {
        fclose(access_file);
    }
}

int username_exists(const char *username) {
    FILE *file = fopen(DATABASE_FILE, "r");
    if (file == NULL) {
        return 0;
    }
    
    char line[BUF_SIZE];
    char stored_username[MAX_USERNAME_LENGTH];
    
    while (fgets(line, sizeof(line), file) != NULL) {
        if (sscanf(line, "%s", stored_username) == 1) {
            if (strcmp(username, stored_username) == 0) {
                fclose(file);
                return 1;  // Username exists
            }
        }
    }
    
    fclose(file);
    return 0;  // Username doesn't exist
}

void register_engineer(int client_socket, char *username) {
    char specialization[BUF_SIZE];
    char experience[BUF_SIZE];
    char education[BUF_SIZE];
    char skills[BUF_SIZE];
    
    send_message(client_socket, "Enter your specialization: ");
    strcpy(specialization, receive_string(client_socket));
    
    send_message(client_socket, "Enter your years of experience: ");
    strcpy(experience, receive_string(client_socket));
    
    send_message(client_socket, "Enter your education: ");
    strcpy(education, receive_string(client_socket));
    
    send_message(client_socket, "Enter your skills (comma separated): ");
    strcpy(skills, receive_string(client_socket));
    
    // Save engineer profile to file
    FILE *file = fopen(ENGINEERS_FILE, "a");
    if (file == NULL) {
        send_message(client_socket, "Error saving engineer profile!\n");
        return;
    }
    
    fprintf(file, "%s|%s|%s|%s|%s\n", username, specialization, experience, education, skills);
    fclose(file);
    
    send_message(client_socket, "Engineer profile created successfully!\n");
}

void register_organization(int client_socket, char *username) {
    char org_name[BUF_SIZE];
    char industry[BUF_SIZE];
    char description[BUF_SIZE];
    
    send_message(client_socket, "Enter organization name: ");
    strcpy(org_name, receive_string(client_socket));
    
    send_message(client_socket, "Enter industry: ");
    strcpy(industry, receive_string(client_socket));
    
    send_message(client_socket, "Enter description: ");
    strcpy(description, receive_string(client_socket));
    
    // Save organization profile to file
    FILE *file = fopen(ORGANIZATIONS_FILE, "a");
    if (file == NULL) {
        send_message(client_socket, "Error saving organization profile!\n");
        return;
    }
    
    fprintf(file, "%s|%s|%s|%s\n", username, org_name, industry, description);
    fclose(file);
    
    send_message(client_socket, "Organization profile created successfully!\n");
}

void show_main_menu(int client_socket, char *username, int user_type) {
    int choice = 0;
    char buffer[BUF_SIZE];
    
    while (1) {
        send_message(client_socket, "\n===== Main Menu =====\n");
        send_message(client_socket, "1. View Profile\n");
        
        if (user_type == 1) { // Engineer
            send_message(client_socket, "2. List Organizations\n");
        } else { // Organization
            send_message(client_socket, "2. List Engineers\n");
        }
        
        send_message(client_socket, "3. Start Conversation\n");
        send_message(client_socket, "4. View Conversations\n");
        send_message(client_socket, "5. Block/Unblock Users\n");
        send_message(client_socket, "6. Logout\n");
        send_message(client_socket, "Enter your choice: ");
        
        int nread = recv(client_socket, buffer, BUF_SIZE - 1, 0);
        if (nread <= 0) {
            break;
        }
        
        buffer[nread] = '\0';
        choice = atoi(buffer);
        
        switch (choice) {
            case 1:
                view_profile(client_socket, username, user_type);
                break;
            case 2:
                if (user_type == 1) {
                    list_organizations(client_socket);
                } else {
                    list_engineers(client_socket);
                }
                break;
            case 3:
                send_message(client_socket, "Start conversation functionality not yet implemented.\n");
                break;
            case 4:
                send_message(client_socket, "View conversations functionality not yet implemented.\n");
                break;
            case 5:
                send_message(client_socket, "Block/Unblock users functionality not yet implemented.\n");
                break;
            case 6:
                send_message(client_socket, "Logging out...\n");
                // Update user's online status
                for (int i = 0; i < numUsers; i++) {
                    if (strcmp(users[i].username, username) == 0) {
                        users[i].is_online = 0;
                        break;
                    }
                }
                return;
            default:
                send_message(client_socket, "Invalid choice. Please try again.\n");
        }
    }
}

void view_profile(int client_socket, char *username, int user_type) {
    char profile_info[BUF_SIZE * 4] = {0};
    sprintf(profile_info, "\n===== Profile Information =====\n");
    sprintf(profile_info + strlen(profile_info), "Username: %s\n", username);
    sprintf(profile_info + strlen(profile_info), "User Type: %s\n", (user_type == 1) ? "Engineer" : "Organization");
    
    if (user_type == 1) {
        // Get engineer details
        FILE *file = fopen(ENGINEERS_FILE, "r");
        if (file == NULL) {
            send_message(client_socket, "Error retrieving profile information!\n");
            return;
        }
        
        char line[BUF_SIZE * 4];
        char stored_username[MAX_USERNAME_LENGTH];
        char specialization[BUF_SIZE];
        char experience[BUF_SIZE];
        char education[BUF_SIZE];
        char skills[BUF_SIZE];
        int found = 0;
        
        while (fgets(line, sizeof(line), file) != NULL) {
            char *token = strtok(line, "|");
            if (token != NULL && strcmp(token, username) == 0) {
                strcpy(stored_username, token);
                
                token = strtok(NULL, "|");
                if (token != NULL) strcpy(specialization, token);
                
                token = strtok(NULL, "|");
                if (token != NULL) strcpy(experience, token);
                
                token = strtok(NULL, "|");
                if (token != NULL) strcpy(education, token);
                
                token = strtok(NULL, "\n");
                if (token != NULL) strcpy(skills, token);
                
                found = 1;
                break;
            }
        }
        fclose(file);
        
        if (found) {
            sprintf(profile_info + strlen(profile_info), "Specialization: %s\n", specialization);
            sprintf(profile_info + strlen(profile_info), "Experience: %s years\n", experience);
            sprintf(profile_info + strlen(profile_info), "Education: %s\n", education);
            sprintf(profile_info + strlen(profile_info), "Skills: %s\n", skills);
        } else {
            sprintf(profile_info + strlen(profile_info), "No additional profile information found.\n");
        }
    } else {
        // Get organization details
        FILE *file = fopen(ORGANIZATIONS_FILE, "r");
        if (file == NULL) {
            send_message(client_socket, "Error retrieving profile information!\n");
            return;
        }
        
        char line[BUF_SIZE * 4];
        char stored_username[MAX_USERNAME_LENGTH];
        char org_name[BUF_SIZE];
        char industry[BUF_SIZE];
        char description[BUF_SIZE];
        int found = 0;
        
        while (fgets(line, sizeof(line), file) != NULL) {
            char *token = strtok(line, "|");
            if (token != NULL && strcmp(token, username) == 0) {
                strcpy(stored_username, token);
                
                token = strtok(NULL, "|");
                if (token != NULL) strcpy(org_name, token);
                
                token = strtok(NULL, "|");
                if (token != NULL) strcpy(industry, token);
                
                token = strtok(NULL, "\n");
                if (token != NULL) strcpy(description, token);
                
                found = 1;
                break;
            }
        }
        fclose(file);
        
        if (found) {
            sprintf(profile_info + strlen(profile_info), "Organization Name: %s\n", org_name);
            sprintf(profile_info + strlen(profile_info), "Industry: %s\n", industry);
            sprintf(profile_info + strlen(profile_info), "Description: %s\n", description);
        } else {
            sprintf(profile_info + strlen(profile_info), "No additional profile information found.\n");
        }
    }
    
    send_message(client_socket, profile_info);
}

void list_engineers(int client_socket) {
    FILE *file = fopen(ENGINEERS_FILE, "r");
    if (file == NULL) {
        send_message(client_socket, "No engineers found in the system.\n");
        return;
    }
    
    char line[BUF_SIZE * 4];
    char engineer_list[BUF_SIZE * 10] = {0};
    int count = 0;
    
    sprintf(engineer_list, "\n===== Available Engineers =====\n");
    
    while (fgets(line, sizeof(line), file) != NULL) {
        char username[MAX_USERNAME_LENGTH];
        char specialization[BUF_SIZE];
        char experience[BUF_SIZE];
        
        char *token = strtok(line, "|");
        if (token != NULL) {
            strcpy(username, token);
            
            token = strtok(NULL, "|");
            if (token != NULL) strcpy(specialization, token);
            
            token = strtok(NULL, "|");
            if (token != NULL) strcpy(experience, token);
            
            // Check if user is online
            int is_online = 0;
            for (int i = 0; i < numUsers; i++) {
                if (strcmp(users[i].username, username) == 0 && users[i].is_online) {
                    is_online = 1;
                    break;
                }
            }
            
            sprintf(engineer_list + strlen(engineer_list), 
                    "%d. %s - %s (%s years) [%s]\n", 
                    ++count, username, specialization, experience, 
                    is_online ? "Online" : "Offline");
        }
    }
    fclose(file);
    
    if (count == 0) {
        send_message(client_socket, "No engineers found in the system.\n");
    } else {
        send_message(client_socket, engineer_list);
    }
}

void list_organizations(int client_socket) {
    FILE *file = fopen(ORGANIZATIONS_FILE, "r");
    if (file == NULL) {
        send_message(client_socket, "No organizations found in the system.\n");
        return;
    }
    
    char line[BUF_SIZE * 4];
    char org_list[BUF_SIZE * 10] = {0};
    int count = 0;
    
    sprintf(org_list, "\n===== Available Organizations =====\n");
    
    while (fgets(line, sizeof(line), file) != NULL) {
        char username[MAX_USERNAME_LENGTH];
        char org_name[BUF_SIZE];
        char industry[BUF_SIZE];
        
        char *token = strtok(line, "|");
        if (token != NULL) {
            strcpy(username, token);
            
            token = strtok(NULL, "|");
            if (token != NULL) strcpy(org_name, token);
            
            token = strtok(NULL, "|");
            if (token != NULL) strcpy(industry, token);
            
            // Check if organization is online
            int is_online = 0;
            for (int i = 0; i < numUsers; i++) {
                if (strcmp(users[i].username, username) == 0 && users[i].is_online) {
                    is_online = 1;
                    break;
                }
            }
            
            sprintf(org_list + strlen(org_list), 
                    "%d. %s - %s [%s]\n", 
                    ++count, org_name, industry, 
                    is_online ? "Online" : "Offline");
        }
    }
    fclose(file);
    
    if (count == 0) {
        send_message(client_socket, "No organizations found in the system.\n");
    } else {
        send_message(client_socket, org_list);
    }
}

void process_client(int client_fd) {
    int choice;
    int exit_flag = 0;
    
    while (!exit_flag) {
        choice = show_login_menu(client_fd);
        
        switch (choice) {
            case 1: // Login
                process_login(client_fd);
                break;
            case 2: // Register
                process_registration(client_fd);
                break;
            case 3: // Exit
                send_message(client_fd, "Goodbye!\n");
                exit_flag = 1;
                break;
            default:
                send_message(client_fd, "Invalid choice! Please try again.\n");
        }
    }
    
    close(client_fd);
}

void erro(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    int fd, client;
    struct sockaddr_in addr, client_addr;
    socklen_t client_addr_size;
    
    // Set up signal handler for child processes
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        erro("Error setting up signal handler");
    }
    
    // Create initial files if they don't exist
    FILE *credential_file = fopen(DATABASE_FILE, "a+");
    if (credential_file != NULL) {
        fclose(credential_file);
    }
    
    FILE *engineers_file = fopen(ENGINEERS_FILE, "a+");
    if (engineers_file != NULL) {
        fclose(engineers_file);
    }
    
    FILE *organizations_file = fopen(ORGANIZATIONS_FILE, "a+");
    if (organizations_file != NULL) {
        fclose(organizations_file);
    }
    
    // Setup server socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);
    
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        erro("error in socket function");
    
    // Set socket option to reuse address
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        erro("error in setsockopt");
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        erro("error in bind function");
    
    if (listen(fd, DEFAULT_BACKLOG) < 0)
        erro("error in listen function");
    
    printf("Server started on port %d. Waiting for connections...\n", SERVER_PORT);
    
    client_addr_size = sizeof(client_addr);
    
    while (1) {
        // Wait for a new connection
        client = accept(fd, (struct sockaddr *)&client_addr, &client_addr_size);
        
        if (client < 0) {
            if (errno == EINTR)
                continue; // Interrupted by a signal
                
            perror("Error in accept function");
            continue;
        }
        
        printf("New connection established from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        if (fork() == 0) {
            // Child process
            close(fd);
            process_client(client);
            printf("Connection with client closed\n");
            exit(0);
        }
        
        // Parent process
        close(client);
    }
    
    return 0;
}
