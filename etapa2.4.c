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
#include <ctype.h>

#define SERVER_PORT     9000
#define BUF_SIZE        1024
#define DEFAULT_BACKLOG 5
#define MAX_USERS       100
#define MAX_USERNAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 50
#define DATABASE_FILE "credentials.txt"
#define ENGINEERS_FILE "engineers.txt"
#define ORGANIZATIONS_FILE "organizations.txt"
#define PENDING_FILE "pending.txt"

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
char *receive_string(int client_socket);
int contains_invalid_chars(const char *input);
int contains_invalid_file_chars(const char *str);
void sanitize_filename(char *dest, const char *src, size_t max_len);
void show_admin_menu(int client_socket, char *username);
void accept_new_user(int client_socket);
void delete_user(int client_socket);
int is_admin(char *username);
void move_user_from_pending_to_active(const char *username, const char *password, int user_type);


// Handler to prevent zombie processes
void handle_sigchld(int sig) {
    // Reap all dead processes
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void erro(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
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

// Helper function to check if a string is a valid integer
int is_valid_integer(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}


char *receive_string(int client_socket) {
    static char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);

    int bytes_received = recv(client_socket, buffer, BUF_SIZE - 1, 0);

    if (bytes_received == 0) {
        // Client closed connection
        printf("Client desconnected.\n");
        return NULL;
    } else if (bytes_received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            printf("Timeout: client didn't respond in time.\n");
        } else {
            perror("Error receiving data from client");
        }
        return NULL;
    }

    // Remove newline at the end, if exists
    if (buffer[bytes_received - 1] == '\n') {
        buffer[bytes_received - 1] = '\0';
    } else {
        buffer[bytes_received] = '\0';  // Ensure null termination 
    }

    return buffer;
}

//Check if special characters are being used, whent they shouldn't (regular users username)
int contains_invalid_chars(const char *input) {
    const char *invalid_chars = " \t\n;|&<>*\"";

    return strpbrk(input, invalid_chars) != NULL;
}

//Check for sensible characters handling files
int contains_invalid_file_chars(const char *str) {
    while (*str) {
        if (*str == '\n' || *str == '\r' || *str == ':' || *str == ' ') {
            return 1;
        }
        str++;
    }
    return 0;
}

// Add this function to check if a username is admin
int is_admin(char *username) {
    return strcmp(username, "admin") == 0;
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
    char *input_username = receive_string(client_socket);
    if (input_username == NULL) {
        send_message(client_socket, "Error: Connection closed or timeout while receiving username.\n");
        close(client_socket);
        return;
    }
    if (strlen(input_username) == 0 || strlen(input_username) >= MAX_USERNAME_LENGTH) {
        send_message(client_socket, "Error: Invalid username (empty or to long).\n");
        return;
    }

    if (contains_invalid_chars(input_username)) {
        send_message(client_socket, "Error: Username contains invalid characters.Please try again. .\n");
        return;
    }

    strncpy(username, input_username, MAX_USERNAME_LENGTH - 1);
    username[MAX_USERNAME_LENGTH - 1] = '\0';  // Ensures null termination

    // Check if the username is in pending status
    FILE *pending_file = fopen(PENDING_FILE, "r");
    if (pending_file != NULL) {
        char line[BUF_SIZE];
        char pending_username[MAX_USERNAME_LENGTH];
        
        while (fgets(line, sizeof(line), pending_file) != NULL) {
            if (sscanf(line, "%s", pending_username) == 1) {
                if (strcmp(username, pending_username) == 0) {
                    fclose(pending_file);
                    send_message(client_socket, "Your account is pending approval by an administrator.\n");
                    return;
                }
            }
        }
        
        fclose(pending_file);
    }

    send_message(client_socket, "Enter password: ");
    char *input_password = receive_string(client_socket);
    if (input_password == NULL) {
        send_message(client_socket, "Error: Connection closed or timeout when receiving password.\n");
        close(client_socket);
        return;
    }
    if (strlen(input_password) == 0 || strlen(input_password) >= MAX_PASSWORD_LENGTH) {
        send_message(client_socket, "Error: Invalid password (empty or to long).\n");
        return;
    }

    if (contains_invalid_chars(input_password)) {
        send_message(client_socket, "Erro: Password contains invalid characters.\n");
        return;
    }

    strncpy(password, input_password, MAX_PASSWORD_LENGTH - 1);
    password[MAX_PASSWORD_LENGTH - 1] = '\0';

    int user_type = check_credentials(client_socket, username, password);
    if (user_type > 0) {
        char success_msg[BUF_SIZE];
        snprintf(success_msg, BUF_SIZE, "Login successful! Welcome, %s!\n", username);
        send_message(client_socket, success_msg);
        
        show_main_menu(client_socket, username, user_type);
    } else {
        send_message(client_socket, "Invalid username or password. Please try again.\n");
    }
}



