#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>

#define PORT 2908

extern int errno;

typedef struct dateThread
{
    int idThread; // id-ul thread-ului tinut in evidenta de acest program
    int cl;       // descriptorul intors de accept
} dateThread;

sqlite3 *dataDeBaze;
int nrDeThreaduriCreate = 0;
int primitSemnal = 0;

static void *trateazaThread(void *);
void raspunde(void *);
char *verificareCorectitudineComanda(char *mesaj);
void actualizareStatus(char *nume, char *parolaDecriptata, int cazStatus);
int verificareLogIn(char *nume, char *parola, char *parolaDecriptata);
void trimiteMesajCatreClient(void *arg, char *mesaj);
unsigned long hashParola(unsigned char *parola);
void login_singup(char *text, char *nume, char *parola, int *contor_nume, int *contor_parola, int *i);
void singUpBazaDeDate(char *nume, char *parolaDecriptataUsor);
int verificareNumeDiferit(char *nume);
void parametriiFunctiiTren(char *text, char *parametrul1, char *parametrul2, char *parametrul3, int *contor_parametrul1, int *contor_parametrul2, int *contor_parametrul3, int *nrParametrii, int *i);
void functieGeneralaCititModificatXML(char *idTren, char *numeGaraParametru, char *minute, char *informatiiTren, int nrFunctie);
void blocheazaFisierul(int descriptorFisiser);
void deblocheazaFisierul(int descriptorFisiser);
void adaugaPreferintaInFisier(char *nume, char *idTrenFisier);
void stergePreferintaDinFisier(char *nume);
void adaugaMoficariXml(char *idTren, char *gara, char *minute, int nrFunctie);
int dePeCeLinieCitesc();
void mesajNotificare(char *nume, int *linieModificariXML, char *informatiiNotificari);
void gestioneazaSemnal(int semnal);

