// -*- Mode: C++ ; indent-tabs-mode: t -*-
/* This file is part of Patchage.
 * Copyright (C) 2008 Nedko Arnaudov <nedko@arnaudov.name>
 * 
 * Patchage is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "common.hpp"
#include "project.hpp"
#include "session.hpp"
#include "lash_client.hpp"

struct session_impl
{
  list<shared_ptr<project> > projects;
  list<shared_ptr<lash_client> > clients;
};

session::session()
{
  _impl_ptr = new session_impl;
}

session::~session()
{
  delete _impl_ptr;
}

void
session::clear()
{
  shared_ptr<project> project_ptr;

  _impl_ptr->clients.clear();

  while (!_impl_ptr->projects.empty())
  {
    project_ptr = _impl_ptr->projects.front();
    _impl_ptr->projects.pop_front();
    project_ptr->clear();
    _signal_project_closed.emit(project_ptr);
  }
}

void
session::project_add(
  shared_ptr<project> project_ptr)
{
  _impl_ptr->projects.push_back(project_ptr);

  _signal_project_added.emit(project_ptr);
}

shared_ptr<project>
session::find_project_by_name(
  const string& name)
{
  shared_ptr<project> project_ptr;
  string temp_name;

  for (list<shared_ptr<project> >::iterator iter = _impl_ptr->projects.begin(); iter != _impl_ptr->projects.end(); iter++)
  {
    project_ptr = *iter;
    project_ptr->get_name(temp_name);

    if (temp_name == name)
    {
      return project_ptr;
    }
  }

  return shared_ptr<project>();
}

void
session::project_close(
  const string& project_name)
{
  shared_ptr<project> project_ptr;
  string temp_name;
  list<shared_ptr<lash_client> > clients;

  for (list<shared_ptr<project> >::iterator iter = _impl_ptr->projects.begin(); iter != _impl_ptr->projects.end(); iter++)
  {
    project_ptr = *iter;
    project_ptr->get_name(temp_name);

    if (temp_name == project_name)
    {
      _impl_ptr->projects.erase(iter);
      _signal_project_closed.emit(project_ptr);

      // remove clients from session, if not removed already
      project_ptr->get_clients(clients);
      for (list<shared_ptr<lash_client> >::iterator iter = clients.begin(); iter != clients.end(); iter++)
      {
        string id;

        (*iter)->get_id(id);

        client_remove(id);
      }

      return;
    }
  }
}

void
session::client_add(
  shared_ptr<lash_client> client_ptr)
{
  _impl_ptr->clients.push_back(client_ptr);
}

void
session::client_remove(
  const string& id)
{
  shared_ptr<lash_client> client_ptr;
  string temp_id;

  for (list<shared_ptr<lash_client> >::iterator iter = _impl_ptr->clients.begin(); iter != _impl_ptr->clients.end(); iter++)
  {
    client_ptr = *iter;
    client_ptr->get_id(temp_id);

    if (temp_id == id)
    {
      _impl_ptr->clients.erase(iter);
      return;
    }
  }
}

shared_ptr<lash_client>
session::find_client_by_id(const string& id)
{
  shared_ptr<lash_client> client_ptr;
  string temp_id;

  for (list<shared_ptr<lash_client> >::iterator iter = _impl_ptr->clients.begin(); iter != _impl_ptr->clients.end(); iter++)
  {
    client_ptr = *iter;
    client_ptr->get_id(temp_id);

    if (temp_id == id)
    {
      return client_ptr;
    }
  }

  return shared_ptr<lash_client>();
}
