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

#define SERVER_PORT     9000
#define BUF_SIZE        1024
#define DEFAULT_BACKLOG 5

//Try code:  netcat 127.0.0.1 9000
//Etapa 1, mas com graceful exit

// Function prototypes
void process_client(int fd);
void erro(const char *msg);
void handle_sigchld(int sig);
int show_main_menu(int client_socket);
void process_option(int client_socket, int option);
void send_message(int client_socket, const char *message);

// Handlers:

// Handler to prevent zombie processes (child process whose parent is yet to acknowledge its termination)
void handle_sigchld(int sig) {
    // Reap all dead processes
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Message sending with error handling
void send_message(int client_socket, const char *message) {
    ssize_t bytes_sent = send(client_socket, message, strlen(message), 0); // send message
    if (bytes_sent < 0) {
        perror("Erro ao enviar mensagem"); // message not send
    } else if (bytes_sent < strlen(message)) {
        fprintf(stderr, "Aviso: Mensagem enviada parcialmente\n"); //message sent with errors
    }
}

void process_option(int client_socket, int option) {
    char response[BUF_SIZE];
    
    switch (option) {
        case 1:
            snprintf(response, BUF_SIZE, "Engineer\n");
            break;
        case 2:
            snprintf(response, BUF_SIZE, "Organization\n");
            break;
        
        default:
            snprintf(response, BUF_SIZE, "Opção inválida recebida: %d\n", option); //lidar com outros casos
    }
    
    // Envia a resposta para o cliente
    send_message(client_socket, response);
    
    // Mensagem de conclusão para indicar fim do programa (conveniente só para esta etapa)
    send_message(client_socket, "\nSessão concluída. Obrigado por utilizar nosso serviço!\n");
    
    // Exibe opcao do cliente, no lado do servidor
    printf("Cliente escolheu: %s", response);
}

int show_main_menu(int client_socket) {
    char buffer[BUF_SIZE];
    int option1;
    
    // Envia o menu principal para o cliente
    send_message(client_socket, "Hello, Welcome!\n");
    send_message(client_socket, "Please select an option:\n");
    send_message(client_socket, "\n1: Engineer\n");
    send_message(client_socket, "2: Organization\n");
    
    // Lê a opção do cliente usando recv
    int nread = recv(client_socket, buffer, BUF_SIZE - 1, 0);
    if (nread < 0) {
        perror("Erro ao ler do socket");
        send_message(client_socket, "Erro ao processar sua solicitação. Por favor, tente novamente.\n");
        return -1;
    } else if (nread == 0) {
        printf("Cliente desconectou antes de selecionar uma opção\n");
        return -1;
    }
    
    buffer[nread] = '\0';  // character end of string
    
    // Verifica se há problemas na string
    for (int i = 0; i < nread; i++) {
        if (buffer[i] != '\n' && buffer[i] != '\r' && (buffer[i] < '0' || buffer[i] > '9')) { //\n -> newline; \r -> carriage return; e se há algum digito
            send_message(client_socket, "Entrada inválida. Por favor, digite apenas números.\n"); // mensagem de erro caso haja
            return -1;
        }
    }
    
    // Converte a opção lida para um número inteiro
    option1 = atoi(buffer);
    
    // Valida a opção; ultima verificacao de que esta tudo bem
    if (option1 <= 0 || option1 > 2) {
        send_message(client_socket, "Opção inválida. Por favor, escolha 1 ou 2.\n");
        return -1;
    }
    
    return option1; // Retorna a opção escolhida
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
        erro("erro ao configurar manipulador de sinal");
    }
    
    // Setup server socket using AF_INET (linux sockets)
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);
    
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        erro("erro na funcao socket");
    
    // Set socket option to reuse address; use in time_await state socket; avoid "Address already in use";
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        erro("erro no setsockopt");
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) //assigns socket to respective address
        erro("erro na funcao bind");
    
    if (listen(fd, DEFAULT_BACKLOG) < 0)
        erro("erro na funcao listen");
    
    printf("Servidor iniciado na porta %d. À espera de conexões...\n", SERVER_PORT);
    
    client_addr_size = sizeof(client_addr); //guarda tamanho endereço
    
 while (1) {
    // Espera por uma nova conexão
    client = accept(fd, (struct sockaddr *)&client_addr, &client_addr_size); // aceitar conexao
    
    if (client < 0) {
        if (errno == EINTR)
            continue; // Foi interrompido por um sinal...
            
        perror("Erro na função accept");
        continue;
    }
    
    printf("Nova conexão estabelecida\n");
    
    if (fork() == 0) {
        /* Close the server listening socket in the child process.
        The child process only communicates with its assigned client, connections. This prevents the child from accidentally accepting new clients and avoids file descriptor leaks. */
        close(fd);
        
        /* Process this specific client connection.
        This function handles all communication with the client
        including showing menus, receiving selections, and sending responses.
        The child process dedicates 100% of its resources to this single client. */
        process_client(client); 
        printf("Conexão com cliente encerrada\n");
        
        /* Terminate the child process completely.
        This ensures the child process exits cleanly after serving its client */
        exit(0);
    }
    
    /* PARENT PROCESS: Close the client socket in the parent process
    since it's being handled by the child process */
    close(client);
}
return 0;
}

void process_client(int client_fd) 
{
    int chosen = show_main_menu(client_fd); // Mostra o menu e obtém a escolha
    
    if (chosen != -1) {
        process_option(client_fd, chosen); // Processa a opção escolhida
    } else {
        printf("Erro ou opção inválida do cliente.\n");
        send_message(client_fd, "Não foi possível processar sua solicitação. Encerrando conexão.\n");
    }
    
    close(client_fd); // Fecha a conexão com o cliente
}

void erro(const char *msg) //error handling function
{
    perror(msg);
    exit(EXIT_FAILURE);
}
