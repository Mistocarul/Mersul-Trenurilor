#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

extern int errno;

unsigned long hashParola(unsigned char *parola);
void login_singup(char *text, char *nume, char *parola, int *contor_nume, int *contor_parola, int *i);

int port = 2908;

int main(int argc, char *argv[])
{
    int descriptor_socket;     // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare

    if (argc != 1)
    {
        printf("Sintaxa este grasita: %s\n", argv[0]);
        return -1;
    }

    if ((descriptor_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket()!\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(port);

    if (connect(descriptor_socket, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("Eroare la connect()!\n");
        return errno;
    }

    int dimensiune;
    char text[4112];
    int verificareLoop = 0;

    while (verificareLoop == 0) // conditie loop
    {
        read(descriptor_socket, &dimensiune, sizeof(int));
        memset(&text, 0, sizeof(text));
        read(descriptor_socket, text, dimensiune);
        printf("%s", text);
        fflush(stdout);

        memset(&text, 0, sizeof(text));
        read(0, text, sizeof(text));

        char nume[128] = {0}, parola[128] = {0};
        int contor_nume = 0, contor_parola = 0, i = 0;
        if (strncmp(text, "sing-up ", 8) == 0)
        {
            nume[0] = parola[0] = '\0';
            contor_nume = contor_parola = 0;
            i = 8;
            login_singup(text, nume, parola, &contor_nume, &contor_parola, &i);
            if (contor_nume == 0 || contor_parola == 0 || text[i])
            {
                // formatul gresit al comandei login, nu avem cum sa facem ceva
            }
            else
            {
                for (int i = 0; i < contor_parola; i++)
                {
                    parola[i] = parola[i] + '@';
                }
                char copieText[1024] = {0};
                copieText[0] = '\0';
                strncpy(copieText, text, 8 + contor_nume + 1);
                strncat(copieText, parola, strlen(parola));
                copieText[strlen(copieText)] = '\n';
                copieText[strlen(copieText)] = '\0';
                strncpy(text, copieText, strlen(copieText));
                text[strlen(text)] = '\0';
            }
        }
        if (strncmp(text, "login ", 6) == 0)
        {
            nume[0] = parola[0] = '\0';
            contor_nume = contor_parola = 0;
            i = 6;
            text[strlen(text)] = '\0';
            login_singup(text, nume, parola, &contor_nume, &contor_parola, &i);
            // printf("%s \n", nume);
            // printf("%s \n", parola);
            if (contor_nume == 0 || contor_parola == 0 || text[i])
            {
                // formatul gresit al comandei login, nu avem cum sa facem hash pentru parola
            }
            else
            {
                unsigned long valoareHash = hashParola((unsigned char *)parola);
                char valoareHashSirDeCaractere[256] = {0}, copieText[1024] = {0};
                valoareHashSirDeCaractere[0] = copieText[0] = '\0';
                sprintf(valoareHashSirDeCaractere, "%lu", valoareHash);
                strncpy(copieText, text, 6 + contor_nume + 1);
                strncat(copieText, valoareHashSirDeCaractere, strlen(valoareHashSirDeCaractere));
                copieText[strlen(copieText)] = '\n';
                copieText[strlen(copieText)] = '\0';
                strncpy(text, copieText, strlen(copieText));
                text[strlen(text)] = '\0';
            }
        }
        dimensiune = strlen(text) + 1;
        write(descriptor_socket, &dimensiune, sizeof(int));
        write(descriptor_socket, text, dimensiune);

        if (strcmp(text, "quit\n") == 0)
        {
            verificareLoop = 1;
        }

        read(descriptor_socket, &dimensiune, sizeof(int));
        memset(&text, 0, sizeof(text));
        read(descriptor_socket, text, dimensiune);

        printf("%s\n", text);

        pid_t copilClient;
        int amFacutCopil = 0;

        if (strcmp(text, "V-ati logat cu succes!") == 0)
            while (1)
            {
                if (amFacutCopil == 0)
                {
                    copilClient = fork();
                    amFacutCopil = 1;
                }
                if (copilClient == 0)
                {
                    int dimensiuneCopil = 0;
                    char textCopil[4112] = {0};
                    int descriptorSocketCopil = dup(descriptor_socket);
                    while (1)
                    {

                        read(descriptorSocketCopil, &dimensiuneCopil, sizeof(int));
                        memset(&textCopil, 0, sizeof(textCopil));
                        read(descriptorSocketCopil, textCopil, dimensiuneCopil);
                        printf("%s\n", textCopil);

                        if (strcmp(textCopil, "V-ati deconectat de la server cu succes!") == 0)
                        {
                            verificareLoop = 1;
                            close(descriptorSocketCopil);
                            exit(EXIT_SUCCESS);
                        }

                        if (strcmp(textCopil, "V-ati deconectat contul cu succes!") == 0)
                        {
                            close(descriptorSocketCopil);
                            exit(EXIT_SUCCESS);
                        }
                    }
                }
                else
                {
                    memset(&text, 0, sizeof(text));
                    read(0, text, sizeof(text));
                    dimensiune = strlen(text) + 1;
                    write(descriptor_socket, &dimensiune, sizeof(int));
                    write(descriptor_socket, text, dimensiune);

                    if (strcmp(text, "quit\n") == 0)
                    {
                        verificareLoop = 1;
                        waitpid(copilClient, NULL, 0);
                        close(descriptor_socket);
                        exit(EXIT_SUCCESS);
                    }
                    else if (strcmp(text, "logout\n") == 0)
                    {
                        waitpid(copilClient, NULL, 0);
                        break;
                    }
                }
            }
    }
    close(descriptor_socket);
    // return 0;
}

void login_singup(char *text, char *nume, char *parola, int *contor_nume, int *contor_parola, int *i)
{
    int verif = 0;
    for (; text[*i] && verif != 2; (*i)++)
    {
        if (verif == 0)
        {
            if (text[*i] != ' ' && text[*i] != '\n')
                nume[(*contor_nume)++] = text[*i];
            else if (text[*i] == ' ')
                verif = 1;
            else if (text[*i] == '\n')
                verif = 2;
        }
        else if (verif == 1)
        {
            if (text[*i] != ' ' && text[*i] != '\n')
                parola[(*contor_parola)++] = text[*i];
            else if (text[*i] == ' ' || text[*i] == '\n')
                verif = 2;
        }
    }
}
unsigned long hashParola(unsigned char *parola) // functia hash djb2 - nu este realizata de mine
{
    unsigned long nrHash = 5381;
    int c;
    while (c = *parola++)
        nrHash = ((nrHash << 5) + nrHash) + c;
    return nrHash;
}