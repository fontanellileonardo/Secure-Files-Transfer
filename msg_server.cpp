#include <iostream>
#include <vector>
#include <unistd.h>

#include "common_util.h"
#include "messageDef.h"
//#include "server_util.h"

#define FRAGM_SIZE 512000 // 512 KiB
#define BLOCK_SIZE 16

static size_t CIPHER_SIZE = ( FRAGM_SIZE / BLOCK_SIZE ) * BLOCK_SIZE;//TODO: forse non tiene conto dell'eventuale padding
// pari al blocco intero

EVP_PKEY* prvkey = NULL;
X509* CA_cert = NULL;
X509_CRL* crl = NULL;
X509_STORE* store = NULL;
X509* server_certificate = NULL;

int connected_user_number = 0;

// da controllare se va bene anche in c++
size_t fsize(FILE* fp){
	fseek(fp, 0, SEEK_END);
	long int clear_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return clear_size;
}

int decryptAndWriteFile(int TCP_socket,  unsigned char* key, unsigned char* iv){
	
	uint64_t ufile_len;
	size_t file_len;
	
	recv(TCP_socket, &ufile_len, sizeof(uint64_t), 0);
	file_len = ntohl(ufile_len);
	std::cout<<"dimensione file:"<< file_len <<std::endl;
	
	EVP_CIPHER_CTX * dctx;
	int dlen = 0;
	int plaintext_len = 0;
	
	//create and initialize context
	dctx = EVP_CIPHER_CTX_new();
	//decrypt init
	EVP_DecryptInit(dctx, EVP_aes_128_cbc(), key, NULL);
	//decrypt update, one call is enough because our message is very short
	
	// string o lasciare unsigned char??
	unsigned char ciphertext[CIPHER_SIZE + BLOCK_SIZE + 1];
	unsigned char plaintext[CIPHER_SIZE + BLOCK_SIZE + 1];
	uint32_t ulen_cipher;
	uint len_cipher;
	unsigned int i;
	int fw;
	FILE *fpp = fopen("icericez.jpg", "w");
	//FILE *fpp = fopen("icericez.jpg", "w");
	std::cout <<"Iterazioni da fare nel for sono:"<< (file_len/FRAGM_SIZE ) << std::endl;
	int ret;
	for(i = 0; i < (file_len/FRAGM_SIZE ); i++) {
		std::cout<<"Iterazione:"<<i<<std::endl;
		ret = recv(TCP_socket, &ulen_cipher, sizeof(uint32_t), MSG_WAITALL);
		std::cout << "Valore di ret nella ricezione della grandezza del chunck: "<< ret << std::endl;
		if(ret != sizeof(uint32_t)) {
			std::cout<<"Errore nella ricezione della lunghezza del chunk" << std::endl;
			exit(1);
		}
		len_cipher = ntohl(ulen_cipher);
		std::cout << "ulen_cipher dopo la recv: " << ulen_cipher << std::endl;
		std::cout << "grandezza chuck tradotta: " << len_cipher << std::endl;
		// DUBBIO: per farlo bene forse qui si dovrebbe allora un buffer in memoria dinamica di dimensione len_cipher
		// Aspetto che sia ricevuto tutto il ciphertext
		ret = recv(TCP_socket, &ciphertext, len_cipher, MSG_WAITALL);
		std::cout << "Valore di ret nella ricezione del cipher: "<< ret << std::endl; 
		if(ret != len_cipher) {
			std::cout<<"Errore nella ricezione del chunk" << std::endl;
			exit(1);
		}
		//std::cout << "Ciphertext received is: " << std::endl;
		//BIO_dump_fp(stdout, (const char * ) ciphertext, len_cipher);
		if(!EVP_DecryptUpdate(dctx, plaintext, &dlen, ciphertext, len_cipher)) {
			//printf("errore nella DecryptUpdate. dlen: %d\n",dlen);
			std::cout<<"errore nella DecryptUpdate. dlen: "<<dlen<<std::endl;
			exit(1);
		}
		// DUBBIO: anche se dlen è 32 plaintext_len è 16... Perchè??
		plaintext_len +=dlen;
		//printf("plaintext_len  :%i dlen: %d\n", plaintext_len,dlen);
		fw = fwrite(plaintext, 1, dlen, fpp);
		//printf("scritti %i bytes \n", fw);
		std::cout<<"plain size is: "<<dlen<<std::endl;
		//printf("plain is: %d\n",dlen);
		//BIO_dump_fp(stdout, (const char * ) plaintext, dlen);
		std::cout<<std::endl;	
  	}
	std::cout<<"Sono fuori dal for"<<std::endl;
	//printf("i alla fine del for: %d\n",i);
	ret = recv(TCP_socket, &ulen_cipher, sizeof(uint32_t), MSG_WAITALL);
	std::cout << "Valore di ret nella ricezione della grandezza del chunck: "<< ret << std::endl; 
	if(ret == -1) {
		std::cout<<"Errore nella ricezione del chunk" << std::endl;
		exit(1);
	}	
	len_cipher = ntohl(ulen_cipher);
	// DUBBIO: qui dovrei allocare dinamicamente un nuovo array ciphertext di lunghezza len_cipher
	ret = recv(TCP_socket, ciphertext, len_cipher, MSG_WAITALL); 	
	std::cout << "Valore di ret nella ricezione del chunck: "<< ret << std::endl; 
	if(ret != len_cipher) {
		std::cout<<"Errore nella ricezione del chunk" << std::endl;
		exit(1);
	}	
	//printf("ciphertext received out the for is: %ld\n", len_cipher);
	//BIO_dump_fp(stdout, (const char * ) ciphertext, len_cipher);

	// ultimo dato ricevuto potrebbe essere o solo padding, o contenente anche del plaintext significativo	
	if (file_len % FRAGM_SIZE != 0) {
		if( !EVP_DecryptUpdate(dctx, plaintext, &dlen, ciphertext, len_cipher)) {
				//printf("errore nella DecryptUpdate. dlen: %d\n",dlen);
				std::cout<<"errore nella DecryptUpdate. dlen: "<<dlen<<std::endl;
				exit(1);
			}
			// DUBBIO: anche se dlen è 32 plaintext_len è 16... Perchè??
			plaintext_len +=dlen;
			//printf("plaintext_len  :%i dlen: %d\n", plaintext_len,dlen);
			fw = fwrite(plaintext, 1, dlen, fpp);
	}	

  	//printf("plain is BEFORE FINAL:\n");
	//BIO_dump_fp(stdout, (const char * ) plaintext, dlen);
  	//decrypt finalize
	std::cout << "byte decriptati nell'ultimo frammento prima della final: "<< dlen << std::endl;
	if( 1 != EVP_DecryptFinal(dctx, (unsigned char*)plaintext, &dlen)) {
		//printf("errore final. dlen è: %d\n",dlen);
		std::cout<<"errore final. dlen è: "<<dlen<<std::endl;
		exit(1);
	}
	std::cout << "byte decriptati con la final: "<< dlen << std::endl;
	plaintext_len += dlen;
	std::cout << "byte decriptati in totatle: "<< plaintext_len << std::endl;
	// qui dovrei controllare che dlen non sia 0 altrimenti è inutile scrivere nel file
	//printf("plain is AFTER FINAL:\n");
	//BIO_dump_fp(stdout, (const char * ) plaintext, dlen);
	if(dlen != 0)
		fw = fwrite(plaintext, 1, dlen, fpp);
	//printf("plain is: %d\n", dlen);
	//BIO_dump_fp(stdout, (const char * ) plaintext, dlen);	
	fclose(fpp);
	//clean context decr
	//printf("print prima\n");
	EVP_CIPHER_CTX_free(dctx);
	//printf("print dopo\n");
	return 0;	
}

