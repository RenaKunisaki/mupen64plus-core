//Mupen64Plus built-in Lua modules.
#include "lua.h"
#include "debugger/dbg_memory.h"

enum {
	ROM_FIELD_HEADER,
	ROM_FIELD_SETTINGS,
	NUM_ROM_FIELDS
};
static const char *romFieldName[] = {"header", "settings", NULL};

enum {
	MEM_TYPE_S8,
	MEM_TYPE_U8,
	MEM_TYPE_S16,
	MEM_TYPE_U16,
	MEM_TYPE_S32,
	MEM_TYPE_U32,
	MEM_TYPE_FLOAT,
	MEM_TYPE_DOUBLE,
	MEM_TYPE_STRING, //used for write
	NUM_MEM_TYPES
};
static const char *memTypeName[] = {
	"s8", "u8", "s16", "u16", "s32", "u32", "float", "double", "string", NULL};


static int rom_open(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);
	return m64p_lua_return_errcode(L,
		CoreDoCommand(M64CMD_ROM_OPEN, 0, (void*)path));
}

static int rom_close(lua_State *L) {
	return m64p_lua_return_errcode(L,
		CoreDoCommand(M64CMD_ROM_CLOSE, 0, NULL));
}

static int rom_meta_index(lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, "rom_fields"); //-1: fields
	lua_pushvalue(L, 2); //-1: key, -2: fields
	lua_gettable(L, -2); //-1: val, -2: fields
	int field = luaL_optinteger(L, -1, -1);
	lua_pop(L, 2);

	switch(field) {
		case ROM_FIELD_HEADER: {
			m64p_rom_header header;
			if(CoreDoCommand(M64CMD_ROM_GET_HEADER, sizeof(header), &header)) {
				lua_pushnil(L);
				break;
			}

			//ensure name is null terminated.
			char name[sizeof(header.Name)+1];
			strncpy(name, (char*)header.Name, sizeof(name));

			lua_createtable(L, 0, 16); //-1: tbl
			LUA_SET_FIELD(L, -1, "init_PI_BSB_DOM1_LAT_REG", integer,
				header.init_PI_BSB_DOM1_LAT_REG);
			LUA_SET_FIELD(L, -1, "init_PI_BSB_DOM1_PGS_REG", integer,
				header.init_PI_BSB_DOM1_PGS_REG);
			LUA_SET_FIELD(L, -1, "init_PI_BSB_DOM1_PWD_REG", integer,
				header.init_PI_BSB_DOM1_PWD_REG);
			LUA_SET_FIELD(L, -1, "init_PI_BSB_DOM1_PGS_REG2", integer,
				header.init_PI_BSB_DOM1_PGS_REG2);
			LUA_SET_FIELD(L, -1, "ClockRate", integer, header.ClockRate);
			LUA_SET_FIELD(L, -1, "PC",        integer, header.PC);
			LUA_SET_FIELD(L, -1, "Release",   integer, header.Release);
			LUA_SET_FIELD(L, -1, "CRC1",      integer, header.CRC1);
			LUA_SET_FIELD(L, -1, "CRC2",      integer, header.CRC2);
			LUA_SET_FIELD(L, -1, "_x18",      integer, header.Unknown[0]);
			LUA_SET_FIELD(L, -1, "_x1C",      integer, header.Unknown[1]);
			LUA_SET_FIELD(L, -1, "Name", string,  name);
			LUA_SET_FIELD(L, -1, "_x34", integer, header.unknown);
			LUA_SET_FIELD(L, -1, "Manufacturer_ID", integer,
				header.Manufacturer_ID);
			LUA_SET_FIELD(L, -1, "Cartridge_ID", integer, header.Cartridge_ID);
			LUA_SET_FIELD(L, -1, "Country_code", integer, header.Country_code);
			break;
		}

		case ROM_FIELD_SETTINGS: {
			m64p_rom_settings settings;
			if(CoreDoCommand(M64CMD_ROM_GET_SETTINGS,
			  sizeof(settings), &settings)) {
				lua_pushnil(L);
				break;
			}

			lua_createtable(L, 0, 6); //-1: tbl
			LUA_SET_FIELD(L, -1, "goodname", string,  settings.goodname);
			LUA_SET_FIELD(L, -1, "MD5",      string,  settings.MD5);
			LUA_SET_FIELD(L, -1, "savetype", integer, settings.savetype);
			LUA_SET_FIELD(L, -1, "status",   integer, settings.status);
			LUA_SET_FIELD(L, -1, "players",  integer, settings.players);
			LUA_SET_FIELD(L, -1, "rumble",   boolean, settings.rumble);
			break;
		}

		default:
			lua_pushnil(L);
	}

	return 1;
}

