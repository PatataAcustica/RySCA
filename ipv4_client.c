#include "arp.h"
#include "eth.h"
#include "ipv4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>


#define DEFAULT_PAYLOAD_LEN 200

 
//int ipv4_send (ipv4_layer_t * layer, ipv4_addr_t dst, uint8_t protocol,unsigned char * payload, int DEFAULT_PAYLOAD_LEN)

int main(int argc, char* argv[]){

	if(argc!=3){
		printf("ERROR\n");
		printf("					USO\n");
		printf("	ipv4_client.c <IP_destino> <protocolo>");
        return 1;
	}
    ipv4_layer_t* layer;
    layer = ipv4_open("ipv4_config_client.txt","ipv4_route_table_client.txt"); //Esto se puede psaasar por parametro
    if(layer ==NULL){
        fprintf(stderr, "ERROR en ipv4_open()");
    }
    
    ipv4_addr_t dst;
    memset(&dst, 0, sizeof(ipv4_addr_t));
    ipv4_str_addr(argv[1], dst);
    uint8_t protocol = atoi(argv[2]);
    
    

    /* Generar payload */
    unsigned char payload[DEFAULT_PAYLOAD_LEN];
    int i;
    for (i=0; i<DEFAULT_PAYLOAD_LEN; i++) {
        payload[i] = (unsigned char) i;
    }  

    printf("Enviando %d bytes al Servidor IP:\n", DEFAULT_PAYLOAD_LEN);
    print_pkt(payload, DEFAULT_PAYLOAD_LEN, 0);

    //int ipv4_send (ipv4_layer_t * layer, ipv4_addr_t dst, uint8_t protocol,unsigned char * payload, int DEFAULT_PAYLOAD_LEN)
    int err = ipv4_send(layer,dst,protocol, payload, DEFAULT_PAYLOAD_LEN); 
    if (err == -1) {
        fprintf(stderr, "ERROR en ipv4_send()\n");
        exit(-1);
    }   
//int ipv4_recv(ipv4_layer_t * layer, uint8_t protocol,unsigned char buffer [], ipv4_addr_t sender, int buf_len,long int timeout)
    unsigned char buffer[IP_MTU]; //
    ipv4_addr_t sender;
    long int timeout = 2000;
    int len= ipv4_recv(layer,(uint8_t)TYPE_IP,buffer,sender,IP_MTU,timeout);
    if (len == -1) {
        fprintf(stderr, " ERROR en ipv4_recv()\n");
    } else if (len == 0) {
        fprintf(stderr, " ERROR: No hay respuesta del Servidor IPv4\n");
    }
    if(len>0){
        char src_ip_str[IPv4_STR_MAX_LENGTH];
        ipv4_addr_str( sender, src_ip_str ); 
        printf("Recibidos %d bytes del Servidor IP (%s)\n", len, src_ip_str);
        print_pkt(buffer, len, 0);
    }

    printf("Cerrando interfaz IP.\n");

    ipv4_close(layer);
    return 0;
    
    
    
}
