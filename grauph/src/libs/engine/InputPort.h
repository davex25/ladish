/* This file is part of Om.  Copyright (C) 2006 Dave Robillard.
 * 
 * Om is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Om is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef INPUTPORT_H
#define INPUTPORT_H

#include <string>
#include <cstdlib>
#include <cassert>
#include "PortBase.h"
#include "List.h"
#include "MidiMessage.h"
using std::string;

namespace Om {

template <typename T> class ConnectionBase;
template <typename T> class OutputPort;
class Node;


/** An input port on a Node or Patch.
 *
 * All ports have a Buffer, but the actual contents (data) of that buffer may be
 * set directly to the incoming connection's buffer if there's only one inbound
 * connection, to eliminate the need to copy/mix.
 *
 * If a port has multiple connections, they will be mixed down into the local
 * buffer and it will be used.
 *
 * \ingroup engine
 */
template <typename T>
class InputPort : public PortBase<T>
{
public:
	InputPort(Node* parent, const string& name, size_t index, size_t poly, DataType type, size_t buffer_size);
	virtual ~InputPort() {}
	
	void                          add_connection(ListNode<ConnectionBase<T>*>* const c);
	ListNode<ConnectionBase<T>*>* remove_connection(const OutputPort<T>* const src_port);

	const List<ConnectionBase<T>*>& connections() { return m_connections; }

	void prepare_buffers(size_t nframes);
	
	void tie(OutputPort<T>* const port);

	bool is_connected() const { return (m_connections.size() > 0); }
	bool is_connected_to(const OutputPort<T>* const port) const;
	
	bool is_input()  const { return true; }
	bool is_output() const { return false; }
	
private:
	// Prevent copies (Undefined)
	InputPort<T>(const InputPort<T>& copy);
	InputPort<T>& operator=(const InputPort<T>&);

	void update_buffers();

	List<ConnectionBase<T>*> m_connections;

	// This is just stupid...
	using PortBase<T>::m_is_tied;
	using PortBase<T>::m_tied_port;
	using PortBase<T>::m_buffers;
	using PortBase<T>::_poly;
	using PortBase<T>::_index;
	using PortBase<T>::_buffer_size;
	using PortBase<T>::m_fixed_buffers;
};


template class InputPort<sample>;
template class InputPort<MidiMessage>;

} // namespace Om

#endif // INPUTPORT_H
