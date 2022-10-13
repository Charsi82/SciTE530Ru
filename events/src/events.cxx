#include "lua.hpp"
#include <vector>
#include <map>
#include <string>
#include <windows.h>
constexpr auto EVENTS_CLASS = "events_class";

static void lua_print(lua_State* L, const char* args)
{
	lua_getglobal(L, "print");
	lua_pushstring(L, args);
	lua_call(L, 1, 0);
}

static void throw_error(lua_State* L, const char* msg)
{
	lua_pushstring(L, msg);
	lua_error(L);
}

class Event
{
private:
	bool m_break;
	bool m_removeThisCallback;
	int in_progress;
	std::string finger_print;
	std::vector<int> callbacks;
	std::vector<int> to_remove;
public:
	std::string name;
	explicit Event(const char* _name): name(_name), finger_print("") { in_progress = 0; m_break = m_removeThisCallback = false; };
	~Event() = default;
	// methods
	void reg(int func_ref) { callbacks.push_back(func_ref); };
	void insert_begin(int func_ref) { callbacks.emplace(callbacks.begin(),func_ref); };
	void unreg(lua_State* L, int func_ref) {
		luaL_unref(L, LUA_REGISTRYINDEX, func_ref);
		callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), func_ref));
	};
	bool trigger(lua_State* L) {
		if (++in_progress > 2) {
			char msg[128]{};
			sprintf_s(msg, "recursive call trigger for event '%s' with deep = %d", name.c_str(), in_progress);
			lua_print(L, msg);
			int stack_size = lua_gettop(L);
			for (int i = 2; i < stack_size; ++i)
			{
				lua_getglobal(L, "print");
				lua_pushfstring(L, "arg[%d][%s]", i - 1, lua_tostring(L, i));
				lua_call(L, 1, 0);
			}
			if (in_progress > 10) {
				m_removeThisCallback = true;
				--in_progress;
				return false;
			}
		}
		bool result = false;
		int stack_size = lua_gettop(L);
		for (const int callbackRef : callbacks)
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, callbackRef);
			if (lua_isfunction(L, -1))
			{
				for (int i = stack_size; i; --i) lua_pushvalue(L, stack_size - i + 1);
				lua_call(L, stack_size, 1);
				result = !!lua_toboolean(L, -1);
				lua_pop(L, 1);
				if (m_removeThisCallback) {
					to_remove.push_back(callbackRef);
					m_removeThisCallback = false;
				}
				if (m_break or result) { m_break = false; result = true; break; }
			}
			else
				lua_pop(L, 1);
		}
		for (int ref : to_remove) unreg(L, ref);
		to_remove.clear();
		finger_print = "";
		--in_progress;
		return result;
	};
	void stop() { m_break = true; };
	void print(lua_State* L) {
		char buff[128]{};
		sprintf_s(buff, "name = %s\nsize = %d", name.c_str(), (int)callbacks.size());
		lua_print(L, buff);
		if (finger_print.size()) {
			sprintf_s(buff, "fp = %s", finger_print.c_str());
			lua_print(L, buff);
		}
		lua_print(L, m_break ? "m_break:true" : "m_break:false");
		lua_print(L, m_removeThisCallback ? "m_removeThisCallback:true" : "m_removeThisCallback:false");
	};
	void removeThisCallback() { m_removeThisCallback = true; };
	void set_fp(const char* fp_name) { finger_print = fp_name; };
	void clear(lua_State* L) {
		if (in_progress) m_break = true;
		to_remove.clear();
		for (int ref : callbacks) luaL_unref(L, LUA_REGISTRYINDEX, ref);
		callbacks.clear();
	};
};

class CEvtManager {
	std::map<std::string, Event*> map_events;
public:
	CEvtManager() = default;
	~CEvtManager() { clear_all(); };
	Event* get(const char* name) {
		auto it = map_events.find(name);
		if (it == map_events.end())
		{
			//Event* pEvt = new Event(name);
			//map_events.emplace(name, pEvt);
			//return pEvt;
			return map_events.emplace(name, new Event(name)).first->second;
		}
		return it->second;
	}
	void clear_all()
	{
		for (const auto& it : map_events) delete it.second;
		map_events.clear();
	}
};
static CEvtManager* pEM;

