
#include "arp.h"
#include "eth.h"
#include "ipv4.h" 
#include "ipv4_config.h" 
#include "ipv4_route_table.h"

#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <rawnet.h>
#include <timerms.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>


struct ipv4_layer {
eth_iface_t * iface;  /*iface=eth_open("eth1"); */
ipv4_addr_t addr; /* 192.168.1.1 */
ipv4_addr_t netmask; /* 255.255.255.0 */
ipv4_route_table_t * routing_table;
};

struct ipv4_frame
{
  uint8_t version_headerLen;
  uint8_t dscp;
  uint16_t total_length;
  uint16_t identification;
  uint16_t flags_fragmentOffset;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  ipv4_addr_t src_ip;
  ipv4_addr_t dst_ip;
  unsigned char payload[IP_MTU] ;
};
//Inicializamos el layer de IP para posteriormente poder utilizarlo en el send y en el recv
ipv4_layer_t* ipv4_open(char * file_conf, char * file_conf_route) {
  
    ipv4_layer_t * layer = malloc(sizeof(ipv4_layer_t));
    
    char ifname[16];
    ipv4_addr_t addr;
    ipv4_addr_t netmask;
    ipv4_config_read( file_conf, ifname , addr,netmask);
    printf("%s\n",ifname);
    /* 2. Leer direcciones y subred de file_conf */

    memcpy(layer->addr, addr, IPv4_ADDR_SIZE);
    memcpy(layer->netmask, netmask, IPv4_ADDR_SIZE);


    //ipv4_route_table_t * routing_table;
    layer->routing_table=ipv4_route_table_create(); //HAY QUE LIBERAR!!!!!!!! ipv4_route_table_free()

    int numRutasLeidas = ipv4_route_table_read(file_conf_route, layer->routing_table);/* 3. Leer tabla de reenvío IP de file_conf_route */
    if(numRutasLeidas == 0){
      printf("No se ha leido ninguna ruta\n");
    }else if(numRutasLeidas ==-1){
      printf("Se ha producido algún error al leer el fichero de rutas.\n");
    }

    //memcpy(layer->routing_table, routing_table, sizeof(ipv4_route_table_t *));

    
    layer->iface=eth_open ( ifname );/* 4. Inicializar capa Ethernet con eth_open() */
    return layer;

}

int ipv4_close (ipv4_layer_t * layer) {

/* 1. Liberar table de rutas (layer -> routing_table) */
  ipv4_route_table_free(layer->routing_table);
/* 2. Cerrar capa Ethernet con eth_close() */
  eth_close ( layer->iface );

free(layer);
return 0;
}


