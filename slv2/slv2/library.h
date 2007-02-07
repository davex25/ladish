/* SLV2
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 *  
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __SLV2_LIBRARY_H
#define __SLV2_LIBRARY_H

#ifdef __cplusplus
extern "C" {
#endif


/** Initialize SLV2.
 *
 * This MUST be called before calling any other SLV2 functions, or fatal
 * errors will likely occur.
 */
void
slv2_init();


/** Finialize SLV2.
 *
 * Frees any resources allocated by slv2_init().
 */
void
slv2_finish();


/** The URI of the lv2.ttl file.
 */
extern raptor_uri* slv2_ontology_uri;


#ifdef __cplusplus
}
#endif

#endif /* __SLV2_LIBRARY_H */
