/*
* Copyright or © or Copr. 2012, Michael Hauspie
*
* Author e-mail: michael.hauspie@lifl.fr
*
* This software is a computer program whose purpose is to design an
* efficient Web server for very-constrained embedded system.
*
* This software is governed by the CeCILL license under French law and
* abiding by the rules of distribution of free software.  You can  use,
* modify and/ or redistribute the software under the terms of the CeCILL
* license as circulated by CEA, CNRS and INRIA at the following URL
* "http://www.cecill.info".
*
* As a counterpart to the access to the source code and  rights to copy,
* modify and redistribute granted by the license, users are provided only
* with a limited warranty  and the software's author,  the holder of the
* economic rights,  and the successive licensors  have only  limited
* liability.
*
* In this respect, the user's attention is drawn to the risks associated
* with loading,  using,  modifying and/or developing or reproducing the
* software by the user in light of its specific status of free software,
* that may mean  that it is complicated to manipulate,  and  that  also
* therefore means  that it is reserved for developers  and  experienced
* professionals having in-depth computer knowledge. Users are therefore
* encouraged to load and test the software's suitability as regards their
* requirements in conditions enabling the security of their systems and/or
* data to be ensured and,  more generally, to use and operate it in the
* same conditions as regards security.
*
* The fact that you are presently reading this means that you have had
* knowledge of the CeCILL license and that you accept its terms.
*/
/*
<generator>
        <handlers init="dhcp_init" doGet="dhcp_get"/>
</generator>
 */

#ifdef DISABLE_GP_IP_HANDLER
	#error "This application can not work with disable_general_purpose_ip_handler"
#endif

#ifdef IPV6
        #error "This application only works for IPV4 yet."
#endif

#if !defined(LINK_LAYER_ADDRESS) || !defined(LINK_LAYER_ADDRESS_SIZE)
        #error "This application only works for target that define their LINK_LAYER_ADDRESS."
#endif

#include "apps/udp/udp.h"


/* DHCP_MESSAGE_TYPES */
#define DHCPDISCOVER 1     
#define DHCPOFFER    2     
#define DHCPREQUEST  3    
#define DHCPDECLINE  4     
#define DHCPACK      5     
#define DHCPNAK      6     
#define DHCPRELEASE  7     
#define DHCPINFORM   8  

/* PORTS */   
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

/* BOOTP Type */
#define BOOTREQUEST 1

/* Options Code */
#define OPTION_SUBNET_MASK 1
#define OPTION_ROUTER 3
#define OPTION_DHCP_REQUESTED_IP 50
#define OPTION_DHCP_LEASE_TIME 51
#define OPTION_DHCP_MESSAGE_TYPE 53
#define OPTION_DHCP_SERVER_IDENTIFIER 54
#define OPTION_END 0xff


/* States names from RFC2131 Fig. 5: State transition diagram for DHCP clients */
typedef enum
{
    DHCP_STATE_INIT,
    DHCP_STATE_SELECTING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
} dhcp_state_t;


struct dhcp_transaction_t
{
    dhcp_state_t state;
    uint32_t xid;
    uint32_t offered_ip;
    uint32_t server_ip;
};

static struct dhcp_transaction_t _current_transaction;

#ifdef IPV6
#else
/* Should be a CONST_VAR to store in EEPROM, but will be then difficult
   to use in udp_request...
*/
static unsigned char _broadcast_ip[4] = {0xff, 0xff, 0xff, 0xff};
#endif

static char dhcp_get(struct args_t *args)
{
    return 0;
}

static uint32_t in_32(void)
{
    uint32_t val;
    val = udp_in() << 24;
    val |= udp_in() << 16;
    val |= udp_in() << 8;
    val |= udp_in();
    return val;
}

static char dhcp_in(struct udp_args_t *udp_args)
{
    uint8_t i;
    uint32_t ciaddr,yiaddr,siaddr;
    uint8_t message_type;

    /* Check current state */
    if (_current_transaction.state != DHCP_STATE_SELECTING &&
	_current_transaction.state != DHCP_STATE_REQUESTING)
	return 0; /* Should not receive a packet when not in those states */

    /* Discard first fields (op,htype,hlen,hops)*/
    in_32();
    /* Get XID and compare it to current */
    if (in_32() != _current_transaction.xid)
	return 0; /* drop packet */
    in_32(); /* discard secs and flags */
    ciaddr = in_32();
    yiaddr = in_32();
    siaddr = in_32();
    in_32(); /* discard giaddr */
    for (i = 0 ; i < 208 ; ++i) /* discard chaddr, sname, file */
	udp_in();
    /* Check magic cookie, drop if bad */
    if (udp_in() != 99) return 0;
    if (udp_in() != 130) return 0;
    if (udp_in() != 83) return 0;
    if (udp_in() != 99) return 0;

    /* Process options */
    while (1)
    {
	uint8_t code, len;
	code = udp_in();
	if (code == OPTION_END)
	    break;
	len = udp_in();
	switch (code)
	{
	    case OPTION_DHCP_MESSAGE_TYPE:
	    {
		message_type = udp_in();
		--len;
		switch (message_type)
		{
		    case DHCPOFFER:
			if (_current_transaction.state != DHCP_STATE_SELECTING)
			    return 0; /* drop if not is corresponding state */
			_current_transaction.offered_ip = yiaddr;
			break;
		    case DHCPACK:
			if (_current_transaction.state != DHCP_STATE_REQUESTING)
			    return 0; /* drop */
		    default:
		    case DHCPNAK:
			return 0; /* Drop not handled packets */
		}
		break;
	    }
	    case OPTION_DHCP_SERVER_IDENTIFIER:
		_current_transaction.server_ip = in_32();
		len -= 4;
		break;
	    default: 
		break;
	}
	for (i = 0 ; i < len ; ++i)
	    udp_in(); /* discard non handled options bytes */
    }
    /* update state */
    if (message_type == DHCPOFFER)
    {
	_current_transaction.state = DHCP_STATE_REQUESTING;
	return 1; /* 1 for requesting a reply generation */
    }
    else if (message_type == DHCPACK)
	_current_transaction.state = DHCP_STATE_BOUND;
    return 0;
}


