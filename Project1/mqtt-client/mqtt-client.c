/*
 * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "mqtt-prop.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "net/ipv6/uiplib.h"
#include "net/routing/rpl-lite/rpl-conf.h"
#include "net/routing/rpl-lite/rpl.h"
#include "net/routing/rpl-lite/rpl-neighbor.h"
#include "net/routing/rpl-lite/rpl-const.h"
#include "net/nbr-table.h"
#include "net/link-stats.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "os/sys/log.h"
#include "sys/node-id.h"
#include "simple-udp.h"
#include "mqtt-client.h"
#ifdef CONTIKI_TARGET_COOJA
#include "dev/moteid.h"
#endif

#include <string.h>
#include <strings.h>
#include <stdarg.h>
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "mqtt-client"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif
/*---------------------------------------------------------------------------*/
/* Controls whether the example will work in IBM Watson IoT platform mode */
#ifdef MQTT_CLIENT_CONF_WITH_IBM_WATSON
#define MQTT_CLIENT_WITH_IBM_WATSON MQTT_CLIENT_CONF_WITH_IBM_WATSON
#else
#define MQTT_CLIENT_WITH_IBM_WATSON 0
#endif
/*---------------------------------------------------------------------------*/
/* MQTT broker address. Ignored in Watson mode */
#ifdef MQTT_CLIENT_CONF_BROKER_IP_ADDR
#define MQTT_CLIENT_BROKER_IP_ADDR MQTT_CLIENT_CONF_BROKER_IP_ADDR
#else
#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"
#endif
/*---------------------------------------------------------------------------*/
/*
 * MQTT Org ID.
 *
 * If it equals "quickstart", the client will connect without authentication.
 * In all other cases, the client will connect with authentication mode.
 *
 * In Watson mode, the username will be "use-token-auth". In non-Watson mode
 * the username will be MQTT_CLIENT_USERNAME.
 *
 * In all cases, the password will be MQTT_CLIENT_AUTH_TOKEN.
 */
#ifdef MQTT_CLIENT_CONF_ORG_ID
#define MQTT_CLIENT_ORG_ID MQTT_CLIENT_CONF_ORG_ID
#else
#define MQTT_CLIENT_ORG_ID "quickstart"
#endif
/*---------------------------------------------------------------------------*/
/* MQTT token */
#ifdef MQTT_CLIENT_CONF_AUTH_TOKEN
#define MQTT_CLIENT_AUTH_TOKEN MQTT_CLIENT_CONF_AUTH_TOKEN
#else
#define MQTT_CLIENT_AUTH_TOKEN "AUTHTOKEN"
#endif
/*---------------------------------------------------------------------------*/
#if MQTT_CLIENT_WITH_IBM_WATSON
/* With IBM Watson support */
static const char *broker_ip = "0064:ff9b:0000:0000:0000:0000:b8ac:7cbd";
#define MQTT_CLIENT_USERNAME "use-token-auth"

#else /* MQTT_CLIENT_WITH_IBM_WATSON */
/* Without IBM Watson support. To be used with other brokers, e.g. Mosquitto */
static const char *broker_ip = MQTT_CLIENT_BROKER_IP_ADDR;

#ifdef MQTT_CLIENT_CONF_USERNAME
#define MQTT_CLIENT_USERNAME MQTT_CLIENT_CONF_USERNAME
#else
#define MQTT_CLIENT_USERNAME "use-token-auth"
#endif

#endif /* MQTT_CLIENT_WITH_IBM_WATSON */
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_STATUS_LED
#define MQTT_CLIENT_STATUS_LED MQTT_CLIENT_CONF_STATUS_LED
#else
#define MQTT_CLIENT_STATUS_LED LEDS_GREEN
#endif
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_WITH_EXTENSIONS
#define MQTT_CLIENT_WITH_EXTENSIONS MQTT_CLIENT_CONF_WITH_EXTENSIONS
#else
#define MQTT_CLIENT_WITH_EXTENSIONS 0
#endif
/*---------------------------------------------------------------------------*/
/*
 * A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect)
 */
#define STATE_MACHINE_PERIODIC (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION (CLOCK_SECOND >> 2)

/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER 0xFF
#define RECONNECT_INTERVAL (CLOCK_SECOND * 2)

/*---------------------------------------------------------------------------*/
/*
 * Number of times to try reconnecting to the broker.
 * Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS RETRY_FOREVER
#define CONNECTION_STABLE_TIME (CLOCK_SECOND * 5)
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;
#define STATE_INIT 0
#define STATE_REGISTERED 1
#define STATE_CONNECTING 2
#define STATE_CONNECTED 3
#define STATE_PUBLISHING 4
#define STATE_DISCONNECTED 5
#define STATE_NEWCONFIG 6
#define STATE_CONFIG_ERROR 0xFE
#define STATE_ERROR 0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_ORG_ID_LEN 32
#define CONFIG_TYPE_ID_LEN 32
#define CONFIG_AUTH_TOKEN_LEN 32
#define CONFIG_EVENT_TYPE_ID_LEN 32
#define CONFIG_CMD_TYPE_LEN 8
#define CONFIG_IP_ADDR_STR_LEN 64
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
/* Default configuration values */
#define DEFAULT_TYPE_ID "mqtt-client"
#define DEFAULT_EVENT_TYPE_ID "status"
#define DEFAULT_SUBSCRIBE_CMD_TYPE "+"
#define DEFAULT_BROKER_PORT 1883
#define DEFAULT_PUBLISH_INTERVAL (30 * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER 60
#define DEFAULT_RSSI_MEAS_INTERVAL (CLOCK_SECOND * 30)
/*---------------------------------------------------------------------------*/
/*  Link-local neighbor verification (LL-NV)  */
#define LLNV_PROBE_PORT 61616
#define LLNV_MAX_RECENT 16
#define LLNV_RECENT_AGE (60 * CLOCK_SECOND)
#define LLNV_MAX_PROBES_PER_TICK 6
/*---------------------------------------------------------------------------*/
#define MQTT_CLIENT_SENSOR_NONE (void *)0xFFFFFFFF
/*---------------------------------------------------------------------------*/
/* Payload length of ICMPv6 echo requests used to measure RSSI with def rt */
#define ECHO_REQ_PAYLOAD_LEN 20
/*---------------------------------------------------------------------------*/
PROCESS_NAME(mqtt_client_process);
AUTOSTART_PROCESSES(&mqtt_client_process);
/*---------------------------------------------------------------------------*/
/**
 * \brief Data structure declaration for the MQTT client configuration
 */
