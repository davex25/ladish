/* This file is part of Ingen.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * 
 * Ingen is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Ingen is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef CLASHAVOIDER_H
#define CLASHAVOIDER_H

#include <inttypes.h>
#include <string>
#include <map>
#include "raul/Atom.hpp"
#include "raul/Path.hpp"
#include "interface/CommonInterface.hpp"

namespace Ingen {
namespace Shared {

class Store;


/** A wrapper for a CommonInterface that creates objects but possibly maps
 * symbol names to avoid clashes with the existing objects in a store.
 */
class ClashAvoider : public CommonInterface
{
public:
	ClashAvoider(Store& store, const Raul::Path& prefix, CommonInterface& target,
			Store* also_avoid=NULL)
		: _prefix(prefix), _store(store), _target(target), _also_avoid(also_avoid) {}

	void set_target(CommonInterface& target) { _target = target; }
	
	// Bundles
	void bundle_begin() { _target.bundle_begin(); }
	void bundle_end()   { _target.bundle_end(); }
	
	// Object commands
	
	void new_object(const GraphObject* object);

	void new_patch(const std::string& path,
	               uint32_t           poly);
	
	void new_node(const std::string& path,
	              const std::string& plugin_uri);
	
	void new_port(const std::string& path,
	              const std::string& type,
	              uint32_t           index,
	              bool               is_output);
	
	void connect(const std::string& src_port_path,
	             const std::string& dst_port_path);
	
	void disconnect(const std::string& src_port_path,
	                const std::string& dst_port_path);
	
	void set_variable(const std::string& subject_path,
	                  const std::string& predicate,
	                  const Raul::Atom&  value);
	
	void set_property(const std::string& subject_path,
	                  const std::string& predicate,
	                  const Raul::Atom&  value);
	
	void set_port_value(const std::string& port_path,
	                    const Raul::Atom&  value);
	
	void set_voice_value(const std::string& port_path,
	                     uint32_t           voice,
	                     const Raul::Atom&  value);
	
	void destroy(const std::string& path);

private:
	const Raul::Path map_path(const Raul::Path& in);

	const Raul::Path& _prefix;
	Store&            _store;
	CommonInterface&  _target;

	Store* _also_avoid;
	bool exists(const Raul::Path& path) const;

	typedef std::map<Raul::Path, unsigned> Offsets;
	Offsets _offsets;

	typedef std::map<Raul::Path, Raul::Path> SymbolMap;
	SymbolMap _symbol_map;
};


} // namespace Shared
} // namespace Ingen

#endif // CLASHAVOIDER_H

