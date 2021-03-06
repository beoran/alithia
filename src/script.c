/*
 * Alithia Engine
 * Copyright (C) 2009-2010 Kostas Michalopoulos
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Kostas Michalopoulos <badsector@runtimeterror.com>
 */

#include "atest.h"

typedef struct _objref_t
{
    void* obj;
    int type;
} objref_t;

typedef struct _execat_entry_t
{
    char* event;
    char* id;
    lil_value_t code;
} execat_entry_t;

typedef struct _regvar_t
{
    char* name;
    int type;
    void* ptr;
} regvar_t;

static objref_t* ref;
static size_t refs;
static list_t* execats;
static list_t* regvars;

lil_t lil;

/* Object references */
size_t or_new(int type, void* obj)
{
    size_t i;
    if (!obj) return 0;
    for (i=0; i<refs; i++)
        if (ref[i].obj == obj) return i + 1;
    for (i=0; i<refs; i++)
        if (!ref[i].type) {
            ref[i].type = type;
            ref[i].obj = obj;
            return i + 1;
        }
    ref = realloc(ref, sizeof(objref_t)*(refs + 1));
    ref[refs].type = type;
    ref[refs].obj = obj;
    return ++refs;
}

void* or_get(int type, size_t idx)
{
    if (!idx || idx > refs) return NULL;
    if (ref[idx - 1].type == OT_NOTHING) return NULL;
    return ref[idx - 1].obj;
}

void or_release(size_t idx)
{
    if (!idx || idx > refs) return;
    ref[idx - 1].type = OT_NOTHING;
}

void or_deleted(void* obj)
{
    size_t i;
    for (i=0; i<refs; i++)
        if (ref[i].obj == obj) {
            or_release(i);
            return;
        }
}

static void or_free(void)
{
    free(ref);
    ref = NULL;
    refs = 0;
}

/* Engine procs */
static lil_value_t nat_exit(lil_t lil, size_t argc, lil_value_t* argv)
{
    running = 0;
    return NULL;
}

static lil_value_t nat_exec_at(lil_t lil, size_t argc, lil_value_t* argv)
{
    execat_entry_t* e;
    listitem_t* li;
    const char* id;
    const char* event;
    if (argc < 3) {
        console_write("not enough arguments in exec-at"); console_newline();
        return NULL;
    }
    event = lil_to_string(argv[0]);
    id = lil_to_string(argv[1]);
    for (li=execats->first; li; li=li->next) {
        e = li->ptr;
        if (!strcmp(e->id, id) && !strcmp(e->event, event)) {
            lil_free_value(e->code);
            e->code = lil_clone_value(argv[2]);
        }
    }
    e = new(execat_entry_t);
    e->event = strdup(event);
    e->id = strdup(id);
    e->code = lil_clone_value(argv[2]);
    list_add(execats, e);
    return NULL;
}

static lil_value_t nat_clear(lil_t lil, size_t argc, lil_value_t* argv)
{
    console_clear();
    return NULL;
}

static lil_value_t nat_cellsize(lil_t lil, size_t argc, lil_value_t* argv)
{
    return lil_alloc_integer(CELLSIZE);
}

static lil_value_t nat_run_script(lil_t lil, size_t argc, lil_value_t* argv)
{
    if (argc < 1) {
        console_write("not enough arguments in run-script"); console_newline();
        return NULL;
    }
    script_run(lil_to_string(argv[0]));
    return NULL;
}

static lil_value_t nat_load_model(lil_t lil, size_t argc, lil_value_t* argv)
{
    model_t* mdl;
    if (argc < 1) {
        console_write("not enough arguments in load-model"); console_newline();
        return NULL;
    }
    mdl = modelcache_get(lil_to_string(argv[0]));
    if (!mdl) return NULL;
    return lil_alloc_integer(or_new(OT_MODEL, mdl));
}

static lil_value_t nat_load_texture(lil_t lil, size_t argc, lil_value_t* argv)
{
    texture_t* tex;
    if (argc < 1) {
        console_write("not enough arguments in load-texture"); console_newline();
        return NULL;
    }
    tex = texbank_get(lil_to_string(argv[0]));
    if (!tex) return NULL;
    return lil_alloc_integer(or_new(OT_TEXTURE, tex));
}

static lil_value_t nat_model_cache_clear(lil_t lil, size_t argc, lil_value_t* argv)
{
    modelcache_clear();
    return NULL;
}

static void reg_engine_procs(void)
{
    lil_register(lil, "exit", nat_exit);
    lil_register(lil, "exec-at", nat_exec_at);
    lil_register(lil, "clear", nat_clear);
    lil_register(lil, "cellsize", nat_cellsize);
    lil_register(lil, "run-script", nat_run_script);
    lil_register(lil, "load-model", nat_load_model);
    lil_register(lil, "load-texture", nat_load_texture);
    lil_register(lil, "model-cache-clear", nat_model_cache_clear);
}

/* Editor procs */
static lil_value_t nat_relight(lil_t lil, size_t argc, lil_value_t* argv)
{
    lmap_update();
    return NULL;
}

static lil_value_t nat_lightmap_quality(lil_t lil, size_t argc, lil_value_t* argv)
{
    int q;
    if (argc < 2) {
        console_write("not enough arguments in lightmap-quality"); console_newline();
        return NULL;
    }
    q = lil_to_integer(argv[0]);
    if (q < 0) q = 0; else if (q > 2) q = 2;
    lmap_quality = q;
    return NULL;
}

static lil_value_t nat_get_cursor(lil_t lil, size_t argc, lil_value_t* argv)
{
    int x, y;
    lil_value_t val;
    lil_list_t list;
    editor_get_cursor(&x, &y);
    list = lil_alloc_list();
    lil_list_append(list, lil_alloc_integer(x));
    lil_list_append(list, lil_alloc_integer(y));
    val = lil_list_to_value(list, 0);
    lil_free_list(list);
    return val;
}

