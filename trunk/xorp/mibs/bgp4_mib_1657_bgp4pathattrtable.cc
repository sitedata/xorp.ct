// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

// Copyright (c) 2001-2003 International Computer Science Institute
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software")
// to deal in the Software without restriction, subject to the conditions
// listed in the XORP LICENSE file. These conditions include: you must
// preserve this copyright notice, and you cannot mention the copyright
// holders in advertising related to the Software without their permission.
// The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
// notice is a summary of the XORP LICENSE file; the license in that file is
// legally binding.

#include <stack>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/snmp_assert.h>

#include "bgp4_mib_module.h"
#include "xorpevents.hh"
#include "bgp4_mib_1657.hh"
#include "bgp4_mib_1657_bgp4pathattrtable.hh"

#undef USE_DMALLOC

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

// Local classes and typedefs
class UpdateManager
{
public:
    UpdateManager(): status(RESTING) {};

    uint32_t list_token;
    enum Status { RESTING = 0, UPDATING, CLEANING } status;
    stack<netsnmp_index> old_routes;
};

// Local variables
static     netsnmp_handler_registration *my_handler = NULL;
static     netsnmp_table_array_callbacks cb;
static     XorpTimer * pLocalUpdateTimer = NULL;
static     OneoffTimerCallback tcb;
static     UpdateManager update;

SnmpEventLoop& eventloop = SnmpEventLoop::the_instance();

oid bgp4PathAttrTable_oid[] = { bgp4PathAttrTable_TABLE_OID };
size_t bgp4PathAttrTable_oid_len = OID_LENGTH(bgp4PathAttrTable_oid);

// Local prototypes
static void  get_v4_route_list_start_done(const XrlError&, const uint32_t*);
static void get_v4_route_list_next_done(const XrlError& e, const IPv4* peer_id,
    const IPv4Net*, const uint32_t *, const vector<uint8_t>*, const IPv4*,
    const int32_t*, const int32_t*, const int32_t*, const vector<uint8_t>*,
    const int32_t*, const vector<uint8_t>*, const bool* valid);
static void free_old_routes (void *, void *); 
static uint32_t rows_are_equal(bgp4PathAttrTable_context * lr, 
    bgp4PathAttrTable_context * rr);
static void bgp4PathAttrTable_delete_row(bgp4PathAttrTable_context * ctx);
static int bgp4PathAttrTable_extract_index(bgp4PathAttrTable_context * ctx, 
    netsnmp_index * hdr);
static u_char * stl_vector_to_char(const vector<uint8_t>*, unsigned long &);

/************************************************************
 * local_route_table_update - update local table
 *
 * This function will drive the local table update.  The update
 * can be in one of the following states: RESTING, UPDATING or
 * CLEANING.  
 *
 * If this function is called while update is RESTING, it will
 * invoke the route_list_start XRL, which will change the state to
 * UPDATING upon successful execution.
 *
 * If this function is called while UPDATING, it will send the XRL that
 * requests the next route table entry.  As long as new routes are being
 * received, the XRL callback keeps calling local_route_table_update 
 * When the last route is received, the callback will change the update state
 * to CLEANING.
 *
 * If this function is called while CLEANING, it will cycle through the local
 * table, and remove all the routes that were not received in the last update.
 *
 * The reason for doing this two step update is because we cannot afford to
 * build the entire table from scratch:  should a request arrive during the
 * beginning of an update, it is better to send the routes received in the
 * previous update than sending an almost empty table. 
 */