int check_credentials(int client_socket, char *username, char *password) {
    if (strcmp(username, "admin") == 0 && strcmp(password, "admin") == 0) {
        return 3;  // 3 = admin user type
    }
    
    FILE *file = fopen(DATABASE_FILE, "r");
    if (file == NULL) {
        send_message(client_socket, "Error opening the credentials file.\n");
        perror("Error opening DATABASE_FILE");
        return 0;
    }

    char line[BUF_SIZE];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';

        // Create copy to not modify the original
        char temp_line[BUF_SIZE];
        strncpy(temp_line, line, sizeof(temp_line));
        temp_line[sizeof(temp_line) - 1] = '\0';

        // Separate tokens (expect 3 tokens: username, password, type)
        char *token_username = strtok(temp_line, " ");
        char *token_password = strtok(NULL, " ");
        char *token_user_type = strtok(NULL, " ");

        // If you don't have all 3 tokens, ignore the line.
        if (!token_username || !token_password || !token_user_type) {
            continue;
        }

        // Validate if user_type is numeric
        char *endptr;
        int user_type = strtol(token_user_type, &endptr, 10);
        if (*endptr != '\0') {
            continue;  // Não é um número válido
        }

        //Compare with the data provided
        if (strcmp(username, token_username) == 0 && strcmp(password, token_password) == 0) {
            fclose(file);
            return user_type;
        }
    }
if (fclose(file) != 0) {
        perror("Error closing DATABASE_FILE");
    }
    return 0;  // Autentication failed
}

//Check if username already exists
// Function to check credentials should also check pending users
int username_exists(const char *username) {
    // First check active users
    FILE *file = fopen(DATABASE_FILE, "r");
    if (file != NULL) {
        char line[BUF_SIZE];
        char stored_username[MAX_USERNAME_LENGTH];
        
        while (fgets(line, sizeof(line), file) != NULL) {
            if (sscanf(line, "%s", stored_username) == 1) {
                if (strcmp(username, stored_username) == 0) {
                    fclose(file);
                    return 1;  // Username exists in active users
                }
            }
        }
        
        fclose(file);
    }
    
    // Then check pending users
    file = fopen(PENDING_FILE, "r");
    if (file != NULL) {
        char line[BUF_SIZE];
        char stored_username[MAX_USERNAME_LENGTH];
        
        while (fgets(line, sizeof(line), file) != NULL) {
            if (sscanf(line, "%s", stored_username) == 1) {
                if (strcmp(username, stored_username) == 0) {
                    fclose(file);
                    return 1;  // Username exists in pending users
                }
            }
        }
        
        fclose(file);
    }
    
    return 0;  // Username doesn't exist anywhere
}