static lil_value_t nat_get_cursor_in_world(lil_t lil, size_t argc, lil_value_t* argv)
{
    int x, y;
    lil_value_t val;
    lil_list_t list;
    editor_get_cursor(&x, &y);
    list = lil_alloc_list();
    lil_list_append(list, lil_alloc_integer(x*CELLSIZE + CELLSIZE*0.5));
    lil_list_append(list, lil_alloc_integer(cell[y*map_width + x].floorz));
    lil_list_append(list, lil_alloc_integer(y*CELLSIZE + CELLSIZE*0.5));
    val = lil_list_to_value(list, 0);
    lil_free_list(list);
    return val;
}

static void apply_cell_modifier_modifier(int cx, int cy, cell_t* cell, void* data)
{
    lil_set_var(lil, "cellx", lil_alloc_integer(cx), LIL_SETVAR_LOCAL);
    lil_set_var(lil, "celly", lil_alloc_integer(cy), LIL_SETVAR_LOCAL);
    lil_free_value(lil_parse_value(lil, data, 0));
}

static lil_value_t nat_apply_cell_modifier(lil_t lil, size_t argc, lil_value_t* argv)
{
    if (argc < 1) {
        console_write("not enough arguments in apply-cell-modifier"); console_newline();
        return NULL;
    }
    editor_apply_cell_modifier(apply_cell_modifier_modifier, argv[0]);
    return NULL;
}

static lil_value_t nat_foreach_cell(lil_t lil, size_t argc, lil_value_t* argv)
{
    int x1, y1, x2, y2, x, y;
    if (argc < 1) {
        console_write("not enough arguments in foreach-cell"); console_newline();
        return NULL;
    }
    editor_get_selection(&x1, &y1, &x2, &y2);
    for (y=y1; y<=y2; y++) {
        for (x=x1; x<=x2; x++) {
            lil_set_var(lil, "cellx", lil_alloc_integer(x), LIL_SETVAR_LOCAL);
            lil_set_var(lil, "celly", lil_alloc_integer(y), LIL_SETVAR_LOCAL);
            lil_free_value(lil_parse_value(lil, argv[0], 0));
        }
    }
    return NULL;
}

static lil_value_t nat_get_selection(lil_t lil, size_t argc, lil_value_t* argv)
{
    int x1, y1, x2, y2;
    lil_value_t val;
    lil_list_t list;
    editor_get_selection(&x1, &y1, &x2, &y2);
    list = lil_alloc_list();
    lil_list_append(list, lil_alloc_integer(x1));
    lil_list_append(list, lil_alloc_integer(y1));
    lil_list_append(list, lil_alloc_integer(x2));
    lil_list_append(list, lil_alloc_integer(y2));
    val = lil_list_to_value(list, 0);
    lil_free_list(list);
    return val;
}

static lil_value_t nat_set_selection(lil_t lil, size_t argc, lil_value_t* argv)
{
    if (argc < 4) {
        console_write("not enough arguments in set-selection"); console_newline();
        return NULL;
    }
    editor_set_selection(lil_to_integer(argv[0]), lil_to_integer(argv[1]), lil_to_integer(argv[2]), lil_to_integer(argv[3]));
    return NULL;
}

static lil_value_t nat_get_pick_type(lil_t lil, size_t argc, lil_value_t* argv)
{
    pickdata_t* pd = editor_get_pickdata();
    if (pd->nopick) return lil_alloc_string("nothing");
    switch (pd->result) {
    case PICK_ENTITY: return lil_alloc_string("entity");
    case PICK_WORLD: return lil_alloc_string("world");
    case PICK_LIGHT: return lil_alloc_string("light");
    }
    return NULL;
}

static lil_value_t nat_get_picked_light(lil_t lil, size_t argc, lil_value_t* argv)
{
    pickdata_t* pd = editor_get_pickdata();
    if (pd->nopick || pd->result != PICK_LIGHT) return NULL;
    return lil_alloc_integer(or_new(OT_LIGHT, pd->light));
}

static lil_value_t nat_get_picked_entity(lil_t lil, size_t argc, lil_value_t* argv)
{
    pickdata_t* pd = editor_get_pickdata();
    if (pd->nopick || pd->result != PICK_ENTITY) return NULL;
    return lil_alloc_integer(or_new(OT_ENTITY, pd->entity));
}

static lil_value_t nat_get_picked_cell(lil_t lil, size_t argc, lil_value_t* argv)
{
    pickdata_t* pd = editor_get_pickdata();
    lil_value_t val;
    lil_list_t list;
    list = lil_alloc_list();
    lil_list_append(list, lil_alloc_integer(pd->cellx));
    lil_list_append(list, lil_alloc_integer(pd->celly));
    val = lil_list_to_value(list, 0);
    lil_free_list(list);
    return val;
}

static lil_value_t nat_get_picked_point(lil_t lil, size_t argc, lil_value_t* argv)
{
    pickdata_t* pd = editor_get_pickdata();
    lil_value_t val;
    lil_list_t list;
    list = lil_alloc_list();
    lil_list_append(list, lil_alloc_integer(pd->ip.x));
    lil_list_append(list, lil_alloc_integer(pd->ip.y));
    lil_list_append(list, lil_alloc_integer(pd->ip.z));
    val = lil_list_to_value(list, 0);
    lil_free_list(list);
    return val;
}