int main()
{
    functieGeneralaCititModificatXML("Nu este cazul", "Nu este cazul", "Nu este cazul", "Nu este cazul", 0);
    struct sockaddr_in server; // structura folosita de server
    struct sockaddr_in client;
    int descriptor_socket;
    pthread_t identificatoriThread[101]; // Identificatorii thread-urilor care se vor crea

    if ((descriptor_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) // cream unui socket
    {
        perror("Eroare la socket()!\n");
        return errno;
    }

    int on = 1;
    setsockopt(descriptor_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); // utilizarea optiunii SO_REUSEADDR - reutilizarea unei adrese de socket chiar dacă aceasta este încă în stadiul de așteptare a conexiunii anterioare

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    server.sin_family = AF_INET; // stabilirea familiei de socket-uri
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(PORT); // utilizam un port utilizator

    if (bind(descriptor_socket, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) // atasam socketul
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    if (listen(descriptor_socket, 5) == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    while (1)
    {
        int client;
        dateThread *threadInitial; // parametru functia executata de thread
        int length = sizeof(client);

        if ((client = accept(descriptor_socket, (struct sockaddr *)&client, &length)) < 0)
        {
            perror("[server]Eroare la accept().\n");
            continue;
        }

        threadInitial = (struct dateThread *)malloc(sizeof(struct dateThread));
        threadInitial->idThread = nrDeThreaduriCreate++;
        threadInitial->cl = client;

        pthread_create(&identificatoriThread[nrDeThreaduriCreate], NULL, &trateazaThread, threadInitial);
    }
};

static void *trateazaThread(void *arg)
{
    struct dateThread copieThread;
    copieThread = *((struct dateThread *)arg);
    pthread_detach(pthread_self());
    raspunde((struct dateThread *)arg);
    close((intptr_t)arg);
    return (NULL);
};

void raspunde(void *arg)
{
    int i = 0, dimensiune;
    struct dateThread copieThread;
    copieThread = *((struct dateThread *)arg);
    char text[4112] = {0};
    char tipuri_comenzi_nelogat[512] = "\nSunteti nelogat! Ce comanda din urmatorele doriti sa efectuati? \n\n"
                                       "-> login [username] [parola] (pentru a va autentifica)\n"
                                       "-> sing-up [username] [parola] (pentru a va face un cont)\n"
                                       "-> quit (pentru a va deconecta de la server)\n\n";

    char tipuri_comenzi_logat[1024] = "\nSunteti logat! Ce comanda din urmatorele doriti sa efectuati? \n\n"
                                      "-> logout\n"
                                      "-> quit\n"
                                      "-> adaugaPreferintaTren [id_tren]\n"
                                      "-> listaTrenuriZiuaCurenta\n"
                                      "-> informatiiDespreTren [id_tren]\n"
                                      "-> plecariTrenuriOraUrmatoare {[statia_plecare]}\n"
                                      "-> sosiriTrenuriOraUrmatoare {[statia_sosire]}\n"
                                      "-> listaTrenuriPlecariDinStatia [statia_plecare]\n"
                                      "-> listaTrenuriSosiriInStatia [statia_sosire]\n"
                                      "-> ruteDisponibileTrenuri [statia_plecare] [statia_sosire]\n"
                                      "-> seteazaIntarzierePlecareTren [id_tren] [statia_plecare] [minute]\n"
                                      "-> seteazaIntarziereSosireTren [id_tren] [statia_sosire] [minute]\n"
                                      "-> seteazaDevremePlecareTren [id_tren] [statia_plecare] [minute]\n"
                                      "-> seteazaDevremeSosireTren [id_tren] [statia_plecare] [minute]\n\n";
    int verificareLoop = 0;
    while (verificareLoop == 0)
    {
        trimiteMesajCatreClient((struct dateThread *)arg, tipuri_comenzi_nelogat);
        read(copieThread.cl, &dimensiune, sizeof(int));
        read(copieThread.cl, text, dimensiune);

        // printf("%s", text);
        char *raspunsFunctie = verificareCorectitudineComanda(text);
        // printf("%s", raspunsFunctie);
        if (strcmp(raspunsFunctie, "Incorect") == 0)
        {
            strncpy(text, "Comanda incorecta - verifica formatul!", sizeof("Comanda incorecta - verifica formatul!"));
            text[strlen(text)] = '\0';
            trimiteMesajCatreClient((struct dateThread *)arg, text);
            free(raspunsFunctie);
        }
        else if (strcmp(raspunsFunctie, "quit") == 0)
        {
            verificareLoop = 1;
            strncpy(text, "V-ati deconectat de la server cu succes!", sizeof("V-ati deconectat de la server cu succes!"));
            text[strlen(text)] = '\0';
            trimiteMesajCatreClient((struct dateThread *)arg, text);
            free(raspunsFunctie);
        }
        else if (strcmp(raspunsFunctie, "sing-up") == 0)
        {
            char nume[128] = {0}, parolaCriptataUsor[128] = {0}, parolaDecriptataUsor[128] = {0};
            int contor_nume = 0, contor_parola = 0, i = 0;
            nume[0] = parolaCriptataUsor[0] = parolaDecriptataUsor[0] = '\0';
            contor_nume = contor_parola = 0;
            i = 8;
            login_singup(text, nume, parolaCriptataUsor, &contor_nume, &contor_parola, &i);
            for (int j = 0; parolaCriptataUsor[j]; j++)
                parolaDecriptataUsor[j] = parolaCriptataUsor[j] - '@';

            if (contor_nume == 0 || contor_parola == 0 || text[i])
            {
                strncpy(text, "Ati introdus username-ul sau parola gresit!", sizeof("Ati introdus username-ul sau parola gresit!"));
                text[strlen(text)] = '\0';
                trimiteMesajCatreClient((struct dateThread *)arg, text);
            }
            else
            {
                int rezultat = sqlite3_open("utilizatori.db", &dataDeBaze);
                if (rezultat == SQLITE_OK)
                {
                    int cazVerificareNumeDiferit = verificareNumeDiferit(nume);
                    if (cazVerificareNumeDiferit == 1)
                    {
                        strncpy(text, "Acest nume este deja atribuit la alt cont!", sizeof("Acest nume este deja atribuit la alt cont!"));
                        text[strlen(text)] = '\0';
                    }
                    else if (cazVerificareNumeDiferit == 0)
                    {
                        singUpBazaDeDate(nume, parolaDecriptataUsor);
                        strncpy(text, "Sing-up facut cu succes!", sizeof("Sing-up facut cu succes!"));
                        text[strlen(text)] = '\0';
                    }
                }
                sqlite3_close(dataDeBaze);
                trimiteMesajCatreClient((struct dateThread *)arg, text);
            }
            free(raspunsFunctie);
        }
        else
        {
            // printf("%s\n", text);
            char nume[128] = {0}, parolaCriptata[128] = {0};
            int contor_nume = 0, contor_parola = 0, i = 0;
            if (strcmp(raspunsFunctie, "login") == 0)
            {
                nume[0] = parolaCriptata[0] = '\0';
                contor_nume = contor_parola = 0;
                i = 6;
                login_singup(text, nume, parolaCriptata, &contor_nume, &contor_parola, &i);
                // printf("%s \n", nume);
                // printf("%s \n", parolaCriptata);
                if (contor_nume == 0 || contor_parola == 0 || text[i])
                {
                    strncpy(text, "Ati introdus username-ul sau parola gresit!", sizeof("Ati introdus username-ul sau parola gresit!"));
                    text[strlen(text)] = '\0';
                    trimiteMesajCatreClient((struct dateThread *)arg, text);
                }
                else
                {
                    char parametrul1[64] = {0}, parametrul2[64] = {0}, parametrul3[64] = {0}, parolaDecriptata[256] = {0};
                    int contor_parametrul1 = 0, contor_parametrul2 = 0, contor_parametrul3 = 0;
                    parolaDecriptata[0] = '\0';
                    int rezultat = sqlite3_open("utilizatori.db", &dataDeBaze);
                    if (rezultat == SQLITE_OK)
                    {
                        int cazulRezultat = verificareLogIn(nume, parolaCriptata, parolaDecriptata);
                        sqlite3_close(dataDeBaze);
                        if (cazulRezultat == 0)
                        {
                            strncpy(text, "Username-ul sau parola a fost introdusa gresit!", sizeof("Username-ul sau parola a fost introdusa gresit!"));
                            text[strlen(text)] = '\0';
                            trimiteMesajCatreClient((struct dateThread *)arg, text);
                        }
                        else if (cazulRezultat == 1)
                        {
                            strncpy(text, "Acest cont este deja logat!", sizeof("Acest cont este deja logat!"));
                            text[strlen(text)] = '\0';
                            trimiteMesajCatreClient((struct dateThread *)arg, text);
                        }
                        else if (cazulRezultat == 2)
                        {
                            strncpy(text, "V-ati logat cu succes!", sizeof("V-ati logat cu succes!"));
                            text[strlen(text)] = '\0';
                            trimiteMesajCatreClient((struct dateThread *)arg, text);
                            int linieModificariXML = dePeCeLinieCitesc();

                            pid_t copilServer;
                            int amCopilServer = 0;
                            primitSemnal = 0;
                            while (1)
                            {
                                if (amCopilServer == 0)
                                {
                                    copilServer = fork();
                                    amCopilServer = 1;
                                }
                                if (copilServer == 0)
                                {

                                    char textServer[4112] = {0};
                                    char numeCopil[256];
                                    strcpy(numeCopil, nume);
                                    numeCopil[strlen(numeCopil)] = '\0';

                                    while (1)
                                    {
                                        signal(SIGUSR1, gestioneazaSemnal);
                                        if (primitSemnal == 1)
                                        {
                                            primitSemnal = 0;
                                            exit(EXIT_SUCCESS);
                                        }
                                        char informatiiNotificari[256] = {0};
                                        informatiiNotificari[0] = '\0';
                                        mesajNotificare(numeCopil, &linieModificariXML, informatiiNotificari);
                                        strcpy(textServer, informatiiNotificari);
                                        textServer[strlen(textServer)] = '\0';
                                        if (strcmp(textServer, "Nu sunt notificari momentan!\n") != 0)
                                            trimiteMesajCatreClient((struct dateThread *)arg, textServer);
                                    }
                                }
                                else
                                {
                                    trimiteMesajCatreClient((struct dateThread *)arg, tipuri_comenzi_logat);

                                    read(copieThread.cl, &dimensiune, sizeof(int));
                                    read(copieThread.cl, text, dimensiune);
                                    if (strncmp(text, "adaugaPreferintaTren ", 21) == 0)
                                    {
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 1;
                                        int i = 21;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (contor_parametrul1 == 0 || text[i])
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else
                                        {
                                            char informatiiTren[4112] = {0};
                                            informatiiTren[0] = '\0';
                                            functieGeneralaCititModificatXML(parametrul1, "Nu este cazul!", "Nu este cazul!", informatiiTren, 12);
                                            if (strcmp(informatiiTren, "Am gasit trenul cu id-ul respectiv") == 0)
                                            {
                                                adaugaPreferintaInFisier(nume, parametrul1);
                                                strcpy(informatiiTren, "Preferinta a fost adaugata cu succes");
                                            }
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strcmp(text, "quit\n") == 0)
                                    {
                                        rezultat = sqlite3_open("utilizatori.db", &dataDeBaze);
                                        actualizareStatus(nume, parolaDecriptata, 1);
                                        sqlite3_close(dataDeBaze);
                                        strncpy(text, "V-ati deconectat de la server cu succes!", sizeof("V-ati deconectat de la server cu succes!"));
                                        text[strlen(text)] = '\0';
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                        verificareLoop = 1;
                                        stergePreferintaDinFisier(nume);

                                        kill(copilServer, SIGUSR1);
                                        waitpid(copilServer, NULL, 0);

                                        break;
                                    }
                                    else if (strcmp(text, "logout\n") == 0)
                                    {
                                        rezultat = sqlite3_open("utilizatori.db", &dataDeBaze);
                                        actualizareStatus(nume, parolaDecriptata, 1);
                                        sqlite3_close(dataDeBaze);
                                        strncpy(text, "V-ati deconectat contul cu succes!", sizeof("V-ati deconectat contul cu succes!"));
                                        text[strlen(text)] = '\0';
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                        stergePreferintaDinFisier(nume);

                                        kill(copilServer, SIGUSR1);
                                        waitpid(copilServer, NULL, 0);

                                        break;
                                    }
                                    else if (strcmp(text, "listaTrenuriZiuaCurenta\n") == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        functieGeneralaCititModificatXML("Nu este cazul!", "Nu este cazul!", "Nu este cazul!", informatiiTren, 1);
                                        // printf("%s\n", informatiiTren);
                                        strcpy(text, informatiiTren);
                                        text[strlen(text)] = '\0';
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "informatiiDespreTren ", 21) == 0) //!!!!!! similar pentru restul functiilor (vreau sa fac o singura functie pentru toate cazurile)
                                    {
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 1;
                                        int i = 21;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);

                                        if (contor_parametrul1 == 0 || text[i])
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else
                                        {
                                            char informatiiTren[4112] = {0};
                                            informatiiTren[0] = '\0';
                                            functieGeneralaCititModificatXML(parametrul1, "Nu este cazul!", "Nu este cazul!", informatiiTren, 2);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "plecariTrenuriOraUrmatoare ", 27) == 0 || strncmp(text, "plecariTrenuriOraUrmatoare", 26) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 1;
                                        int i = 27;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i])
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (contor_parametrul1 == 0 && text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML("Nu este cazul!", NULL, "Nu este cazul!", informatiiTren, 3);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (contor_parametrul1 != 0 && text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML("Nu este cazul!", parametrul1, "Nu este cazul!", informatiiTren, 3);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "sosiriTrenuriOraUrmatoare ", 26) == 0 || strncmp(text, "sosiriTrenuriOraUrmatoare", 25) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 1;
                                        int i = 26;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i])
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (contor_parametrul1 == 0 && text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML("Nu este cazul!", NULL, "Nu este cazul!", informatiiTren, 4);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (contor_parametrul1 != 0 && text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML("Nu este cazul!", parametrul1, "Nu este cazul!", informatiiTren, 4);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "seteazaIntarzierePlecareTren ", 29) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 3;
                                        int i = 29;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i] || contor_parametrul1 == 0 || contor_parametrul2 == 0 || contor_parametrul3 == 0)
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (text[i] == '\0')
                                        {
                                            // printf("%s\n%s\n%s\n", parametrul1, parametrul2, parametrul3);
                                            functieGeneralaCititModificatXML(parametrul1, parametrul2, parametrul3, informatiiTren, 5);
                                            adaugaMoficariXml(parametrul1, parametrul2, parametrul3, 1);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "seteazaIntarziereSosireTren ", 28) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 3;
                                        int i = 28;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i] || contor_parametrul1 == 0 || contor_parametrul2 == 0 || contor_parametrul3 == 0)
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML(parametrul1, parametrul2, parametrul3, informatiiTren, 6);
                                            adaugaMoficariXml(parametrul1, parametrul2, parametrul3, 2);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "seteazaDevremePlecareTren ", 26) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 3;
                                        int i = 26;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i] || contor_parametrul1 == 0 || contor_parametrul2 == 0 || contor_parametrul3 == 0)
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML(parametrul1, parametrul2, parametrul3, informatiiTren, 7);
                                            adaugaMoficariXml(parametrul1, parametrul2, parametrul3, 3);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "seteazaDevremeSosireTren ", 25) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 3;
                                        int i = 25;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i] || contor_parametrul1 == 0 || contor_parametrul2 == 0 || contor_parametrul3 == 0)
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML(parametrul1, parametrul2, parametrul3, informatiiTren, 8);
                                            adaugaMoficariXml(parametrul1, parametrul2, parametrul3, 4);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "listaTrenuriPlecariDinStatia ", 29) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 1;
                                        int i = 29;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i] || contor_parametrul1 == 0)
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML("Nu este cazul", parametrul1, "Nu este cazul", informatiiTren, 9);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "listaTrenuriSosiriInStatia ", 27) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 1;
                                        int i = 27;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i] || contor_parametrul1 == 0)
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML("Nu este cazul", parametrul1, "Nu este cazul", informatiiTren, 10);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else if (strncmp(text, "ruteDisponibileTrenuri ", 23) == 0)
                                    {
                                        char informatiiTren[1024] = {0};
                                        informatiiTren[0] = '\0';
                                        parametrul1[0] = parametrul2[0] = parametrul3[0] = '\0';
                                        contor_parametrul1 = contor_parametrul2 = contor_parametrul3 = 0;
                                        int nrParametrii = 2;
                                        int i = 23;

                                        parametriiFunctiiTren(text, parametrul1, parametrul2, parametrul3, &contor_parametrul1, &contor_parametrul2, &contor_parametrul3, &nrParametrii, &i);
                                        if (text[i] || contor_parametrul1 == 0 || contor_parametrul2 == 0)
                                        {
                                            strncpy(text, "Ati introdus comanda gresit! - verificati formatul", sizeof("Ati introdus comanda gresit! - verificati formatul"));
                                            text[strlen(text)] = '\0';
                                        }
                                        else if (text[i] == '\0')
                                        {
                                            functieGeneralaCititModificatXML(parametrul1, parametrul2, "Nu este cazul", informatiiTren, 11);
                                            strcpy(text, informatiiTren);
                                            text[strlen(text)] = '\0';
                                        }
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                    else
                                    {
                                        strncpy(text, "Comanda gresita!", sizeof("Comanda gresita!"));
                                        text[strlen(text)] = '\0';
                                        trimiteMesajCatreClient((struct dateThread *)arg, text);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            free(raspunsFunctie);
        }
    }
}