typedef struct mqtt_client_config
{
  char org_id[CONFIG_ORG_ID_LEN];
  char type_id[CONFIG_TYPE_ID_LEN];
  char auth_token[CONFIG_AUTH_TOKEN_LEN];
  char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
  char broker_ip[CONFIG_IP_ADDR_STR_LEN];
  char cmd_type[CONFIG_CMD_TYPE_LEN];
  clock_time_t pub_interval;
  int def_rt_ping_interval;
  uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE 32
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 *
 * d:quickstart:status:EUI64 is 32 bytes long
 * iot-2/evt/status/fmt/json is 25 bytes
 * We also need space for the null termination
 */
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 2048
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
#define QUICKSTART "quickstart"
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
/* Parent RSSI functionality */
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static struct etimer echo_request_timer;
static int def_rt_rssi = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;
/*---------------------------------------------------------------------------*/
#if MQTT_CLIENT_WITH_EXTENSIONS
extern const mqtt_client_extension_t *mqtt_client_extensions[];
extern const uint8_t mqtt_client_extension_count;
#else
static const mqtt_client_extension_t *mqtt_client_extensions[] = {NULL};
static const uint8_t mqtt_client_extension_count = 0;
#endif
/*---------------------------------------------------------------------------*/
/* MQTTv5 */
#if MQTT_5
static uint8_t PUB_TOPIC_ALIAS;

struct mqtt_prop_list *publish_props;

/* Control whether or not to perform authentication (MQTTv5) */
#define MQTT_5_AUTH_EN 0
#if MQTT_5_AUTH_EN
struct mqtt_prop_list *auth_props;
#endif
#endif
/*---------------------------------------------------------------------------*/
PROCESS(mqtt_client_process, "MQTT Client");
/*---------------------------------------------------------------------------*/
static bool
have_connectivity(void)
{
  if (uip_ds6_get_global(ADDR_PREFERRED) == NULL ||
      uip_ds6_defrt_choose() == NULL)
  {
    return false;
  }
  return true;
}
/*---------------------------------------------------------------------------*/
static int
ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
{
  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for (i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2)
  {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if (a == 0 && f >= 0)
    {
      if (f++ == 0)
      {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    }
    else
    {
      if (f > 0)
      {
        f = -1;
      }
      else if (i > 0)
      {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}
/*---------------------------------------------------------------------------*/
static void
echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data,
                   uint16_t datalen)
{
  if (uip_ip6addr_cmp(source, uip_ds6_defrt_choose()))
  {
    def_rt_rssi = (int)uipbuf_get_attr(UIPBUF_ATTR_RSSI);
  }
}
/*---------------------------------------------------------------------------*/
static void
publish_led_off(void *d)
{
  leds_off(MQTT_CLIENT_STATUS_LED);
}
/*---------------------------------------------------------------------------*/
static void
pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
            uint16_t chunk_len)
{
  LOG_DBG("Pub Handler: topic='%s' (len=%u), chunk_len=%u, chunk='%s'\n", topic,
          topic_len, chunk_len, chunk);

  /* If we don't like the length, ignore */
  if (topic_len != 23 || chunk_len != 1)
  {
    LOG_ERR("Incorrect topic or chunk len. Ignored\n");
    return;
  }

  /* If the format != json, ignore */
  if (strncmp(&topic[topic_len - 4], "json", 4) != 0)
  {
    LOG_ERR("Incorrect format\n");
  }

  if (strncmp(&topic[10], "leds", 4) == 0)
  {
    LOG_DBG("Received MQTT SUB\n");
    if (chunk[0] == '1')
    {
      leds_on(LEDS_RED);
    }
    else if (chunk[0] == '0')
    {
      leds_off(LEDS_RED);
    }
    return;
  }
}
/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch (event)
  {
  case MQTT_EVENT_CONNECTED:
  {
    LOG_DBG("Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    break;
  }
  case MQTT_EVENT_DISCONNECTED:
  case MQTT_EVENT_CONNECTION_REFUSED_ERROR:
  {
    LOG_DBG("MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

    state = STATE_DISCONNECTED;
    process_poll(&mqtt_client_process);
    break;
  }
  case MQTT_EVENT_PUBLISH:
  {
    msg_ptr = data;

    /* Implement first_flag in publish message? */
    if (msg_ptr->first_chunk)
    {
      msg_ptr->first_chunk = 0;
      LOG_DBG("Application received publish for topic '%s'. Payload "
              "size is %i bytes.\n",
              msg_ptr->topic, msg_ptr->payload_chunk_length);
    }

    pub_handler(msg_ptr->topic, strlen(msg_ptr->topic),
                msg_ptr->payload_chunk, msg_ptr->payload_chunk_length);
#if MQTT_5
    /* Print any properties received along with the message */
    mqtt_prop_print_input_props(m);
#endif
    break;
  }
  case MQTT_EVENT_PUBACK:
  {
    LOG_DBG("Publishing complete.\n");
    break;
  }
#if MQTT_5_AUTH_EN
  case MQTT_EVENT_AUTH:
  {
    LOG_DBG("Continuing auth.\n");
    struct mqtt_prop_auth_event *auth_event = (struct mqtt_prop_auth_event *)data;
    break;
  }
#endif
  default:
    LOG_DBG("Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
static int
construct_pub_topic(void)
{
  // int len = snprintf(pub_topic, BUFFER_SIZE, "iot-2/evt/%s/fmt/json", conf.event_type_id);

  int len = snprintf(pub_topic, BUFFER_SIZE, "rplstats/json");

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if (len < 0 || len >= BUFFER_SIZE)
  {
    LOG_DBG("Pub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

#if MQTT_5
  PUB_TOPIC_ALIAS = 1;
#endif

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
construct_client_id(void)
{
  int len = snprintf(client_id, BUFFER_SIZE, "d:%s:%s:%02x%02x%02x%02x%02x%02x",
                     conf.org_id, conf.type_id,
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if (len < 0 || len >= BUFFER_SIZE)
  {
    LOG_ERR("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
update_config(void)
{
  if (construct_client_id() == 0)
  {
    /* Fatal error. Client ID larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }
  if (construct_pub_topic() == 0)
  {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  /* Reset the counter */
  seq_nr_value = 0;

  state = STATE_INIT;

  /*
   * Schedule next timer event ASAP
   *
   * If we entered an error state then we won't do anything when it fires.
   *
   * Since the error at this stage is a config error, we will only exit this
   * error state if we get a new config.
   */
  etimer_set(&publish_periodic_timer, 0);

#if MQTT_5
  LIST_STRUCT_INIT(&(conn.will), properties);

  mqtt_props_init();
#endif

  return;
}
/*---------------------------------------------------------------------------*/
static int
init_config()
{
  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));

  memcpy(conf.org_id, MQTT_CLIENT_ORG_ID, strlen(MQTT_CLIENT_ORG_ID));
  memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
  memcpy(conf.auth_token, MQTT_CLIENT_AUTH_TOKEN,
         strlen(MQTT_CLIENT_AUTH_TOKEN));
  memcpy(conf.event_type_id, DEFAULT_EVENT_TYPE_ID,
         strlen(DEFAULT_EVENT_TYPE_ID));
  memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
  conf.def_rt_ping_interval = DEFAULT_RSSI_MEAS_INTERVAL;

  return 1;
}
/*---------------------------------------------------------------------------*/
// Logging helpers
static void lladdr_to_hex(const uip_lladdr_t *ll, char *out, size_t outlen)
{
  if (!out || outlen < 2)
    return;
  if (!ll)
  {
    snprintf(out, outlen, "-");
    return;
  }
  /* uip_lladdr_t.addr is 8 bytes on 802.15.4 */
  char *p = out;
  for (size_t i = 0; i < sizeof(ll->addr) && (p + 2) < (out + outlen); i++)
  {
    p += snprintf(p, (size_t)(out + outlen - p), "%02x", ll->addr[i]);
  }
  *p = '\0';
}

static const char *ds6_state_str(uint8_t s)
{
#if defined(NBR_INCOMPLETE)
  switch (s)
  {
  case NBR_INCOMPLETE:
    return "INCOMPLETE";
  case NBR_REACHABLE:
    return "REACHABLE";
  case NBR_STALE:
    return "STALE";
  case NBR_DELAY:
    return "DELAY";
  case NBR_PROBE:
    return "PROBE";
  default:
    return "?";
  }
#else
  (void)s;
  return "?";
#endif
}

static void log_rpl_table(void)
{
  int count = 0;
  LOG_DBG("---- RPL neighbors ----\n");
  for (rpl_nbr_t *rn = nbr_table_head(rpl_neighbors);
       rn != NULL;
       rn = nbr_table_next(rpl_neighbors, rn))
  {

    const uip_ipaddr_t *ip = rpl_neighbor_get_ipaddr(rn);
    char ipstr[64];
    ipstr[0] = '\0';
    if (ip)
    {
      uiplib_ipaddr_snprint(ipstr, sizeof ipstr, ip);
    }

    /* Link stats (if available) */
    const struct link_stats *ls = rpl_neighbor_get_link_stats(rn);
    int etx = (ls ? ls->etx : -1);
    int rssi = (ls ? ls->rssi : -128);
    bool fresh = false;
    unsigned long age = 0;
#if LINK_STATS_CONF_WITH_TIME
    if (ls)
    {
      fresh = link_stats_is_fresh(ls);
      if (ls->last_tx_time)
      {
        age = (unsigned long)(clock_time() - ls->last_tx_time);
      }
    }
#else
    if (ls)
    {
      fresh = link_stats_is_fresh(ls);
    }
#endif

    uint16_t rank_raw = rn->rank;
    uint16_t dag_rank = DAG_RANK(rn->rank);
    bool is_parent = rpl_neighbor_is_parent(rn);
    bool is_pref = (rn == curr_instance.dag.preferred_parent);
    bool acceptable = rpl_neighbor_is_acceptable_parent(rn);

    LOG_DBG("RPL[%02d] ip=%s rank_raw=%u dag_rank=%u etx=%d rssi=%d fresh=%d age=%lu parent=%d pref=%d acceptable=%d\n",
            ++count, ip ? ipstr : "-", rank_raw, dag_rank, etx, rssi,
            fresh, age, is_parent, is_pref, acceptable);
  }
  if (count == 0)
  {
    LOG_DBG("RPL: (empty)\n");
  }
}

static void log_ds6_table(void)
{
  int count = 0;
  LOG_DBG("---- DS6 neighbors ----\n");
  for (uip_ds6_nbr_t *dn = uip_ds6_nbr_head();
       dn != NULL;
       dn = uip_ds6_nbr_next(dn))
  {

    char ipstr[64];
    ipstr[0] = '\0';
    uiplib_ipaddr_snprint(ipstr, sizeof ipstr, &dn->ipaddr);

    const uip_lladdr_t *ll = uip_ds6_nbr_get_ll(dn);
    char llhex[2 * sizeof(ll ? ll->addr : (uint8_t[8]){0}) + 1];
    lladdr_to_hex(ll, llhex, sizeof llhex);

    /* If you want link-stats here too, map LL -> link_stats */
    int etx = -1, rssi = -128;
    bool fresh = false;
    unsigned long age = 0;
    if (ll)
    {
      linkaddr_t la;
      memcpy(la.u8, ll->addr, sizeof la.u8);
      const struct link_stats *ls = link_stats_from_lladdr(&la);
      if (ls)
      {
        etx = ls->etx;
        rssi = ls->rssi;
        fresh = link_stats_is_fresh(ls);
        if (ls->last_tx_time)
        {
          age = (unsigned long)(clock_time() - ls->last_tx_time);
        }
      }
    }

    LOG_DBG("DS6[%02d] ip=%s ll=%s state=%s isrouter=%d etx=%d rssi=%d fresh=%d age=%lu\n",
            ++count, ipstr, ll ? llhex : "-", ds6_state_str(dn->state),
            dn->isrouter, etx, rssi, fresh, age);
  }
  if (count == 0)
  {
    LOG_DBG("DS6: (empty)\n");
  }
}
/*---------------------------------------------------------------------------*/
/* Neighbor probing */

/* Recent link-local replies: proves one-hop without routing */
typedef struct
{
  uip_ipaddr_t ip;
  clock_time_t last_rx;
  uint8_t in_use;
} llnv_entry_t;

static llnv_entry_t llnv_cache[LLNV_MAX_RECENT];
static struct simple_udp_connection llnv_conn;

// static struct etimer llnv_probe_timer;

static void llnv_cache_init(void)
{
  memset(llnv_cache, 0, sizeof(llnv_cache));
}

static int llnv_equal(const uip_ipaddr_t *a, const uip_ipaddr_t *b)
{
  return uip_ip6addr_cmp(a, b);
}

static llnv_entry_t *llnv_find(const uip_ipaddr_t *ip)
{
  for (int i = 0; i < LLNV_MAX_RECENT; i++)
  {
    if (llnv_cache[i].in_use && llnv_equal(&llnv_cache[i].ip, ip))
      return &llnv_cache[i];
  }
  return NULL;
}

static llnv_entry_t *llnv_alloc(const uip_ipaddr_t *ip)
{
  llnv_entry_t *oldest = NULL;
  for (int i = 0; i < LLNV_MAX_RECENT; i++)
    if (!llnv_cache[i].in_use)
    {
      llnv_cache[i].in_use = 1;
      uip_ipaddr_copy(&llnv_cache[i].ip, ip);
      llnv_cache[i].last_rx = 0;
      return &llnv_cache[i];
    }
  /* evict the stalest */
  for (int i = 0; i < LLNV_MAX_RECENT; i++)
  {
    if (!oldest || llnv_cache[i].last_rx < oldest->last_rx)
      oldest = &llnv_cache[i];
  }
  if (oldest)
  {
    uip_ipaddr_copy(&oldest->ip, ip);
    oldest->last_rx = 0;
    oldest->in_use = 1;
  }
  return oldest;
}

static void llnv_mark_recent(const uip_ipaddr_t *ip)
{
  llnv_entry_t *e = llnv_find(ip);
  if (!e)
    e = llnv_alloc(ip);
  if (e)
    e->last_rx = clock_time();
}

static int llnv_is_recent(const uip_ipaddr_t *ip)
{
  llnv_entry_t *e = llnv_find(ip);
  if (!e)
    return 0;
  return (clock_time() - e->last_rx) <= LLNV_RECENT_AGE;
}

/* Build fe80::/64 + IID from the 802.15.4 8-byte link-layer address.*/
static int make_linklocal_from_ll(const uip_lladdr_t *ll, uip_ipaddr_t *out)
{
  if (!ll || !out)
    return 0;

  /* fe80::/64 */
  memset(out, 0, sizeof(*out));
  out->u8[0] = 0xfe;
  out->u8[1] = 0x80; /* fe80:: */
  /* u8[2..7] already zero (link-local prefix length 64) */

  /* Contiki-NG 802.15.4 lladdr is 8 bytes (EUI-64-style, already IID-ready).
   * Copy to IID (bytes 8..15) and flip U/L bit per RFC 4291 if you need to.
   * Most builds already have a proper IID in ll->addr, so flipping is optional.
   */
  out->u8[8] = ll->addr[0] ^ 0x02; /* flip the U/L bit */
  out->u8[9] = ll->addr[1];
  out->u8[10] = ll->addr[2];
  out->u8[11] = ll->addr[3];
  out->u8[12] = ll->addr[4];
  out->u8[13] = ll->addr[5];
  out->u8[14] = ll->addr[6];
  out->u8[15] = ll->addr[7];
  return 1;
}
// UDP Reciever handler
static void llnv_rx_cb(struct simple_udp_connection *c,
                       const uip_ipaddr_t *sender_addr,
                       uint16_t sender_port,
                       const uip_ipaddr_t *receiver_addr,
                       uint16_t receiver_port,
                       const uint8_t *data, uint16_t datalen)
{
  /* Only consider link-local senders (fe80::/10) */
  if (uip_is_addr_linklocal(sender_addr))
  {
    //LOG_DBG("LLNV: rx from ");
    //LOG_DBG_6ADDR(sender_addr);
    //LOG_DBG_("\n");

    /* mark proof and echo back */
    llnv_mark_recent(sender_addr);
    simple_udp_sendto(&llnv_conn, data, datalen, sender_addr);
    // LOG_DBG("LLNV: echoed to ");
    // LOG_DBG_6ADDR(sender_addr);
    // LOG_DBG_("\n");
  }
}

// UDP Prober
static void llnv_send_probe(const uip_ipaddr_t *ll_ip)
{
  static const char payload[] = "P"; /* tiny */
  if (!ll_ip)
    return;
  if (!uip_is_addr_linklocal(ll_ip))
    return; /* safety */
  simple_udp_sendto(&llnv_conn, payload, sizeof(payload), ll_ip);
  LOG_DBG("LLNV: probe -> ");
  LOG_DBG_6ADDR(ll_ip);
  LOG_DBG_("\n");
}

static void llnv_probe_candidates(void)
{
  int sent = 0;

  /* Pass 1: RPL neighbors (prefer their IP; many are already link-local) */
  for (rpl_nbr_t *rn = nbr_table_head(rpl_neighbors);
       rn && sent < LLNV_MAX_PROBES_PER_TICK;
       rn = nbr_table_next(rpl_neighbors, rn))
  {

    const uip_ipaddr_t *rip = rpl_neighbor_get_ipaddr(rn);
    uip_ipaddr_t ll_ip;
    const uip_ipaddr_t *dst = NULL;

    if (rip && uip_is_addr_linklocal(rip))
    {
      dst = rip; /* already link-local */
    }
    else
    {
      /* try DS6 mapping via lladdr */
      uip_ds6_nbr_t *dn = rip ? uip_ds6_nbr_lookup(rip) : NULL;
      const uip_lladdr_t *ll = dn ? uip_ds6_nbr_get_ll(dn) : NULL;
      if (ll && make_linklocal_from_ll(ll, &ll_ip))
        dst = &ll_ip;
    }

    if (!dst)
      continue; /* no link-local → skip */
    if (llnv_is_recent(dst))
      continue; /* already fresh */

    llnv_send_probe(dst);
    sent++;
  }

  /* Pass 2: DS6 neighbors (emit those not covered above) */
  for (uip_ds6_nbr_t *dn = uip_ds6_nbr_head();
       dn && sent < LLNV_MAX_PROBES_PER_TICK;
       dn = uip_ds6_nbr_next(dn))
  {

    const uip_lladdr_t *ll = uip_ds6_nbr_get_ll(dn);
    uip_ipaddr_t ll_ip;
    if (!ll || !make_linklocal_from_ll(ll, &ll_ip))
      continue;

    /* If it was already probed/fresh through pass 1, skip */
    if (llnv_is_recent(&ll_ip))
      continue;

    llnv_send_probe(&ll_ip);
    sent++;
  }
}

/*---------------------------------------------------------------------------*/
#define DS6_WARMUP_S 20
#define DS6_MAX_AGE (60 * CLOCK_SECOND)

#ifndef LINK_STATS_RSSI_UNKNOWN
#define LINK_STATS_RSSI_UNKNOWN (-100) // Typical sentinel value for unknown RSSI
#endif

static void publish(void)
{
  int len;
  int remaining = APP_BUFFER_SIZE;
  // int i;
  char def_rt_str[64];
  char ipv6_addr_str[64];
  char node_id_str[32];
  char preferred_parent_str[64];
  char dodag_id_str[64];
  uip_ds6_addr_t *global_addr;
  unsigned long timestamp;
  uint16_t rpl_rank;
  uint16_t rpl_dag_rank;
  uint8_t rpl_instance_id;
  uint8_t dodag_version;
#if MQTT_5
  static uint8_t prop_err = 1;
#endif

  seq_nr_value++;
  buf_ptr = app_buffer;

  log_rpl_table();
  log_ds6_table();
  // llnv_probe_candidates();

  /* IPv6 global address */
  global_addr = uip_ds6_get_global(ADDR_PREFERRED);
  if (global_addr != NULL)
  {
    ipaddr_sprintf(ipv6_addr_str, sizeof(ipv6_addr_str), &global_addr->ipaddr);
  }
  else
  {
    strcpy(ipv6_addr_str, "unknown");
  }

  /* Node ID (Cooja mote ID if available) */
#ifdef CONTIKI_TARGET_COOJA
  snprintf(node_id_str, sizeof(node_id_str), "%d", simMoteID);
#else
  snprintf(node_id_str, sizeof(node_id_str), "%u", node_id);
#endif

  /* RPL instance info  */
  rpl_rank = curr_instance.dag.rank;
  rpl_dag_rank = DAG_RANK(curr_instance.dag.rank);
  rpl_instance_id = curr_instance.instance_id;
  dodag_version = curr_instance.dag.version;
  uiplib_ipaddr_snprint(dodag_id_str, sizeof(dodag_id_str), &curr_instance.dag.dag_id);

  /* Preferred parent*/
  strcpy(preferred_parent_str, "null");
  if (curr_instance.dag.preferred_parent != NULL)
  {
    uip_ipaddr_t *parent_addr = rpl_neighbor_get_ipaddr(curr_instance.dag.preferred_parent);
    if (parent_addr != NULL)
    {
      uiplib_ipaddr_snprint(preferred_parent_str, sizeof(preferred_parent_str), parent_addr);
    }
  }

  /* Timestamp (seconds) */
  timestamp = (unsigned long)(clock_time() / CLOCK_SECOND);

  /* ---- JSON prefix ---- */
  len = snprintf(buf_ptr, remaining,
                 "{"
#ifdef CONTIKI_BOARD_STRING
                 "\"Board\":\"" CONTIKI_BOARD_STRING "\","
#endif
                 "\"Seq #\":%d,"
                 "\"Timestamp\":%lu,"
                 "\"Node ID\":\"%s\","
                 "\"IPv6 Address\":\"%s\","
                 "\"RPL Instance ID\":%u,"
                 "\"DODAG ID\":\"%s\","
                 "\"DODAG Version\":%u,"
                 "\"RPL Rank\":%u,"
                 "\"RPL DAG Rank\":%u,"
                 "\"Preferred Parent\":\"%s\"",
                 seq_nr_value, timestamp, node_id_str, ipv6_addr_str,
                 rpl_instance_id, dodag_id_str, dodag_version,
                 rpl_rank, rpl_dag_rank, preferred_parent_str);
  if (len < 0 || len >= remaining)
  {
    LOG_ERR("Buffer too short (hdr)\n");
    return;
  }
  remaining -= len;
  buf_ptr += len;

  /* Default route (string) */
  memset(def_rt_str, 0, sizeof(def_rt_str));
  ipaddr_sprintf(def_rt_str, sizeof(def_rt_str), uip_ds6_defrt_choose());
  len = snprintf(buf_ptr, remaining, ",\"Def Route\":\"%s\"", def_rt_str);
  if (len < 0 || len >= remaining)
  {
    LOG_ERR("Buffer too short (defrt)\n");
    return;
  }
  remaining -= len;
  buf_ptr += len;
  /* =========================
   *   neighbors: RPL ∪ DS6
   * ========================= */

  /* Open neighbors array */
  len = snprintf(buf_ptr, remaining, ",\"neighbors\":[");
  if (len < 0 || len >= remaining)
  {
    LOG_ERR("Buffer too short (neighbors open)\n");
    return;
  }
  remaining -= len;
  buf_ptr += len;

  bool first = true;

  /* ---- Pass 1: RPL neighbors (one-hop proven only); merge DS6 info if present ---- */
  for (rpl_nbr_t *rn = nbr_table_head(rpl_neighbors); rn; rn = nbr_table_next(rpl_neighbors, rn))
  {
    if (remaining < 220)
      break;

    const uip_ipaddr_t *rip = rpl_neighbor_get_ipaddr(rn);
    if (!rip)
      continue;

    /* Find DS6 entry (for lladdr / state / isrouter merge) */
    uip_ds6_nbr_t *dn = uip_ds6_nbr_lookup(rip);
    const uip_lladdr_t *uip_ll = dn ? uip_ds6_nbr_get_ll(dn) : NULL;

    /* Build/choose a link-local IP for one-hop proof */
    uip_ipaddr_t dst_ll_tmp;
    const uip_ipaddr_t *dst_ll = NULL;
    if (uip_is_addr_linklocal(rip))
    {
      dst_ll = rip;
    }
    else if (uip_ll && make_linklocal_from_ll(uip_ll, &dst_ll_tmp))
    {
      dst_ll = &dst_ll_tmp;
    }

    if (!dst_ll)
      continue; /* cannot prove one-hop */

    char ipstr[64];
    uiplib_ipaddr_snprint(ipstr, sizeof ipstr, rip);

    bool recent_ok = (dst_ll && llnv_is_recent(dst_ll));
    bool is_preferred = (rn == curr_instance.dag.preferred_parent);
    bool is_defrouter = false;
    const uip_ipaddr_t *dr = uip_ds6_defrt_choose();
    if (dr)
    {
      /* compare with RPL IP; also compare with link-local we derived */
      if ((rip && uip_ipaddr_cmp(dr, rip)) || (dst_ll && uip_ipaddr_cmp(dr, dst_ll)))
      {
        is_defrouter = true;
      }
    }

    /* final allow/deny */
    bool allow = (recent_ok || is_preferred || is_defrouter);
    if (!allow)
    {
      /* warm it up for next time: probe only if we have a link-local */
      if (dst_ll)
        llnv_send_probe(dst_ll);
      LOG_DBG("Skipping RPL nbr %s, recent %d, preferred %d, defrouter %d\n", ipstr, recent_ok, is_preferred, is_defrouter);
      continue;
    }

    LOG_DBG("Emitting RPL nbr %s, recent: %d, preferred: %d, defrouter: %d\n", ipstr, recent_ok, is_preferred, is_defrouter);

    /* From here down, we *emit* the neighbor */
    if (!first)
    {
      len = snprintf(buf_ptr, remaining, ",");
      if (len < 0 || len >= remaining)
        break;
      remaining -= len;
      buf_ptr += len;
    }
    first = false;

    len = snprintf(buf_ptr, remaining, "{\"addr\":\"%s\",", ipstr);
    if (len < 0 || len >= remaining)
      break;
    remaining -= len;
    buf_ptr += len;

    /* LLADDR (from DS6 if available) */
    if (uip_ll)
    {
      char llbuf[2 * sizeof(uip_ll->addr) + 1];
      for (size_t i = 0; i < sizeof(uip_ll->addr); i++)
        snprintf(&llbuf[2 * i], 3, "%02x", uip_ll->addr[i]);
      len = snprintf(buf_ptr, remaining, "\"lladdr\":\"%s\",", llbuf);
    }
    else
    {
      len = snprintf(buf_ptr, remaining, "\"lladdr\":null,");
    }
    if (len < 0 || len >= remaining)
      break;
    remaining -= len;
    buf_ptr += len;

    /* RPL fields */
    len = snprintf(buf_ptr, remaining,
                   "\"rpl_rank_raw\":%u,\"rpl_dag_rank\":%u,\"rpl_link_metric\":%u,",
                   (unsigned)rn->rank, (unsigned)DAG_RANK(rn->rank), rpl_neighbor_get_link_metric(rn));
    if (len < 0 || len >= remaining)
      break;
    remaining -= len;
    buf_ptr += len;

    len = snprintf(buf_ptr, remaining,
                   "\"is_rpl_parent\":%s,\"is_preferred_parent\":%s}",
                   rpl_neighbor_is_parent(rn) ? "true" : "false",
                   (rn == curr_instance.dag.preferred_parent) ? "true" : "false");
    if (len < 0 || len >= remaining)
      break;
    remaining -= len;
    buf_ptr += len;
  }

  /* ---- Pass 2: DS6 neighbors (one-hop proven only), skip ones already emitted by RPL pass ---- */
  for (uip_ds6_nbr_t *dn = uip_ds6_nbr_head(); dn; dn = uip_ds6_nbr_next(dn))
  {
    if (remaining < 250)
      break;

    /* If RPL has this neighbor, it was already considered in Pass 1 */
    uip_ipaddr_t *gip = &dn->ipaddr;
    rpl_nbr_t *rn = rpl_neighbor_get_from_ipaddr(gip);
    char ipstr[64];
    uiplib_ipaddr_snprint(ipstr, sizeof ipstr, gip);
    if (rn)
    {
      // LOG_DBG("Skipping DS6 nbr %s (in RPL)\n", ipstr);
      continue;
    }

    /* Build link-local from LLADDR for one-hop proof */
    const uip_lladdr_t *ll = uip_ds6_nbr_get_ll(dn);
    uip_ipaddr_t dst_ll;
    if (!ll || !make_linklocal_from_ll(ll, &dst_ll))
      continue;

    /* Only include if recently proven one hop */
    if (!llnv_is_recent(&dst_ll))
    {
      llnv_send_probe(&dst_ll);
      LOG_DBG("Skipping DS6 nbr %s (not recent)\n", ipstr);
      continue;
    }

    LOG_DBG("Emitting DS6 nbr %s\n", ipstr);

    /* Emit DS6-only neighbor */
    if (!first)
    {
      len = snprintf(buf_ptr, remaining, ",");
      if (len < 0 || len >= remaining)
        break;
      remaining -= len;
      buf_ptr += len;
    }
    first = false;

    len = snprintf(buf_ptr, remaining, "{\"addr\":\"%s\",", ipstr);
    if (len < 0 || len >= remaining)
      break;
    remaining -= len;
    buf_ptr += len;

    char llbuf[2 * sizeof(ll->addr) + 1];
    for (size_t i = 0; i < sizeof(ll->addr); i++)
      snprintf(&llbuf[2 * i], 3, "%02x", ll->addr[i]);
    len = snprintf(buf_ptr, remaining, "\"lladdr\":\"%s\",", llbuf);
    if (len < 0 || len >= remaining)
      break;
    remaining -= len;
    buf_ptr += len;
    /* DS6-only: no RPL metrics */
    len = snprintf(buf_ptr, remaining,
                   "\"rpl_rank_raw\":null,\"rpl_dag_rank\":null,\"rpl_link_metric\":null,"
                   "\"is_rpl_parent\":false,\"is_preferred_parent\":false}");
    if (len < 0 || len >= remaining)
      break;
    remaining -= len;
    buf_ptr += len;
  }

  /* Close neighbors array */
  len = snprintf(buf_ptr, remaining, "]");
  if (len < 0 || len >= remaining)
  {
    LOG_ERR("Buffer too short (neighbors close)\n");
    return;
  }
  remaining -= len;
  buf_ptr += len;

  /* Objective function */
  const char *of_name = "Unknown";
  if (curr_instance.of != NULL)
  {
    if (curr_instance.of->ocp == RPL_OCP_OF0)
      of_name = "OF0";
    else if (curr_instance.of->ocp == RPL_OCP_MRHOF)
      of_name = "MRHOF";
  }
  LOG_DBG("Objective function: %s \n", of_name);
  len = snprintf(buf_ptr, remaining, ",\"objective_function\":\"%s\"", of_name);
  if (len < 0 || len >= remaining)
  {
    LOG_ERR("Buffer too short (OF)\n");
    return;
  }
  remaining -= len;
  buf_ptr += len;

  /* ---- Parent info ---- */
  bool is_root = (DAG_RANK(curr_instance.dag.rank) <= 1);
  len = snprintf(buf_ptr, remaining, ",\"is_root\":%s", is_root ? "true" : "false");
  if (len < 0 || len >= remaining)
  {
    LOG_ERR("Buffer too short (is_root)\n");
    return;
  }
  remaining -= len;
  buf_ptr += len;

  /* Close JSON */
  len = snprintf(buf_ptr, remaining, "}");
  if (len < 0 || len >= remaining)
  {
    LOG_ERR("Buffer too short (close)\n");
    return;
  }

  /* ---- Publish ---- */
#if MQTT_5
  if (seq_nr_value == 1)
  {
    mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
                 strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF,
                 PUB_TOPIC_ALIAS, MQTT_TOPIC_ALIAS_OFF, publish_props);
    prop_err = mqtt_prop_register(&publish_props, NULL,
                                  MQTT_FHDR_MSG_TYPE_PUBLISH,
                                  MQTT_VHDR_PROP_TOPIC_ALIAS, PUB_TOPIC_ALIAS);
  }
  else
  {
    mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
                 strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF,
                 PUB_TOPIC_ALIAS, (mqtt_topic_alias_en_t)!prop_err, publish_props);
  }
#else
  mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
               strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
#endif

  LOG_DBG("Publish!\n");
}
/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
  /* Connect to MQTT server */
  mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
               (conf.pub_interval * 3) / CLOCK_SECOND,
#if MQTT_5
               MQTT_CLEAN_SESSION_ON,
               MQTT_PROP_LIST_NONE);
#else
               MQTT_CLEAN_SESSION_ON);
#endif

  state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/
#if MQTT_5_AUTH_EN
static void
send_auth(struct mqtt_prop_auth_event *auth_info, mqtt_auth_type_t auth_type)
{
  mqtt_prop_clear_prop_list(&auth_props);

  if (auth_info->auth_method.length)
  {
    (void)mqtt_prop_register(&auth_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_AUTH,
                             MQTT_VHDR_PROP_AUTH_METHOD,
                             auth_info->auth_method.string);
  }

  if (auth_info->auth_data.len)
  {
    (void)mqtt_prop_register(&auth_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_AUTH,
                             MQTT_VHDR_PROP_AUTH_DATA,
                             auth_info->auth_data.data,
                             auth_info->auth_data.len);
  }

  /* Connect to MQTT server */
  mqtt_auth(&conn, auth_type, auth_props);

  if (state != STATE_CONNECTING)
  {
    LOG_DBG("MQTT reauthenticating\n");
  }
}
#endif
/*---------------------------------------------------------------------------*/
static void
ping_parent(void)
{
  if (have_connectivity())
  {
    uip_icmp6_send(uip_ds6_defrt_choose(), ICMP6_ECHO_REQUEST, 0,
                   ECHO_REQ_PAYLOAD_LEN);
  }
  else
  {
    LOG_WARN("ping_parent() is called while we don't have connectivity\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
state_machine(void)
{
  switch (state)
  {
  case STATE_INIT:
    /* If we have just been configured register MQTT connection */
    mqtt_register(&conn, &mqtt_client_process, client_id, mqtt_event,
                  MAX_TCP_SEGMENT_SIZE);

    /*
     * If we are not using the quickstart service (thus we are an IBM
     * registered device), we need to provide user name and password
     */
    if (strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) != 0)
    {
      if (strlen(conf.auth_token) == 0)
      {
        LOG_ERR("User name set, but empty auth token\n");
        state = STATE_ERROR;
        break;
      }
      else
      {
        mqtt_set_username_password(&conn, MQTT_CLIENT_USERNAME,
                                   conf.auth_token);
      }
    }

    /* _register() will set auto_reconnect. We don't want that. */
    conn.auto_reconnect = 0;
    connect_attempt = 1;

#if MQTT_5
    mqtt_prop_create_list(&publish_props);

    /* this will be sent with every publish packet */
    (void)mqtt_prop_register(&publish_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_PUBLISH,
                             MQTT_VHDR_PROP_USER_PROP,
                             "Contiki", "v4.5+");

    mqtt_prop_print_list(publish_props, MQTT_VHDR_PROP_ANY);
#endif

    state = STATE_REGISTERED;
    LOG_DBG("Init MQTT version %d\n", MQTT_PROTOCOL_VERSION);
    /* Continue */
  case STATE_REGISTERED:
    if (have_connectivity())
    {
      /* Registered and with a public IP. Connect */
      LOG_DBG("Registered. Connect attempt %u\n", connect_attempt);
      ping_parent();
      connect_to_broker();
    }
    else
    {
      leds_on(MQTT_CLIENT_STATUS_LED);
      ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
    }
    etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
    return;
    break;
  case STATE_CONNECTING:
    leds_on(MQTT_CLIENT_STATUS_LED);
    ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
    /* Not connected yet. Wait */
    LOG_DBG("Connecting (%u)\n", connect_attempt);
    break;
  case STATE_CONNECTED:
    /* Don't subscribe unless we are a registered device */
    if (strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) == 0)
    {
      LOG_DBG("Using 'quickstart': Skipping subscribe\n");
      state = STATE_PUBLISHING;
    }
    /* Continue */
  case STATE_PUBLISHING:
    /* If the timer expired, the connection is stable. */
    if (timer_expired(&connection_life))
    {
      /*
       * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
       * attempts if we disconnect after a successful connect
       */
      connect_attempt = 0;
    }

    if (mqtt_ready(&conn) && conn.out_buffer_sent)
    {
      /* Connected. Publish */
      if (state == STATE_CONNECTED)
      {
        state = STATE_PUBLISHING;
      }
      else
      {
        leds_on(MQTT_CLIENT_STATUS_LED);
        ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
        LOG_DBG("Publishing\n");

        publish();
      }
      etimer_set(&publish_periodic_timer, conf.pub_interval);
      /* Return here so we don't end up rescheduling the timer */
      return;
    }
    else
    {
      /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or that
       * simply there is some network delay. In both cases, we refuse to
       * trigger a new message and we wait for TCP to either ACK the entire
       * packet after retries, or to timeout and notify us.
       */
      LOG_DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
              conn.out_queue_full);
    }
    break;
  case STATE_DISCONNECTED:
    LOG_DBG("Disconnected\n");
    if (connect_attempt < RECONNECT_ATTEMPTS ||
        RECONNECT_ATTEMPTS == RETRY_FOREVER)
    {
      /* Disconnect and backoff */
      clock_time_t interval;
#if MQTT_5
      mqtt_disconnect(&conn, MQTT_PROP_LIST_NONE);
#else
      mqtt_disconnect(&conn);
#endif
      connect_attempt++;

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt : RECONNECT_INTERVAL << 3;

      LOG_DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);

      etimer_set(&publish_periodic_timer, interval);

      state = STATE_REGISTERED;
      return;
    }
    else
    {
      /* Max reconnect attempts reached. Enter error state */
      state = STATE_ERROR;
      LOG_DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;
  case STATE_CONFIG_ERROR:
    /* Idle away. The only way out is a new config */
    LOG_ERR("Bad configuration.\n");
    return;
  case STATE_ERROR:
  default:
    leds_on(MQTT_CLIENT_STATUS_LED);
    /*
     * 'default' should never happen.
     *
     * If we enter here it's because of some error. Stop timers. The only thing
     * that can bring us out is a new config event
     */
    LOG_ERR("Default case: State=0x%02x\n", state);
    return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}
/*---------------------------------------------------------------------------*/
static void
init_extensions(void)
{
  int i;

  for (i = 0; i < mqtt_client_extension_count; i++)
  {
    if (mqtt_client_extensions[i]->init)
    {
      mqtt_client_extensions[i]->init();
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_client_process, ev, data)
{

  PROCESS_BEGIN();

  printf("MQTT Client Process\n");

  if (init_config() != 1)
  {
    PROCESS_EXIT();
  }

  init_extensions();

  update_config();

  simple_udp_register(&llnv_conn, LLNV_PROBE_PORT, NULL, LLNV_PROBE_PORT, llnv_rx_cb);
  LOG_INFO("LLNV: simple-udp registered on port %u\n", LLNV_PROBE_PORT);
  llnv_cache_init();

  def_rt_rssi = 0x8000000;
  uip_icmp6_echo_reply_callback_add(&echo_reply_notification,
                                    echo_reply_handler);
  etimer_set(&echo_request_timer, conf.def_rt_ping_interval);

  /* Main loop */
  while (1)
  {

    PROCESS_YIELD();

    if (ev == button_hal_release_event &&
        ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO)
    {
      if (state == STATE_ERROR)
      {
        connect_attempt = 1;
        state = STATE_REGISTERED;
      }
    }

    if ((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
        ev == PROCESS_EVENT_POLL ||
        (ev == button_hal_release_event &&
         ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO))
    {
      state_machine();
    }

    if (ev == PROCESS_EVENT_TIMER && data == &echo_request_timer)
    {
      ping_parent();
      llnv_probe_candidates();
      etimer_set(&echo_request_timer, conf.def_rt_ping_interval);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/