int ipv4_send (ipv4_layer_t * layer, ipv4_addr_t dst, uint8_t protocol,unsigned char * payload, int payload_len) {

  //Metodo para enviar una trama ip
  struct ipv4_frame pkt_ip_send;
  mac_addr_t macdst;
  memset(&macdst, 0, sizeof(mac_addr_t));
  //Variable donde guardaremos la MAC de destino
  memset(&pkt_ip_send, 0, sizeof(struct ipv4_frame)); //Limpiamos la estructura para que no haya basura
  
  //INICIALIZAMOS LA ESTRUCTURA
  pkt_ip_send.version_headerLen = htons(VERSION_HEADERLEN);//Preguntar si tiene sentido poner aqui hton?----------------------------------------------------------------------------------------------
  pkt_ip_send.total_length = htons(HEADER_LEN_IP+payload_len);
  pkt_ip_send.identification = htons(0x2816);
  pkt_ip_send.flags_fragmentOffset = htons(FLAGS_FO); 
  pkt_ip_send.ttl = htons(64); //Hay que ver que numero ponemos de ttl(Puede que sea 64)
  pkt_ip_send.protocol = htons(protocol);
  pkt_ip_send.checksum = ipv4_checksum((unsigned char*) &pkt_ip_send,HEADER_LEN_IP);
  memcpy(pkt_ip_send.src_ip, layer->addr, IPv4_ADDR_SIZE);
  memcpy(pkt_ip_send.dst_ip, dst, IPv4_ADDR_SIZE);
  //pkt_ip_send.payload=payload;//Ahora hacemos el lookup 
  memcpy(pkt_ip_send.payload, payload, payload_len);
  
  ipv4_route_t* route;
  route = ipv4_route_table_lookup(layer->routing_table, dst);
  
  //route es la ruta más rápida encontrada en la tabla de rutas del layer hasta la dirección dst.
  //De no funcionar, devuelve -1
  
  //Si el destino se encuentra en la misma subred que nuestro host, encontramos su MAC y enviamos
  if(memcmp(route->gateway_addr, IPv4_ZERO_ADDR, IPv4_ADDR_SIZE)==0)
  {
    printf("%s", eth_getname(layer->iface));
    int arp = arp_resolve(layer->iface, dst, macdst);  
    //Sacamos la dirección MAC de destino
    if (arp < 0)
      {
          printf("Ha ocurrido un error en el arp_resolve(el destino esta en nuestra subred)\n");
      }
      else if (arp == 0)
      {
          
          printf("Enviamos bien el arp");
      }
    
    
    int a = eth_send(layer->iface, macdst, protocol, (unsigned char*) &pkt_ip_send, payload_len+HEADER_LEN_IP);
    if (a < 0)
      {
          printf("Ha ocurrido un error en eth_send\n");
      }
      else if (a > 0)
      {
          printf("Número de bytes enviados: %d\n", a);
          printf("Esto es lo que enviamos en str: \n %s", (unsigned char *) &pkt_ip_send);
      }
    //No estoy seguro de cómo enviar pkt_ip_send, así solo enviamos el payload
    return 0;
  }
  //Si el destino está fuera de la subred (hay salto), tendremos que sacar la MAC del siguiente salto y enviárselo a él
  else
  {
    int arp = arp_resolve(layer->iface, route->gateway_addr, macdst);
    if (arp < 0)
      {
          printf("Ha ocurrido un error en el arp_resolve(el destino no esta en nuestra subred)\n");
      }
      else if (arp == 0)
      {
          
          printf("Enviamos bien el arp");
      }
    //Sacamos dirección MAC del salto
    int a = eth_send(layer->iface, macdst, protocol, (unsigned char*)&pkt_ip_send, payload_len+HEADER_LEN_IP); 
    if (a < 0)
      {
          printf("Ha ocurrido un error\n");
          return -1;
      }
      else if (a > 0)
      {
          printf("Número de bytes enviados: %d\n", a);
          printf("Esto es lo que enviamos en str: \n %s", (unsigned char *) &pkt_ip_send);
          return 0;
      }
   return 0;
    
  }
}

int ipv4_recv(ipv4_layer_t * layer, uint8_t protocol,unsigned char buffer [], ipv4_addr_t sender, int buf_len,long int timeout) 
{
//Seria necesario poner ntonh?--------------------------------------------------------------------------------------------------------------------------------------------------
//Metodo para recibir una trama ip
struct ipv4_frame pkt_ip_recv;

/* Inicializar temporizador para mantener timeout si se reciben tramas con tipo incorrecto. */
  timerms_t timer;
  timerms_reset(&timer, timeout);
  // OBTENER MAC E IP PROPIA
    mac_addr_t macPropia;
    eth_getaddr(layer->iface, macPropia);
    int isIP;
    int isProtocol;
  do {
    long int time_left = timerms_left(&timer);

    /* Recibir trama del interfaz Ethernet y procesar errores */
    int eth = eth_recv(layer->iface, macPropia ,TYPE_IP, (unsigned char*) &pkt_ip_recv, sizeof(struct ipv4_frame), time_left);
    if (eth == -1)
        {
            printf("Ha ocurrido un error\n");
            return -1;
        }
        else
        {
            printf("Número de bytes recibidos: %d\n", eth);
            printf("IPv4 Recibido: ");
            for (int i = 0; i < sizeof(struct ipv4_frame); i++) {
                printf("%02x ", ((unsigned char *)&pkt_ip_recv)[i]);
            }
            printf("\n");
        }
  //Hacemos las comprobaciones necesarias(Que esta bien) para salir del do while
  
  isIP = (memcmp(layer->addr,pkt_ip_recv.src_ip,IPv4_ADDR_SIZE)==0); //Miramos si la ip que nos pasan por parametro es igual a la que nos llega
  if(pkt_ip_recv.protocol == protocol)//Comprobamos si es el protocolo que nos pasan por parametro
  {
     isProtocol= 0;
  } else 
  { 
    isProtocol = 1;
  }

  }while (!(isIP && isProtocol));

  memcpy(buffer, ipv4_frame->payload, buffer_len);
  //hacer aqui debemos hacer una verificacion del chekcsum(incluyendo la cabecera y los datos), la calculamos de la misma forma que en el send
//y comprobamos q coincide con el campo cheksum del paquete recibido

return 0;
  
}


