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

#include <string.h>
#include <dbus/dbus.h>

#include "common.hpp"
#include "lash_proxy.hpp"
#include "session.hpp"
#include "project.hpp"
#include "lash_client.hpp"
#include "globals.hpp"
#include "dbus_helpers.h"

#define LASH_SERVICE       "org.nongnu.LASH"
#define LASH_OBJECT        "/"
#define LASH_IFACE_CONTROL "org.nongnu.LASH.Control"

using namespace std;

struct lash_proxy_impl
{
        void init();

        void fetch_loaded_projects();
        void fetch_project_clients(shared_ptr<project> project_ptr);

        static void error_msg(const std::string& msg);
        static void info_msg(const std::string& msg);

        static
        DBusHandlerResult
        dbus_message_hook(
                DBusConnection * connection,
                DBusMessage * message,
                void * proxy);

        bool
        call(
                bool response_expected,
                const char* iface,
                const char* method,
                DBusMessage ** reply_ptr_ptr,
                int in_type,
                ...);

        shared_ptr<project>
        on_project_added(
                string name);

        shared_ptr<lash_client>
        on_client_added(
                shared_ptr<project> project_ptr,
                string id,
                string name);

        void exit();

        bool _server_responding;
        session * _session_ptr;
        lash_proxy * _interface_ptr;
};

lash_proxy::lash_proxy(session * session_ptr)
{
        _impl_ptr = new lash_proxy_impl;
        _impl_ptr->_interface_ptr = this;
        _impl_ptr->_session_ptr = session_ptr;
        _impl_ptr->init();
}

lash_proxy::~lash_proxy()
{
        delete _impl_ptr;
}

void
lash_proxy_impl::init()
{
        _server_responding = false;

        patchage_dbus_add_match("type='signal',interface='" DBUS_INTERFACE_DBUS "',member=NameOwnerChanged,arg0='" LASH_SERVICE "'");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=StudioAppeared");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=StudioDisappeared");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ProjectAppeared");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ProjectDisappeared");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ProjectNameChanged");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ProjectModifiedStatusChanged");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ProjectDescriptionChanged");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ProjectNotesChanged");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ClientAppeared");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ClientDisappeared");
        patchage_dbus_add_match("type='signal',interface='" LASH_IFACE_CONTROL "',member=ClientNameChanged");

        patchage_dbus_add_filter(dbus_message_hook, this);

        // get initial list of projects
        // calling any method to updates server responding status
        // this also actiavtes lash object if it not activated already
        //fetch_loaded_projects();

        g_app->set_studio_availability(false);
}

bool
lash_proxy::is_active()
{
        return _impl_ptr->_server_responding;
}

void
lash_proxy::try_activate()
{
        list<lash_project_info> projects;
        get_available_projects(projects);
}

void
lash_proxy::deactivate()
{
        _impl_ptr->exit();
}

void
lash_proxy_impl::error_msg(const std::string& msg)
{
        g_app->error_msg((std::string)"[LASH] " + msg);
}

void
lash_proxy_impl::info_msg(const std::string& msg)
{
        g_app->info_msg((std::string)"[LASH] " + msg);
}