static Event* event_arg(lua_State* L, int idx = 1)
{
	Event* ev_object = reinterpret_cast<Event*>(lua_touserdata(L, idx));
	if (!ev_object) throw_error(L, "not a 'event' object");
	return ev_object;
}

int do_event(lua_State* L)
{
	const char* name = luaL_checkstring(L, 1);
	if (!strlen(name)) throw_error(L, "empty name for event");
	lua_pushlightuserdata(L, pEM->get(name));
	luaL_getmetatable(L, EVENTS_CLASS);
	lua_setmetatable(L, -2);
	return 1;
}

int do_register(lua_State* L)
{
	luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_pushvalue(L, 2);
	int ref_idx = luaL_ref(L, LUA_REGISTRYINDEX);
	if(lua_toboolean(L,3)) 
		event_arg(L)->insert_begin(ref_idx);
	else
		event_arg(L)->reg(ref_idx);
	lua_settop(L, 1);
	return 1;
}

int do_unregister(lua_State* L)
{
	luaL_checktype(L, 2, LUA_TFUNCTION);
	int ref_idx = -1;
#if LUA_VERSION_NUM < 502
	int max_idx = lua_objlen(L, LUA_REGISTRYINDEX);
#else
	int max_idx = (int)luaL_len(L, LUA_REGISTRYINDEX);
#endif
	for (int i = max_idx; i; --i)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, i);
#if LUA_VERSION_NUM < 502
		if (lua_equal(L, -1, -2) == 1)
#else
		if (lua_compare(L, -1, -2, 0) == 1)
#endif
		{
			ref_idx = i;
			break;
		};
		lua_pop(L, 1);
	}
	if (ref_idx != -1)
		event_arg(L)->unreg(L, ref_idx);
	lua_settop(L, 1);
	return 1;
}

int do_trigger(lua_State* L)
{
	lua_pushboolean(L, event_arg(L)->trigger(L));
	return 1;
}

int do_print(lua_State* L)
{
	event_arg(L)->print(L);
	return 1;
}

int do_stop(lua_State* L)
{
	event_arg(L)->stop();
	return 1;
}

int do_setRemove(lua_State* L)
{
	event_arg(L)->removeThisCallback();
	return 1;
}

int do_setfp(lua_State* L)
{
	event_arg(L)->set_fp(luaL_checkstring(L, 2));
	lua_pop(L, 1);
	return 1;
}

int do_clear(lua_State* L)
{
	event_arg(L)->clear(L);
	return 1;
}

int do_name(lua_State* L)
{
	lua_pushstring(L, event_arg(L)->name.c_str());
	return 1;
}

static const struct luaL_Reg event_methods[] = {
	{"name",	do_name},
	{"register",do_register},
	{"unregister",do_unregister},
	{"trigger",do_trigger},
	{"__call",	do_trigger},
	{"stop",do_stop},
	{"setFingerprint",do_setfp},
	{"removeThisCallback",do_setRemove},
	{"clear",do_clear},
	{"print",do_print},
	{NULL,NULL}
};

extern "C" __declspec(dllexport)
int luaopen_events(lua_State * L)
{
	if (pEM) pEM->clear_all();
	luaL_newmetatable(L, EVENTS_CLASS);  // create metatable for event objects
	lua_pushvalue(L, -1);  // copy metatable to top stack
	lua_setfield(L, -2, "__index");  // metatable.__index = metatable
#if LUA_VERSION_NUM < 502
	luaL_register(L, NULL, event_methods);
#else
	luaL_setfuncs(L, event_methods, 0); // add methods
#endif
	lua_pushcfunction(L, do_event);
	lua_setglobal(L, "event");
	return 1;
}

extern "C"  // inhibit C++ name mangling
BOOL APIENTRY DllMain(
	HINSTANCE /*hinstDLL*/,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID /*lpvReserved*/   // reserved
)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		pEM = new CEvtManager;
		break;
	case DLL_PROCESS_DETACH:
		delete pEM;
		break;
	};
	return TRUE;
}
