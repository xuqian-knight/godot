/*************************************************************************/
/*  base_object_glue.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/string_name.h"

#include "../csharp_script.h"
#include "../mono_gd/gd_mono_cache.h"
#include "../mono_gd/gd_mono_internals.h"
#include "../mono_gd/gd_mono_utils.h"
#include "../signal_awaiter_utils.h"

void godot_icall_Object_Disposed(Object *p_ptr) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p_ptr == nullptr);
#endif

	if (p_ptr->get_script_instance()) {
		CSharpInstance *cs_instance = CAST_CSHARP_INSTANCE(p_ptr->get_script_instance());
		if (cs_instance) {
			if (!cs_instance->is_destructing_script_instance()) {
				cs_instance->mono_object_disposed();
				p_ptr->set_script_instance(nullptr);
			}
			return;
		}
	}

	void *data = CSharpLanguage::get_existing_instance_binding(p_ptr);

	if (data) {
		CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->get();
		if (script_binding.inited) {
			MonoGCHandleData &gchandle = script_binding.gchandle;
			if (!gchandle.is_released()) {
				CSharpLanguage::release_script_gchandle(nullptr, gchandle);
				script_binding.inited = false;
			}
		}
	}
}

void godot_icall_RefCounted_Disposed(Object *p_ptr, MonoBoolean p_is_finalizer) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p_ptr == nullptr);
	// This is only called with RefCounted derived classes
	CRASH_COND(!Object::cast_to<RefCounted>(p_ptr));
#endif

	RefCounted *rc = static_cast<RefCounted *>(p_ptr);

	if (rc->get_script_instance()) {
		CSharpInstance *cs_instance = CAST_CSHARP_INSTANCE(rc->get_script_instance());
		if (cs_instance) {
			if (!cs_instance->is_destructing_script_instance()) {
				bool delete_owner;
				bool remove_script_instance;

				cs_instance->mono_object_disposed_baseref(p_is_finalizer, delete_owner, remove_script_instance);

				if (delete_owner) {
					memdelete(rc);
				} else if (remove_script_instance) {
					rc->set_script_instance(nullptr);
				}
			}
			return;
		}
	}

	// Unsafe refcount decrement. The managed instance also counts as a reference.
	// See: CSharpLanguage::alloc_instance_binding_data(Object *p_object)
	CSharpLanguage::get_singleton()->pre_unsafe_unreference(rc);
	if (rc->unreference()) {
		memdelete(rc);
	} else {
		void *data = CSharpLanguage::get_existing_instance_binding(rc);

		if (data) {
			CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->get();
			if (script_binding.inited) {
				MonoGCHandleData &gchandle = script_binding.gchandle;
				if (!gchandle.is_released()) {
					CSharpLanguage::release_script_gchandle(nullptr, gchandle);
					script_binding.inited = false;
				}
			}
		}
	}
}

void godot_icall_Object_ConnectEventSignal(Object *p_ptr, const StringName *p_event_signal) {
	CSharpInstance *csharp_instance = CAST_CSHARP_INSTANCE(p_ptr->get_script_instance());
	if (csharp_instance) {
		csharp_instance->connect_event_signal(*p_event_signal);
	}
}

int32_t godot_icall_SignalAwaiter_connect(Object *p_source, StringName *p_signal, Object *p_target, GCHandleIntPtr p_awaiter_handle_ptr) {
	StringName signal = p_signal ? *p_signal : StringName();
	return (int32_t)gd_mono_connect_signal_awaiter(p_source, signal, p_target, p_awaiter_handle_ptr);
}

void godot_register_object_icalls() {
	GDMonoUtils::add_internal_call("Godot.Object::godot_icall_Object_Disposed", godot_icall_Object_Disposed);
	GDMonoUtils::add_internal_call("Godot.Object::godot_icall_RefCounted_Disposed", godot_icall_RefCounted_Disposed);
	GDMonoUtils::add_internal_call("Godot.Object::godot_icall_Object_ConnectEventSignal", godot_icall_Object_ConnectEventSignal);
	GDMonoUtils::add_internal_call("Godot.SignalAwaiter::godot_icall_SignalAwaiter_connect", godot_icall_SignalAwaiter_connect);
}