static void local_route_table_update()
{
    BgpMib& bgp_mib = BgpMib::the_instance();


    switch (update.status) {
	case UpdateManager::RESTING:
	{
	    DEBUGMSGTL((BgpMib::the_instance().name(),
		"updating local bgp4PathAttrTable...\n"));
	    DEBUGMSGTL((BgpMib::the_instance().name(),
		"local table size: %d\n", CONTAINER_SIZE(cb.container)));
	    bgp_mib.send_get_v4_route_list_start("bgp",
			     callback(get_v4_route_list_start_done));
	    break;
	}
	case UpdateManager::UPDATING:
	{
	    bgp_mib.send_get_v4_route_list_next("bgp", update.list_token,
					callback(get_v4_route_list_next_done));
	    break;
	}
	case UpdateManager::CLEANING:
	{
	    DEBUGMSGTL((BgpMib::the_instance().name(),
		"removing old routes from bgp4PathAttrTable...\n"));
	    DEBUGMSGTL((BgpMib::the_instance().name(),
		"local table size: %d old_routes stack: %d\n", 
		CONTAINER_SIZE(cb.container), update.old_routes.size()));
	    CONTAINER_FOR_EACH(cb.container, free_old_routes, NULL);
	    while (update.old_routes.size()) {  
		DEBUGMSGTL((BgpMib::the_instance().name(),
		    "update.old_routes.size() = %d\n", 
		    update.old_routes.size()));
		CONTAINER_REMOVE(cb.container,&update.old_routes.top());
		update.old_routes.pop();
	    }
	    update.status = UpdateManager::RESTING;
#ifdef USE_DMALLOC
	    static unsigned long dmalloc_mark_1 = dmalloc_mark();
	    dmalloc_message("dump allocated pointers\n");
	    dmalloc_log_changed(dmalloc_mark_1, 1, 0, 1);
#endif
	    // schedule next update
	    *pLocalUpdateTimer = eventloop.new_oneoff_after_ms (
	    	UPDATE_REST_INTERVAL_ms, tcb);
	    break;
	}
	default:
	    XLOG_UNREACHABLE();
    }
}

/*********************************************************************
 * init_bgp4_mib_1657_bgp4pathattrtable - Initialization of bgp4PathAttrTable 
 */
void
init_bgp4_mib_1657_bgp4pathattrtable(void)
{
    initialize_table_bgp4PathAttrTable();

    // Create the timer that will be used to trigger updates of the local 
    // and schedule the first update
    pLocalUpdateTimer = new XorpTimer;
    tcb = callback(local_route_table_update);
    *pLocalUpdateTimer = eventloop.new_oneoff_after_ms(0, tcb);
}

/*********************************************************************
 * deinit_bgp4_mib_1657_bgp4pathattrtable - cleanup before unloading
 */
void
deinit_bgp4_mib_1657_bgp4pathattrtable(void)
{
    if (pLocalUpdateTimer != NULL) {
	DEBUGMSGTL((BgpMib::the_instance().name(),
	    "unscheduling bgp4PathAttrTable update timer...\n"));
	pLocalUpdateTimer->unschedule();
	delete pLocalUpdateTimer;
	pLocalUpdateTimer = NULL;
    }
#ifdef USE_DMALLOC
    dmalloc_shutdown();
#endif
}

/*********************************************************************
 * initialize_table_bgp4PathAttrTable - initialize table descriptor
 *
 * Initialize the bgp4PathAttrTable table by defining its contents and how it's
 * structured
 */
void
initialize_table_bgp4PathAttrTable(void)
{
    netsnmp_table_registration_info *table_info;

    if(my_handler) {
        snmp_log(LOG_ERR, "initialize_table_bgp4PathAttrTable_handler called again\n");
        return;
    }

    memset(&cb, 0x00, sizeof(cb));

    /** create the table structure itself */
    table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);

    /* if your table is read only, it's easiest to change the
       HANDLER_CAN_RWRITE definition below to HANDLER_CAN_RONLY */
    my_handler = netsnmp_create_handler_registration("bgp4PathAttrTable",
                                             netsnmp_table_array_helper_handler,
                                             bgp4PathAttrTable_oid,
                                             bgp4PathAttrTable_oid_len,
                                             HANDLER_CAN_RWRITE);
            
    if (!my_handler || !table_info) {
        snmp_log(LOG_ERR, "malloc failed in "
                 "initialize_table_bgp4PathAttrTable_handler\n");
        return; /** mallocs failed */
    }

    /** index: bgp4PathAttrIpAddrPrefix */
    netsnmp_table_helper_add_index(table_info, ASN_IPADDRESS);
    /** index: bgp4PathAttrIpAddrPrefixLen */
    netsnmp_table_helper_add_index(table_info, ASN_INTEGER);
    /** index: bgp4PathAttrPeer */
    netsnmp_table_helper_add_index(table_info, ASN_IPADDRESS);

    table_info->min_column = bgp4PathAttrTable_COL_MIN;
    table_info->max_column = bgp4PathAttrTable_COL_MAX;

    /***************************************************
     * registering the table with the master agent
     */
    cb.get_value = bgp4PathAttrTable_get_value;
    cb.container = netsnmp_container_find("bgp4PathAttrTable_primary:"
                                          "bgp4PathAttrTable:"
                                          "table_container");
    DEBUGMSGTL(("initialize_table_bgp4PathAttrTable",
                "Registering table bgp4PathAttrTable "
                "as a table array\n"));
    netsnmp_table_container_register(my_handler, table_info, &cb,
                                     cb.container, 1);
}