void parametriiFunctiiTren(char *text, char *parametrul1, char *parametrul2, char *parametrul3, int *contor_parametrul1, int *contor_parametrul2, int *contor_parametrul3, int *nrParametrii, int *i)
{
    int verif = 0;
    if ((*nrParametrii) == 1)
    {
        for (; text[*i] && verif != 1; (*i)++)
        {
            if (verif == 0)
            {
                if (text[*i] != '\n' && text[*i] != ' ')
                    parametrul1[(*contor_parametrul1)++] = text[*i];
                else if (text[*i] == '\n' || text[*i] == ' ')
                    verif = 1;
            }
        }
    }
    if ((*nrParametrii) == 2)
    {
        for (; text[*i] && verif != 2; (*i)++)
        {
            if (verif == 0)
            {
                if (text[*i] != ' ' && text[*i] != '\n')
                    parametrul1[(*contor_parametrul1)++] = text[*i];
                else if (text[*i] == ' ')
                    verif = 1;
                else if (text[*i] == '\n')
                    verif = 2;
            }
            else if (verif == 1)
            {
                if (text[*i] != ' ' && text[*i] != '\n')
                    parametrul2[(*contor_parametrul2)++] = text[*i];
                else if (text[*i] == ' ' || text[*i] == '\n')
                    verif = 2;
            }
        }
    }
    if ((*nrParametrii) == 3)
    {
        for (; text[*i] && verif != 3; (*i)++)
        {
            if (verif == 0)
            {
                if (text[*i] != ' ' && text[*i] != '\n')
                    parametrul1[(*contor_parametrul1)++] = text[*i];
                else if (text[*i] == ' ')
                    verif = 1;
                else if (text[*i] == '\n')
                    verif = 3;
            }
            else if (verif == 1)
            {
                if (text[*i] != ' ' && text[*i] != '\n')
                    parametrul2[(*contor_parametrul2)++] = text[*i];
                else if (text[*i] == ' ')
                    verif = 2;
                else if (text[*i] == '\n')
                    verif = 3;
            }
            else if (verif == 2)
            {
                if (text[*i] != ' ' && text[*i] != '\n')
                    parametrul3[(*contor_parametrul3)++] = text[*i];
                else if (text[*i] == ' ' || text[*i] == '\n')
                    verif = 3;
            }
        }
    }
    parametrul1[*contor_parametrul1] = parametrul2[*contor_parametrul2] = parametrul3[*contor_parametrul3] = '\0';
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
    nume[*contor_nume] = parola[*contor_parola] = '\0';
}

void trimiteMesajCatreClient(void *arg, char *mesaj)
{
    struct dateThread copieThread;
    copieThread = *((struct dateThread *)arg);
    int dimensiune = strlen(mesaj) + 1;
    write(copieThread.cl, &dimensiune, sizeof(int));
    write(copieThread.cl, mesaj, dimensiune);
}

char *verificareCorectitudineComanda(char *mesaj)
{
    int i;
    char *primulCuvantDinComanda = (char *)malloc(64 * sizeof(char));
    for (i = 0; i < strlen(mesaj) && mesaj[i] != ' ' && mesaj[i] != '\n'; i++)
    {
    };
    strncpy(primulCuvantDinComanda, mesaj, i);
    primulCuvantDinComanda[i] = '\0';
    if (strcmp(primulCuvantDinComanda, "login") == 0 || strcmp(primulCuvantDinComanda, "quit") == 0 || strcmp(primulCuvantDinComanda, "sing-up") == 0)
    {
        return primulCuvantDinComanda;
    }
    else
    {
        strncpy(primulCuvantDinComanda, "Incorect", sizeof("Incorect"));
        primulCuvantDinComanda[strlen(primulCuvantDinComanda)] = '\0';
        printf("%s", primulCuvantDinComanda);
        return primulCuvantDinComanda;
    }
}