/* Dirección IPv4 a cero: "0.0.0.0" */
ipv4_addr_t IPv4_ZERO_ADDR = { 0, 0, 0, 0 };


/* void ipv4_addr_str ( ipv4_addr_t addr, char* str );
 *
 * DESCRIPCIÓN:
 *   Esta función genera una cadena de texto que representa la dirección IPv4
 *   indicada.
 *
 * PARÁMETROS:
 *   'addr': La dirección IP que se quiere representar textualente.
 *    'str': Memoria donde se desea almacenar la cadena de texto generada.
 *           Deben reservarse al menos 'IPv4_STR_MAX_LENGTH' bytes.
 */
void ipv4_addr_str ( ipv4_addr_t addr, char* str )
{
  if (str != NULL) {
    sprintf(str, "%d.%d.%d.%d",
            addr[0], addr[1], addr[2], addr[3]);
  }
}


/* int ipv4_str_addr ( char* str, ipv4_addr_t addr );
 *
 * DESCRIPCIÓN:
 *   Esta función analiza una cadena de texto en busca de una dirección IPv4.
 *
 * PARÁMETROS:
 *    'str': La cadena de texto que se desea procesar.
 *   'addr': Memoria donde se almacena la dirección IPv4 encontrada.
 *
 * VALOR DEVUELTO:
 *   Se devuelve 0 si la cadena de texto representaba una dirección IPv4.
 *
 * ERRORES:
 *   La función devuelve -1 si la cadena de texto no representaba una
 *   dirección IPv4.
 */
int ipv4_str_addr ( char* str, ipv4_addr_t addr )
{
  int err = -1;

  if (str != NULL) {
    unsigned int addr_int[IPv4_ADDR_SIZE];
    int len = sscanf(str, "%d.%d.%d.%d", 
                     &addr_int[0], &addr_int[1], 
                     &addr_int[2], &addr_int[3]);

    if (len == IPv4_ADDR_SIZE) {
      int i;
      for (i=0; i<IPv4_ADDR_SIZE; i++) {
        addr[i] = (unsigned char) addr_int[i];
      }
      
      err = 0;
    }
  }
  
  return err;
}


/*
 * uint16_t ipv4_checksum ( unsigned char * data, int len )
 *
 * DESCRIPCIÓN:
 *   Esta función calcula el checksum IP de los datos especificados.
 *
 * PARÁMETROS:
 *   'data': Puntero a los datos sobre los que se calcula el checksum.
 *    'len': Longitud en bytes de los datos.
 *
 * VALOR DEVUELTO:
 *   El valor del checksum calculado.
 */
uint16_t ipv4_checksum ( unsigned char * data, int len )
{
  int i;
  uint16_t word16;
  unsigned int sum = 0;
    
  /* Make 16 bit words out of every two adjacent 8 bit words in the packet
   * and add them up */
  for (i=0; i<len; i=i+2) {
    word16 = ((data[i] << 8) & 0xFF00) + (data[i+1] & 0x00FF);
    sum = sum + (unsigned int) word16;	
  }

  /* Take only 16 bits out of the 32 bit sum and add up the carries */
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  /* One's complement the result */
  sum = ~sum;

  return (uint16_t) sum;
}