static int emu_run(lua_State *L) {
	return m64p_lua_return_errcode(L,
		CoreDoCommand(M64CMD_EXECUTE, 0, NULL));
}

static int emu_stop(lua_State *L) {
	return m64p_lua_return_errcode(L,
		CoreDoCommand(M64CMD_STOP, 0, NULL));
}

static int emu_pause(lua_State *L) {
	return m64p_lua_return_errcode(L,
		CoreDoCommand(M64CMD_PAUSE, 0, NULL));
}

static int emu_resume(lua_State *L) {
	return m64p_lua_return_errcode(L,
		CoreDoCommand(M64CMD_RESUME, 0, NULL));
}

static int emu_register_callback(lua_State *L) {
	luaL_checktype(L, 2, LUA_TFUNCTION);

	char tname[256];
	const char *name = luaL_checkstring(L, 1);
	snprintf(tname, sizeof(tname), "%s_callbacks", name);

	lua_getfield(L, LUA_REGISTRYINDEX, tname);
	if(!lua_istable(L, -1)) return luaL_error(L, "Invalid callback ID");
	//-1: callbacks tbl

	//scan this table until we find an empty slot
	int i=0, done=0;
	while(!done) {
		lua_rawgeti(L, -1, ++i); //-1: slot, -2: tbl
		if(!lua_isfunction(L, -1)) done = 1;
		lua_pop(L, 1); //-1: tbl
	}

	lua_pushvalue(L, 2); //-1: func, -2: tbl
	lua_rawseti(L, -2, i); //-1: tbl
	lua_pop(L, 1); //remove tbl

	lua_pushboolean(L, 1);
	return 1;
}

static int emu_unregister_callback(lua_State *L) {
	luaL_checktype(L, 2, LUA_TFUNCTION);

	char tname[256];
	const char *name = luaL_checkstring(L, 1);
	snprintf(tname, sizeof(tname), "%s_callbacks", name);

	lua_getfield(L, LUA_REGISTRYINDEX, tname);
	if(!lua_istable(L, -1)) return luaL_error(L, "Invalid callback ID");
	//-1: callbacks tbl

	//scan this table until we find the given function.
	int i=0, done=0, found=0;
	while(!done) {
		lua_rawgeti(L, -1, ++i); //-1: slot, -2: tbl
		if(lua_isnil(L, -1)) done = 1;
		else if(lua_compare(L, -1, 2, LUA_OPEQ)) { //value == arg2
			//replace with a non-function, non-nil value.
			lua_pushboolean(L, 0); //-1: false, -2: slot, -3: tbl
			lua_rawseti(L, -3, i); //-1: slot, -2: tbl
			found = 1; done = 1;
		}
		lua_pop(L, 1); //-1: tbl
	}

	lua_pop(L, 1); //remove tbl
	lua_pushboolean(L, found);
	return 1;
}


static int mem_meta_index(lua_State *L) {
	if(lua_isinteger(L, 2)) {
		uint32 addr = luaL_checkinteger(L, 2);
		lua_pushinteger(L, read_memory_8(addr));
	}
	else {
		lua_getfield(L, LUA_REGISTRYINDEX, "memory_methods"); //-1: methods
		lua_pushvalue(L, 2); //-1: key, -2: methods
		lua_gettable(L, -2); //-1: val, -2: methods
		lua_remove(L, -2); //-1: val
	}
	return 1;
}