void decrypt(int TCP_socket){
	unsigned char *key = (unsigned char*) "0123456789012345";
	unsigned char* iv;
	decryptAndWriteFile(TCP_socket, key, iv);
	//printf("sono fuori dal for\n");
}

std::vector<Session*> clients;

Session *get_client_by_fd(unsigned int fd){
	for(auto i = clients.begin(); i != clients.end(); i++){
		if((*i)->get_fd() == fd)
			return *i;
	}
	return NULL;
}

//pone l'utente in stato offline e chiude la connessione tcp
void quit_client(unsigned int socket, fd_set* master){
	close(socket);
	FD_CLR(socket, master);
	
	// Elimino la struttura corrispondente al client appena disconnesso
	for(auto i = clients.begin(); i != clients.end(); i++){
		if((*i)->get_fd() == socket){
			delete(*i);
			clients.erase(i);
			break;
		}
	}
	
	connected_user_number--;
	
	return;	
}

void terminate(int value){// Distruggo lo store dei certificati
	// Dealloco il certificato del server
	if(server_certificate != NULL)
		free(server_certificate);
	// Dealloco lo store
	if(store != NULL)
		X509_STORE_free(store);
	// Dealloco il crl
	if(crl != NULL)
		free(crl);
	// Dealloco il certificato della CA
	if(CA_cert != NULL)
		free(CA_cert);
	// Dealloco la chiave privata
	if(prvkey != NULL)
		EVP_PKEY_free(prvkey);
	//Distruggo la tabella interna degli algoritmi
	EVP_cleanup();
	std::cout << "Server terminato";
	exit(value);
}