static void out_32(uint32_t val)
{
    udp_outc(val >> 24);
    udp_outc((val >> 16) & 0xff);
    udp_outc((val >> 8) & 0xff);
    udp_outc(val & 0xff);
}

static void out_16bits(uint32_t val)
{
    udp_outc((val >> 8) & 0xff);
    udp_outc(val & 0xff);
}


static void dhcp_send_common_header(void)
{
    udp_outc(BOOTREQUEST);             /* OP */
    udp_outc(1);                       /* HTYP: Ethernet address */
    udp_outc(LINK_LAYER_ADDRESS_SIZE); /* HLEN: 6 bytes addresses */
    udp_outc(0);                       /* HOPS: set to 0 by client */
    out_32(_current_transaction.xid);          /* XID: transaction ID */
    out_16bits(0);                     /* SECS: seconds elapsed since start of processs. @todo: fill it correctly */
    out_16bits(0);                     /* Flags: set to 0 */
}

static void dhcp_send_common_footer(void)
{
    uint8_t i,llsize = LINK_LAYER_ADDRESS_SIZE > 16 ? 16 : LINK_LAYER_ADDRESS_SIZE;
    for (i = 0 ; i < llsize ; ++i)
    {
	udp_outc(LINK_LAYER_ADDRESS[i]); /* chaddr */
    }
    for (i = 0 ; i < (16 - llsize) ; ++i)
	udp_outc(0); /* chaddr padding */
    for (i = 0 ; i < 64 + 128 ; ++i) /* sname + file */
	udp_outc(0);
    /* Magic cookie (see rfc for details) */
    udp_outc(99);
    udp_outc(130);
    udp_outc(83);
    udp_outc(99);
}

static void dhcp_put_option_a(uint8_t code, uint8_t len, void *val)
{
    uint8_t i;
    udp_outc(code);
    udp_outc(len);
    for (i = 0 ; i < len ; ++i)
	udp_outc(((uint8_t*)val)[i]);
}


static void dhcp_put_option_8(uint8_t code, uint8_t val)
{
    dhcp_put_option_a(code, 1, &val);
}

static void dhcp_put_option_32(uint8_t code, uint32_t val)
{
    val = (val >> 24) | ((val & 0xff) << 24) | ((val & 0xff0000) >> 8) | ((val & 0xff00) << 8);
    dhcp_put_option_a(code, 4, &val);
}


static void dhcp_send_discover(void)
{
    uint8_t i;
    _current_transaction.xid = TIME_MILLIS; /* @todo: check other targets for rand availability. */
    dhcp_send_common_header();
    for (i = 0 ; i < 4 ; ++i)
	out_32(0); /* ciaddr, yiaddr, siaddr, giaddr */
    dhcp_send_common_footer(); /* chaddr, sname, file, magic cookie */
    dhcp_put_option_8(OPTION_DHCP_MESSAGE_TYPE, DHCPDISCOVER);
    udp_outc(OPTION_END);
    _current_transaction.state = DHCP_STATE_SELECTING;
}

static void dhcp_send_request(void)
{
    dhcp_send_common_header();
    /* Offered IP */
    out_32(_current_transaction.offered_ip);
    out_32(0); /* yiaddr */
    out_32(_current_transaction.server_ip); /* siaddr */
    out_32(0); /* giaddr */
    dhcp_send_common_footer();
    dhcp_put_option_8(OPTION_DHCP_MESSAGE_TYPE, DHCPREQUEST);
    dhcp_put_option_32(OPTION_DHCP_REQUESTED_IP, _current_transaction.offered_ip);
    dhcp_put_option_32(OPTION_DHCP_SERVER_IDENTIFIER, _current_transaction.server_ip);
    udp_outc(OPTION_END);
    _current_transaction.state = DHCP_STATE_REQUESTING;
}

static char dhcp_out(struct udp_args_t *udp_args)
{
    switch (_current_transaction.state)
    {
	case DHCP_STATE_INIT:
	    dhcp_send_discover();
	    break;
	case DHCP_STATE_REQUESTING:
	    dhcp_send_request();
	default:
	    break;
    }
    return 0;
}

static char dhcp_init(void)
{
    udp_listen(DHCP_CLIENT_PORT, dhcp_in, dhcp_out);
    /* Send DHCPDISCOVER */
    _current_transaction.state = DHCP_STATE_INIT;
    udp_request_send(_broadcast_ip, DHCP_SERVER_PORT, DHCP_CLIENT_PORT);
    return 1;
}