void process_registration(int client_socket) {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    int user_type;

    send_message(client_socket, "Enter new username: ");
    char *input_username = receive_string(client_socket);
    if (input_username == NULL) {
        send_message(client_socket, "Error receiving username.\n");
        return;
    }

    if (strlen(input_username) == 0 || strlen(input_username) >= MAX_USERNAME_LENGTH) {
        send_message(client_socket, "Error: Invalid username (empty or to long).\n");
        return;
    }

    if (contains_invalid_chars(input_username)) {
        send_message(client_socket, "Error: username contains invalid characters.\n");
        return;
    }

    strncpy(username, input_username, MAX_USERNAME_LENGTH - 1);
    username[MAX_USERNAME_LENGTH - 1] = '\0';

    // Check if username exists in both active and pending users
    if (username_exists(username)) {
        send_message(client_socket, "Username already exists. Please choose another one.\n");
        return;
    }

    // Also check pending file
    FILE *pending_file = fopen(PENDING_FILE, "r");
    if (pending_file != NULL) {
        char line[BUF_SIZE];
        char stored_username[MAX_USERNAME_LENGTH];
        
        while (fgets(line, sizeof(line), pending_file) != NULL) {
            if (sscanf(line, "%s", stored_username) == 1) {
                if (strcmp(username, stored_username) == 0) {
                    fclose(pending_file);
                    send_message(client_socket, "Username already pending approval. Please choose another one.\n");
                    return;
                }
            }
        }
        
        fclose(pending_file);
    }

    send_message(client_socket, "Enter new password: ");
    char *input_password = receive_string(client_socket);
    if (input_password == NULL) {
        send_message(client_socket, "Error receiving password.\n");
        return;
    }

    if (strlen(input_password) < 4 || strlen(input_password) >= MAX_PASSWORD_LENGTH) {
        send_message(client_socket, "Error: Invalid password (minimum 4 characters, maximum reached).\n");
        return;
    }

    strncpy(password, input_password, MAX_PASSWORD_LENGTH - 1);
    password[MAX_PASSWORD_LENGTH - 1] = '\0';

    send_message(client_socket, "Select user type:\n");
    send_message(client_socket, "1. Engineer\n");
    send_message(client_socket, "2. Organization\n");
    send_message(client_socket, "Enter your choice: ");

    char *choice_input = receive_string(client_socket);
    if (choice_input == NULL) {
        send_message(client_socket, "Error receiving user type.\n");
        return;
    }

    if (strlen(choice_input) != 1 || (choice_input[0] != '1' && choice_input[0] != '2')) {
        send_message(client_socket, "Invalid user type. Registration failed.\n");
        return;
    }

    user_type = choice_input[0] - '0';

    // Add user to pending file instead of credentials file
    if (contains_invalid_file_chars(username) || contains_invalid_file_chars(password)) {
        printf("Error: username ou password contains invalid characters.\n");
        return;
    }

    FILE *file = fopen(PENDING_FILE, "a");
    if (file == NULL) {
        printf("Error opening pending file!\n");
        send_message(client_socket, "Error in registration. Please try again later.\n");
        return;
    }

    if (fprintf(file, "%s %s %d\n", username, password, user_type) < 0) {
        perror("Error writing to pending file");
        fclose(file);
        send_message(client_socket, "Error in registration. Please try again later.\n");
        return;
    }

    if (fclose(file) != 0) {
        perror("Error closing PENDING_FILE");
    }

    // Collect profile information but mark it as pending
    if (user_type == 1) {
        send_message(client_socket, "Complete your engineer profile:\n");
        register_engineer(client_socket, username);
    } else if (user_type == 2) {
        send_message(client_socket, "Complete your organization profile:\n");
        register_organization(client_socket, username);
    }

    send_message(client_socket, "\nYour registration is pending approval by an administrator.\n");
    send_message(client_socket, "You will be able to login once your account is approved.\n");
}