int main(int argc, char *argv[]){
	
	// File descriptor e il "contatore" di socket
	fd_set master;
	fd_set read_fds;
	unsigned int fdmax;
	
	// Strutture per gli indirizzi di server e client
	struct sockaddr_in sv_addr;
	struct sockaddr_in cl_addr;
	
	unsigned int listener; //descrittore del socket principale
	unsigned int newfd; //descrittore del socket con nuovo client
	
	X509_NAME* abc = NULL;
	char* temp_buffer = NULL;
	
	// Controllo che ci siano tutti i parametri necessari
	if(argc != 2){
		std::cout << "Inserire la porta" << std::endl;
		//printf("Inserire la porta\n");
		return 1;
	}
	
	// Controllo che la porta sia valida
	int server_port = atoi(argv[1]);
	if(server_port < 1 || server_port > USHRT_MAX){
		std::cout << "Errore: Porta non valida" << std::endl;
		return 1;
	}
	
	//===== Chiave privata =====
	OpenSSL_add_all_algorithms();
	if(load_private_key(SERVER_PRVKEY, SERVER_PRVKEY_PASSWORD, &prvkey) < 0){
		std::cerr << "Errore durante il caricamento della chiave privata" << std::endl;
		terminate(-1);
	}
	
	//===== Creazione store =====
	
	// Leggo il certificato CA
	if(load_cert(CA_CERTIFICATE_FILENAME, &CA_cert) < 0){
		std::cerr << "Errore durante il caricamento del certificato CA" << std::endl;
		terminate(-2);
	}
	
	// Leggo il CRL
	if(load_crl(CRL_FILENAME, &crl) < 0){
		std::cerr << "Errore durante il caricamento del CRL" << std::endl;
		terminate(-3);
	}
	
	// Creazione dello store dei certificati
	if(create_store(&store, CA_cert, crl) < 0){
		std::cerr << "Errore durante la creazioni dello store" << std::endl;
		terminate(-4);
	}
	
	//===== Certificato server =====
	
	// Leggo il certificato del server
	if(load_cert(SERVER_CERTIFICATE_FILENAME, &server_certificate) < 0){
		std::cerr << "Errore durante il caricamento del certificato del server" << std::endl;
		terminate(-5);
	}
	
	//  Debug
	abc = X509_get_subject_name(server_certificate);
	temp_buffer = X509_NAME_oneline(abc, NULL, 0);
	std::cout << "Certificato server: " << temp_buffer << std::endl;
	delete temp_buffer;
	temp_buffer = NULL;
	free(abc);
	// /Debug
	
	//===== Creazione socket =====
	
	// Reset FDs
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	
	// Creazione del socket principale
	if((listener = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		std::cerr << "Errore nella creazione del socket listener, errore: " << errno << std::endl;
		terminate(-6);
	}
	
	// Specifico di riusare il socket
	const int trueFlag = 1;
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &trueFlag, sizeof(int));
	
	// Creazione dell'indirizzo del server
	memset(&sv_addr, 0, sizeof(sv_addr));
	sv_addr.sin_family = AF_INET;
	sv_addr.sin_port = htons(server_port);
	sv_addr.sin_addr.s_addr = INADDR_ANY;
	
	// Binding
	if(bind(listener, (struct sockaddr*)&sv_addr, sizeof(sv_addr)) < 0){
		std::cerr << "Errore bind listener, errore: " << errno << std::endl;
		terminate(-7);
	}	
	
	// Putting in listen mode
	if(listen(listener, 10) < 0){
		std::cerr << "Errore su settaggio listen, errore: " << errno << std::endl;
		terminate(-8);
	}
	
	// Add "listener" socket to the "master" set and update "socket counter"
	FD_SET(listener, &master);
	fdmax = listener;
	
	std::cout << "Server attivo, in attesa di connessioni." << std::endl;
		
	while(1){
		read_fds = master;
		select(fdmax + 1, &read_fds, NULL, NULL, NULL);
		for(unsigned int i=0; i<=fdmax; i++){
			if(FD_ISSET(i, &read_fds)){
				if(i == listener){// Nuova richiesta di connessione
					socklen_t addrlen = sizeof(cl_addr);
					if((newfd = accept(listener, (struct sockaddr*)&cl_addr, &addrlen)) < 0){
						std::cerr << "Accept non riuscita, errore: " << errno << std::endl;
						continue;
					}
					
					// Add "newfd" socket to the "master" set and update "socket counter"
					FD_SET(newfd, &master);
					if(newfd > fdmax)
						fdmax = newfd;
					
					// Controllo il numero di utenti connessi
					connected_user_number++;
					uint8_t message_type;
					if(connected_user_number > MAX_USER_CONNECTED){
						message_type = MESSAGE_FULL;
						std::cout << "Numero massimo utenti raggiunto" << std::endl;
					}
					else{
						message_type = MESSAGE_NOT_FULL;
						std::cout << "Nuovo utente connesso" << std::endl;
					}
					
					send(newfd, &message_type, sizeof(message_type), 0);
					
					if(connected_user_number > MAX_USER_CONNECTED){
						close(newfd);
						FD_CLR(newfd, &master);
						connected_user_number--;
					}
					else{
						// Aggiungo un nuovo elemento alla struttura contenente le info sui client connessi
						Session *client = new Session(newfd);
						clients.push_back(client);
					}
				}
				else{// Richiesta da client già connesso
					size_t buflen, ciphertextlen;
					char* input_buffer = NULL;
					char* output_buffer = NULL;
					char* plaintext_buffer = NULL;
					char* ciphertext_buffer = NULL;
					uint8_t message_type;
					uint32_t nonce;
					int ret;
					
					X509* client_certificate = NULL;
					EVP_PKEY* client_pubkey = NULL;
					
					// Vengono utilizzati solo nella parte di handshake
					unsigned char* encrypted_key = NULL;
					size_t encrypted_key_len = 0;
					unsigned char* iv = NULL;
					
					// Recupero la struttura che contiene i dati relativi al client che ha inviato il messaggio
					Session *client = get_client_by_fd(i);
					
					// Ricevo comando
					if(recv(i, &message_type, sizeof(message_type), 0) <= 0){
						quit_client(i, &master);
						std::cout << "Client disconnesso senza !quit, verra' messo offline" << std::endl;
						continue; //passo al prossimo fd pronto
					}
					
					fflush(stdout);//TODO: a cosa serve?
					
					switch(message_type){
						case HANDSHAKE_1:
							// Ricevo i dati in ingresso (certificato)
							if(receive_data(i, &input_buffer, &buflen) < 0){
								std::cerr << "Errore durante la ricezione del certificato del client" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Deserializzo il certificato del client appena ricevuto
							// d2i_X509(...) incrementa il puntatore, è necessario conservarne il valore originale per deallocarlo successivamente
							temp_buffer = input_buffer;
							client_certificate = d2i_X509(NULL, (const unsigned char**)&temp_buffer, buflen);
							if(client_certificate == NULL){
								std::cerr << "Errore durante la deserializzazione del certificato del client" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Dealloco il buffer allocato nella funzione receive_data(...)
							delete[] input_buffer;
							input_buffer = NULL;
							
							//  Debug
							abc = X509_get_subject_name(client_certificate);
							temp_buffer = X509_NAME_oneline(abc, NULL, 0);
							std::cout << "Certificato client: " << temp_buffer << std::endl;
							delete temp_buffer;
							temp_buffer = NULL;
							free(abc);
							// /Debug
							
							// Verifico il certificato
							ret = verify_cert(store, client_certificate);
							if(ret < 0){// Errore interno durante la verifica
								quit_client(i, &master);
								continue;
							}
							if(ret == 0){// Certificato non valido
								std::cout << "Certificato del client non valido" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Estraggo la chiave pubblica del client dal certificato
							client_pubkey = X509_get_pubkey(client_certificate);
							if(client_pubkey == NULL){
								std::cerr << "Errore durante l'estrazione della chiave pubblica del client" << std::endl;
								quit_client(i, &master);
								continue;
							}
							else{
								client->set_counterpart_pubkey(client_pubkey);
							}
							
							// Ricevo i dati in ingresso (nonce)
							if(receive_data(i, &input_buffer, &buflen) < 0){
								std::cerr << "Errore durante la ricezione del numero sequenziale del client" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Salvo il numero sequenziale del client
							nonce = *((uint32_t*)input_buffer);
							client->set_counterpart_nonce(ntohl(nonce));
							
							//  Debug
							std::cout << "Numero sequenziale client: " << ntohl(nonce) << std::endl;
							// /Debug
							
							// Dealloco il buffer allocato nella funzione receive_data(...)
							delete[] input_buffer;
							input_buffer = NULL;
							
							//===== PASSO 2 =====
							
							// Serializzo il certificato del server
							size_t cert_size;
							cert_size = i2d_X509(server_certificate, (unsigned char**)&output_buffer);
							if(cert_size < 0){
								std::cerr << "Errore nella serializzazione del certificato" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Invio il certificato
							if(send_data(i, output_buffer, cert_size) < 0){
								std::cerr << "Errore durante l'invio del certificato" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							delete[] output_buffer;
							output_buffer = NULL;
							
							// Genero iv e le chiavi di cifratura e autenticazione
							if(client->initialize(EVP_aes_128_cbc()) < 0){
								std::cerr << "Errore durante la generazione delle chiavi simmetriche" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Cifro le chiavi appena generate con la chiave pubblica del client
							plaintext_buffer = new char[EVP_CIPHER_key_length(EVP_aes_128_cbc()) * 2];
							client->get_key_auth(plaintext_buffer);
							client->get_key_encr(plaintext_buffer + EVP_CIPHER_key_length(EVP_aes_128_cbc()));
							
							// Cifro le chiavi simmetriche
							if(encrypt_asym(plaintext_buffer, (EVP_CIPHER_key_length(EVP_aes_128_cbc()) * 2), client->get_counterpart_pubkey(), EVP_aes_128_cbc(), (unsigned char**)&ciphertext_buffer, &ciphertextlen, &encrypted_key, &encrypted_key_len, &iv) < 0){
								std::cerr << "Errore durante la cifratura delle chiavi simmetriche" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							delete[] plaintext_buffer;
							plaintext_buffer = NULL;
							
							// Inizializzo il buffer per la parte da firmare ({chiavi simmetriche}Kek, {Kek}Ka+, IV, numero_sequenziale)
							buflen = ciphertextlen + encrypted_key_len + EVP_CIPHER_iv_length(EVP_aes_128_cbc()) + sizeof(nonce);
							plaintext_buffer = new char[buflen];
							
							// Copio le chiavi cifrate nel buffer appena creato
							memcpy(plaintext_buffer, ciphertext_buffer, ciphertextlen);
							
							delete[] ciphertext_buffer;
							ciphertext_buffer = NULL;
							
							// Invio le chiavi cifrate al client
							if(send_data(i, plaintext_buffer, ciphertextlen) < 0){
								std::cerr << "Errore durante l'invio delle chiavi simmetriche cifrate" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Copio encrypted_key nel buffer da firmare
							memcpy((plaintext_buffer + ciphertextlen), encrypted_key, encrypted_key_len);
							
							delete[] encrypted_key;
							encrypted_key = NULL;
							
							// Invio encrypted_key al client
							if(send_data(i, (plaintext_buffer + ciphertextlen), encrypted_key_len) < 0){
								std::cerr << "Errore durante l'invio di encrypted_key" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Copio IV nel buffer da firmare
							memcpy((plaintext_buffer + ciphertextlen + encrypted_key_len), iv, EVP_CIPHER_iv_length(EVP_aes_128_cbc()));
							
							delete[] iv;
							iv = NULL;
							
							// Invio IV al client
							if(send_data(i, (plaintext_buffer + ciphertextlen + encrypted_key_len), EVP_CIPHER_iv_length(EVP_aes_128_cbc())) < 0){
								std::cerr << "Errore durante l'invio di IV" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Copio il numero sequenziale del client nel buffer da firmare
							memcpy(plaintext_buffer + ciphertextlen + encrypted_key_len + EVP_CIPHER_iv_length(EVP_aes_128_cbc()), &nonce, sizeof(nonce));
							
							// Invio il numero sequenziale al client
							if(send_data(i, (const char*)&nonce, sizeof(nonce)) < 0){
								std::cerr << "Errore durante l'invio del numero sequenziale del client" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Firmo l'insieme dei dati ({chiavi simmetriche}Kek, {Kek}Ka+, IV, numero_sequenziale)
							if(sign_asym(plaintext_buffer, buflen, prvkey, (unsigned char**)&ciphertext_buffer, &ciphertextlen) < 0){
								std::cerr << "Errore durante la firma digitale" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							delete[] plaintext_buffer;
							plaintext_buffer = NULL;
							
							// Invio la firma di ({chiavi simmetriche}Kek, {Kek}Ka+, IV, numero_sequenziale)
							if(send_data(i, ciphertext_buffer, ciphertextlen) < 0){
								std::cerr << "Errore durante l'invio della firma" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							delete[] ciphertext_buffer;
							ciphertext_buffer = NULL;
							
							// Recupero il numero sequenziale
							nonce = client->get_my_nonce();
							if(nonce == UINT32_MAX){
								std::cerr << "Il numero sequenziale ha raggiunto il limite. Terminazione..." << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							//  Debug
							std::cout << "Numero sequenziale server: " << nonce << std::endl;
							// /Debug
							
							nonce = htonl(nonce);
							
							// Invio il numero sequenziale
							if(send_data(i, (const char*)&nonce, sizeof(nonce)) < 0){
								std::cerr << "Errore durante l'invio del numero sequenziale del server" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							//===== PASSO 3 =====
							
							// Ricevo i dati in ingresso (numero sequenziale del server)
							if(receive_data(i, &input_buffer, &buflen) < 0){
								std::cerr << "Errore durante la ricezione del numero sequenziale del server" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							if(nonce != *((uint32_t*)input_buffer)){
								std::cerr << "Il numero sequenziale del server non corrisponde" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							delete[] input_buffer;
							input_buffer = NULL;
							
							// Ricevo i dati in ingresso (firma del numero sequenziale del server)
							if(receive_data(i, &input_buffer, &buflen) < 0){
								std::cerr << "Errore durante la ricezione della firma del numero sequenziale del server" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							// Verifico la firma
							ret = sign_asym_verify((unsigned char*)&nonce, sizeof(nonce), (unsigned char*)input_buffer, buflen, client->get_counterpart_pubkey());
							if(ret < 0){
								std::cerr << "Errore durante la verifica della firma" << std::endl;
								quit_client(i, &master);
								continue;
							}
							if(ret == 0){
								std::cerr << "Firma non valida" << std::endl;
								quit_client(i, &master);
								continue;
							}
							
							//  Debug
							std::cout << "Scambio chiavi simmetriche con il client eseguito" << std::endl;
							// /Debug
							
							break;
							
						case COMMAND_FILELIST:
							//TODO: implementare funzionalità
							break;
						case COMMAND_DOWNLOAD:
							decrypt(i);
							break;
						case COMMAND_UPLOAD:		
							//TODO: implementare funzionalità	
							break;
						case COMMAND_QUIT:
							quit_client(i, &master);
							break;
						default:
							std::cout<<"Message type: " << message_type << std::endl;
							std::cout<<"errore nella comunicazione con il client"<<std::endl;
							//printf("errore nella comunicazione con il client\n");
							continue;		
					}// switch
				}// else
			}// if
		}// for
	}// while
	
	
	
	terminate(0);
	return 0;
}

//TODO: Si usa a volte return, a volte exit. Sistemare
//TODO: controllare dopo ogni new che i puntatori non siano null