DBusHandlerResult
lash_proxy_impl::dbus_message_hook(
        DBusConnection * connection,
        DBusMessage * message,
        void * proxy)
{
        const char * project_name;
        const char * new_project_name;
        const char * object_name;
        const char * old_owner;
        const char * new_owner;
        const char * value_string;
        const char * client_id;
        const char * client_name;
        dbus_bool_t modified_status;
        shared_ptr<project> project_ptr;
        shared_ptr<lash_client> client_ptr;

        assert(proxy);
        lash_proxy_impl * me = reinterpret_cast<lash_proxy_impl *>(proxy);

        info_msg("dbus_message_hook() called.");

        // Handle signals we have subscribed for in attach()

        if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameOwnerChanged"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &object_name,
                            DBUS_TYPE_STRING, &old_owner,
                            DBUS_TYPE_STRING, &new_owner,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract NameOwnerChanged signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                if ((string)object_name != LASH_SERVICE)
                {
                        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                }

                if (old_owner[0] == '\0')
                {
                        info_msg((string)"LASH activated.");
                }
                else if (new_owner[0] == '\0')
                {
                        info_msg((string)"LASH deactivated.");
                        g_app->set_studio_availability(false);
                        me->_server_responding = false;
                }

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "StudioAppeared"))
        {
                info_msg((string)"Studio appeared.");
                g_app->set_studio_availability(true);

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "StudioDisappeared"))
        {
                info_msg((string)"Studio disappeared.");
                g_app->set_studio_availability(false);

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ProjectAppeared"))
        {
                if (!dbus_message_get_args( message, &g_dbus_error,
                                        DBUS_TYPE_STRING, &project_name,
                                        DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ProjectAppeared signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Project '" + project_name + "' appeared.");
                me->on_project_added(project_name);

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ProjectDisappeared"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &project_name,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ProjectDisappeared signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Project '" + project_name + "' disappeared.");
                me->_session_ptr->project_close(project_name);

                return DBUS_HANDLER_RESULT_HANDLED;
        }


        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ProjectNameChanged"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &project_name,
                            DBUS_TYPE_STRING, &new_project_name,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ProjectNameChanged signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Project '" + project_name + "' renamed to '" + new_project_name + "'.");

                project_ptr = me->_session_ptr->find_project_by_name(project_name);
                if (project_ptr)
                {
                        project_ptr->on_name_changed(new_project_name);
                }

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ProjectModifiedStatusChanged"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &project_name,
                            DBUS_TYPE_BOOLEAN, &modified_status,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ProjectModifiedStatusChanged signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Project '" + project_name + "' modified status changed to '" + (modified_status ? "true" : "false") + "'.");

                project_ptr = me->_session_ptr->find_project_by_name(project_name);
                if (project_ptr)
                {
                        project_ptr->on_modified_status_changed(modified_status);
                }

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ProjectDescriptionChanged"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &project_name,
                            DBUS_TYPE_STRING, &value_string,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ProjectDescriptionChanged signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Project '" + project_name + "' description changed.");

                project_ptr = me->_session_ptr->find_project_by_name(project_name);
                if (project_ptr)
                {
                        project_ptr->on_description_changed(value_string);
                }

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ProjectNotesChanged"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &project_name,
                            DBUS_TYPE_STRING, &value_string,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ProjectNotesChanged signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Project '" + project_name + "' notes changed.");

                project_ptr = me->_session_ptr->find_project_by_name(project_name);
                if (project_ptr)
                {
                        project_ptr->on_notes_changed(value_string);
                }

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ClientAppeared"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &client_id,
                            DBUS_TYPE_STRING, &project_name,
                            DBUS_TYPE_STRING, &client_name,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ClientAppeared signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Client '" + client_id + "':'" + client_name + "' appeared in project '" + project_name + "'.");

                project_ptr = me->_session_ptr->find_project_by_name(project_name);
                if (project_ptr)
                {
                        me->on_client_added(project_ptr, client_id, client_name);
                }

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ClientDisappeared"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &client_id,
                            DBUS_TYPE_STRING, &project_name,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ClientDisappeared signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Client '" + client_id + "' of project '" + project_name + "' disappeared.");

                client_ptr = me->_session_ptr->find_client_by_id(client_id);
                if (client_ptr)
                {
                        client_ptr->get_project()->on_client_removed(client_id);
                        me->_session_ptr->client_remove(client_id);
                }

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (dbus_message_is_signal(message, LASH_IFACE_CONTROL, "ClientNameChanged"))
        {
                if (!dbus_message_get_args(
                            message, &g_dbus_error,
                            DBUS_TYPE_STRING, &client_id,
                            DBUS_TYPE_STRING, &client_name,
                            DBUS_TYPE_INVALID))
                {
                        error_msg(str(boost::format("dbus_message_get_args() failed to extract ClientNameChanged signal arguments (%s)") % g_dbus_error.message));
                        dbus_error_free(&g_dbus_error);
                        return DBUS_HANDLER_RESULT_HANDLED;
                }

                info_msg((string)"Client '" + client_id + "' name changed to '" + client_name + "'.");

                client_ptr = me->_session_ptr->find_client_by_id(client_id);
                if (client_ptr)
                {
                        client_ptr->on_name_changed(client_name);
                }

                return DBUS_HANDLER_RESULT_HANDLED;
        }

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool
lash_proxy_impl::call(
        bool response_expected,
        const char* iface,
        const char* method,
        DBusMessage ** reply_ptr_ptr,
        int in_type,
        ...)
{
        va_list ap;

        va_start(ap, in_type);

        _server_responding = patchage_dbus_call_valist(
                response_expected,
                LASH_SERVICE,
                LASH_OBJECT,
                iface,
                method,
                reply_ptr_ptr,
                in_type,
                ap);

        va_end(ap);

        return _server_responding;
}

void
lash_proxy_impl::fetch_loaded_projects()
{
        DBusMessage* reply_ptr;
        const char * reply_signature;
        DBusMessageIter iter;
        DBusMessageIter array_iter;
        const char * project_name;
        shared_ptr<project> project_ptr;

        if (!call(true, LASH_IFACE_CONTROL, "ProjectsGet", &reply_ptr, DBUS_TYPE_INVALID)) {
                return;
        }

        reply_signature = dbus_message_get_signature(reply_ptr);

        if (strcmp(reply_signature, "as") != 0)
        {
                error_msg((string)"ProjectsGet() reply signature mismatch. " + reply_signature);
                goto unref;
        }

        dbus_message_iter_init(reply_ptr, &iter);

        for (dbus_message_iter_recurse(&iter, &array_iter);
             dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID;
             dbus_message_iter_next(&array_iter))
        {
                dbus_message_iter_get_basic(&array_iter, &project_name);
                project_ptr = on_project_added(project_name);
                fetch_project_clients(project_ptr);
        }

unref:
        dbus_message_unref(reply_ptr);
}

void
lash_proxy_impl::fetch_project_clients(
        shared_ptr<project> project_ptr)
{
        DBusMessage* reply_ptr;
        const char * reply_signature;
        DBusMessageIter iter;
        DBusMessageIter array_iter;
        DBusMessageIter struct_iter;
        const char * client_id;
        const char * client_name;
        string project_name;
        const char * project_name_cstr;

        project_ptr->get_name(project_name);
        project_name_cstr = project_name.c_str();

        if (!call(
                    true,
                    LASH_IFACE_CONTROL,
                    "ProjectGetClients",
                    &reply_ptr,
                    DBUS_TYPE_STRING, &project_name_cstr,
                    DBUS_TYPE_INVALID))
        {
                return;
        }

        reply_signature = dbus_message_get_signature(reply_ptr);

        if (strcmp(reply_signature, "a(ss)") != 0)
        {
                error_msg((string)"ProjectGetClients() reply signature mismatch. " + reply_signature);
                goto unref;
        }

        dbus_message_iter_init(reply_ptr, &iter);

        for (dbus_message_iter_recurse(&iter, &array_iter);
             dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID;
             dbus_message_iter_next(&array_iter))
        {
                dbus_message_iter_recurse(&array_iter, &struct_iter);

                dbus_message_iter_get_basic(&struct_iter, &client_id);
                dbus_message_iter_next(&struct_iter);
                dbus_message_iter_get_basic(&struct_iter, &client_name);
                dbus_message_iter_next(&struct_iter);

                on_client_added(project_ptr, client_id, client_name);
        }

unref:
        dbus_message_unref(reply_ptr);
}

shared_ptr<project>
lash_proxy_impl::on_project_added(
        string name)
{
        shared_ptr<project> project_ptr(new project(_interface_ptr, name));

        _session_ptr->project_add(project_ptr);

        return project_ptr;
}

shared_ptr<lash_client>
lash_proxy_impl::on_client_added(
        shared_ptr<project> project_ptr,
        string id,
        string name)
{
        shared_ptr<lash_client> client_ptr(
                new lash_client(
                        _interface_ptr,
                        project_ptr.get(),
                        id,
                        name));

        project_ptr->on_client_added(client_ptr);
        _session_ptr->client_add(client_ptr);

        return client_ptr;
}

void
lash_proxy::get_available_projects(
        list<lash_project_info>& projects)
{
        DBusMessage * reply_ptr;
        const char * reply_signature;
        DBusMessageIter iter;
        DBusMessageIter array_iter;
        DBusMessageIter struct_iter;
        DBusMessageIter dict_iter;
        DBusMessageIter dict_entry_iter;
        DBusMessageIter variant_iter;
        const char * project_name;
        const char * key;
        const char * value_type;
        dbus_uint32_t value_uint32;
        const char * value_string;
        lash_project_info project_info;

        if (!_impl_ptr->call(true, LASH_IFACE_CONTROL, "ProjectsGetAvailable", &reply_ptr, DBUS_TYPE_INVALID))
        {
                return;
        }

        reply_signature = dbus_message_get_signature(reply_ptr);

        if (strcmp(reply_signature, "a(sa{sv})") != 0)
        {
                lash_proxy_impl::error_msg((string)"ProjectsGetAvailable() reply signature mismatch. " + reply_signature);
                goto unref;
        }

        dbus_message_iter_init(reply_ptr, &iter);

        for (dbus_message_iter_recurse(&iter, &array_iter);
             dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID;
             dbus_message_iter_next(&array_iter))
        {
                dbus_message_iter_recurse(&array_iter, &struct_iter);

                dbus_message_iter_get_basic(&struct_iter, &project_name);

                project_info.name = project_name;
                project_info.modification_time = 0;
                project_info.description.erase();

                dbus_message_iter_next(&struct_iter);

                for (dbus_message_iter_recurse(&struct_iter, &dict_iter);
                     dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID;
                     dbus_message_iter_next(&dict_iter))
                {
                        dbus_message_iter_recurse(&dict_iter, &dict_entry_iter);
                        dbus_message_iter_get_basic(&dict_entry_iter, &key);
                        dbus_message_iter_next(&dict_entry_iter);
                        dbus_message_iter_recurse(&dict_entry_iter, &variant_iter);
                        value_type = dbus_message_iter_get_signature(&variant_iter);
                        if (value_type[0] != 0 && value_type[1] == 0)
                        {
                                switch (*value_type)
                                {
                                case DBUS_TYPE_UINT32:
                                        if (strcmp(key, "Modification Time") == 0)
                                        {
                                                dbus_message_iter_get_basic(&variant_iter, &value_uint32);
                                                project_info.modification_time = value_uint32;
                                        }
                                        break;
                                case DBUS_TYPE_STRING:
                                        if (strcmp(key, "Description") == 0)
                                        {
                                                dbus_message_iter_get_basic(&variant_iter, &value_string);
                                                project_info.description = value_string;
                                        }
                                        break;
                                }
                        }
                }

                projects.push_back(project_info);
        }

unref:
        dbus_message_unref(reply_ptr);
}

void
lash_proxy::load_project(
        const string& project_name)
{
        DBusMessage * reply_ptr;
        const char * project_name_cstr;

        project_name_cstr = project_name.c_str();

        if (!_impl_ptr->call(true, LASH_IFACE_CONTROL, "ProjectOpen", &reply_ptr, DBUS_TYPE_STRING, &project_name_cstr, DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}

void
lash_proxy::save_all_projects()
{
        DBusMessage * reply_ptr;

        if (!_impl_ptr->call(true, LASH_IFACE_CONTROL, "ProjectsSaveAll", &reply_ptr, DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}

void
lash_proxy::save_project(
        const string& project_name)
{
        DBusMessage * reply_ptr;
        const char * project_name_cstr;

        project_name_cstr = project_name.c_str();

        if (!_impl_ptr->call(true, LASH_IFACE_CONTROL, "ProjectSave", &reply_ptr, DBUS_TYPE_STRING, &project_name_cstr, DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}

void
lash_proxy::close_project(
        const string& project_name)
{
        DBusMessage * reply_ptr;
        const char * project_name_cstr;

        project_name_cstr = project_name.c_str();

        if (!_impl_ptr->call(true, LASH_IFACE_CONTROL, "ProjectClose", &reply_ptr, DBUS_TYPE_STRING, &project_name_cstr, DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}

void
lash_proxy::close_all_projects()
{
        DBusMessage * reply_ptr;

        if (!_impl_ptr->call(true, LASH_IFACE_CONTROL, "ProjectsCloseAll", &reply_ptr, DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}

void
lash_proxy::project_rename(
        const string& old_name,
        const string& new_name)
{
        DBusMessage * reply_ptr;
        const char * old_name_cstr;
        const char * new_name_cstr;

        old_name_cstr = old_name.c_str();
        new_name_cstr = new_name.c_str();

        if (!_impl_ptr->call(
                    true,
                    LASH_IFACE_CONTROL,
                    "ProjectRename",
                    &reply_ptr,
                    DBUS_TYPE_STRING, &old_name_cstr,
                    DBUS_TYPE_STRING, &new_name_cstr,
                    DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}

void
lash_proxy::get_loaded_project_properties(
        const string& name,
        lash_loaded_project_properties& properties)
{
        DBusMessage * reply_ptr;
        const char * reply_signature;
        DBusMessageIter iter;
        DBusMessageIter dict_iter;
        DBusMessageIter dict_entry_iter;
        DBusMessageIter variant_iter;
        const char * key;
        const char * value_type;
        dbus_bool_t value_bool;
        const char * value_string;
        const char * project_name_cstr;

        project_name_cstr = name.c_str();

        if (!_impl_ptr->call(
                    true,
                    LASH_IFACE_CONTROL,
                    "ProjectGetProperties",
                    &reply_ptr,
                    DBUS_TYPE_STRING, &project_name_cstr,
                    DBUS_TYPE_INVALID))
        {
                return;
        }

        reply_signature = dbus_message_get_signature(reply_ptr);

        if (strcmp(reply_signature, "a{sv}") != 0)
        {
                lash_proxy_impl::error_msg((string)"ProjectGetProperties() reply signature mismatch. " + reply_signature);
                goto unref;
        }

        dbus_message_iter_init(reply_ptr, &iter);

        for (dbus_message_iter_recurse(&iter, &dict_iter);
             dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID;
             dbus_message_iter_next(&dict_iter))
        {
                dbus_message_iter_recurse(&dict_iter, &dict_entry_iter);
                dbus_message_iter_get_basic(&dict_entry_iter, &key);
                dbus_message_iter_next(&dict_entry_iter);
                dbus_message_iter_recurse(&dict_entry_iter, &variant_iter);
                value_type = dbus_message_iter_get_signature(&variant_iter);
                if (value_type[0] != 0 && value_type[1] == 0)
                {
                        switch (*value_type)
                        {
                        case DBUS_TYPE_BOOLEAN:
                                if (strcmp(key, "Modified Status") == 0)
                                {
                                        dbus_message_iter_get_basic(&variant_iter, &value_bool);
                                        properties.modified_status = value_bool;
                                }
                                break;
                        case DBUS_TYPE_STRING:
                                if (strcmp(key, "Description") == 0)
                                {
                                        dbus_message_iter_get_basic(&variant_iter, &value_string);
                                        properties.description = value_string;
                                }
                                else if (strcmp(key, "Notes") == 0)
                                {
                                        dbus_message_iter_get_basic(&variant_iter, &value_string);
                                        properties.notes = value_string;
                                }
                                break;
                        }
                }
        }

unref:
        dbus_message_unref(reply_ptr);
}

void
lash_proxy::project_set_description(
        const string& project_name,
        const string& description)
{
        DBusMessage * reply_ptr;
        const char * project_name_cstr;
        const char * description_cstr;

        project_name_cstr = project_name.c_str();
        description_cstr = description.c_str();

        if (!_impl_ptr->call(
                    true,
                    LASH_IFACE_CONTROL,
                    "ProjectSetDescription",
                    &reply_ptr,
                    DBUS_TYPE_STRING, &project_name_cstr,
                    DBUS_TYPE_STRING, &description_cstr,
                    DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}

void
lash_proxy::project_set_notes(
        const string& project_name,
        const string& notes)
{
        DBusMessage * reply_ptr;
        const char * project_name_cstr;
        const char * notes_cstr;

        project_name_cstr = project_name.c_str();
        notes_cstr = notes.c_str();

        if (!_impl_ptr->call(
                    true,
                    LASH_IFACE_CONTROL,
                    "ProjectSetNotes",
                    &reply_ptr,
                    DBUS_TYPE_STRING, &project_name_cstr,
                    DBUS_TYPE_STRING, &notes_cstr,
                    DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}

void
lash_proxy_impl::exit()
{
        DBusMessage * reply_ptr;

        if (!call(
                    true,
                    LASH_IFACE_CONTROL,
                    "Exit",
                    &reply_ptr,
                    DBUS_TYPE_INVALID))
        {
                return;
        }

        dbus_message_unref(reply_ptr);
}