static int mem_meta_newindex(lua_State *L) {
	if(lua_isinteger(L, 2)) {
		uint32 addr = luaL_checkinteger(L, 2);
		uint8  val  = luaL_checkinteger(L, 3);
		write_memory_8(addr, val);
		return 0;
	}
	else if(lua_tostring(L, 2))
		return luaL_error(L, "Cannot assign to field '%s' in memory",
		lua_tostring(L, 2));
	else return luaL_error(L, "Cannot assign to field (%s) in memory",
		luaL_typename(L, 2));
	return 0;
}


static int mem_method_read(lua_State *L) {
	u32 addr = luaL_checkinteger(L, 2);
	if(lua_isinteger(L, 3)) { //read specified number of bytes as string
		int len = lua_tointeger(L, 3);
		if(len < 1) {
			lua_pushnil(L);
			lua_pushstring(L, "Invalid length");
			return 2;
		}

		//this could be used if the host system byte order allowed for it.
		//XXX will this work with TLB?
		/* if(addr >= 0x80000000 && (addr+len) <= 0x80800000) {
			lua_pushlstring(L, (const char*)(rdramb + (addr & 0xFFFFFF)), len);
		}

		//XXX verify address range
		else if(addr >= 0xB0000000 && (addr+len) <= 0xB4000000) {
			lua_pushlstring(L, (const char*)(rom + (addr & 0xFFFFFF)), len);
		}

		else */ { //fall back to slow method.
			luaL_Buffer buf;
			luaL_buffinitsize(L, &buf, len);
			int i; for(i=0; i<len; i++) {
				luaL_addchar(&buf, read_memory_8(addr+i));
			}
			luaL_pushresultsize(&buf, len);
		}
	}
	else { //must be a variable type name
		lua_getfield(L, LUA_REGISTRYINDEX, "mem_types"); //-1: types
		lua_pushvalue(L, 3); //-1: key, -2: types
		lua_gettable(L, -2); //-1: val, -2: types
		int tp = luaL_optinteger(L, -1, -1);
		lua_pop(L, 2);

		switch(tp) {
			case MEM_TYPE_S8:
				lua_pushinteger(L, (s8)read_memory_8(addr));
				break;

			case MEM_TYPE_U8:
				lua_pushinteger(L, (u8)read_memory_8(addr));
				break;

			case MEM_TYPE_S16:
				lua_pushinteger(L, (s16)read_memory_16(addr));
				break;

			case MEM_TYPE_U16:
				lua_pushinteger(L, (u16)read_memory_16(addr));
				break;

			case MEM_TYPE_S32:
				lua_pushinteger(L, (s32)read_memory_32(addr));
				break;

			case MEM_TYPE_U32:
				lua_pushinteger(L, (u32)read_memory_32(addr));
				break;

			case MEM_TYPE_FLOAT: {
				u32 num = read_memory_32_unaligned(addr);
				lua_pushnumber(L, *(float*)&num);
				break;
			}

			case MEM_TYPE_DOUBLE: {
				u64 num = read_memory_64_unaligned(addr);
				lua_pushnumber(L, *(double*)&num);
				break;
			}

			default:
				lua_pushnil(L);
				lua_pushstring(L, "Invalid type");
				return 2;
		}
	}
	return 1;
}


