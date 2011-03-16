// vim:set sts=4 ts=8:

// Copyright (c) 2001-2011 XORP, Inc and Others
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, Version 2, June
// 1991 as published by the Free Software Foundation. Redistribution
// and/or modification of this program under the terms of any other
// version of the GNU General Public License is not permitted.
// 
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. For more details,
// see the GNU General Public License, Version 2, a copy of which can be
// found in the XORP LICENSE.gpl file.
// 
// XORP Inc, 2953 Bunker Hill Lane, Suite 204, Santa Clara, CA 95054, USA;
// http://xorp.net

// $XORP: xorp/policy/common/filter.hh,v 1.8 2008/10/02 21:58:07 bms Exp $

#ifndef __POLICY_COMMON_FILTER_HH__
#define __POLICY_COMMON_FILTER_HH__



namespace filter {



/**
 * There are three type of filters:
 *
 * IMPORT: deals with import filtering. Incoming routes from other routers and
 * possibly the rib.
 *
 * EXPORT_SOURCEMATCH: a filter which tags routes that need to be
 * redistributed. This filter only modifies policytags.
 *
 * EXPORT: Filters outgoing routes from the routing protocols to other routers
 * and possibly the rib itself.
 */
enum Filter {
    IMPORT =		    1,
    EXPORT_SOURCEMATCH =    2,
    EXPORT =		    4
};


/**
 * @param f filter type to convert to human readable string.
 * @return string representation of filter name.
 */
const char* filter2str(const Filter& f);

} // namespace


#endif // __POLICY_COMMON_FILTER_HH__