static lil_value_t nat_script_menu_add(lil_t lil, size_t argc, lil_value_t* argv)
{
    if (argc < 2) {
        console_write("not enough arguments in script-menu-add"); console_newline();
        return NULL;
    }
    editor_scriptmenu_add(lil_to_string(argv[0]), argv[1]);
    return NULL;
}

static void reg_editor_procs(void)
{
    lil_register(lil, "relight", nat_relight);
    lil_register(lil, "lightmap-quality", nat_lightmap_quality);
    lil_register(lil, "get-cursor", nat_get_cursor);
    lil_register(lil, "get-cursor-in-world", nat_get_cursor_in_world);
    lil_register(lil, "apply-cell-modifier", nat_apply_cell_modifier);
    lil_register(lil, "foreach-cell", nat_foreach_cell);
    lil_register(lil, "get-selection", nat_get_selection);
    lil_register(lil, "set-selection", nat_set_selection);
    lil_register(lil, "get-pick-type", nat_get_pick_type);
    lil_register(lil, "get-picked-light", nat_get_picked_light);
    lil_register(lil, "get-picked-entity", nat_get_picked_entity);
    lil_register(lil, "get-picked-cell", nat_get_picked_cell);
    lil_register(lil, "get-picked-point", nat_get_picked_point);
    lil_register(lil, "script-menu-add", nat_script_menu_add);
}

/* World procs */
static lil_value_t nat_save(lil_t lil, size_t argc, lil_value_t* argv)
{
    lil_value_t filename;
    int i;
    if (argc < 1) {
        console_write("not enough arguments in save"); console_newline();
        return NULL;
    }
    filename = lil_alloc_string("worlds/");
    lil_append_val(filename, argv[0]);
    lil_append_string(filename, ".alw");
    i = world_save(lil_to_string(filename));
    if (!i) {
        console_write("failed to save the world in ");
        console_write(lil_to_string(filename));
        console_newline();
    }
    lil_free_value(filename);
    return lil_alloc_integer(i);
}

static lil_value_t nat_load(lil_t lil, size_t argc, lil_value_t* argv)
{
    lil_value_t filename;
    int i;
    if (argc < 1) {
        console_write("not enough arguments in load"); console_newline();
        return NULL;
    }
    filename = lil_alloc_string("worlds/");
    lil_append_val(filename, argv[0]);
    lil_append_string(filename, ".alw");
    i = world_load(lil_to_string(filename));
    if (!i) {
        console_write("failed to load the world from ");
        console_write(lil_to_string(filename));
        console_newline();
    }
    lil_free_value(filename);
    return lil_alloc_integer(i);
}

static lil_value_t nat_get_map_size(lil_t lil, size_t argc, lil_value_t* argv)
{
    char tmp[64];
    sprintf(tmp, "%i %i", map_width, map_height);
    return lil_alloc_string(tmp);
}

static lil_value_t nat_get_entity_count(lil_t lil, size_t argc, lil_value_t* argv)
{
    return lil_alloc_integer(ents->count);
}

static lil_value_t nat_get_light_count(lil_t lil, size_t argc, lil_value_t* argv)
{
    return lil_alloc_integer(lights->count);
}

static lil_value_t nat_new_raw_entity(lil_t lil, size_t argc, lil_value_t* argv)
{
    return lil_alloc_integer(or_new(OT_ENTITY, ent_new()));
}

static lil_value_t nat_new_entity(lil_t lil, size_t argc, lil_value_t* argv)
{
    if (argc < 1) {
        console_write("not enough arguments in new-entity"); console_newline();
        return NULL;
    }
    return lil_alloc_integer(or_new(OT_ENTITY, ent_new_by_class(lil_to_string(argv[0]))));
}

