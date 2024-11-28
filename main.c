#include <stdio.h>
#include <string.h>
#include "net/af.h"
#include "net/protnum.h"
#include "net/ipv6/addr.h"
#include "net/sock/udp.h"
#include "dht.h"
#include "xtimer.h"
#include "shell.h"
#include "msg.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/ipv6.h"

#define MAIN_QUEUE_SIZE (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#define DHTPIN 5
#define DHTTYPE DHT11

char buf[128];
dht_t dht;
int16_t temperature, humidity;

void configure_global_ipv6(void) {
    // Interfaz de red
    gnrc_netif_t *netif = gnrc_netif_iter(NULL);

    if (netif == NULL) {
        puts("No network interface found");
        return;
    }

    // Dirección IPv6 global a asignar
    const char *ipv6_addr_str = "2001:db8::feb4:67ff:fef1:3c1d";
    ipv6_addr_t ipv6_addr;
    if (ipv6_addr_from_str(&ipv6_addr, ipv6_addr_str) == NULL) {
        puts("Error: unable to parse IPv6 address");
        return;
    }

    // Asignar dirección IPv6 global a la interfaz
    if (gnrc_netif_ipv6_addr_add(netif, &ipv6_addr, 64, GNRC_NETIF_IPV6_ADDRS_FLAGS_STATE_VALID) < 0) {
        puts("Error: unable to add IPv6 address");
        return;
    }

    puts("IPv6 global address configured");
}

void start_client(void) {
    printf("DHT Sensor Test\n");

    dht_params_t params = {
        .pin = DHTPIN,
        .type = DHTTYPE
    };

    if (dht_init(&dht, &params) != DHT_OK) {
        printf("Falla al iniciar DHT sensor\n");
        return;
    }

    sock_udp_ep_t local = SOCK_IPV6_EP_ANY;
    sock_udp_t sock;

    local.port = 54321; // Puerto del cliente, puede ser cualquier número

    if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
        puts("Error creating UDP sock");
        return;
    }

    while (1) {
        printf("Reading sensor...\n");
        if (dht_read(&dht, &temperature, &humidity) != DHT_OK) {
            printf("Conexion fallida con el DHT\n");
            continue;
        }

        // Convertir temperatura y humedad a valores flotantes
        float temperature_C = temperature / 10.0f;
        float humidity_float = humidity / 10.0f;

        // Crear una cadena con los datos del sensor
        sprintf(buf, "Antamba: Temperatura: %.2f°C, Humedad: %.2f%%", temperature_C, humidity_float);

        // Enviar datos al servidor UDP
        sock_udp_ep_t remote = { .family = AF_INET6 };
        remote.port = 8090; // Puerto del servidor
        // Establece la dirección IPv6 del servidor
        ipv6_addr_from_str((ipv6_addr_t *)&remote.addr.ipv6, "fe80::a2b7:65ff:fe61:b6f5");

        if (sock_udp_send(&sock, buf, strlen(buf), &remote) < 0) {
            puts("Error sending sensor data");
            sock_udp_close(&sock);
            return;
        }

        printf("Sensor data sent: %s\n", buf);

        // Esperar antes de realizar otra lectura
        xtimer_sleep(10);
    }
}

int main(void) {
    /* Configurar la cola de mensajes para recibir paquetes de red rápidamente */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT Grupo 4 - Cliente");

    /* Configurar la red IPv6 global */
    configure_global_ipv6();

    /* Iniciar el cliente UDP */
    start_client();

    /* Iniciar el shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* Esto nunca debería alcanzarse */
    return 0;
}