void actualizareStatus(char *nume, char *parolaDecriptata, int cazStatus)
{
    char comandaSql[256] = {0};
    sqlite3_stmt *declr;
    if (cazStatus == 0)
        sprintf(comandaSql, "UPDATE utilizatori SET Status = %d WHERE Username = '%s' AND Password = '%s'", 1, nume, parolaDecriptata);
    else
        sprintf(comandaSql, "UPDATE utilizatori SET Status = %d WHERE Username = '%s' AND Password = '%s'", 0, nume, parolaDecriptata);
    int rezultat = sqlite3_prepare_v2(dataDeBaze, comandaSql, -1, &declr, NULL);
    if (rezultat == SQLITE_OK)
    {
        rezultat = sqlite3_step(declr);
        if (rezultat != SQLITE_DONE)
            printf("Eroare la actualizarea statusului!\n");
        sqlite3_finalize(declr);
    }
}

int verificareNumeDiferit(char *nume)
{
    char comandaSql[256] = {0};
    sqlite3_stmt *declr;
    sprintf(comandaSql, "SELECT * FROM utilizatori WHERE Username = '%s'", nume);
    int rezultat = sqlite3_prepare_v2(dataDeBaze, comandaSql, -1, &declr, NULL);
    if (rezultat == SQLITE_OK)
    {
        rezultat = sqlite3_step(declr);
        if (rezultat == SQLITE_ROW)
        {
            sqlite3_finalize(declr);
            return 1;
        }
        else
        {
            sqlite3_finalize(declr);
            return 0;
        }
    }
    sqlite3_finalize(declr);
    return 2;
}

int verificaIdDiferit(int idRandom)
{
    char comandaSql[256] = {0};
    sqlite3_stmt *declr;
    sprintf(comandaSql, "SELECT * FROM utilizatori WHERE Id = '%i'", idRandom);
    int rezultat = sqlite3_prepare_v2(dataDeBaze, comandaSql, -1, &declr, NULL);
    if (rezultat == SQLITE_OK)
    {
        rezultat = sqlite3_step(declr);
        if (rezultat == SQLITE_ROW)
        {
            sqlite3_finalize(declr);
            return 1;
        }
        else
        {
            sqlite3_finalize(declr);
            return 0;
        }
    }
    sqlite3_finalize(declr);
    return 2;
}

void singUpBazaDeDate(char *nume, char *parolaDecriptataUsor)
{
    int idRandom;
    do
    {
        idRandom = 0;
        srand((unsigned int)time(NULL));
        while (idRandom < 10000)
        {
            idRandom = idRandom * 10 + rand() % 10;
        }
    } while (verificaIdDiferit(idRandom) == 1);

    char comandaSql[256] = {0};
    sqlite3_stmt *declr;
    sprintf(comandaSql, "INSERT INTO utilizatori (Id, Username, Password, Status) VALUES (%i, '%s', '%s', '%i')", idRandom, nume, parolaDecriptataUsor, 0);
    int rezultat = sqlite3_prepare_v2(dataDeBaze, comandaSql, -1, &declr, NULL);
    if (rezultat == SQLITE_OK)
    {
        rezultat = sqlite3_step(declr);
        if (rezultat != SQLITE_DONE)
        {
            sqlite3_finalize(declr);
            printf("%s\n", "Eroare la adaugarea unui nou cont!");
            return;
        }
        else
        {
            sqlite3_finalize(declr);
            return;
        }
        sqlite3_finalize(declr);
    }
}