static int mem_method_write(lua_State *L) {
	u32 addr = luaL_checkinteger(L, 2);

	lua_getfield(L, LUA_REGISTRYINDEX, "mem_types"); //-1: types
	lua_pushvalue(L, 3); //-1: key, -2: types
	lua_gettable(L, -2); //-1: val, -2: types
	int tp = luaL_optinteger(L, -1, -1);
	lua_pop(L, 2);

	switch(tp) {
		case MEM_TYPE_S8:
			write_memory_8(addr, (s8)luaL_checkinteger(L, 4));
			break;

		case MEM_TYPE_U8:
			write_memory_8(addr, (u8)luaL_checkinteger(L, 4));
			break;

		case MEM_TYPE_S16:
			write_memory_16(addr, (s16)luaL_checkinteger(L, 4));
			break;

		case MEM_TYPE_U16:
			write_memory_16(addr, (u16)luaL_checkinteger(L, 4));
			break;

		case MEM_TYPE_S32:
			write_memory_32_unaligned(addr, (s32)luaL_checkinteger(L, 4));
			break;

		case MEM_TYPE_U32:
			write_memory_32_unaligned(addr, (u32)luaL_checkinteger(L, 4));
			break;

		case MEM_TYPE_FLOAT: {
			float num = (float)luaL_checknumber(L, 4);
			write_memory_32_unaligned(addr, *(u32*)&num);
			break;
		}

		case MEM_TYPE_DOUBLE: {
			double num = (double)luaL_checknumber(L, 4);
			write_memory_64_unaligned(addr, *(u64*)&num);
			break;
		}

		case MEM_TYPE_STRING: {
			size_t len;
			const char *str = luaL_checklstring(L, 4, &len);
			int i; for(i=0; i<len; i++) write_memory_8(addr+i, str[i]);
			break;
		}

		default:
			lua_pushnil(L);
			lua_pushstring(L, "Invalid type");
			return 2;
	}

	return 0;
}


void m64p_lua_load_libs(lua_State *L) {
	int i;

	//table of ROM field names
	lua_createtable(L, 0, NUM_ROM_FIELDS);
	for(i=0; romFieldName[i]; i++) {
		lua_pushinteger(L, i);
		lua_setfield(L, -2, romFieldName[i]);
	}
	lua_setfield(L, LUA_REGISTRYINDEX, "rom_fields");


	//table of memory access variable types.
	lua_createtable(L, 0, NUM_MEM_TYPES);
	for(i=0; memTypeName[i]; i++) {
		lua_pushinteger(L, i);
		lua_setfield(L, -2, memTypeName[i]);
	}
	lua_setfield(L, LUA_REGISTRYINDEX, "mem_types");


	//callback tables
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "vi_callbacks");
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "render_callbacks");


	//global m64p table
	static const luaL_Reg funcs_m64p[] = {
		{"run",                emu_run},
		{"stop",               emu_stop},
		{"pause",              emu_pause},
		{"resume",             emu_resume},
		{"registerCallback",   emu_register_callback},
		{"unregisterCallback", emu_unregister_callback},
		{NULL, NULL}
	};
	luaL_newlib(L, funcs_m64p); //-1: m64p


	//m64p.rom table
	static const luaL_Reg funcs_rom[] = {
		{"open",   rom_open},
		{"close",  rom_close},
		{NULL, NULL}
	};
	luaL_newlib(L, funcs_rom); //-1: rom, -2: m64p

	//m64p.rom metatable
	static const luaL_Reg meta_rom[] = {
		{"__index", rom_meta_index},
		{NULL, NULL}
	};
	luaL_newlib(L, meta_rom); //-1: meta, -2: rom, -3: m64p
	lua_setmetatable(L, -2); //-1: rom, -2: m64p
	lua_setfield(L, -2, "rom"); //-1: m64p


	//m64p.memory table
	static const luaL_Reg funcs_mem[] = {
		//{"read", mem_read},
		{NULL, NULL}
	};
	luaL_newlib(L, funcs_mem); //-1: mem, -2: m64p

	//m64p.memory metatable
	static const luaL_Reg meta_mem[] = {
		{"__index",    mem_meta_index},
		{"__newindex", mem_meta_newindex},
		{NULL, NULL}
	};
	luaL_newlib(L, meta_mem); //-1: meta, -2: mem, -3: m64p
	lua_setmetatable(L, -2); //-1: mem, -2: m64p
	lua_setfield(L, -2, "memory"); //-1: m64p

	//m64p.memory methods
	static const luaL_Reg methods_mem[] = {
		{"read",  mem_method_read},
		{"write", mem_method_write},
		{NULL, NULL}
	};
	luaL_newlib(L, methods_mem); //-1: methods, -2: m64p
	lua_setfield(L, LUA_REGISTRYINDEX, "memory_methods"); //-1: m64p


	//install m64p table as global variable
	lua_setglobal(L, "m64p");
}
