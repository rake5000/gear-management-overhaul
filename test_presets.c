#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stddef.h>
#include "gear_management.c"
typedef struct {char text[48];uint32_t hash;} TestString;
typedef struct {char identity[256];TestString equipment[PM_EQUIPMENT];char class_id[48];unsigned stats[16];} TestTrooper;
typedef struct {char unit[48],name[32];int count;TestTrooper roster[32];unsigned doctrine[32];} TestSquad;
typedef struct {void *screen,*selected;} TestGUI;
static TestSquad test_squad,before;
static TestGUI test_gui;
static void *gui_pointer=&test_gui,*library_pointer=(void *)1;
static int saves,inventory_refresh,stats_refresh;
static wchar_t test_config[MAX_PATH];
static const char *pressed_button;
static bool picker_shown;
static int portrait_refresh,thumb_loads;
static void set_shown(void *item,int visible) {if(item==(void *)9)picker_shown=visible!=0;}
static DK2ML_Status set_text(void *item,const char *text) {(void)item;assert(strstr(text,"Page"));return DK2ML_OK;}
static unsigned thumb_load(void *manager,const char *path,bool flag) {assert(manager==(void *)10 && path[0] && flag);++thumb_loads;return pm_hash(path);}
static void thumb_set(void *image,unsigned id) {assert(image && id);}
static void update_portrait(void *trooper,void *panel) {assert(trooper==test_gui.selected && panel==(void *)5);++portrait_refresh;}
static bool ui_begin(const char *name,bool *open,int flags) {(void)name;(void)open;(void)flags;return true;}
static void ui_end(void) {}
static void ui_same_line(float x,float y) {(void)x;(void)y;}
static bool ui_child(const char *name,unsigned id,const PM_Vec2 *size,bool border,int flags) {(void)name;(void)id;(void)size;(void)border;(void)flags;return false;}
static bool ui_button(const char *name,const PM_Vec2 *size,int flags) {(void)size;(void)flags;return pressed_button && !strcmp(name,pressed_button);}
static bool ui_input(const char *label,const char *hint,char *text,int count,const PM_Vec2 *size,int flags,void *cb,void *user) {
    (void)label;(void)hint;(void)text;(void)count;(void)size;(void)flags;(void)cb;(void)user;return false;
}
static const wchar_t *config_dir(void) {return test_config;}
static void quiet(const char *fmt,...) {(void)fmt;}
static int shown(void *item) {return item==(void *)2 || (item==(void *)9 && picker_shown);}
static void *find(void *under,const char *name) {(void)under;return !strcmp(name,"#inventory_panel")?(void *)3:!strcmp(name,"#player_profile_panel")?(void *)5:!strcmp(name,"#r5k_portrait_page")?(void *)8:NULL;}
static const void *squad_for(const void *trooper) {
    assert((const unsigned char *)trooper>=(const unsigned char *)test_squad.roster);
    assert((const unsigned char *)trooper<(const unsigned char *)(test_squad.roster+32));return &test_squad;
}
static const void *equipment(const void *lib,const char *name) {assert(lib==library_pointer);return !strcmp(name,"Missing")?NULL:(void *)4;}
static void save_roster(void) {++saves;}
static void refresh_inventory(void *trooper,void *panel) {assert(trooper==test_gui.selected && panel==(void *)3);++inventory_refresh;}
static void refresh_stats(void *trooper,const void *squad,bool unused,void *screen) {
    assert(trooper==test_gui.selected && squad==&test_squad && !unused && screen==(void *)2);++stats_refresh;
}
static DK2ML_Status capture_input(int capture){(void)capture;return DK2ML_OK;}
static void setup(void) {
    static const DK2ML_API api={.Log=quiet,.GuiFind=find,.GuiIsShown=shown,.GetConfigDir=config_dir,.GuiShow=set_shown,.GuiSetText=set_text,.CaptureGameInput=capture_input};loader=&api;
    memset(&test_squad,0,sizeof test_squad);strcpy(test_squad.unit,"FOG");strcpy(test_squad.name,"VECTOR TEST");test_squad.count=32;
    for(int i=0;i<32;++i) {
        TestTrooper *t=&test_squad.roster[i];snprintf(t->identity,sizeof t->identity,"IDENTITY %d",i);
        snprintf(t->class_id,sizeof t->class_id,"bv_operator_%d",i/8+1);
        for(int g=0;g<PM_EQUIPMENT;++g) {snprintf(t->equipment[g].text,48,"Item_%d_%d",i,g);t->equipment[g].hash=pm_hash(t->equipment[g].text);}
        for(int j=0;j<16;++j)t->stats[j]=(unsigned)(1000+i*16+j);
    }
    inventory_offset=offsetof(TestTrooper,equipment);string_offset=0;hash_offset=offsetof(TestString,hash);string_size=sizeof(TestString);trooper_size=sizeof(TestTrooper);
    pm_game_gui=&gui_pointer;test_gui.screen=(void *)2;test_gui.selected=&test_squad.roster[0];
    pm_get_squad=squad_for;pm_gui_screen=offsetof(TestGUI,screen);pm_gui_selected=offsetof(TestGUI,selected);
    pm_squad_roster=offsetof(TestSquad,roster);pm_squad_count=offsetof(TestSquad,count);pm_squad_unit=offsetof(TestSquad,unit);pm_squad_name=offsetof(TestSquad,name);
    pm_class_offset=offsetof(TestTrooper,class_id);layout_active=true;object_library=&library_pointer;get_equipment=equipment;
    pm_roster_save=save_roster;pm_update_inventory=refresh_inventory;pm_update_stats=refresh_stats;
    for(int g=0;g<PM_EQUIPMENT;++g)pm_inventory_slots[g]=g;
    saves=inventory_refresh=stats_refresh=0;pm_scope=0;pm_selected=0;
    strcpy(pm_name,"Test loadout");assert(pm_get_context());
}
static void assert_other_fields_unchanged(void) {
    assert(!memcmp(test_squad.unit,before.unit,sizeof test_squad.unit));
    assert(!memcmp(test_squad.name,before.name,sizeof test_squad.name));assert(test_squad.count==before.count);
    assert(!memcmp(test_squad.doctrine,before.doctrine,sizeof before.doctrine));
    for(int i=0;i<32;++i) {
        assert(!memcmp(test_squad.roster[i].identity,before.roster[i].identity,sizeof before.roster[i].identity));
        assert(!memcmp(test_squad.roster[i].class_id,before.roster[i].class_id,sizeof before.roster[i].class_id));
        assert(!memcmp(test_squad.roster[i].stats,before.roster[i].stats,sizeof before.roster[i].stats));
    }
}
int main(int argc,char **argv) {
    assert(argc==2);setup();
    pm_kind=PM_SECTION;assert(pm_capture(&pm_work));assert(pm_work.member_count==8 && pm_work.section_count==1);
    // CHALK 1 -> CHALK 2, preserving eight distinct loadouts.
    memset(pm_work.members[3].gear[14],0,48);memset(pm_work.members[4].gear[15],0,48);
    pm_library[0].preset=pm_work;test_gui.selected=&test_squad.roster[8];before=test_squad;pm_apply();
    assert(saves==1 && inventory_refresh==1 && stats_refresh==1);
    for(int i=0;i<8;++i)for(int g=0;g<PM_EQUIPMENT;++g) {
        assert(!strcmp(test_squad.roster[8+i].equipment[g].text,pm_work.members[i].gear[g]));
        assert(test_squad.roster[8+i].equipment[g].hash==pm_hash(pm_work.members[i].gear[g]));
    }
    assert_other_fields_unchanged();
    for(int i=0;i<32;++i)if(i<8 || i>=16)assert(!memcmp(&test_squad.roster[i],&before.roster[i],sizeof(TestTrooper)));
    assert(pm_get_context());PM_Plan plan;
    assert(pm_plan(&pm_work,&pm_context,0,&plan));assert(plan.count==8 && plan.target[0]==8 && plan.source[7]==7);
    pm_work.sections[0].count=7;pm_work.member_count=7;assert(!pm_plan(&pm_work,&pm_context,0,&plan));

    setup();pm_kind=PM_TROOPER;assert(pm_capture(&pm_work));
    assert(pm_plan(&pm_work,&pm_context,0,&plan) && plan.count==1);
    assert(pm_plan(&pm_work,&pm_context,1,&plan) && plan.count==8);
    assert(pm_plan(&pm_work,&pm_context,2,&plan) && plan.count==32);
    strcpy(pm_work.unit,"Rangers");assert(!pm_plan(&pm_work,&pm_context,0,&plan));
    strcpy(pm_context.unit,"Rangers");strcpy(pm_work.members[0].class_id,"Marksman");strcpy(pm_work.sections[0].class_id,"Marksman");
    for(int i=0;i<32;++i)strcpy(pm_context.classes[i],"Assault");assert(pm_context_build(&pm_context));
    assert(!pm_plan(&pm_work,&pm_context,0,&plan));
    assert(!pm_same_role("AnotherMod","bv_operator_1","bv_operator_2"));

    setup();pm_kind=PM_UNIT;assert(pm_capture(&pm_work));assert(pm_work.section_count==4 && pm_work.member_count==32);
    pm_library[0].preset=pm_work;
    for(int i=0;i<32;++i)for(int g=0;g<PM_EQUIPMENT;++g)strcpy(test_squad.roster[i].equipment[g].text,"Changed");
    before=test_squad;pm_apply();assert(saves==1);
    for(int i=0;i<32;++i)for(int g=0;g<PM_EQUIPMENT;++g)assert(!strcmp(test_squad.roster[i].equipment[g].text,pm_work.members[i].gear[g]));
    assert_other_fields_unchanged();pm_context.count=31;assert(pm_context_build(&pm_context));assert(!pm_plan(&pm_work,&pm_context,0,&plan));
    pm_work.members[0].gear[14][0]=0;strcpy(pm_work.members[1].gear[15],"Missing");
    pm_library[0].preset=pm_work;before=test_squad;pm_apply();
    assert(!test_squad.roster[0].equipment[14].text[0]);assert(!strcmp(test_squad.roster[1].equipment[15].text,before.roster[1].equipment[15].text));

    // Real file round trip, Unicode names, rename, malformed and unsupported files.
    wchar_t folder[MAX_PATH],file[MAX_PATH];assert(pm_wide(argv[1],folder,MAX_PATH));
    assert(CreateDirectoryW(folder,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
    swprintf(file,MAX_PATH,L"%ls\\roundtrip.ini",folder);
    strcpy(pm_work.name,"Nacht Ã¼ber æ±äº¬");assert(pm_write_preset(file,&pm_work));
    static PM_Preset loaded;assert(pm_read_preset(file,&loaded));
    assert(loaded.kind==pm_work.kind && loaded.member_count==pm_work.member_count && loaded.section_count==pm_work.section_count);
    assert(!strcmp(loaded.name,pm_work.name) && !strcmp(loaded.unit,pm_work.unit));
    for(int s=0;s<loaded.section_count;++s)assert(loaded.sections[s].count==pm_work.sections[s].count && !strcmp(loaded.sections[s].class_id,pm_work.sections[s].class_id));
    for(int m=0;m<loaded.member_count;++m)for(int g=0;g<PM_EQUIPMENT;++g)assert(!strcmp(loaded.members[m].gear[g],pm_work.members[m].gear[g]));
    strcpy(pm_work.name,"Renamed preset");assert(pm_write_preset(file,&pm_work));assert(pm_read_preset(file,&loaded));assert(!strcmp(loaded.name,"Renamed preset"));
    assert(WritePrivateProfileStringW(L"preset",L"members",L"999",file));assert(!pm_read_preset(file,&loaded));
    assert(pm_write_preset(file,&pm_work));assert(WritePrivateProfileStringW(L"member0",L"Custom9",NULL,file));assert(!pm_read_preset(file,&loaded));
    assert(pm_write_preset(file,&pm_work));assert(WritePrivateProfileStringW(L"preset",L"format",L"99",file));assert(!pm_read_preset(file,&loaded));
    assert(DeleteFileW(file));
    setup();swprintf(test_config,MAX_PATH,L"%ls\\dk2ml\\test-plugin\\",folder);
    pm_root[0]=0;pm_library_count=0;pm_selected=-1;pm_selected_file[0]=0;pm_kind=PM_SECTION;
    strcpy(pm_name,"Saved section");pm_save_current();
    assert(pm_library_count==1 && pm_selected==0 && pm_library[0].preset.member_count==8);
    assert(wcsstr(pm_library[0].file,L"\\loadout_presets\\section\\"));
    assert(!strcmp(pm_library[0].preset.name,"Saved section"));
    pm_reload();assert(pm_library_count==1 && pm_selected==0);
    wchar_t original[MAX_PATH],duplicate[MAX_PATH],other_unit[MAX_PATH],other_kind[MAX_PATH];wcscpy(original,pm_library[0].file);
    strcpy(test_squad.roster[0].equipment[14].text,"Replacement");strcpy(pm_name," saved SECTION ");
    pm_save_current();assert(pm_library_count==1 && !wcscmp(original,pm_library[0].file));
    assert(!strcmp(pm_library[0].preset.members[0].gear[14],"Replacement"));
    swprintf(duplicate,MAX_PATH,L"%ls\\loadout_presets\\section\\duplicate.ini",folder);
    assert(pm_write_preset(duplicate,&pm_library[0].preset));
    loaded=pm_library[0].preset;strcpy(loaded.unit,"DifferentUnit");
    swprintf(other_unit,MAX_PATH,L"%ls\\loadout_presets\\section\\other-unit.ini",folder);assert(pm_write_preset(other_unit,&loaded));
    pm_kind=PM_TROOPER;assert(pm_capture(&loaded));
    swprintf(other_kind,MAX_PATH,L"%ls\\loadout_presets\\trooper\\other-kind.ini",folder);assert(pm_write_preset(other_kind,&loaded));
    pm_kind=PM_SECTION;pm_reload();assert(pm_library_count==4);
    // Existing-name save updates one stable file and retires only compatible duplicates.
    pm_save_current();assert(pm_library_count==3 && !wcscmp(pm_library[pm_selected].file,original));
    assert(GetFileAttributesW(duplicate)==INVALID_FILE_ATTRIBUTES);
    assert(GetFileAttributesW(other_unit)!=INVALID_FILE_ATTRIBUTES && GetFileAttributesW(other_kind)!=INVALID_FILE_ATTRIBUTES);
    pm_library_count=PM_LIBRARY;pm_save_current();assert(pm_library_count==3); // overwrite works at the library limit
    pm_delete_selected();assert(GetFileAttributesW(original)==INVALID_FILE_ATTRIBUTES && pm_library_count==2);
    assert(DeleteFileW(other_unit) && DeleteFileW(other_kind));pm_reload();assert(pm_library_count==0);
    // The modal preset window blocks the underlying gear GUI immediately.
    void *manager=(void *)6;pm_gui_manager=&manager;
    pm_open=true;layout_active=true;DK2ML_Regs input={.rcx=(uintptr_t)manager};
    assert(pm_gui_input_pre(&input,NULL)==DK2ML_SKIP_ORIGINAL && input.rax==1);
    input.rcx=(uintptr_t)(void *)7;assert(pm_gui_input_pre(&input,NULL)==DK2ML_CALL_ORIGINAL);
    input.rcx=(uintptr_t)manager;pm_open=false;assert(pm_gui_input_pre(&input,NULL)==DK2ML_CALL_ORIGINAL);
    // Each scope button applies in the same UI frame, with no second Apply action.
    const char *apply_buttons[]={"Apply to current trooper","Apply to current section","Apply to matching troopers in unit"};
    const int apply_counts[]={1,8,32};
    for(int scope=0;scope<3;++scope) {
        setup();pm_kind=PM_TROOPER;assert(pm_capture(&pm_work));pm_library[0].preset=pm_work;pm_library_count=1;pm_selected=0;
        pm_begin=ui_begin;pm_end=ui_end;pm_text=quiet;pm_same_line=ui_same_line;pm_begin_child=ui_child;pm_end_child=ui_end;
        pm_input=ui_input;pm_button_ex=ui_button;pm_open=true;pm_captured=false;pressed_button=apply_buttons[scope];
        for(int i=0;i<32;++i)strcpy(test_squad.roster[i].equipment[14].text,"Changed");
        pm_frame(NULL,NULL);assert(saves==1);
        int applied=0;for(int i=0;i<32;++i)if(!strcmp(test_squad.roster[i].equipment[14].text,pm_work.members[0].gear[14]))++applied;
        assert(applied==apply_counts[scope]);
    }
    // Portrait catalogue filtering, duplicate paths, paging, selection and persistence.
    setup();
    typedef struct {char tex[128];uint32_t unit,class_id;int gender;} TestPortrait;
    typedef struct {int capacity;TestPortrait *data;int count;} TestPortraitList;
    static TestPortrait portraits[37];TestPortraitList portrait_list={37,portraits,37};
    for(int i=0;i<37;++i) {
        snprintf(portraits[i].tex,128,"data/textures/portraits/test%d.dds",i);
        portraits[i].unit=pm_hash("FOG");portraits[i].class_id=0;portraits[i].gender=4;
    }
    portraits[32].unit=pm_hash("Rangers");portraits[33].class_id=pm_hash("bv_operator_1");
    portraits[34].class_id=pm_hash("bv_operator_2");strcpy(portraits[35].tex,portraits[0].tex);
    memset(portraits[36].tex,'x',128);
    library_pointer=&portrait_list;pg_list_offset=0;pg_data_offset=offsetof(TestPortraitList,data);pg_count_offset=offsetof(TestPortraitList,count);
    pg_stride=sizeof(TestPortrait);pg_tex_offset=offsetof(TestPortrait,tex);pg_unit_offset=offsetof(TestPortrait,unit);
    pg_class_offset=offsetof(TestPortrait,class_id);pg_gender_offset=offsetof(TestPortrait,gender);
    pg_id_offset=0;pg_id_tex=64;pg_id_hash=192;pg_id_gender=196;pg_render_offset=0;
    pg_target=test_gui.selected;strcpy((char *)pg_target+pg_id_tex,portraits[31].tex);
    assert(pg_collect() && pg_count==33 && pg_page==1);
    void *texture_manager=(void *)10;pg_texture_manager=&texture_manager;pg_load_texture=thumb_load;pg_set_texture=thumb_set;
    pg_update_portrait=update_portrait;pg_panel=(void *)9;picker_shown=true;pg_active=true;
    for(int i=0;i<PG_CELLS;++i) {pg_cells[i]=(void *)(intptr_t)(100+i);pg_images[i]=(void *)(intptr_t)(200+i);pg_markers[i]=(void *)(intptr_t)(300+i);}
    thumb_loads=0;pg_render();assert(thumb_loads==9 && !pg_choice(9));
    pg_turn_page(NULL,0,0,(void *)(intptr_t)-1);assert(pg_page==0 && thumb_loads==33);
    before=test_squad;pg_choose(NULL,0,0,(void *)(intptr_t)3);
    assert(saves==1 && portrait_refresh==1 && !pg_active && !picker_shown);
    assert(!strcmp(test_squad.roster[0].identity+pg_id_tex,portraits[3].tex));
    uint32_t portrait_hash;memcpy(&portrait_hash,test_squad.roster[0].identity+pg_id_hash,sizeof portrait_hash);assert(portrait_hash==pm_hash(portraits[3].tex));
    int gender;memcpy(&gender,test_squad.roster[0].identity+pg_id_gender,sizeof gender);assert(gender==4);
    assert(!memcmp(test_squad.roster[0].identity,before.roster[0].identity,64));
    assert(!memcmp(test_squad.roster[0].equipment,before.roster[0].equipment,sizeof before.roster[0].equipment));
    for(int i=1;i<32;++i)assert(!memcmp(&test_squad.roster[i],&before.roster[i],sizeof(TestTrooper)));
    pg_active=true;pg_target=&test_squad.roster[0];picker_shown=true;test_gui.selected=&test_squad.roster[8];
    pg_choose(NULL,0,0,0);assert(saves==1 && !pg_active);
    puts("Portrait checks passed: unit/class filtering, duplicates, invalid paths, paging, texture binding, one-click selection, gender/hash updates, save/refresh, unchanged name and equipment, and stale-selection protection.");
    puts("Preset tests passed: CHALK transfers, strict role/unit/size filtering, all three scopes, distinct member mapping, all 25 slots and empty values, missing gear, cached hashes, unchanged identities/stats, save/refresh, Unicode file round trips, rename/delete, and corrupt-file rejection.");
    return 0;
}
