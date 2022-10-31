/*
Program 3
Authors: Eesha Gadhia, Jose Herrera
Class: EECE 446
Files to Submit: peer.c, Makefile
test
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <dirent.h> //for opendir() and readdir()

/*
 * Lookup a host IP address and connect to it using service. Arguments match the first two
 * arguments to getaddrinfo(3).
 *
 * Returns a connected socket descriptor or -1 on error. Caller is responsible for closing
 * the returned socket.
 */
int lookup_and_connect( const char *host, const char *service ); 

int main(int argc, char *argv[]) {

        //initialize arguments
        const char* registry; //argv[1]
        char* port; //argv[2]
        uint32_t peerID; //argv[3]
        
        //check to see if there are any errors in args
        if(argc < 4){//if we have less than 3 arguments passed into our main
                printf("Error: Not enough arguments.\n");
                printf("Usage: ./peer <registry_name> <port_number> <peer_id>\n");
                exit(1);
            
        }
        else if(argc > 4){//if we have more than 3 arguments passed into our main
                printf("Error: Too many arguments.\n");
                printf("Usage: ./peer <registry_name> <port_number> <peer_id>\n");
                exit(1);
        }
        else{//assign arguments to appropriate variables
                registry = argv[1];
                port = argv[2];
                peerID = atoi(argv[3]);
        }

        //initialize socket descriptor
        char s;
        /*Lookup IP + Connect to Server*/
        if ( ( s = lookup_and_connect( registry, port ) ) < 0 ) {
                exit( 1 ); //Account for errors when trying to lookup and connect
        }
        
        /* AT THIS POINT, WE HAVE SUCCESSFULLY CONNECTED TO SERVER*/
        
        char peer_action[100]; //action chosen by peer
        uint32_t NBO_ID; //variable to hold peer id in Network Byte Order
        uint32_t action; 
        int send_request;
        int done = 1; //keeps while loop going until user enters 'EXIT'
        int join_status = 0; //makes sure user can only join registry once per session
        int file_count = 0; //counts the number of files our peer publishes
        uint32_t NBO_count; //Network Byte Order of file count
        char* file_list[1000]; //a list which holds the names of all the files we want to publish
        struct dirent *pdirect; //(struct of direct)
        DIR *pdir; //(ptr to directory - publish) 

        //keeps program going until user enters keyword to exit
        while (done != 0){
                printf("Enter a Command: "); // get input
                scanf("%s", peer_action); // takes in user input and assigns it to variable
                //account for lower/upper case differences between input and action name
                int count = 0;
                for (int i = 0; i < sizeof(peer_action); i++){
                        peer_action[i] = toupper(peer_action[i]);
                        count++;
                }
                peer_action[count+1] = '\0'; //need to add null at end

                //JOIN - if user inputs join AND the peer app hasn't sent a join request yet
                if((memcmp(peer_action, "JOIN", 4) == 0) && join_status != 1){
                        //Join's action = 0
                        action = 0;

                        //first send action part of request (1B) + check for errors
                        send_request = send(s, &action, 1, 0);
                        if  (send_request < 0){
                                perror("Unable to Join: ");
                                exit(1);
                        }

                        NBO_ID = htonl(peerID); //change peerID to Network Byte Order

                        //then send peerID part of request (4B) + check for errors
                        send_request = send(s, &NBO_ID, sizeof(NBO_ID), 0);
                        if  (send_request < 0){
                                perror("Unable to Join: ");
                                exit(1);
                        }

                        join_status = 1; //peer can only join once per session

                }

                //PUBLISH - cannot publish until peer has joined reg and updates registry with peer's files/locations

                else if((memcmp(peer_action, "PUBLISH", 1) == 0) && join_status == 1){
                        //Publish's action = 1
                        action = 1;

                        
                        //if peer wants to publish files, we need to first find location of file in the directory
                        pdir = opendir("SharedFiles"); //open the directory
                        if (pdir == NULL){//if directory cannot be opened, exit action
                                printf("Unable to Open Directory: SharedFiles\n");
                                break;
                        }

                        //once directory is open, loop through until we can no longer read files from directory
                        while((pdirect = readdir(pdir)) != NULL){
                                if(strcmp(pdirect->d_name, ".")){
                                        if(strcmp(pdirect->d_name, "..")){
                                                file_list[file_count] = (char*)malloc(strlen(pdirect->d_name) + 1);
                                                file_list[file_count] = pdirect->d_name;
                                                file_count++;


                                        }
                                }

                        }

                        //once we know how many files are readable in the directory, we can put the file_count in Network Byte Order
                        NBO_count = htonl(file_count);

                        //the publish request sends the action, filecount, and filenames

                        send_request = send(s, &action, 1, 0); //send action first
                        if(send_request < 0){
                                perror("Unable to Publish: ");
                                exit(1);
                        }

                        send_request = send(s, &NBO_count, 4, 0); //send filecount
                        if(send_request < 0){
                                perror("Unable to Publish: ");
                                exit(1);
                        }

                        char* nullvar = '\0';
                        //loop through file directory to send each file to registry
                        for(int i = 0; i < file_count; i++){
                                send_request = send(s, file_list[i], strlen(file_list[i]), 0);
                                send_request = send(s, &nullvar, 1, 0);
                        }


                        //now that we've sent our publish request and file information out successfully, we can close the directory ptr
                        closedir(pdir);

                }

                //SEARCH - sends request to registry to locate another peer that might contain file of interest
                else if((memcmp(peer_action, "SEARCH", 6) == 0) && join_status == 1){
                        //we know the search action results in the user entering the desired file on a new line 
                        //Search action = 2
                        action = 2;
                        
                        //get file name to search
                        printf("Enter File Name: ");
                        char file_name[100];
                        scanf("%s", file_name);

                        send_request = send(s, &action, 1, 0); //send action first
                        if(send_request < 0){
                                perror("Unable to Publish: ");
                                exit(1);
                        }

                        //send file name
                        char* nullvar = '\0';
                        send_request = send(s, &file_name, strlen(file_name), 0);
                        send_request = send(s, &nullvar, 1, 0);

                        //now we need to account for the registry's response-> 10B
                        char buf[10]; //to store the peer id bytes
                        //check to see if there are any errors with receiving-> if not, then we've got our peer id
                        if(recv(s, &buf, sizeof(buf), 0) < 0){
                                perror("Unable to Receive Response from Registry: ");
                                exit(1);
                        }


                        //peer id->need to change back from NBO to HBO and print out
                        uint32_t NBO_IDR; 
                        memcpy(&NBO_IDR, &buf[0], sizeof(buf));
                        uint32_t HBO_ID = ntohl(NBO_IDR);


                        //receive IPv4 address->need to change back from NBO to HBO and print out
                        uint32_t NBO_IPR;
                        memcpy(&NBO_IPR, &buf[sizeof(HBO_ID)], sizeof(NBO_IPR));
                        uint32_t HBO_IP = ntohl(NBO_IPR);

                        //receive peer port number->need to change back from NBO to HBO and print out
                        uint16_t NBO_PORT;
                        memcpy(&NBO_PORT, &buf[8], sizeof(NBO_PORT));
                        uint16_t HBO_PORT = ntohs(NBO_PORT);
                       
                        //check to see if file isn't indexed in registry->if all fields are 0
                        if(HBO_ID == 0 && HBO_IP == 0 && HBO_PORT == 0){
                                printf("File not indexed by registry.\n");
                        }
                        else{
                                char ipstr[INET_ADDRSTRLEN];
                                inet_ntop(AF_INET,&NBO_IPR,ipstr,sizeof(ipstr));
                                printf("File found at:\n");
                                printf("   Peer %u\n", HBO_ID);
                                printf("   %s:%d\n", ipstr, HBO_PORT);
                        }
                        done = 1;

                }

		//FETCH - fetches a file from another peer and saves it locally
		else if(memcmp(peer_action, "FETCH", 4) == 0){
			
		}

                //EXIT - the moment user inputs exit, p2p application is closed and command line stops taking input

                else if(memcmp(peer_action, "EXIT", 4) == 0){
                        //exiting the program means we must close the connection
                        done = 0; //exits the while loop
                }
                else{//accounts for any typos in the command line
                        printf("Invalid Action. Please try again (JOIN, PUBLISH, SEARCH, EXIT)\n");
                }
        }
        close( s ); //close socket descriptor
        return EXIT_SUCCESS;
}

int lookup_and_connect( const char *host, const char *service ) {
        struct addrinfo hints;
        struct addrinfo *rp, *result;
        int s;

        /* Translate host name into peer's IP address */
        memset( &hints, 0, sizeof( hints ) );
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;
        
        if ( ( s = getaddrinfo( host, service, &hints, &result ) ) != 0 ) {
                printf("%s %s\n", host, service );
                fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror( s ) );
                return -1;
        }

        /* Iterate through the address list and try to connect */
        for ( rp = result; rp != NULL; rp = rp->ai_next ) {
                if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
                        continue;
                }

                if ( connect( s, rp->ai_addr, rp->ai_addrlen ) != -1 ) {
                        break;
                }

                close( s );
        }
        if ( rp == NULL ) {
                perror( "stream-talk-client: connect" );
                return -1;
        }
        freeaddrinfo( result );

        return s;
}
 