/************************************************************
 * bgp4PathAttrTable_get_value
 */
int bgp4PathAttrTable_get_value(
            netsnmp_request_info *request,
            netsnmp_index *item,
            netsnmp_table_request_info *table_info )
{
    netsnmp_variable_list *var = request->requestvb;
    bgp4PathAttrTable_context *context = (bgp4PathAttrTable_context *)item;

    switch(table_info->colnum) {

        case COLUMN_BGP4PATHATTRPEER:
            /** IPADDR = ASN_IPADDRESS */
            snmp_set_var_typed_value(var, ASN_IPADDRESS,
                         (u_char*)&context->bgp4PathAttrPeer,
                         sizeof(context->bgp4PathAttrPeer) );
        break;
    
        case COLUMN_BGP4PATHATTRIPADDRPREFIXLEN:
            /** INTEGER = ASN_INTEGER */
            snmp_set_var_typed_value(var, ASN_INTEGER,
                         (u_char*)&context->bgp4PathAttrIpAddrPrefixLen,
                         sizeof(context->bgp4PathAttrIpAddrPrefixLen) );
        break;
    
        case COLUMN_BGP4PATHATTRIPADDRPREFIX:
            /** IPADDR = ASN_IPADDRESS */
            snmp_set_var_typed_value(var, ASN_IPADDRESS,
                         (u_char*)&context->bgp4PathAttrIpAddrPrefix,
                         sizeof(context->bgp4PathAttrIpAddrPrefix) );
        break;
    
        case COLUMN_BGP4PATHATTRORIGIN:
            /** INTEGER = ASN_INTEGER */
            snmp_set_var_typed_value(var, ASN_INTEGER,
                         (u_char*)&context->bgp4PathAttrOrigin,
                         sizeof(context->bgp4PathAttrOrigin) );
        break;
    
        case COLUMN_BGP4PATHATTRASPATHSEGMENT:
	{
            /** OCTETSTR = ASN_OCTET_STR */
            snmp_set_var_typed_value(var, ASN_OCTET_STR,
			 context->bgp4PathAttrASPathSegment,
                         context->bgp4PathAttrASPathSegmentLen);
	    
        break;
	} 
        case COLUMN_BGP4PATHATTRNEXTHOP:
            /** IPADDR = ASN_IPADDRESS */
            snmp_set_var_typed_value(var, ASN_IPADDRESS,
                         (u_char*)&context->bgp4PathAttrNextHop,
                         sizeof(context->bgp4PathAttrNextHop) );
        break;
    
        case COLUMN_BGP4PATHATTRMULTIEXITDISC:
            /** INTEGER = ASN_INTEGER */
            snmp_set_var_typed_value(var, ASN_INTEGER,
                         (u_char*)&context->bgp4PathAttrMultiExitDisc,
                         sizeof(context->bgp4PathAttrMultiExitDisc) );
        break;
    
        case COLUMN_BGP4PATHATTRLOCALPREF:
            /** INTEGER = ASN_INTEGER */
            snmp_set_var_typed_value(var, ASN_INTEGER,
                         (u_char*)&context->bgp4PathAttrLocalPref,
                         sizeof(context->bgp4PathAttrLocalPref) );
        break;
    
        case COLUMN_BGP4PATHATTRATOMICAGGREGATE:
            /** INTEGER = ASN_INTEGER */
            snmp_set_var_typed_value(var, ASN_INTEGER,
                         (u_char*)&context->bgp4PathAttrAtomicAggregate,
                         sizeof(context->bgp4PathAttrAtomicAggregate) );
        break;
    
        case COLUMN_BGP4PATHATTRAGGREGATORAS:
            /** INTEGER = ASN_INTEGER */
            snmp_set_var_typed_value(var, ASN_INTEGER,
                         (u_char*)&context->bgp4PathAttrAggregatorAS,
                         sizeof(context->bgp4PathAttrAggregatorAS) );
        break;
    
        case COLUMN_BGP4PATHATTRAGGREGATORADDR:
            /** IPADDR = ASN_IPADDRESS */
            snmp_set_var_typed_value(var, ASN_IPADDRESS,
                         (u_char*)&context->bgp4PathAttrAggregatorAddr,
                         sizeof(context->bgp4PathAttrAggregatorAddr) );
        break;
    
        case COLUMN_BGP4PATHATTRCALCLOCALPREF:
            /** INTEGER = ASN_INTEGER */
            snmp_set_var_typed_value(var, ASN_INTEGER,
                         (u_char*)&context->bgp4PathAttrCalcLocalPref,
                         sizeof(context->bgp4PathAttrCalcLocalPref) );
        break;
    
        case COLUMN_BGP4PATHATTRBEST:
            /** INTEGER = ASN_INTEGER */
            snmp_set_var_typed_value(var, ASN_INTEGER,
                         (u_char*)&context->bgp4PathAttrBest,
                         sizeof(context->bgp4PathAttrBest) );
        break;
    
        case COLUMN_BGP4PATHATTRUNKNOWN:
	{
            /** OCTETSTR = ASN_OCTET_STR */
            snmp_set_var_typed_value(var, ASN_OCTET_STR,
			 context->bgp4PathAttrUnknown,
                         context->bgp4PathAttrUnknownLen);
        break;
	} 
    default: /** We shouldn't get here */
        snmp_log(LOG_ERR, "unknown column in "
                 "bgp4PathAttrTable_get_value\n");
        return SNMP_ERR_GENERR;
    }
    return SNMP_ERR_NOERROR;
}