static lil_value_t nat_del_entity(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 1) {
        console_write("not enough arguments in del-entity"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    ent_free(ent);
    return NULL;
}

static lil_value_t nat_set_entity_model(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    model_t* mdl;
    if (argc < 2) {
        console_write("not enough arguments in set-entity-model"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    mdl = or_get(OT_MODEL, lil_to_integer(argv[1]));
    if (!mdl) {
        console_write("invalid model reference"); console_newline();
        return NULL;
    }
    ent_set_model(ent, mdl);
    return NULL;
}

static lil_value_t nat_set_entity_position(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 4) {
        console_write("not enough arguments in set-entity-position"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    ent_move(ent, lil_to_double(argv[1]), lil_to_double(argv[2]), lil_to_double(argv[3]));
    return NULL;
}

static lil_value_t nat_set_entity_rotation(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 2) {
        console_write("not enough arguments in set-entity-rotation"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    ent->yrot = lil_to_double(argv[1]);
    ent_update(ent);
    return NULL;
}

static lil_value_t nat_set_entity_attribute(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 3) {
        console_write("not enough arguments in set-entity-attribute"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    ent_set_attr(ent, argv[1], argv[2]);
    return NULL;
}

static lil_value_t nat_get_entity_model(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 1) {
        console_write("not enough arguments in get-entity-model"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    return lil_alloc_integer(or_new(OT_MODEL, ent->mdl));
}

static lil_value_t nat_get_entity_position(lil_t lil, size_t argc, lil_value_t* argv)
{
    char tmp[128];
    entity_t* ent;
    if (argc < 1) {
        console_write("not enough arguments in get-entity-position"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    sprintf(tmp, "%f %f %f", ent->p.x, ent->p.y, ent->p.z);
    return lil_alloc_string(tmp);
}

static lil_value_t nat_get_entity_rotation(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 1) {
        console_write("not enough arguments in get-entity-rotation"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    return lil_alloc_double(ent->yrot);
}

static lil_value_t nat_get_entity_attribute(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 2) {
        console_write("not enough arguments in get-entity-attribute"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    return ent_get_attr(ent, argv[1]);
}

static lil_value_t nat_call_entity_attribute(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 2) {
        console_write("not enough arguments in call-entity-attribute"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    return ent_call_attr(ent, lil_to_string(argv[1]));
}

static lil_value_t nat_get_entity_class(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 1) {
        console_write("not enough arguments in get-entity-class"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    return lil_alloc_string(ent->class);
}

static lil_value_t nat_set_entity_class(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 2) {
        console_write("not enough arguments in set-entity-class"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    free(ent->class);
    ent->class = strdup(lil_to_string(argv[1]));
    return lil_alloc_string(ent->class);
}

static lil_value_t nat_set_cell_texture(lil_t lil, size_t argc, lil_value_t* argv)
{
    int part, cx, cy;
    texture_t* tex;
    if (argc < 4) {
        console_write("not enough arguments in set-cell-texture"); console_newline();
        return NULL;
    }
    part = lil_to_integer(argv[0]);
    cx = lil_to_integer(argv[1]);
    cy = lil_to_integer(argv[2]);
    if (part < 0 || part > 3) {
        console_write("invalid part number (must be 0 to 3) in set-cell-texture"); console_newline();
        return NULL;
    }
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (lil_to_integer(argv[3])) {
        tex = or_get(OT_TEXTURE, lil_to_integer(argv[3]));
        if (!tex) {
            console_write("invalid texture reference"); console_newline();
            return NULL;
        }
    } else tex = NULL;
    switch (part) {
    case 0: cell[cy*map_width + cx].toptex = tex; break;
    case 1: cell[cy*map_width + cx].uppertex = tex; break;
    case 2: cell[cy*map_width + cx].lowetex = tex; break;
    case 3: cell[cy*map_width + cx].bottomtex = tex; break;
    }
    return NULL;
}

static lil_value_t nat_set_cell_floor_height(lil_t lil, size_t argc, lil_value_t* argv)
{
    int cx, cy;
    if (argc < 3) {
        console_write("not enough arguments in set-cell-floor-height"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    cell[cy*map_width + cx].floorz = lil_to_integer(argv[2]);
    return NULL;
}

static lil_value_t nat_set_cell_ceiling_height(lil_t lil, size_t argc, lil_value_t* argv)
{
    int cx, cy;
    if (argc < 3) {
        console_write("not enough arguments in set-cell-ceiling-height"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    cell[cy*map_width + cx].ceilz = lil_to_integer(argv[2]);
    return NULL;
}

static lil_value_t nat_set_cell_floor_offset(lil_t lil, size_t argc, lil_value_t* argv)
{
    int offidx, cx, cy;
    if (argc < 4) {
        console_write("not enough arguments in set-cell-floor-offset"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    offidx = lil_to_integer(argv[2]);
    if (offidx < 0 || offidx > 3) {
        console_write("invalid offset index (must be 0 to 3) in set-cell-floor-offset"); console_newline();
        return NULL;
    }
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    cell[cy*map_width + cx].zfoffs[offidx] = lil_to_integer(argv[3]);
    return NULL;
}

static lil_value_t nat_set_cell_ceiling_offset(lil_t lil, size_t argc, lil_value_t* argv)
{
    int offidx, cx, cy;
    if (argc < 4) {
        console_write("not enough arguments in set-cell-ceiling-offset"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    offidx = lil_to_integer(argv[2]);
    if (offidx < 0 || offidx > 3) {
        console_write("invalid offset index (must be 0 to 3) in set-cell-ceiling-offset"); console_newline();
        return NULL;
    }
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    cell[cy*map_width + cx].zcoffs[offidx] = lil_to_integer(argv[3]);
    return NULL;
}

static lil_value_t nat_reset_cell_floor_offsets(lil_t lil, size_t argc, lil_value_t* argv)
{
    int i, cx, cy;
    if (argc < 2) {
        console_write("not enough arguments in reset-cell-floor-offsets"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    for (i=0; i<4; i++)
        cell[cy*map_width + cx].zfoffs[i] = 0;
    return NULL;
}

static lil_value_t nat_reset_cell_ceiling_offsets(lil_t lil, size_t argc, lil_value_t* argv)
{
    int i, cx, cy;
    if (argc < 2) {
        console_write("not enough arguments in reset-cell-ceiling-offsets"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    for (i=0; i<4; i++)
        cell[cy*map_width + cx].zcoffs[i] = 0;
    return NULL;
}

static lil_value_t nat_update_cell(lil_t lil, size_t argc, lil_value_t* argv)
{
    int cx, cy;
    if (argc < 2) {
        console_write("not enough arguments in update-cell"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    map_update_cell(cx, cy);
    return NULL;
}

static lil_value_t nat_get_cell_texture(lil_t lil, size_t argc, lil_value_t* argv)
{
    int part, cx, cy;
    texture_t* tex = NULL;
    if (argc < 3) {
        console_write("not enough arguments in get-cell-texture"); console_newline();
        return NULL;
    }
    part = lil_to_integer(argv[0]);
    cx = lil_to_integer(argv[1]);
    cy = lil_to_integer(argv[2]);
    if (part < 0 || part > 3) {
        console_write("invalid part number (must be 0 to 3) in get-cell-texture"); console_newline();
        return NULL;
    }
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    switch (part) {
    case 0: tex = cell[cy*map_width + cx].toptex; break;
    case 1: tex = cell[cy*map_width + cx].uppertex; break;
    case 2: tex = cell[cy*map_width + cx].lowetex; break;
    case 3: tex = cell[cy*map_width + cx].bottomtex; break;
    default: return NULL;
    }
    return lil_alloc_integer(or_new(OT_TEXTURE, tex));
}

static lil_value_t nat_get_cell_floor_height(lil_t lil, size_t argc, lil_value_t* argv)
{
    int cx, cy;
    if (argc < 2) {
        console_write("not enough arguments in get-cell-floor-height"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    return lil_alloc_integer(cell[cy*map_width + cx].floorz);
}

static lil_value_t nat_get_cell_ceiling_height(lil_t lil, size_t argc, lil_value_t* argv)
{
    int cx, cy;
    if (argc < 2) {
        console_write("not enough arguments in get-cell-ceiling-height"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    return lil_alloc_integer(cell[cy*map_width + cx].ceilz);
}

static lil_value_t nat_get_cell_floor_offset(lil_t lil, size_t argc, lil_value_t* argv)
{
    int offidx, cx, cy;
    if (argc < 3) {
        console_write("not enough arguments in get-cell-floor-offset"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    offidx = lil_to_integer(argv[2]);
    if (offidx < 0 || offidx > 3) {
        console_write("invalid offset index (must be 0 to 3) in get-cell-floor-offset"); console_newline();
        return NULL;
    }
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    return lil_alloc_integer(cell[cy*map_width + cx].zfoffs[offidx]);
}

static lil_value_t nat_get_cell_ceiling_offset(lil_t lil, size_t argc, lil_value_t* argv)
{
    int offidx, cx, cy;
    if (argc < 3) {
        console_write("not enough arguments in get-cell-ceiling-offset"); console_newline();
        return NULL;
    }
    cx = lil_to_integer(argv[0]);
    cy = lil_to_integer(argv[1]);
    offidx = lil_to_integer(argv[2]);
    if (offidx < 0 || offidx > 3) {
        console_write("invalid offset index (must be 0 to 3) in get-cell-ceiling-offset"); console_newline();
        return NULL;
    }
    if (cx < 0 || cx >= map_width) {
        console_write("cell column (x coordinate) out of map range"); console_newline();
        return NULL;
    }
    if (cy < 0 || cy >= map_height) {
        console_write("cell row (y coordinate) out of map range"); console_newline();
        return NULL;
    }
    return lil_alloc_integer(cell[cy*map_width + cx].zcoffs[offidx]);
}

static lil_value_t nat_new_light(lil_t lil, size_t argc, lil_value_t* argv)
{
    float x, y, z, r, g, b, rad = 32;
    light_t* light;
    if (argc < 3) {
        int cx, cy;
        editor_get_cursor(&cx, &cy);
        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        if (cx >= map_width) cx = map_width - 1;
        if (cy >= map_height) cy = map_height - 1;
        x = cx*CELLSIZE + CELLSIZE*0.5;
        y = cell[cy*map_width + cx].floorz + 16;
        z = cy*CELLSIZE + CELLSIZE*0.5;
        if (argc > 1) rad = lil_to_double(argv[0]);
    } else {
        x = lil_to_double(argv[0]);
        y = lil_to_double(argv[1]);
        z = lil_to_double(argv[2]);
        if (argc == 4) rad = lil_to_double(argv[3]);
        else if (argc >= 7) rad = lil_to_double(argv[6]);
    }
    if (argc < 6) {
        r = g = b = 0.5;
    } else {
        r = lil_to_double(argv[3]);
        g = lil_to_double(argv[4]);
        b = lil_to_double(argv[5]);
    }
    light = light_new(x, y, z, r, g, b, rad);
    return lil_alloc_integer(or_new(OT_LIGHT, light));
}

static lil_value_t nat_del_light(lil_t lil, size_t argc, lil_value_t* argv)
{
    light_t* light;
    size_t idx;
    if (argc < 1) {
        console_write("not enough arguments in del-light"); console_newline();
        return NULL;
    }
    light = or_get(OT_LIGHT, idx = lil_to_integer(argv[0]));
    if (!light) {
        console_write("invalid light reference"); console_newline();
        return NULL;
    }
    light_free(light);
    return NULL;
}

static lil_value_t nat_set_light_position(lil_t lil, size_t argc, lil_value_t* argv)
{
    light_t* light;
    size_t idx;
    if (argc < 4) {
        console_write("not enough arguments in set-light-position"); console_newline();
        return NULL;
    }
    light = or_get(OT_LIGHT, idx = lil_to_integer(argv[0]));
    if (!light) {
        console_write("invalid light reference"); console_newline();
        return NULL;
    }
    light->p.x = lil_to_double(argv[1]);
    light->p.y = lil_to_double(argv[2]);
    light->p.z = lil_to_double(argv[3]);
    return NULL;
}

static lil_value_t nat_set_light_color(lil_t lil, size_t argc, lil_value_t* argv)
{
    light_t* light;
    size_t idx;
    if (argc < 4) {
        console_write("not enough arguments in set-light-color"); console_newline();
        return NULL;
    }
    light = or_get(OT_LIGHT, idx = lil_to_integer(argv[0]));
    if (!light) {
        console_write("invalid light reference"); console_newline();
        return NULL;
    }
    light->r = lil_to_double(argv[1]);
    light->g = lil_to_double(argv[2]);
    light->b = lil_to_double(argv[3]);
    return NULL;
}

static lil_value_t nat_set_light_radius(lil_t lil, size_t argc, lil_value_t* argv)
{
    light_t* light;
    size_t idx;
    if (argc < 2) {
        console_write("not enough arguments in set-light-radius"); console_newline();
        return NULL;
    }
    light = or_get(OT_LIGHT, idx = lil_to_integer(argv[0]));
    if (!light) {
        console_write("invalid light reference"); console_newline();
        return NULL;
    }
    light->rad = lil_to_double(argv[1]);
    return NULL;
}

static lil_value_t nat_get_light_position(lil_t lil, size_t argc, lil_value_t* argv)
{
    char tmp[256];
    light_t* light;
    size_t idx;
    if (argc < 1) {
        console_write("not enough arguments in get-light-position"); console_newline();
        return NULL;
    }
    light = or_get(OT_LIGHT, idx = lil_to_integer(argv[0]));
    if (!light) {
        console_write("invalid light reference"); console_newline();
        return NULL;
    }
    sprintf(tmp, "%f %f %f", light->p.x, light->p.y, light->p.z);
    return lil_alloc_string(tmp);
}

static lil_value_t nat_get_light_color(lil_t lil, size_t argc, lil_value_t* argv)
{
    char tmp[256];
    light_t* light;
    size_t idx;
    if (argc < 1) {
        console_write("not enough arguments in get-light-color"); console_newline();
        return NULL;
    }
    light = or_get(OT_LIGHT, idx = lil_to_integer(argv[0]));
    if (!light) {
        console_write("invalid light reference"); console_newline();
        return NULL;
    }
    sprintf(tmp, "%f %f %f", light->r, light->g, light->b);
    return lil_alloc_string(tmp);
}

static lil_value_t nat_get_light_radius(lil_t lil, size_t argc, lil_value_t* argv)
{
    light_t* light;
    size_t idx;
    if (argc < 1) {
        console_write("not enough arguments in get-light-position"); console_newline();
        return NULL;
    }
    light = or_get(OT_LIGHT, idx = lil_to_integer(argv[0]));
    if (!light) {
        console_write("invalid light reference"); console_newline();
        return NULL;
    }
    return lil_alloc_double(light->rad);
}

static void reg_world_procs(void)
{
    lil_register(lil, "save", nat_save);
    lil_register(lil, "load", nat_load);
    lil_register(lil, "get-map-size", nat_get_map_size);
    lil_register(lil, "get-entity-count", nat_get_entity_count);
    lil_register(lil, "get-light-count", nat_get_light_count);
    lil_register(lil, "new-raw-entity", nat_new_raw_entity);
    lil_register(lil, "del-entity", nat_del_entity);
    lil_register(lil, "new-entity", nat_new_entity);
    lil_register(lil, "del-entity", nat_del_entity);
    lil_register(lil, "set-entity-model", nat_set_entity_model);
    lil_register(lil, "set-entity-position", nat_set_entity_position);
    lil_register(lil, "set-entity-rotation", nat_set_entity_rotation);
    lil_register(lil, "set-entity-attribute", nat_set_entity_attribute);
    lil_register(lil, "get-entity-model", nat_get_entity_model);
    lil_register(lil, "get-entity-position", nat_get_entity_position);
    lil_register(lil, "get-entity-rotation", nat_get_entity_rotation);
    lil_register(lil, "get-entity-attribute", nat_get_entity_attribute);
    lil_register(lil, "call-entity-attribute", nat_call_entity_attribute);
    lil_register(lil, "get-entity-class", nat_get_entity_class);
    lil_register(lil, "set-entity-class", nat_set_entity_class);
    lil_register(lil, "set-cell-texture", nat_set_cell_texture);
    lil_register(lil, "set-cell-floor-height", nat_set_cell_floor_height);
    lil_register(lil, "set-cell-ceiling-height", nat_set_cell_ceiling_height);
    lil_register(lil, "set-cell-floor-offset", nat_set_cell_floor_offset);
    lil_register(lil, "set-cell-ceiling-offset", nat_set_cell_ceiling_offset);
    lil_register(lil, "reset-cell-floor-offsets", nat_reset_cell_floor_offsets);
    lil_register(lil, "reset-cell-ceiling-offsets", nat_reset_cell_ceiling_offsets);
    lil_register(lil, "update-cell", nat_update_cell);
    lil_register(lil, "get-cell-texture", nat_get_cell_texture);
    lil_register(lil, "get-cell-floor-height", nat_get_cell_floor_height);
    lil_register(lil, "get-cell-ceiling-height", nat_get_cell_ceiling_height);
    lil_register(lil, "get-cell-floor-offset", nat_get_cell_floor_offset);
    lil_register(lil, "get-cell-ceiling-offset", nat_get_cell_ceiling_offset);
    lil_register(lil, "new-light", nat_new_light);
    lil_register(lil, "del-light", nat_del_light);
    lil_register(lil, "set-light-position", nat_set_light_position);
    lil_register(lil, "set-light-color", nat_set_light_color);
    lil_register(lil, "set-light-radius", nat_set_light_radius);
    lil_register(lil, "get-light-position", nat_get_light_position);
    lil_register(lil, "get-light-color", nat_get_light_color);
    lil_register(lil, "get-light-radius", nat_get_light_radius);
}

/* Game procs */
static lil_value_t nat_get_camera_entity(lil_t lil, size_t argc, lil_value_t* argv)
{
    return lil_alloc_integer(or_new(OT_ENTITY, camera_ent));
}

static lil_value_t nat_get_player_entity(lil_t lil, size_t argc, lil_value_t* argv)
{
    return lil_alloc_integer(or_new(OT_ENTITY, camera_ent));
}

static lil_value_t nat_init_entity_ai(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 1) {
        console_write("not enough arguments in init-entity-ai"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    ai_new(ent);
    return NULL;
}

static lil_value_t nat_init_entity_motion(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 1) {
        console_write("not enough arguments in init-entity-motion"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    mot_new(ent);
    return NULL;
}

static lil_value_t nat_apply_force_to_entity(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 4) {
        console_write("not enough arguments in apply-force-to-entity"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    if (!ent->mot) {
        console_write("entity has no motion controller. Use init-entity-motion first."); console_newline();
        return NULL;
    }
    mot_force(ent->mot, lil_to_double(argv[1]), lil_to_double(argv[2]), lil_to_double(argv[3]));
    return NULL;
}

static lil_value_t nat_set_entity_motion_option(lil_t lil, size_t argc, lil_value_t* argv)
{
    const char* opt;
    entity_t* ent;
    motion_t* mot;
    if (argc < 2) {
        console_write("not enough arguments in set-entity-motion-option"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    if (!ent->mot) {
        console_write("entity has no motion controller. Use init-entity-motion first."); console_newline();
        return NULL;
    }
    mot = ent->mot;
    opt = lil_to_string(argv[1]);
    if (!strcmp(opt, "constant-motion")) {
        if (argc < 5) {
            console_write("not enough arguments for constant-motion motion option"); console_newline();
            return NULL;
        }
        mot->c.x = lil_to_double(argv[2]);
        mot->c.y = lil_to_double(argv[3]);
        mot->c.z = lil_to_double(argv[4]);
        return NULL;
    }
    if (!strcmp(opt, "gravity")) {
        if (argc < 5) {
            console_write("not enough arguments for gravity motion option"); console_newline();
            return NULL;
        }
        mot->g.x = lil_to_double(argv[2]);
        mot->g.y = lil_to_double(argv[3]);
        mot->g.z = lil_to_double(argv[4]);
        return NULL;
    }
    if (!strcmp(opt, "air-friction")) {
        if (argc < 3) {
            console_write("not enough arguments for air-friction motion option"); console_newline();
            return NULL;
        }
        mot->airf = lil_to_double(argv[2]);
        return NULL;
    }
    if (!strcmp(opt, "world-friction")) {
        if (argc < 3) {
            console_write("not enough arguments for world-friction motion option"); console_newline();
            return NULL;
        }
        mot->wldf = lil_to_double(argv[2]);
        return NULL;
    }
    if (!strcmp(opt, "bounce-multiplier")) {
        if (argc < 3) {
            console_write("not enough arguments for bounce-multiplier motion option"); console_newline();
            return NULL;
        }
        mot->bncf = lil_to_double(argv[2]);
        return NULL;
    }
    if (!strcmp(opt, "force-receive-factor")) {
        if (argc < 3) {
            console_write("not enough arguments for force-receive-factor motion option"); console_newline();
            return NULL;
        }
        mot->frf = lil_to_double(argv[2]);
        return NULL;
    }
    if (!strcmp(opt, "force-damp-factor")) {
        if (argc < 3) {
            console_write("not enough arguments for force-damp-factor motion option"); console_newline();
            return NULL;
        }
        mot->fdf = lil_to_double(argv[2]);
        return NULL;
    }
    if (!strcmp(opt, "extra-force-factor")) {
        if (argc < 3) {
            console_write("not enough arguments for extra-force-factor motion option"); console_newline();
            return NULL;
        }
        mot->xff = lil_to_double(argv[2]);
        return NULL;
    }
    if (!strcmp(opt, "slide-up")) {
        if (argc < 4) {
            console_write("not enough arguments for slide-up motion option"); console_newline();
            return NULL;
        }
        mot->slideup = lil_to_double(argv[2]);
        mot->slideupc = lil_to_integer(argv[3]);
        return NULL;
    }
    if (!strcmp(opt, "for-character")) {
        mot_for_char(mot);
        return NULL;
    }
    console_write("unknown motion option "); console_write(opt); console_newline();
    return NULL;
}

static lil_value_t nat_move_entity_to(lil_t lil, size_t argc, lil_value_t* argv)
{
    entity_t* ent;
    if (argc < 5) {
        console_write("not enough arguments in move-entity-to"); console_newline();
        return NULL;
    }
    ent = or_get(OT_ENTITY, lil_to_integer(argv[0]));
    if (!ent) {
        console_write("invalid entity reference"); console_newline();
        return NULL;
    }
    if (!ent->mot) {
        console_write("entity has no motion controller. Use init-entity-motion first."); console_newline();
        return NULL;
    }
    if (!ent->ai) {
        console_write("entity has no AI enabled. Use init-entity-ai first."); console_newline();
        return NULL;
    }
    ai_move_to(ent, lil_to_double(argv[1]), lil_to_double(argv[2]), lil_to_double(argv[3]), lil_to_double(argv[4]));
    return NULL;
}

static void reg_game_procs(void)
{
    lil_register(lil, "get-camera-entity", nat_get_camera_entity);
    lil_register(lil, "get-player-entity", nat_get_player_entity);
    lil_register(lil, "init-entity-ai", nat_init_entity_ai);
    lil_register(lil, "init-entity-motion", nat_init_entity_motion);
    lil_register(lil, "apply-force-to-entity", nat_apply_force_to_entity);
    lil_register(lil, "set-entity-motion-option", nat_set_entity_motion_option);
    lil_register(lil, "move-entity-to", nat_move_entity_to);
}

/* Script interface */
static void script_write_callback(lil_t lil, const char* msg)
{
    char* part = NULL;
    size_t len = 0, i;
    for (i=0; msg[i]; i++)
        if (msg[i] == '\n') {
            part = realloc(part, len + 1);
            part[len] = 0;
            console_write(part);
            console_newline();
            free(part);
            part = NULL;
            len = 0;
        } else {
            part = realloc(part, len + 1);
            part[len++] = msg[i];
        }
    if (len) {
        part = realloc(part, len + 1);
        part[len] = 0;
        console_write(part);
        free(part);
    }
}

static void script_error_callback(lil_t lil, size_t pos, const char* msg)
{
    console_write("Script error: ");
    console_write(msg);
    console_newline();
}

static void script_exit_callback(lil_t lil, size_t pos, const char* msg)
{
    running = 0;
}

static int script_getvar_callback(lil_t lil, const char* name, lil_value_t* value)
{
    listitem_t* it;
    if (!regvars) return 0;
    for (it=regvars->first; it; it=it->next) {
        regvar_t* rv = it->ptr;
        if (!strcmp(rv->name, name)) {
            switch (rv->type) {
            case RVT_INT:
                *value = lil_alloc_integer(*((int*)rv->ptr));
                return 1;
            case RVT_FLOAT:
                *value = lil_alloc_double(*((float*)rv->ptr));
                return 1;
            case RVT_DOUBLE:
                *value = lil_alloc_double(*((double*)rv->ptr));
                return 1;
            case RVT_LILVALUE:
                *value = lil_clone_value(*((lil_value_t*)rv->ptr));
                return 1;
            }
        }
    }
    return 0;
}

static int script_setvar_callback(lil_t lil, const char* name, lil_value_t* value)
{
    listitem_t* it;
    int* ival;
    float* fval;
    double* dval;
    lil_value_t* lval;
    if (!regvars) return 0;
    for (it=regvars->first; it; it=it->next) {
        regvar_t* rv = it->ptr;
        if (!strcmp(rv->name, name)) {
            switch (rv->type) {
            case RVT_INT:
                ival = rv->ptr;
                *ival = lil_to_integer(*value);
                return -1;
            case RVT_FLOAT:
                fval = rv->ptr;
                *fval = lil_to_double(*value);
                return -1;
            case RVT_DOUBLE:
                dval = rv->ptr;
                *dval = lil_to_double(*value);
                return -1;
            case RVT_LILVALUE:
                lval = rv->ptr;
                lil_free_value(*lval);
                *lval = lil_clone_value(*value);
                return -1;
            }
        }
    }
    return 0;
}

static void script_run_file(const char* filename)
{
    char* buff = rio_read(filename, NULL);
    if (!buff) return;
    lil_free_value(lil_parse(lil, buff, 0, 0));
    free(buff);
}

static void script_run_scripts(void)
{
    list_t* files;
    listitem_t* it;

    files = rio_files("scripts");
    for (it=files->first; it; it=it->next) {
        const char* file = it->ptr;
        if (strstr(file, ".lil")) {
            char* tmpfilename = malloc(strlen(file) + 16);
            sprintf(tmpfilename, "scripts/%s", file);
            script_run_file(tmpfilename);
            free(tmpfilename);
        }
    }
    list_free(files);
}

static void execat_entry_free(void* ptr)
{
    execat_entry_t* e = ptr;
    free(e->event);
    free(e->id);
    lil_free_value(e->code);
    free(e);
}

void script_run_execats(const char* event)
{
    listitem_t* li;
    for (li=execats->first; li; li=li->next) {
        execat_entry_t* e = li->ptr;
        if (!strcmp(event, e->event))
            lil_free_value(lil_parse_value(lil, e->code, 0));
    }
}

void script_set_int(const char* name, int64_t value)
{
    lil_value_t val = lil_alloc_integer(value);
    lil_set_var(lil, name, val, LIL_SETVAR_LOCAL);
    lil_free_value(val);
}

void script_set_string(const char* name, const char* value)
{
    lil_value_t val = lil_alloc_string(value);
    lil_set_var(lil, name, val, LIL_SETVAR_LOCAL);
    lil_free_value(val);
}

void script_set_float(const char* name, float value)
{
    lil_value_t val = lil_alloc_double(value);
    lil_set_var(lil, name, val, LIL_SETVAR_LOCAL);
    lil_free_value(val);
}

static void regvar_free(void* ptr)
{
    regvar_t* rv = ptr;
    free(rv->name);
    free(rv);
}

void script_reg_var(const char* name, int type, void* ptr)
{
    regvar_t* rv = new(regvar_t);
    rv->name = strdup(name);
    rv->type = type;
    rv->ptr = ptr;
    if (!regvars) {
        regvars = list_new();
        regvars->item_free = regvar_free;
    }
    list_add(regvars, rv);
}

void script_init(void)
{
    lil = lil_new();
    lil_callback(lil, LIL_CALLBACK_WRITE, (lil_callback_proc_t)script_write_callback);
    lil_callback(lil, LIL_CALLBACK_ERROR, (lil_callback_proc_t)script_error_callback);
    lil_callback(lil, LIL_CALLBACK_EXIT, (lil_callback_proc_t)script_exit_callback);
    lil_callback(lil, LIL_CALLBACK_SETVAR, (lil_callback_proc_t)script_setvar_callback);
    lil_callback(lil, LIL_CALLBACK_GETVAR, (lil_callback_proc_t)script_getvar_callback);
    reg_engine_procs();
    reg_editor_procs();
    reg_world_procs();
    reg_game_procs();
    execats = list_new();
    execats->item_free = execat_entry_free;
    script_run_scripts();
    script_run_execats("init");
}

void script_shutdown(void)
{
    list_free(regvars);
    list_free(execats);
    lil_free(lil);
    or_free();
}

void script_run(const char* name)
{
    char* tmpfilename = malloc(strlen(name) + 20);
    sprintf(tmpfilename, "scripts/%s.lil", name);
    script_run_file(tmpfilename);
    free(tmpfilename);
}

entity_t* ent_new_by_class(const char* clsname)
{
    char* funcname = malloc(strlen(clsname) + 35);
    entity_t* ent;
    lil_value_t entval;
    sprintf(funcname, "call-entity-class-constructor {%s}", clsname);
    entval = lil_parse(lil, funcname, 0, 0);
    ent = or_get(OT_ENTITY, lil_to_integer(entval));
    lil_free_value(entval);
    free(funcname);
    if (ent && !ent->class) ent->class = strdup(clsname);
    return ent;
}