void sanitize_filename(char *dest, const char *src, size_t max_len) {
    size_t j = 0;
    for (size_t i = 0; i < strlen(src) && j < max_len - 1; i++) {
        if (isalnum((unsigned char)src[i]) || src[i] == '_') {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

void add_user_to_file(char *username, char *password, int user_type) {
    if (contains_invalid_file_chars(username) || contains_invalid_file_chars(password)) {
        printf("Error: username ou password contains invalid characters.\n");
        return;
    }

    FILE *file = fopen(DATABASE_FILE, "a");
    if (file == NULL) {
        printf("Error opening file!\n");
        return;
    }

    if (fprintf(file, "%s %s %d\n", username, password, user_type) < 0) {
        perror("Error writing to credentials file");
        fclose(file);
        return;
    }

    if (fclose(file) != 0) {
        perror("Error closing DATABASE_FILE");
    }

    // Sanitize filename for safe use
    char safe_username[MAX_USERNAME_LENGTH];
    sanitize_filename(safe_username, username, sizeof(safe_username));
}

void register_engineer(int client_socket, char *username) {
    char specialization[BUF_SIZE];
    char experience[BUF_SIZE];
    char education[BUF_SIZE];
    char skills[BUF_SIZE];
    char *temp;

    send_message(client_socket, "Enter your specialization: ");
    temp = receive_string(client_socket);
    strncpy(specialization, temp, BUF_SIZE);

    while (1) {
        send_message(client_socket, "Enter your years of experience: ");
        temp = receive_string(client_socket);

        if (!is_valid_integer(temp)) {
            send_message(client_socket, "Invalid input. Enter a number.\n");
            continue;
        }
		
        int years = atoi(temp);
        if (years > 60) {
            send_message(client_socket, "Too many years of experience. Try again.\n");
            continue;
        }

	strncpy(experience, temp, BUF_SIZE);
	break;
    }

    while (1) {
        send_message(client_socket, "Enter your education (in years): ");
        temp = receive_string(client_socket);

        if (!is_valid_integer(temp)) {
            send_message(client_socket, "Education not in years. Try again.\n");
            continue;
        }

        strncpy(education, temp, BUF_SIZE);
        break;
    }

    send_message(client_socket, "Enter your skills (comma separated): ");
    temp = receive_string(client_socket);
    strncpy(skills, temp, BUF_SIZE);

    // Save engineer profile to file
    FILE *file = fopen(ENGINEERS_FILE, "a");
    if (file == NULL) {
        perror("Error opening engineer profile file");
        send_message(client_socket, "Error saving engineer profile!\n");
        return;
    }

    if (fprintf(file, "%s|%s|%s|%s|%s\n", username, specialization, experience, education, skills) < 0) {
        perror("Error writing to engineer file");
        send_message(client_socket, "Error writing engineer profile.\n");
        fclose(file);
        return;
    }

    if (fclose(file) != 0) {
        perror("Error closing the engineer file");
    }

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
		perror("Error opening organization file");
		send_message(client_socket, "Error saving organization profile!\n");
		return;
	}

	if (fprintf(file, "%s|%s|%s|%s\n", username, org_name, industry, description) < 0) {
		perror("Error wrinting to organization file");
		send_message(client_socket, "Error writing organization profile.\n");
		fclose(file);
		return;
	}

	if (fclose(file) != 0) {
		perror("Error closing organization file");
	}

send_message(client_socket, "Organization profile created successfully!\n");

}
void show_main_menu(int client_socket, char *username, int user_type) {
    int choice = 0;
    char buffer[BUF_SIZE];
    
    if (user_type == 3) {
        show_admin_menu(client_socket, username);
        return;
    }
    
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

// Admin menu implementation
void show_admin_menu(int client_socket, char *username) {
    int choice = 0;
    char buffer[BUF_SIZE];
    
    while (1) {
        send_message(client_socket, "\n===== Admin Menu =====\n");
        send_message(client_socket, "1. View Engineers\n");
        send_message(client_socket, "2. View Organizations\n");
        send_message(client_socket, "3. Accept New Users\n");
        send_message(client_socket, "4. Delete Users\n");
        send_message(client_socket, "5. Logout\n");
        send_message(client_socket, "Enter your choice: ");
        
        int nread = recv(client_socket, buffer, BUF_SIZE - 1, 0);
        if (nread <= 0) {
            break;
        }
        
        buffer[nread] = '\0';
        choice = atoi(buffer);
        
        switch (choice) {
            case 1:
                list_engineers(client_socket);
                break;
            case 2:
                list_organizations(client_socket);
                break;
            case 3:
                accept_new_user(client_socket);
                break;
            case 4:
                delete_user(client_socket);
                break;
            case 5:
                send_message(client_socket, "Logging out...\n");
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

// Function to accept new users (implementation would depend on pending users mechanism)
void accept_new_user(int client_socket) {
    FILE *file = fopen(PENDING_FILE, "r");
    if (file == NULL) {
        // If the file doesn't exist, create it
        file = fopen(PENDING_FILE, "w");
        if (file == NULL) {
            send_message(client_socket, "Error accessing pending users!\n");
            return;
        }
        fclose(file);
        send_message(client_socket, "No pending users to approve.\n");
        return;
    }
    
    // Check if the file is empty
    fseek(file, 0, SEEK_END);
    if (ftell(file) == 0) {
        fclose(file);
        send_message(client_socket, "No pending users to approve.\n");
        return;
    }
    rewind(file);

    // Create arrays to store pending user information
    char usernames[MAX_USERS][MAX_USERNAME_LENGTH];
    char passwords[MAX_USERS][MAX_PASSWORD_LENGTH];
    int user_types[MAX_USERS];
    int count = 0;
    
    char line[BUF_SIZE];
    send_message(client_socket, "\n===== Pending Users =====\n");
    
    while (fgets(line, sizeof(line), file) != NULL && count < MAX_USERS) {
        char username[MAX_USERNAME_LENGTH];
        char password[MAX_PASSWORD_LENGTH];
        int user_type;
        
        // Parse the line
        if (sscanf(line, "%s %s %d", username, password, &user_type) == 3) {
            // Skip admin if somehow it got into pending
            if (strcmp(username, "admin") == 0) {
                continue;
            }
            
            // Store user info
            strncpy(usernames[count], username, MAX_USERNAME_LENGTH - 1);
            usernames[count][MAX_USERNAME_LENGTH - 1] = '\0';
            
            strncpy(passwords[count], password, MAX_PASSWORD_LENGTH - 1);
            passwords[count][MAX_PASSWORD_LENGTH - 1] = '\0';
            
            user_types[count] = user_type;
            
            // Display user info
            char user_info[BUF_SIZE];
            sprintf(user_info, "%d. %s - %s\n", count + 1, username, 
                    (user_type == 1) ? "Engineer" : "Organization");
            send_message(client_socket, user_info);
            
            count++;
        }
    }
    
    fclose(file);
    
    if (count == 0) {
        send_message(client_socket, "No pending users to approve.\n");
        return;
    }
    
    // Ask which user to approve
    send_message(client_socket, "\nEnter number of user to approve (0 to cancel): ");
    char *response = receive_string(client_socket);
    
    if (response == NULL || !is_valid_integer(response)) {
        send_message(client_socket, "Invalid input. Operation cancelled.\n");
        return;
    }
    
    int selection = atoi(response);
    if (selection <= 0 || selection > count) {
        send_message(client_socket, "Operation cancelled or invalid selection.\n");
        return;
    }
    
    // Ask for confirmation
    char confirm_msg[BUF_SIZE];
    sprintf(confirm_msg, "Are you sure you want to approve user '%s'? [Y/N]: ", 
            usernames[selection - 1]);
    send_message(client_socket, confirm_msg);
    
    response = receive_string(client_socket);
    if (response == NULL || (toupper(response[0]) != 'Y')) {
        send_message(client_socket, "User approval cancelled.\n");
        return;
    }
    
    // Move user from pending to active
    move_user_from_pending_to_active(usernames[selection - 1], 
                                     passwords[selection - 1], 
                                     user_types[selection - 1]);
    
    send_message(client_socket, "User successfully approved.\n");
}

// Function to move a user from pending to active status
void move_user_from_pending_to_active(const char *username, const char *password, int user_type) {
    // Add user to credentials file
    FILE *cred_file = fopen(DATABASE_FILE, "a");
    if (cred_file == NULL) {
        perror("Error opening credentials file");
        return;
    }
    
    fprintf(cred_file, "%s %s %d\n", username, password, user_type);
    fclose(cred_file);
    
    // Remove user from pending file
    FILE *pending_file = fopen(PENDING_FILE, "r");
    FILE *temp_file = fopen("temp_pending.txt", "w");
    
    if (pending_file == NULL || temp_file == NULL) {
        if (pending_file) fclose(pending_file);
        if (temp_file) fclose(temp_file);
        perror("Error opening files for pending user removal");
        return;
    }
    
    char line[BUF_SIZE];
    while (fgets(line, sizeof(line), pending_file) != NULL) {
        char current_username[MAX_USERNAME_LENGTH];
        sscanf(line, "%s", current_username);
        
        // If this is not the user we're approving, keep them in the pending file
        if (strcmp(current_username, username) != 0) {
            fputs(line, temp_file);
        }
    }
    
    fclose(pending_file);
    fclose(temp_file);
    
    // Replace original pending file with temp file
    remove(PENDING_FILE);
    rename("temp_pending.txt", PENDING_FILE);
}


// Function to delete users
void delete_user(int client_socket) {
    // First display a list of users
    FILE *file = fopen(DATABASE_FILE, "r");
    if (file == NULL) {
        send_message(client_socket, "Error opening user database!\n");
        return;
    }
    
    char line[BUF_SIZE];
    char username[MAX_USERNAME_LENGTH];
    char user_type_str[10];
    int user_type;
    int count = 0;
    
    // Array to store usernames
    char usernames[MAX_USERS][MAX_USERNAME_LENGTH];
    int user_types[MAX_USERS];
    
    send_message(client_socket, "\n===== Users List =====\n");
    
    while (fgets(line, sizeof(line), file) != NULL && count < MAX_USERS) {
        // Split the line by spaces
        char *token = strtok(line, " ");
        if (token != NULL) {
            strcpy(username, token);
            
            // Skip password
            token = strtok(NULL, " ");
            
            // Get user type
            token = strtok(NULL, " \n");
            if (token != NULL) {
                user_type = atoi(token);
                
                // Skip admin user
                if (strcmp(username, "admin") == 0) {
                    continue;
                }
                
                // Store username and type
                strcpy(usernames[count], username);
                user_types[count] = user_type;
                
                // Display user info
                sprintf(line, "%d. %s - %s\n", count + 1, username, 
                        (user_type == 1) ? "Engineer" : "Organization");
                send_message(client_socket, line);
                
                count++;
            }
        }
    }
    
    fclose(file);
    
    if (count == 0) {
        send_message(client_socket, "No users found to delete.\n");
        return;
    }
    
    // Ask which user to delete
    send_message(client_socket, "Enter number of user to delete (0 to cancel): ");
    char *response = receive_string(client_socket);
    
    if (response == NULL || !is_valid_integer(response)) {
        send_message(client_socket, "Invalid input.\n");
        return;
    }
    
    int selection = atoi(response);
    if (selection <= 0 || selection > count) {
        send_message(client_socket, "Operation cancelled or invalid selection.\n");
        return;
    }
    
    // Ask for confirmation
    char confirm_msg[BUF_SIZE];
    sprintf(confirm_msg, "Are you sure you want to delete user '%s'? [Y/N]: ", 
            usernames[selection - 1]);
    send_message(client_socket, confirm_msg);
    
    response = receive_string(client_socket);
    if (response == NULL || (toupper(response[0]) != 'Y')) {
        send_message(client_socket, "User deletion cancelled.\n");
        return;
    }
    
    // Delete user from credentials file
    file = fopen(DATABASE_FILE, "r");
    FILE *temp = fopen("temp_credentials.txt", "w");
    
    if (file == NULL || temp == NULL) {
        send_message(client_socket, "Error accessing user database!\n");
        if (file) fclose(file);
        if (temp) fclose(temp);
        return;
    }
    
    // Copy all lines except the one to delete
    while (fgets(line, sizeof(line), file) != NULL) {
        char current_username[MAX_USERNAME_LENGTH];
        sscanf(line, "%s", current_username);
        
        if (strcmp(current_username, usernames[selection - 1]) != 0) {
            fputs(line, temp);
        }
    }
    
    fclose(file);
    fclose(temp);
    
    // Replace original file with temp file
    remove(DATABASE_FILE);
    rename("temp_credentials.txt", DATABASE_FILE);
    
    // Also delete from appropriate profile file based on user type
    if (user_types[selection - 1] == 1) {
        // Delete engineer profile
        file = fopen(ENGINEERS_FILE, "r");
        temp = fopen("temp_engineers.txt", "w");
        
        if (file == NULL || temp == NULL) {
            send_message(client_socket, "Error accessing engineer profiles!\n");
            if (file) fclose(file);
            if (temp) fclose(temp);
            // User is already deleted from credentials file
            send_message(client_socket, "User deleted, but engineer profile remains.\n");
            return;
        }
        
        while (fgets(line, sizeof(line), file) != NULL) {
            char *token = strtok(strdup(line), "|");
            if (token != NULL && strcmp(token, usernames[selection - 1]) != 0) {
                fputs(line, temp);
            }
        }
        
        fclose(file);
        fclose(temp);
        
        remove(ENGINEERS_FILE);
        rename("temp_engineers.txt", ENGINEERS_FILE);
    } else {
        // Delete organization profile
        file = fopen(ORGANIZATIONS_FILE, "r");
        temp = fopen("temp_organizations.txt", "w");
        
        if (file == NULL || temp == NULL) {
            send_message(client_socket, "Error accessing organization profiles!\n");
            if (file) fclose(file);
            if (temp) fclose(temp);
            // User is already deleted from credentials file
            send_message(client_socket, "User deleted, but organization profile remains.\n");
            return;
        }
        
        while (fgets(line, sizeof(line), file) != NULL) {
            char *token = strtok(strdup(line), "|");
            if (token != NULL && strcmp(token, usernames[selection - 1]) != 0) {
                fputs(line, temp);
            }
        }
        
        fclose(file);
        fclose(temp);
        
        remove(ORGANIZATIONS_FILE);
        rename("temp_organizations.txt", ORGANIZATIONS_FILE);
    }
    
    send_message(client_socket, "User successfully deleted.\n");
}

// Function to create the admin user in the credentials file
void create_admin_user() {
    // Check if admin already exists
    FILE *file = fopen(DATABASE_FILE, "r");
    if (file != NULL) {
        char line[BUF_SIZE];
        while (fgets(line, sizeof(line), file) != NULL) {
            char username[MAX_USERNAME_LENGTH];
            sscanf(line, "%s", username);
            if (strcmp(username, "admin") == 0) {
                fclose(file);
                return; // Admin already exists
            }
        }
        fclose(file);
    }
    
    // Add admin user
    file = fopen(DATABASE_FILE, "a");
    if (file == NULL) {
        perror("Error opening credentials file");
        return;
    }
    
    fprintf(file, "admin admin 3\n");
    fclose(file);
    printf("Admin user created successfully.\n");
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
    
	FILE *pending_file = fopen(PENDING_FILE, "a+");
    if (pending_file != NULL) {
        fclose(pending_file);
    }
    
    // Create admin user
    create_admin_user();
    
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
