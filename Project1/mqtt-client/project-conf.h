/*
 * Copyright (c) 2012, Texas Instruments Incorporated - http://www.ti.com/
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
 *
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
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
#define RPL_CONF_WITH_OF0     1
#define RPL_CONF_WITH_MRHOF   0
#define RPL_CONF_OF_OCP  RPL_OCP_MRHOF
#define RPL_CONF_MOP RPL_MOP_NON_STORING
#define RPL_CONF_WITH_MC 0
#define RPL_CONF_SUPPORTED_OFS { &rpl_mrhof }

//#define RPL_CONF_WITH_MRHOF 0
//#define RPL_CONF_WITH_OF0 1
//#define RPL_CONF_OF_OCP RPL_OCP_OF0
//#define RPL_CONF_MOP RPL_MOP_NON_STORING
//#define RPL_CONF_WITH_MC 0
//#define RPL_CONF_SUPPORTED_OFS { &rpl_of0 }

/* Enable TCP */
#define UIP_CONF_TCP 1

/* ND6/RPL Neighbor Cache Configuration */
#define UIP_CONF_ND6_REACHABLE_TIME 5000  /* 5 seconds */

/* Retransmission timer */
#define UIP_CONF_ND6_RETRANS_TIMER 500     

/* RPL-specific timings */
#define RPL_CONF_PROBING_INTERVAL 25
#define RPL_CONF_DIO_INTERVAL_MIN 12        
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS 8

/* Maximum number of neighbor solicitations */
#define UIP_CONF_ND6_MAX_MULTICAST_SOLICIT 3
#define UIP_CONF_ND6_MAX_UNICAST_SOLICIT 3

#define RPL_CONF_DEFAULT_LIFETIME_UNIT   30
#define RPL_CONF_DEFAULT_LIFETIME        10

#define LINK_STATS_CONF_WITH_TIME 1

#define DEFAULT_DEF_RT_PING_INTERVAL (5 * CLOCK_SECOND)

/* Change to 1 to use with the IBM Watson IoT platform */
#define MQTT_CLIENT_CONF_WITH_IBM_WATSON 0

/*
 * The IPv6 address of the MQTT broker to connect to.
 * Ignored if MQTT_CLIENT_CONF_WITH_IBM_WATSON is 1
 */
#define MQTT_CLIENT_CONF_BROKER_IP_ADDR "fd00::1"
//#define MQTT_CLIENT_CONF_BROKER_IP_ADDR "2001:b07:a96:4775:9d7:f8a3:a131:478"
#define MQTT_BROKER_PORT 1883

/*
 * The Organisation ID.
 *
 * When in Watson mode, the example will default to Org ID "quickstart" and
 * will connect using non-authenticated mode. If you want to use registered
 * devices, set your Org ID here and then make sure you set the correct token
 * through MQTT_CLIENT_CONF_AUTH_TOKEN.
 */
#ifndef MQTT_CLIENT_CONF_ORG_ID
#define MQTT_CLIENT_CONF_ORG_ID "quickstart"
#endif


/* Logging Configuration */
//#define LOG_CONF_LEVEL_RPL LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_MAIN LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_MQTT_CLIENT LOG_LEVEL_DBG

/*
 * The MQTT username.
 *
 * Ignored in Watson mode: In this mode the username is always "use-token-auth"
 */
#define MQTT_CLIENT_CONF_USERNAME "mqtt-client-username"

/*
 * The MQTT auth token (password) used when connecting to the MQTT broker.
 *
 * Used with as well as without Watson.
 *
 * Transported in cleartext!
 */
#define MQTT_CLIENT_CONF_AUTH_TOKEN "AUTHTOKEN"
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
/** @} */