int verificareLogIn(char *nume, char *parolaCriptata, char *parolaDecriptata)
{
    char comandaSql[256] = {0};
    sqlite3_stmt *declr;
    sprintf(comandaSql, "SELECT Password, Status FROM utilizatori WHERE Username = '%s'", nume);
    int rezultat = sqlite3_prepare_v2(dataDeBaze, comandaSql, -1, &declr, NULL);
    if (rezultat == SQLITE_OK)
    {
        rezultat = sqlite3_step(declr); // executam comandaSql
        if (rezultat == SQLITE_ROW)     // verificam daca am gasit vreun rezultat
        {
            char *parolaBazeDeDate = (char *)sqlite3_column_text(declr, 0);
            strncpy(parolaDecriptata, parolaBazeDeDate, strlen(parolaBazeDeDate));
            unsigned long valoareHash = hashParola((unsigned char *)parolaBazeDeDate);
            unsigned long valoareHashClient = strtoul(parolaCriptata, NULL, 10);
            if (valoareHash != valoareHashClient)
            {
                return 0;
            }
            int status = sqlite3_column_int(declr, 1);
            if (status == 0)
            {
                actualizareStatus(nume, parolaDecriptata, 0);
                sqlite3_finalize(declr);
                return 2;
            }
            else
            {
                sqlite3_finalize(declr);
                return 1;
            }
        }
        else
        {
            sqlite3_finalize(declr);
            return 0;
        }
        sqlite3_finalize(declr); // finalizam intructiunea
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

void functieGeneralaCititModificatXML(char *idTren, char *numeGaraParametru, char *minute, char *informatiiTren, int nrFunctie)
{
    int amGasitInformatii = 0;
    char informatiiTrenFunctie[1024] = {0};
    if (nrFunctie != 0)
        informatiiTren[0] = '\0';
    int descriptorFisier = open("MersulTrenurilor.xml", O_RDWR);
    blocheazaFisierul(descriptorFisier);
    xmlDocPtr fisierXML;
    xmlNodePtr radacinaTrenuri, tren, gara, informatiiGara;
    fisierXML = xmlReadFd(descriptorFisier, NULL, NULL, 0);
    radacinaTrenuri = xmlDocGetRootElement(fisierXML);

    time_t timpCurent = time(NULL);
    struct tm timpInformatii;
    localtime_r(&timpCurent, &timpInformatii);
    int oraCurenta = timpInformatii.tm_hour * 60 + timpInformatii.tm_min;

    time_t timpCurentPentruData = time(NULL);
    struct tm *timpInformatiiPentruData = localtime(&timpCurentPentruData);
    char dataActuala[12] = {0};
    strftime(dataActuala, sizeof(dataActuala), "%d-%m-%Y", timpInformatiiPentruData);

    for (tren = radacinaTrenuri->children; tren; tren = tren->next)
    {
        if (tren->type == XML_ELEMENT_NODE && xmlStrcmp(tren->name, (const xmlChar *)"tren") == 0)
        {
            xmlChar *trenIdXML = xmlGetProp(tren, (const xmlChar *)"id"); // Obține id-ul trenului
            if (nrFunctie == 0)
            {
                xmlChar *dataPlecare, *dataSosire;
                for (gara = tren->children; gara; gara = gara->next)
                {
                    if (gara->type == XML_ELEMENT_NODE && xmlStrcmp(gara->name, (const xmlChar *)"gara") == 0)
                    {
                        for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                        {
                            if (informatiiGara->type == XML_ELEMENT_NODE && (xmlStrcmp(informatiiGara->name, (const xmlChar *)"data_plecare") == 0 || xmlStrcmp(informatiiGara->name, (const xmlChar *)"data_sosire") == 0))
                            {
                                if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"data_plecare") == 0)
                                {
                                    dataPlecare = xmlNodeGetContent(informatiiGara);
                                    if (xmlStrcmp(dataPlecare, (const xmlChar *)"Sfarsit ruta") != 0)
                                        xmlNodeSetContent(informatiiGara, (const xmlChar *)dataActuala);
                                }
                                if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"data_sosire") == 0)
                                {
                                    dataSosire = xmlNodeGetContent(informatiiGara);
                                    if (xmlStrcmp(dataSosire, (const xmlChar *)"Inceput ruta") != 0)
                                        xmlNodeSetContent(informatiiGara, (const xmlChar *)dataActuala);
                                }
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 1)
            {
                if (trenIdXML != NULL)
                {
                    sprintf(informatiiTrenFunctie, "%s\n", trenIdXML);
                    strncat(informatiiTren, informatiiTrenFunctie, strlen(informatiiTrenFunctie));
                }
            }
            else if (nrFunctie == 2 || nrFunctie == 12)
            {
                if (strcmp((char *)trenIdXML, idTren) == 0)
                {
                    amGasitInformatii = 1;
                    informatiiTrenFunctie[0] = '\0';
                    if (nrFunctie == 12)
                    {
                        strcpy(informatiiTren, "Am gasit trenul cu id-ul respectiv");
                        break;
                    }
                    for (gara = tren->children; gara; gara = gara->next)
                    {
                        if (gara->type == XML_ELEMENT_NODE && strcmp((char *)gara->name, "gara") == 0)
                        {
                            xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                            sprintf(informatiiTrenFunctie, "Stație: %s\n", numeGara);
                            strcat(informatiiTren, informatiiTrenFunctie);

                            for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                            {
                                if (informatiiGara->type == XML_ELEMENT_NODE)
                                {
                                    sprintf(informatiiTrenFunctie, "%s: %s\n", informatiiGara->name, xmlNodeGetContent(informatiiGara));
                                    strcat(informatiiTren, informatiiTrenFunctie);
                                }
                            }
                            strcat(informatiiTren, "\n");
                        }
                    }
                }
            }
            else if (nrFunctie == 9)
            {
                for (gara = tren->children; gara; gara = gara->next)
                {
                    if (gara->type == XML_ELEMENT_NODE && strcmp((char *)gara->name, "gara") == 0)
                    {
                        xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                        xmlChar *statusPlecare;
                        if (numeGara != NULL && xmlStrcmp(numeGara, (const xmlChar *)numeGaraParametru) == 0)
                        {
                            for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                            {
                                if (informatiiGara->type == XML_ELEMENT_NODE)
                                {
                                    if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"status_plecare") == 0)
                                    {
                                        statusPlecare = xmlNodeGetContent(informatiiGara);
                                        if (xmlStrcmp(statusPlecare, (const xmlChar *)"Sfarsit ruta") != 0)
                                        {
                                            amGasitInformatii = 1;
                                            informatiiTrenFunctie[0] = '\0';
                                            sprintf(informatiiTrenFunctie, "%s\n", trenIdXML);
                                            strcat(informatiiTren, informatiiTrenFunctie);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 10)
            {
                for (gara = tren->children; gara; gara = gara->next)
                {
                    if (gara->type == XML_ELEMENT_NODE && strcmp((char *)gara->name, "gara") == 0)
                    {
                        xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                        xmlChar *statusSosire;
                        if (numeGara != NULL && xmlStrcmp(numeGara, (const xmlChar *)numeGaraParametru) == 0)
                        {
                            for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                            {
                                if (informatiiGara->type == XML_ELEMENT_NODE)
                                {
                                    if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"status_sosire") == 0)
                                    {
                                        statusSosire = xmlNodeGetContent(informatiiGara);
                                        if (xmlStrcmp(statusSosire, (const xmlChar *)"Inceput ruta") != 0)
                                        {
                                            amGasitInformatii = 1;
                                            informatiiTrenFunctie[0] = '\0';
                                            sprintf(informatiiTrenFunctie, "%s\n", trenIdXML);
                                            strcat(informatiiTren, informatiiTrenFunctie);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 3)
            {
                informatiiTrenFunctie[0] = '\0';
                for (gara = tren->children; gara != NULL; gara = gara->next)
                {
                    if (gara->type == XML_ELEMENT_NODE && xmlStrcmp(gara->name, (const xmlChar *)"gara") == 0)
                    {
                        int contorXML = 0;
                        xmlChar *statusPlecare, *oraPlecare, *dataPlecare, *intarzierePlecare, *maiDevremePlecare;
                        xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                        for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                        {
                            if (informatiiGara->type == XML_ELEMENT_NODE)
                            {
                                if (contorXML == 0)
                                    statusPlecare = xmlNodeGetContent(informatiiGara);
                                if (contorXML == 2)
                                    oraPlecare = xmlNodeGetContent(informatiiGara);
                                if (contorXML == 3)
                                    dataPlecare = xmlNodeGetContent(informatiiGara);
                                if (contorXML == 7)
                                    intarzierePlecare = xmlNodeGetContent(informatiiGara);
                                if (contorXML == 8)
                                    maiDevremePlecare = xmlNodeGetContent(informatiiGara);
                                contorXML++;
                            }
                        }

                        if ((numeGaraParametru == NULL || (xmlStrcmp(numeGara, (const xmlChar *)numeGaraParametru) == 0) && xmlStrcmp(oraPlecare, (const xmlChar *)"Sfarsit ruta") != 0))
                        {

                            int oraGara = 0, minuteGara = 0;
                            char *pointerOraPlecare = (char *)oraPlecare;
                            int contorPointerOraPlecare = 0;
                            while (pointerOraPlecare[contorPointerOraPlecare] && pointerOraPlecare[contorPointerOraPlecare] != ':')
                            {
                                oraGara = oraGara * 10 + (pointerOraPlecare[contorPointerOraPlecare] - '0');
                                contorPointerOraPlecare++;
                            }
                            contorPointerOraPlecare++;
                            while (pointerOraPlecare[contorPointerOraPlecare] && pointerOraPlecare[contorPointerOraPlecare] != ':')
                            {
                                minuteGara = minuteGara * 10 + (pointerOraPlecare[contorPointerOraPlecare] - '0');
                                contorPointerOraPlecare++;
                            }
                            int oraGaraInMinute = oraGara * 60 + minuteGara;
                            if (oraGara >= 0 && oraGara < 1 && oraCurenta < 60)
                                oraGaraInMinute = oraGaraInMinute + 24 * 60;

                            if (oraGaraInMinute - oraCurenta < 60 && oraGaraInMinute - oraCurenta > 0)
                            {
                                amGasitInformatii = 1;
                                sprintf(informatiiTrenFunctie, "ID tren: %s\nGara: %s\nStatus plecare: %s\nOra plecare: %s\nData plecare: %s\nIntarziere plecare: %s\nMai devreme plecare: %s\n\n",
                                        trenIdXML, numeGara, statusPlecare, oraPlecare, dataPlecare, intarzierePlecare, maiDevremePlecare);
                                strcat(informatiiTren, informatiiTrenFunctie);
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 4)
            {
                informatiiTrenFunctie[0] = '\0';
                for (gara = tren->children; gara != NULL; gara = gara->next)
                {
                    if (gara->type == XML_ELEMENT_NODE && xmlStrcmp(gara->name, (const xmlChar *)"gara") == 0)
                    {
                        int contorXML = 0;
                        xmlChar *statusSosire, *oraSosire, *dataSosire, *intarziereSosire, *maiDevremeSosire;
                        xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                        for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                        {
                            if (informatiiGara->type == XML_ELEMENT_NODE)
                            {
                                if (contorXML == 1)
                                    statusSosire = xmlNodeGetContent(informatiiGara);
                                if (contorXML == 4)
                                    oraSosire = xmlNodeGetContent(informatiiGara);
                                if (contorXML == 5)
                                    dataSosire = xmlNodeGetContent(informatiiGara);
                                if (contorXML == 6)
                                    intarziereSosire = xmlNodeGetContent(informatiiGara);
                                if (contorXML == 9)
                                    maiDevremeSosire = xmlNodeGetContent(informatiiGara);
                                contorXML++;
                            }
                        }

                        if ((numeGaraParametru == NULL || (xmlStrcmp(numeGara, (const xmlChar *)numeGaraParametru) == 0) && xmlStrcmp(oraSosire, (const xmlChar *)"Inceput ruta") != 0))
                        {

                            int oraGara = 0, minuteGara = 0;
                            char *pointerOraSosire = (char *)oraSosire;
                            int contorPointerOraSosire = 0;
                            while (pointerOraSosire[contorPointerOraSosire] && pointerOraSosire[contorPointerOraSosire] != ':')
                            {
                                oraGara = oraGara * 10 + (pointerOraSosire[contorPointerOraSosire] - '0');
                                contorPointerOraSosire++;
                            }
                            contorPointerOraSosire++;
                            while (pointerOraSosire[contorPointerOraSosire] && pointerOraSosire[contorPointerOraSosire] != ':')
                            {
                                minuteGara = minuteGara * 10 + (pointerOraSosire[contorPointerOraSosire] - '0');
                                contorPointerOraSosire++;
                            }
                            int oraGaraInMinute = oraGara * 60 + minuteGara;
                            if (oraGara >= 0 && oraGara < 1 && oraCurenta < 60)
                                oraGaraInMinute = oraGaraInMinute + 24 * 60;

                            if (oraGaraInMinute - oraCurenta < 60 && oraGaraInMinute - oraCurenta > 0)
                            {
                                amGasitInformatii = 1;
                                sprintf(informatiiTrenFunctie, "ID tren: %s\nGara: %s\nStatus sosire: %s\nOra sosire: %s\nData sosire: %s\nIntarziere sosire: %s\nMai devreme sosire: %s\n\n",
                                        trenIdXML, numeGara, statusSosire, oraSosire, dataSosire, intarziereSosire, maiDevremeSosire);
                                strcat(informatiiTren, informatiiTrenFunctie);
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 5)
            {
                if (xmlStrcmp(trenIdXML, (const xmlChar *)idTren) == 0)
                {
                    for (gara = tren->children; gara; gara = gara->next)
                    {
                        if (gara->type == XML_ELEMENT_NODE && strcmp((char *)gara->name, "gara") == 0)
                        {
                            xmlChar *intarzierePlecare, *maiDevremePlecare, *statusPlecare;
                            xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                            if (numeGara != NULL && xmlStrcmp(numeGara, (const xmlChar *)numeGaraParametru) == 0)
                            {
                                for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                                {
                                    if (informatiiGara->type == XML_ELEMENT_NODE)
                                    {
                                        char vecTemp[32] = {0};
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"status_plecare") == 0)
                                        {
                                            statusPlecare = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(statusPlecare, (const xmlChar *)"Sfarsit ruta") != 0)
                                            {
                                                if (atoi(minute) == 0)
                                                    sprintf(vecTemp, "%s", "Fara intarziere");
                                                else
                                                    sprintf(vecTemp, "%s", "Cu intarziere");
                                                amGasitInformatii = 1;
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                            }
                                        }
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"intarziere_plecare") == 0)
                                        {
                                            intarzierePlecare = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(intarzierePlecare, (const xmlChar *)"Sfarsit ruta") != 0)
                                            {
                                                sprintf(vecTemp, "%s minute", minute);
                                                amGasitInformatii = 1;
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                            }
                                        }
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"mai_devreme_plecare") == 0 && amGasitInformatii == 1)
                                        {
                                            maiDevremePlecare = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(maiDevremePlecare, (const xmlChar *)"Sfarsit ruta") != 0)
                                            {
                                                if (atoi(minute) == 0)
                                                    sprintf(vecTemp, "%s minute", minute);
                                                else
                                                    sprintf(vecTemp, "-%s minute", minute);
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                break;
                                            }
                                        }
                                    }
                                }
                                if (amGasitInformatii == 1)
                                {
                                    xmlChar *intarziereSosire, *maiDevremeSosire, *statusSosire;
                                    char vecTemp[32] = {0};
                                    gara = gara->next;
                                    while (gara)
                                    {
                                        for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                                        {
                                            if (informatiiGara->type == XML_ELEMENT_NODE)
                                            {
                                                if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"status_sosire") == 0)
                                                {
                                                    statusSosire = xmlNodeGetContent(informatiiGara);
                                                    if (xmlStrcmp(statusSosire, (const xmlChar *)"Inceput ruta") != 0)
                                                    {
                                                        if (atoi(minute) == 0)
                                                            sprintf(vecTemp, "%s", "Fara intarziere");
                                                        else
                                                            sprintf(vecTemp, "%s", "Cu intarziere");
                                                        amGasitInformatii = 1;
                                                        xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                    }
                                                }
                                                if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"intarziere_sosire") == 0)
                                                {
                                                    intarziereSosire = xmlNodeGetContent(informatiiGara);
                                                    if (xmlStrcmp(intarziereSosire, (const xmlChar *)"Inceput ruta") != 0)
                                                    {
                                                        sprintf(vecTemp, "%s minute", minute);
                                                        xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                    }
                                                }
                                                if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"mai_devreme_sosire") == 0)
                                                {
                                                    maiDevremeSosire = xmlNodeGetContent(informatiiGara);
                                                    if (xmlStrcmp(maiDevremePlecare, (const xmlChar *)"Inceput ruta") != 0)
                                                    {
                                                        if (atoi(minute) == 0)
                                                            sprintf(vecTemp, "%s minute", minute);
                                                        else
                                                            sprintf(vecTemp, "-%s minute", minute);
                                                        xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                        gara = gara->next;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 6)
            {
                if (xmlStrcmp(trenIdXML, (const xmlChar *)idTren) == 0)
                {
                    for (gara = tren->children; gara; gara = gara->next)
                    {
                        if (gara->type == XML_ELEMENT_NODE && strcmp((char *)gara->name, "gara") == 0)
                        {
                            xmlChar *intarziereSosire, *maiDevremeSosire, *statusSosire;
                            xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                            if (numeGara != NULL && xmlStrcmp(numeGara, (const xmlChar *)numeGaraParametru) == 0)
                            {
                                for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                                {
                                    if (informatiiGara->type == XML_ELEMENT_NODE)
                                    {
                                        char vecTemp[32] = {0};
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"status_sosire") == 0)
                                        {
                                            statusSosire = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(statusSosire, (const xmlChar *)"Inceput ruta") != 0)
                                            {
                                                if (atoi(minute) == 0)
                                                    sprintf(vecTemp, "%s", "Fara intarziere");
                                                else
                                                    sprintf(vecTemp, "%s", "Cu intarziere");
                                                amGasitInformatii = 1;
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                            }
                                        }
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"intarziere_sosire") == 0)
                                        {
                                            intarziereSosire = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(intarziereSosire, (const xmlChar *)"Inceput ruta") != 0)
                                            {
                                                sprintf(vecTemp, "%s minute", minute);
                                                amGasitInformatii = 1;
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                            }
                                        }
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"mai_devreme_sosire") == 0 && amGasitInformatii == 1)
                                        {
                                            maiDevremeSosire = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(maiDevremeSosire, (const xmlChar *)"Inceput ruta") != 0)
                                            {
                                                if (atoi(minute) == 0)
                                                    sprintf(vecTemp, "%s minute", minute);
                                                else
                                                    sprintf(vecTemp, "-%s minute", minute);
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 7)
            {
                if (xmlStrcmp(trenIdXML, (const xmlChar *)idTren) == 0)
                {
                    for (gara = tren->children; gara; gara = gara->next)
                    {
                        if (gara->type == XML_ELEMENT_NODE && strcmp((char *)gara->name, "gara") == 0)
                        {
                            xmlChar *intarzierePlecare, *maiDevremePlecare, *statusPlecare;
                            xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                            if (numeGara != NULL && xmlStrcmp(numeGara, (const xmlChar *)numeGaraParametru) == 0)
                            {
                                for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                                {
                                    if (informatiiGara->type == XML_ELEMENT_NODE)
                                    {
                                        char vecTemp[32] = {0};
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"status_plecare") == 0)
                                        {
                                            statusPlecare = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(statusPlecare, (const xmlChar *)"Sfarsit ruta") != 0)
                                            {
                                                if (atoi(minute) == 0)
                                                    sprintf(vecTemp, "%s", "Fara intarziere");
                                                else
                                                    sprintf(vecTemp, "%s", "Mai devreme");
                                                amGasitInformatii = 1;
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                            }
                                        }
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"intarziere_plecare") == 0)
                                        {
                                            intarzierePlecare = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(intarzierePlecare, (const xmlChar *)"Sfarsit ruta") != 0)
                                            {
                                                if (atoi(minute) == 0)
                                                    sprintf(vecTemp, "%s minute", minute);
                                                else
                                                    sprintf(vecTemp, "-%s minute", minute);
                                                amGasitInformatii = 1;
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                            }
                                        }
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"mai_devreme_plecare") == 0 && amGasitInformatii == 1)
                                        {
                                            maiDevremePlecare = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(maiDevremePlecare, (const xmlChar *)"Sfarsit ruta") != 0)
                                            {
                                                sprintf(vecTemp, "%s minute", minute);
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                break;
                                            }
                                        }
                                    }
                                }
                                if (amGasitInformatii == 1)
                                {
                                    xmlChar *intarziereSosire, *maiDevremeSosire, *statusSosire;
                                    char vecTemp[32] = {0};
                                    gara = gara->next;
                                    while (gara)
                                    {
                                        for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                                        {
                                            if (informatiiGara->type == XML_ELEMENT_NODE)
                                            {
                                                if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"status_sosire") == 0)
                                                {
                                                    statusSosire = xmlNodeGetContent(informatiiGara);
                                                    if (xmlStrcmp(statusSosire, (const xmlChar *)"Inceput ruta") != 0)
                                                    {
                                                        if (atoi(minute) == 0)
                                                            sprintf(vecTemp, "%s", "Fara intarziere");
                                                        else
                                                            sprintf(vecTemp, "%s", "Mai devreme");
                                                        amGasitInformatii = 1;
                                                        xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                    }
                                                }
                                                if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"intarziere_sosire") == 0)
                                                {
                                                    intarziereSosire = xmlNodeGetContent(informatiiGara);
                                                    if (xmlStrcmp(intarziereSosire, (const xmlChar *)"Inceput ruta") != 0)
                                                    {
                                                        if (atoi(minute) == 0)
                                                            sprintf(vecTemp, "%s minute", minute);
                                                        else
                                                            sprintf(vecTemp, "-%s minute", minute);
                                                        xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                    }
                                                }
                                                if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"mai_devreme_sosire") == 0)
                                                {
                                                    maiDevremeSosire = xmlNodeGetContent(informatiiGara);
                                                    if (xmlStrcmp(maiDevremePlecare, (const xmlChar *)"Inceput ruta") != 0)
                                                    {
                                                        sprintf(vecTemp, "%s minute", minute);
                                                        xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                        gara = gara->next;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 8)
            {
                if (xmlStrcmp(trenIdXML, (const xmlChar *)idTren) == 0)
                {
                    for (gara = tren->children; gara; gara = gara->next)
                    {
                        if (gara->type == XML_ELEMENT_NODE && strcmp((char *)gara->name, "gara") == 0)
                        {
                            xmlChar *intarziereSosire, *maiDevremeSosire, *statusSosire;
                            xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                            if (numeGara != NULL && xmlStrcmp(numeGara, (const xmlChar *)numeGaraParametru) == 0)
                            {
                                for (informatiiGara = gara->children; informatiiGara; informatiiGara = informatiiGara->next)
                                {
                                    if (informatiiGara->type == XML_ELEMENT_NODE)
                                    {
                                        char vecTemp[32] = {0};
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"status_sosire") == 0)
                                        {
                                            statusSosire = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(statusSosire, (const xmlChar *)"Inceput ruta") != 0)
                                            {
                                                if (atoi(minute) == 0)
                                                    sprintf(vecTemp, "%s", "Fara intarziere");
                                                else
                                                    sprintf(vecTemp, "%s", "Mai devreme");
                                                amGasitInformatii = 1;
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                            }
                                        }
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"intarziere_sosire") == 0)
                                        {
                                            intarziereSosire = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(intarziereSosire, (const xmlChar *)"Inceput ruta") != 0)
                                            {
                                                if (atoi(minute) == 0)
                                                    sprintf(vecTemp, "%s minute", minute);
                                                else
                                                    sprintf(vecTemp, "-%s minute", minute);
                                                amGasitInformatii = 1;
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                            }
                                        }
                                        if (xmlStrcmp(informatiiGara->name, (const xmlChar *)"mai_devreme_sosire") == 0 && amGasitInformatii == 1)
                                        {
                                            maiDevremeSosire = xmlNodeGetContent(informatiiGara);
                                            if (xmlStrcmp(maiDevremeSosire, (const xmlChar *)"Inceput ruta") != 0)
                                            {
                                                sprintf(vecTemp, "%s minute", minute);
                                                xmlNodeSetContent(informatiiGara, (const xmlChar *)vecTemp);
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (nrFunctie == 11)
            {
                int amGasitPlecare = 0;
                int amGasitSosire = 0;
                for (gara = tren->children; gara; gara = gara->next)
                {
                    if (gara->type == XML_ELEMENT_NODE && xmlStrcmp(gara->name, (const xmlChar *)"gara") == 0)
                    {
                        xmlChar *numeGara = xmlGetProp(gara, (const xmlChar *)"nume");
                        if (strcmp((char *)numeGara, idTren) == 0 && amGasitPlecare == 0)
                        {
                            amGasitPlecare = 1;
                        }
                        else if (strcmp((char *)numeGara, numeGaraParametru) == 0 && amGasitPlecare == 1)
                        {
                            amGasitSosire = 1;
                        }
                    }
                }

                if (amGasitPlecare == 1 && amGasitSosire == 1)
                {
                    amGasitInformatii = 1;
                    strcat(informatiiTren, (char *)trenIdXML);
                    strcat(informatiiTren, "\n");
                }
            }
        }
    }
    if (nrFunctie == 0)
    {
        lseek(descriptorFisier, 0, SEEK_SET);
        xmlSaveFormatFile("MersulTrenurilor.xml", fisierXML, 1);
    }
    if (nrFunctie == 1)
        informatiiTren[strlen(informatiiTren)] = '\0';
    if (nrFunctie == 2 || nrFunctie == 12)
    {
        if (amGasitInformatii == 1)
            informatiiTren[strlen(informatiiTren)] = '\0';
        else
        {
            strcpy(informatiiTren, "Trenul cu ID-ul introdus nu a fost găsit.\n");
            informatiiTren[strlen(informatiiTren)] = '\0';
        }
    }
    if (nrFunctie == 3 || nrFunctie == 4 || nrFunctie == 9 || nrFunctie == 10 || nrFunctie == 11)
    {
        if (amGasitInformatii == 1)
            informatiiTren[strlen(informatiiTren)] = '\0';
        else
        {
            strcpy(informatiiTren, "Numele garii introdus nu a fost gasit SAU nu exista informatii.\n");
            informatiiTren[strlen(informatiiTren)] = '\0';
        }
    }
    if (nrFunctie == 5 || nrFunctie == 6 || nrFunctie == 7 || nrFunctie == 8)
    {
        lseek(descriptorFisier, 0, SEEK_SET);
        xmlSaveFormatFile("MersulTrenurilor.xml", fisierXML, 1);
        if (amGasitInformatii == 1)
        {
            strcpy(informatiiTren, "Modificare dorita a avut loc cu succes.\n");
            informatiiTren[strlen(informatiiTren)] = '\0';
        }
        else
        {
            strcpy(informatiiTren, "Id-ul sau numele garii a fost introdus gresit.\n");
            informatiiTren[strlen(informatiiTren)] = '\0';
        }
    }
    xmlFreeDoc(fisierXML);
    deblocheazaFisierul(descriptorFisier);
    close(descriptorFisier);
}

void blocheazaFisierul(int descriptorFisiser)
{
    struct flock blocare;
    memset(&blocare, 0, sizeof(blocare));
    blocare.l_type = F_WRLCK; // Blocare exclusiva - scriere/citire
    fcntl(descriptorFisiser, F_SETLKW, &blocare);
}

void deblocheazaFisierul(int descriptorFisiser)
{
    struct flock deblocare;
    memset(&deblocare, 0, sizeof(deblocare));
    deblocare.l_type = F_UNLCK; // Deblocare
    fcntl(descriptorFisiser, F_SETLK, &deblocare);
}

void adaugaPreferintaInFisier(char *nume, char *idTrenFisier)
{
    FILE *descriptorFisierTxt;

    int verific = 0;
    char linie[128] = {0};
    char combinatieNumeId[128] = {0};
    sprintf(combinatieNumeId, "%s %s\n", nume, idTrenFisier);
    descriptorFisierTxt = fopen("PreferinteTrenuri.txt", "r");
    int descriptorINT = fileno(descriptorFisierTxt);
    blocheazaFisierul(descriptorINT);
    while (fgets(linie, sizeof(linie), descriptorFisierTxt) != NULL)
    {
        if (strcmp(combinatieNumeId, linie) == 0)
        {
            verific = 1;
            break;
        }
    }
    deblocheazaFisierul(descriptorINT);
    fclose(descriptorFisierTxt);
    if (verific == 1)
        return;

    descriptorFisierTxt = fopen("PreferinteTrenuri.txt", "a");
    descriptorINT = fileno(descriptorFisierTxt);
    blocheazaFisierul(descriptorINT);
    if (descriptorFisierTxt == NULL)
    {
        perror("Eroare la deschiderea fisierului");
        return;
    }
    fprintf(descriptorFisierTxt, "%s %s\n", nume, idTrenFisier);
    deblocheazaFisierul(descriptorINT);
    fclose(descriptorFisierTxt);
}

void stergePreferintaDinFisier(char *nume)
{
    FILE *descriptorFisierTxt = fopen("PreferinteTrenuri.txt", "r");
    int descriptorINT = fileno(descriptorFisierTxt);
    blocheazaFisierul(descriptorINT);
    char linie[128] = {0};
    FILE *tempDescriptorFisierTxt = fopen("PreferinteTrenuri2.txt", "a");

    while (fgets(linie, sizeof(linie), descriptorFisierTxt) != NULL)
    {
        if (strncmp(linie, nume, strlen(nume)) != 0)
        {
            fprintf(tempDescriptorFisierTxt, "%s", linie);
        }
    }
    deblocheazaFisierul(descriptorINT);
    fclose(descriptorFisierTxt);
    fclose(tempDescriptorFisierTxt);
    remove("PreferinteTrenuri.txt");
    rename("PreferinteTrenuri2.txt", "PreferinteTrenuri.txt");
}

void adaugaMoficariXml(char *idTren, char *gara, char *minute, int nrFunctie)
{
    char vecTemporar[256] = {0};
    if (nrFunctie == 1)
        sprintf(vecTemporar, "Trenul cu id-ul %s are intarziere la plecare din gara %s de %s minute", idTren, gara, minute);
    else if (nrFunctie == 2)
        sprintf(vecTemporar, "Trenul cu id-ul %s are intarziere la sosire in gara %s de %s minute", idTren, gara, minute);
    else if (nrFunctie == 3)
        sprintf(vecTemporar, "Trenul cu id-ul %s pleaca mai devreme din gara %s cu %s minute", idTren, gara, minute);
    else if (nrFunctie == 4)
        sprintf(vecTemporar, "Trenul cu id-ul %s soseste mai devreme in gara %s cu %s minute", idTren, gara, minute);

    FILE *descriptorFisierTxt = fopen("ModificariXML.txt", "a");
    int descriptorINT = fileno(descriptorFisierTxt);
    blocheazaFisierul(descriptorINT);
    if (descriptorFisierTxt == NULL)
    {
        perror("Eroare la deschiderea fisierului");
        return;
    }
    fprintf(descriptorFisierTxt, "%s\n", vecTemporar);
    deblocheazaFisierul(descriptorINT);
    fclose(descriptorFisierTxt);
}

int dePeCeLinieCitesc()
{
    FILE *descriptorFisierTxt;

    int contor = 1;
    char linie[128] = {0};
    descriptorFisierTxt = fopen("ModificariXML.txt", "r");
    int descriptorINT = fileno(descriptorFisierTxt);
    blocheazaFisierul(descriptorINT);
    while (fgets(linie, sizeof(linie), descriptorFisierTxt) != NULL)
    {
        contor++;
    }
    deblocheazaFisierul(descriptorINT);
    fclose(descriptorFisierTxt);
    return contor;
}

void mesajNotificare(char *nume, int *linieModificariXML, char *informatiiNotificari)
{
    char numeIdTrenPreferat[101][64] = {0};
    int contor_numeIdTrenPreferat = 0;
    FILE *descriptorFisierTxt;

    char linie[128] = {0};
    descriptorFisierTxt = fopen("PreferinteTrenuri.txt", "r");
    int descriptorINT = fileno(descriptorFisierTxt);
    blocheazaFisierul(descriptorINT);
    while (fgets(linie, sizeof(linie), descriptorFisierTxt) != NULL)
    {
        if (strstr(linie, nume))
        {
            strcpy(numeIdTrenPreferat[contor_numeIdTrenPreferat], linie);
            if (numeIdTrenPreferat[contor_numeIdTrenPreferat][strlen(numeIdTrenPreferat[contor_numeIdTrenPreferat]) - 1] == '\n')
                numeIdTrenPreferat[contor_numeIdTrenPreferat][strlen(numeIdTrenPreferat[contor_numeIdTrenPreferat]) - 1] = '\0';
            contor_numeIdTrenPreferat++;
        }
    }
    deblocheazaFisierul(descriptorINT);
    fclose(descriptorFisierTxt);

    descriptorFisierTxt = fopen("ModificariXML.txt", "r");
    descriptorINT = fileno(descriptorFisierTxt);
    blocheazaFisierul(descriptorINT);
    informatiiNotificari[0] = '\0';
    int i = 0, linieContor = 1, amGasitInformatii = 0;
    while (fgets(linie, sizeof(linie), descriptorFisierTxt) != NULL)
    {
        if (linieContor < *linieModificariXML)
        {
            /// nu facem nimic
        }
        else
        {
            for (i = 0; i < contor_numeIdTrenPreferat; i++)
            {
                if (strstr(linie, &numeIdTrenPreferat[i][strlen(nume) + 1]))
                {
                    amGasitInformatii = 1;
                    strcat(informatiiNotificari, linie);
                }
            }
        }
        linieContor++;
    }
    *linieModificariXML = linieContor;
    if (amGasitInformatii == 0)
        strcpy(informatiiNotificari, "Nu sunt notificari momentan!\n");
    informatiiNotificari[strlen(informatiiNotificari)] = '\0';
    deblocheazaFisierul(descriptorINT);
    fclose(descriptorFisierTxt);

    //
}

void gestioneazaSemnal(int semnal)
{
    if (semnal == SIGUSR1)
        primitSemnal = 1;
}