/************************************************************
 * bgp4PathAttrTable_get_by_idx
 */
const bgp4PathAttrTable_context *
bgp4PathAttrTable_get_by_idx(netsnmp_index * hdr)
{
    return (const bgp4PathAttrTable_context *)
        CONTAINER_FIND(cb.container, hdr );
}


/************************************************************
 * bgp4PathAttrTable_create_row - create a row in the local table
 *
 * returns a newly allocated bgp4PathAttrTable_context
 *   structure if the specified indexes are not illegal
 * returns NULL for errors or illegal index values.
 */
bgp4PathAttrTable_context *
bgp4PathAttrTable_create_row( netsnmp_index* hdr)
{
    bgp4PathAttrTable_context * ctx =
        SNMP_MALLOC_TYPEDEF(bgp4PathAttrTable_context);
    if(!ctx)
        return NULL;

    if(bgp4PathAttrTable_extract_index( ctx, hdr )) {
        free(ctx->index.oids);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/************************************************************
 * bgp4PathAttrTable_delete_row - frees a row structure 
 *
 */
static void
bgp4PathAttrTable_delete_row(bgp4PathAttrTable_context * ctx)
{
    if (NULL == ctx) return;
    free(ctx->index.oids);
    if (ctx->bgp4PathAttrASPathSegment) free(ctx->bgp4PathAttrASPathSegment);
    if (ctx->bgp4PathAttrUnknown) free(ctx->bgp4PathAttrUnknown);
    free(ctx);
    ctx = NULL;
}



/****************************************************************************
 * bgp4PathAttrTable_extract_index - extract the row indices 
 *
 * This function extracts the indices from a netsnmp_index structure, and
 * copies them into the corresponding elements in the provided row
 *
 */ 
int
bgp4PathAttrTable_extract_index(bgp4PathAttrTable_context * ctx, 
				netsnmp_index * hdr )
{
    /*
     * temporary local storage for extracting oid index
     */
    netsnmp_variable_list var_bgp4PathAttrIpAddrPrefix;
    netsnmp_variable_list var_bgp4PathAttrIpAddrPrefixLen;
    netsnmp_variable_list var_bgp4PathAttrPeer;
    int err;

    /*
     * copy index, if provided
     */
    if(hdr) {
        netsnmp_assert(ctx->index.oids == NULL);
        if(snmp_clone_mem( (void**)&ctx->index.oids, hdr->oids,
                           hdr->len * sizeof(oid) )) {
            return -1;
        }
        ctx->index.len = hdr->len;
    }

    /**
     * Create variable to hold each component of the index
     */
    memset(&var_bgp4PathAttrIpAddrPrefix, 0x00, 
	   sizeof(var_bgp4PathAttrIpAddrPrefix));
    var_bgp4PathAttrIpAddrPrefix.type = ASN_IPADDRESS;
    var_bgp4PathAttrIpAddrPrefix.next_variable =
	&var_bgp4PathAttrIpAddrPrefixLen;

    memset(&var_bgp4PathAttrIpAddrPrefixLen, 0x00, 
	sizeof(var_bgp4PathAttrIpAddrPrefixLen));
    var_bgp4PathAttrIpAddrPrefixLen.type = ASN_INTEGER;
    var_bgp4PathAttrIpAddrPrefixLen.next_variable = &var_bgp4PathAttrPeer;

    memset( &var_bgp4PathAttrPeer, 0x00, sizeof(var_bgp4PathAttrPeer) );
    var_bgp4PathAttrPeer.type = ASN_IPADDRESS;
    var_bgp4PathAttrPeer.next_variable = NULL;


    /*
     * parse the oid into the individual components
     */
    err = parse_oid_indexes(hdr->oids, hdr->len, 
	&var_bgp4PathAttrIpAddrPrefix);
    if (err == SNMP_ERR_NOERROR) {
       /*
        * copy components into the context structure
        */
	ctx->bgp4PathAttrIpAddrPrefix = 
	    ntohl(*var_bgp4PathAttrIpAddrPrefix.val.integer);
   
	ctx->bgp4PathAttrIpAddrPrefixLen = 
	    *var_bgp4PathAttrIpAddrPrefixLen.val.integer;
   
	ctx->bgp4PathAttrPeer = 
	    ntohl(*var_bgp4PathAttrPeer.val.integer);
    }

    // parsing may have allocated memory. free it.
    // snmp_reset_var_buffers( &var_bgp4PathAttrIpAddrPrefix );
    //  ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ 
    //  ! ! !
    // faulty implementation of previous function.  Inlined and fixed here
    //
    netsnmp_variable_list * var = &var_bgp4PathAttrIpAddrPrefix;

    while (var) {
        if (var->name && (var->name != var->name_loc)) {
            free(var->name);
            var->name = var->name_loc;
            var->name_length = 0;
        }
        if (var->val.string && (var->val.string != var->buf)) {
            free(var->val.string);
            var->val.string = var->buf;
            var->val_len = 0;
        }
        var = var->next_variable;
    }

    return err;
}


/****************************************************************************
 * get_v4_route_list_start_done - XRL completion callback routine 
 */ 
static void
get_v4_route_list_start_done(
    const XrlError& e,
    const uint32_t* token)
{
    if (e == XrlError::OKAY()) {
	update.status = UpdateManager::UPDATING;
	update.list_token = (*token);
	local_route_table_update();
        DEBUGMSGTL((BgpMib::the_instance().name(),
	    "receiving bgp4PathAttrTable... %ud\n", *token));
    } else {
	*pLocalUpdateTimer = eventloop.new_oneoff_after_ms (
	    UPDATE_REST_INTERVAL_ms, tcb);
    }
}

/****************************************************************************
 * get_v4_route_list_next_done - XRL completion callback routine 
 */ 
static void
get_v4_route_list_next_done(const XrlError& e,
                            const IPv4* peer_id,
                            const IPv4Net* net,
                            const uint32_t *best_and_origin,
                            const vector<uint8_t>* aspath,
                            const IPv4* nexthop,
                            const int32_t* med,
                            const int32_t* localpref,
                            const int32_t* atomic_agg,
                            const vector<uint8_t>* aggregator,
                            const int32_t* calc_localpref,
                            const vector<uint8_t>* attr_unknown,
			    const bool* valid)
{
    if (e != XrlError::OKAY() || false == (*valid)) {
	// Done updating the local table.  Time to remove old routes 
        DEBUGMSGTL((BgpMib::the_instance().name(),
	    "received last route of bgp4PathAttrTable... %ud\n",
	    update.list_token));
	update.status = UpdateManager::CLEANING;
	local_route_table_update();
        return;
    }

    // We have received a new row, store it in the local table
    // First, calculate this row's suboid based on the indices
    netsnmp_index index;
    
    // The sub oid for the rows in this table is 
    // bgp4PathAttrIpAddrPrefix.bgp4PathAttrIpAddrPrefixLen.bgp4PathAttrPeer
    // of the following lengths:
    // ASN_IPADDRESS = 4 + ASN_INTEGER = 1 + ASN_IPADDRESS = 4
    const uint32_t ROW_SUBOID_LEN = 9;
    oid row_suboid[ROW_SUBOID_LEN];
    
    index.oids = row_suboid;    
    index.len = ROW_SUBOID_LEN;

    uint32_t raw_ip = ntohl(net->masked_addr().addr());

    // type oid may be 8 or 32 bit long, so we refrain from doing
    // clever optimizations here 

    row_suboid[0] = (oid) (raw_ip >> 24) & 0xFFl;
    row_suboid[1] = (oid) (raw_ip >> 16) & 0xFFl;
    row_suboid[2] = (oid) (raw_ip >>  8) & 0xFFl;
    row_suboid[3] = (oid) (raw_ip)       & 0xFFl;
    row_suboid[4] = (oid) (net->prefix_len());

    raw_ip = ntohl(peer_id->addr());
    row_suboid[5] = (oid) (raw_ip >> 24) & 0xFFl;
    row_suboid[6] = (oid) (raw_ip >> 16) & 0xFFl;
    row_suboid[7] = (oid) (raw_ip >>  8) & 0xFFl;
    row_suboid[8] = (oid) (raw_ip)       & 0xFFl;

    bgp4PathAttrTable_context * row = bgp4PathAttrTable_create_row(&index); 
    
    XLOG_ASSERT(row != NULL);
    
    // Asserting that bgp4PathAttrTable_create_row has correctly initialized
    // the values for the index columns
    XLOG_ASSERT(row->bgp4PathAttrPeer == peer_id->addr());
    XLOG_ASSERT(row->bgp4PathAttrIpAddrPrefixLen == net->prefix_len());
    XLOG_ASSERT(row->bgp4PathAttrIpAddrPrefix == net->masked_addr().addr());


    row->bgp4PathAttrOrigin = (*best_and_origin) & 0xFF;
    row->bgp4PathAttrBest = (*best_and_origin) >> 16;
    row->bgp4PathAttrASPathSegment = stl_vector_to_char(aspath,
	row->bgp4PathAttrASPathSegmentLen); 
    row->bgp4PathAttrNextHop = nexthop->addr();
    row->bgp4PathAttrMultiExitDisc = (*med);
    row->bgp4PathAttrLocalPref = (*localpref);
    row->bgp4PathAttrAtomicAggregate = (*atomic_agg);
    row->bgp4PathAttrAggregatorAS = 0;
    if (aggregator->size()) {
	row->bgp4PathAttrAggregatorAS |= (*aggregator)[4] << 8;
	row->bgp4PathAttrAggregatorAS |= (*aggregator)[5];
    }
    row->bgp4PathAttrAggregatorAddr = 0;
    if (aggregator->size()) {
	row->bgp4PathAttrAggregatorAddr |= (*aggregator)[0] << 24;
	row->bgp4PathAttrAggregatorAddr |= (*aggregator)[1] << 16;
	row->bgp4PathAttrAggregatorAddr |= (*aggregator)[2] << 8;
	row->bgp4PathAttrAggregatorAddr |= (*aggregator)[3];
    }
    row->bgp4PathAttrCalcLocalPref = (*calc_localpref);
    row->bgp4PathAttrUnknown = stl_vector_to_char(attr_unknown,
	row->bgp4PathAttrUnknownLen);
    row->update_signature = update.list_token;

    bgp4PathAttrTable_context * local_row = (bgp4PathAttrTable_context*)
	CONTAINER_FIND(cb.container, &index);

    if (NULL != local_row) {
	if (rows_are_equal(row, local_row)) {
	    local_row->update_signature = update.list_token;
	    bgp4PathAttrTable_delete_row(row);
	} else {
	    bgp4PathAttrTable_delete_row(local_row);
	    CONTAINER_REMOVE(cb.container, &index);
	    CONTAINER_INSERT(cb.container, row);
	    DEBUGMSGTL((BgpMib::the_instance().name(),
		"updating %s route to local table\n", 
		net->masked_addr().str().c_str()));
	}
    } else {
	CONTAINER_INSERT(cb.container, row);
	DEBUGMSGTL((BgpMib::the_instance().name(),
	    "adding %s route to local table\n", 
	    net->masked_addr().str().c_str()));
    }
    

    // Done with this row, request next
    local_route_table_update();
}

/****************************************************************************
 * rows_are_equal - compare two rows excluding the row signature
 *
 * The function returns non-zero  if both rows are equal, 0 otherwise
 * The index and signature fields are not included in the comparison 
 */
static uint32_t
rows_are_equal(bgp4PathAttrTable_context * lr, bgp4PathAttrTable_context * rr)
{
    return ((lr->bgp4PathAttrPeer == rr->bgp4PathAttrPeer) &&
	(lr->bgp4PathAttrIpAddrPrefixLen == rr->bgp4PathAttrIpAddrPrefixLen) &&
	(lr->bgp4PathAttrIpAddrPrefix == rr->bgp4PathAttrIpAddrPrefix) &&
	(lr->bgp4PathAttrOrigin == rr->bgp4PathAttrOrigin) &&
	(!memcmp(lr->bgp4PathAttrASPathSegment, rr->bgp4PathAttrASPathSegment,
		 lr->bgp4PathAttrASPathSegmentLen)) &&
	(lr->bgp4PathAttrNextHop == rr->bgp4PathAttrNextHop) &&
	(lr->bgp4PathAttrMultiExitDisc == rr->bgp4PathAttrMultiExitDisc) &&
	(lr->bgp4PathAttrLocalPref == rr->bgp4PathAttrLocalPref) &&
	(lr->bgp4PathAttrAtomicAggregate == rr->bgp4PathAttrAtomicAggregate) &&
	(lr->bgp4PathAttrAggregatorAS == rr->bgp4PathAttrAggregatorAS) &&
	(lr->bgp4PathAttrAggregatorAddr == rr->bgp4PathAttrAggregatorAddr) &&
	(lr->bgp4PathAttrCalcLocalPref == rr->bgp4PathAttrCalcLocalPref) &&
	(lr->bgp4PathAttrBest == rr->bgp4PathAttrBest) &&
	(!memcmp(lr->bgp4PathAttrUnknown, rr->bgp4PathAttrUnknown,
		 lr->bgp4PathAttrUnknownLen))); 
}

/****************************************************************************
 * free_old_routes - free old routes from the local table
 *
 * This function compares the signature on the row, with the one from the most
 * recent update, and deletes the row if they don't match
 *
 * NOTE: Ideally in this function we would like to free the old route and the
 * entry for it in the container class, but then the function would not be
 * callable from a FOR_EACH loop.  So we free the old route, but we store the
 * container entries for later deletion.
 */ 
static void 
free_old_routes (void * r, void *) 
{
    bgp4PathAttrTable_context* row = 
	static_cast<bgp4PathAttrTable_context*>(r);

    if (row->update_signature != (update.list_token)) {
        DEBUGMSGTL((BgpMib::the_instance().name(),
	    "removing %#010x from table\n", row->bgp4PathAttrIpAddrPrefix));
	update.old_routes.push(row->index); 
	bgp4PathAttrTable_delete_row(row);
    }
}

/****************************************************************************
 * stl_vector_to_char -  transfer vector<uint8_t>* into an array of chars
 *
 * The second parameter returns the vector size. 
 */
static u_char * stl_vector_to_char(const vector<uint8_t>* v,unsigned long& len)
{
    u_char * char_array;
    len = v->size();
    if (!len) return NULL;
    char_array = (u_char *) malloc(len);
    if (NULL == char_array) 
	len = 0;
    else 
	memcpy(char_array, &(*v)[0], len);
    return char_array;
}
