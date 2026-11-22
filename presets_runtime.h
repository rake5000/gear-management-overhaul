#ifndef R5K_PRESETS_RUNTIME_H
#define R5K_PRESETS_RUNTIME_H
#include "preset_store.h"
typedef struct {float x,y;} PM_Vec2;
typedef struct {PM_Preset preset; wchar_t file[MAX_PATH];} PM_Entry;
static PM_Entry pm_library[PM_LIBRARY];
static PM_Preset pm_work;
static PM_Context pm_context;
static PM_Plan pm_mapping;
static int pm_library_count,pm_selected=-1,pm_kind=PM_TROOPER,pm_scope;
static bool pm_open,pm_captured;
static bool pm_library_loaded,pm_reload_pending;
static unsigned pm_reload_wait;
static char pm_name[PM_NAME],pm_status[256];
static wchar_t pm_root[MAX_PATH],pm_selected_file[MAX_PATH];
static const wchar_t *pm_folders[]={L"",L"trooper",L"section",L"unit"};
static int pm_inventory_slots[PM_EQUIPMENT];
static int pm_gui_selected,pm_gui_screen,pm_squad_roster,pm_squad_count,pm_squad_unit,pm_squad_name,pm_class_offset;
static int pm_window_flags,pm_click_event;
static void **pm_game_gui,**pm_gui_manager;
static const void *(*pm_get_squad)(const void *);
static void (*pm_roster_save)(void);
static void (*pm_update_inventory)(void *,void *);
static void (*pm_update_stats)(void *,const void *,bool,void *);
static bool (*pm_begin)(const char *,bool *,int);
static void (*pm_end)(void),(*pm_text)(const char *,...),(*pm_same_line)(float,float),(*pm_end_child)(void);
static bool (*pm_button_ex)(const char *,const PM_Vec2 *,int);
static bool (*pm_input)(const char *,const char *,char *,int,const PM_Vec2 *,int,void *,void *);
static bool (*pm_selectable)(const char *,bool,int,const PM_Vec2 *);
static bool (*pm_begin_child)(const char *,unsigned,const PM_Vec2 *,bool,int);
static unsigned char *pm_live_roster;
static void *pm_live_selected;
static const void *pm_live_squad;
static bool pm_button(const char *label) {const PM_Vec2 size={0,0};return pm_button_ex(label,&size,0);}
static void *pm_pointer(const void *p,int off) {void *v;memcpy(&v,(const unsigned char *)p+off,sizeof v);return v;}
static bool pm_read_static(const void *p,int off,char *out,size_t capacity) {
    const char *text=(const char *)p+off;
    const char *end=memchr(text,0,capacity);
    if(!end)return false;
    memcpy(out,text,(size_t)(end-text)+1);return true;
}
static bool pm_get_context(void) {
    if(!layout_active || !pm_game_gui || !*pm_game_gui) return false;
    void *gui=*pm_game_gui;
    void *screen=pm_pointer(gui,pm_gui_screen);
    if(!screen || !loader->GuiIsShown(screen)) return false;
    pm_live_selected=pm_pointer(gui,pm_gui_selected);
    if(!pm_live_selected || !(pm_live_squad=pm_get_squad(pm_live_selected))) return false;
    int count;memcpy(&count,(const unsigned char *)pm_live_squad+pm_squad_count,sizeof count);
    if(count<1 || count>PM_MEMBERS) return false;
    pm_live_roster=(unsigned char *)(uintptr_t)pm_live_squad+pm_squad_roster;
    memset(&pm_context,0,sizeof pm_context);pm_context.count=count;pm_context.selected=-1;
    if(!pm_read_static(pm_live_squad,pm_squad_unit+string_offset,pm_context.unit,PM_TEXT))return false;
    for(int i=0;i<count;++i) {
        unsigned char *trooper=pm_live_roster+(size_t)i*trooper_size;
        if(trooper==pm_live_selected)pm_context.selected=i;
        if(!pm_read_static(trooper,pm_class_offset+string_offset,pm_context.classes[i],PM_TEXT))return false;
    }
    return pm_context_build(&pm_context);
}
static bool pm_storage_ready(void) {
    if(pm_root[0])return true;
    const wchar_t *config=loader->GetConfigDir();
    if(!config || wcslen(config)>=MAX_PATH)return false;
    wchar_t root[MAX_PATH];wcscpy(root,config);
    for(wchar_t *p=root;*p;++p)if(*p==L'/')*p=L'\\';
    wchar_t *marker=wcsstr(root,L"\\dk2ml\\");
    if(!marker)return false;
    *marker=0;
    if(wcslen(root)+wcslen(L"\\loadout_presets\\section")>=MAX_PATH)return false;
    wcscat(root,L"\\loadout_presets");
    if(!CreateDirectoryW(root,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    for(int k=1;k<=3;++k) {
        wchar_t folder[MAX_PATH];swprintf(folder,MAX_PATH,L"%ls\\%ls",root,pm_folders[k]);
        if(!CreateDirectoryW(folder,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    }
    wcscpy(pm_root,root);return true;
}
static int pm_compare_entries(const void *a,const void *b) {
    return _stricmp(((const PM_Entry *)a)->preset.name,((const PM_Entry *)b)->preset.name);
}
static void pm_reload(void) {
    ULONGLONG started=GetTickCount64();
    pm_reload_pending=false;pm_status[0]=0;
    pm_selected=-1;pm_library_count=0;
    if(!pm_storage_ready()) {strcpy(pm_status,"Could not open the loadout_presets folder beside your roster.");return;}
    int ignored=0;
    for(int k=1;k<=3;++k) {
        wchar_t pattern[MAX_PATH];swprintf(pattern,MAX_PATH,L"%ls\\%ls\\*.ini",pm_root,pm_folders[k]);
        WIN32_FIND_DATAW data;HANDLE search=FindFirstFileW(pattern,&data);
        if(search==INVALID_HANDLE_VALUE)continue;
        do {
            if(data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
            if(pm_library_count>=PM_LIBRARY) {++ignored;continue;}
            PM_Entry *entry=&pm_library[pm_library_count];
            if(wcslen(pm_root)+wcslen(pm_folders[k])+wcslen(data.cFileName)+3>=MAX_PATH) {++ignored;continue;}
            swprintf(entry->file,MAX_PATH,L"%ls\\%ls\\%ls",pm_root,pm_folders[k],data.cFileName);
            if(pm_read_preset(entry->file,&entry->preset) && entry->preset.kind==k)++pm_library_count;
            else ++ignored;
        } while(FindNextFileW(search,&data));
        FindClose(search);
    }
    qsort(pm_library,(size_t)pm_library_count,sizeof pm_library[0],pm_compare_entries);
    for(int i=0;i<pm_library_count;++i)if(!wcscmp(pm_selected_file,pm_library[i].file))pm_selected=i;
    if(ignored)snprintf(pm_status,sizeof pm_status,"Skipped %d invalid, unsupported, or excess preset files.",ignored);
    pm_library_loaded=true;
    loader->Log("Preset library: %d entries loaded in %llu ms.",pm_library_count,(unsigned long long)(GetTickCount64()-started));
}
static void pm_request_library(void){
    if(!pm_library_loaded){pm_reload_pending=true;pm_reload_wait=1;strcpy(pm_status,"Loading preset library...");}
}
static bool pm_load_due(void){
    if(!pm_reload_pending)return false;
    if(pm_reload_wait){--pm_reload_wait;return false;}
    pm_reload_pending=false;return true;
}
static bool pm_capture(PM_Preset *p) {
    memset(p,0,sizeof *p);p->kind=pm_kind;
    strcpy(p->unit,pm_context.unit);strcpy(p->name,pm_name);
    int first=pm_kind==PM_UNIT?0:pm_context.selected_section;
    int last=pm_kind==PM_UNIT?pm_context.section_count:first+1;
    for(int s=first;s<last;++s) {
        PM_Section *out=&p->sections[p->section_count++];
        strcpy(out->class_id,pm_context.sections[s].class_id);
        out->count=pm_kind==PM_TROOPER?1:pm_context.sections[s].count;
        for(int i=0;i<out->count;++i) {
            int index=pm_kind==PM_TROOPER?pm_context.selected:pm_context.indices[s][i];
            const void *trooper=pm_live_roster+(size_t)index*trooper_size;
            PM_Member *member=&p->members[p->member_count++];strcpy(member->class_id,pm_context.classes[index]);
            for(int g=0;g<PM_EQUIPMENT;++g)if(!pm_read_static(slot_at(trooper,pm_inventory_slots[g]),string_offset,member->gear[g],PM_TEXT))return false;
        }
    }
    return pm_valid(p);
}
static void pm_trim_name(void) {
    size_t length=strlen(pm_name);
    while(length && (pm_name[length-1]==' ' || pm_name[length-1]=='\t'))pm_name[--length]=0;
    char *start=pm_name;while(*start==' ' || *start=='\t')++start;
    if(start!=pm_name)memmove(pm_name,start,strlen(start)+1);
}
static bool pm_save_name_matches(int index) {
    const PM_Preset *p=&pm_library[index].preset;
    wchar_t a[PM_NAME],b[PM_NAME];PM_Plan plan;
    return p->kind==pm_kind && pm_wide(p->name,a,PM_NAME) && pm_wide(pm_name,b,PM_NAME) &&
           CompareStringOrdinal(a,-1,b,-1,TRUE)==CSTR_EQUAL && pm_plan(p,&pm_context,0,&plan);
}
static void pm_save_current(void) {
    pm_trim_name();
    if(!pm_text_valid(pm_name,sizeof pm_name,false)) {strcpy(pm_status,"Enter a name without quotes or line breaks.");return;}
    if(!pm_storage_ready() || !pm_capture(&pm_work)) {strcpy(pm_status,"Could not capture this loadout.");return;}
    int existing=-1;
    for(int i=0;i<pm_library_count;++i)if(pm_save_name_matches(i)) {existing=i;if(i==pm_selected)break;}
    if(existing<0 && pm_library_count>=PM_LIBRARY) {strcpy(pm_status,"The library holds 128 presets. Delete one before saving another.");return;}
    FILETIME now;GetSystemTimeAsFileTime(&now);ULARGE_INTEGER time;time.LowPart=now.dwLowDateTime;time.HighPart=now.dwHighDateTime;
    static unsigned counter;
    if(wcslen(pm_root)+wcslen(pm_folders[pm_kind])+50>=MAX_PATH) {strcpy(pm_status,"The preset folder path is too long.");return;}
    wchar_t file[MAX_PATH];
    if(existing>=0)wcscpy(file,pm_library[existing].file);
    else swprintf(file,MAX_PATH,L"%ls\\%ls\\%016llx-%08lx-%08x.ini",pm_root,pm_folders[pm_kind],(unsigned long long)time.QuadPart,(unsigned long)GetCurrentProcessId(),++counter);
    if(!pm_write_preset(file,&pm_work)) {strcpy(pm_status,"Could not save the preset file.");return;}
    // Retire duplicate names made by the earlier save-new behavior, within the
    // same visible compatibility group only, after the replacement is durable.
    unsigned duplicates_failed=0;
    for(int i=0;i<pm_library_count;++i)if(i!=existing && pm_save_name_matches(i) && !DeleteFileW(pm_library[i].file))++duplicates_failed;
    wcscpy(pm_selected_file,file);pm_reload();
    snprintf(pm_status,sizeof pm_status,"%s '%s' (%d trooper%s).",existing>=0?"Updated":"Saved",pm_work.name,pm_work.member_count,pm_work.member_count==1?"":"s");
    if(duplicates_failed)strcpy(pm_status,"Preset saved; an older duplicate could not be removed.");
}
static void pm_delete_selected(void) {
    if(pm_selected<0 || pm_selected>=pm_library_count)return;
    if(DeleteFileW(pm_library[pm_selected].file)) {pm_selected_file[0]=0;pm_reload();strcpy(pm_status,"Preset deleted.");}
    else strcpy(pm_status,"Could not delete the preset.");
}
static void pm_apply(void) {
    if(!object_library || !*object_library) {strcpy(pm_status,"Equipment definitions are not available yet.");return;}
    if(pm_selected<0 || !pm_get_context() || !pm_plan(&pm_library[pm_selected].preset,&pm_context,pm_scope,&pm_mapping)) {
        strcpy(pm_status,"This preset no longer matches the selected trooper or unit.");return;
    }
    const PM_Preset *p=&pm_library[pm_selected].preset;
    unsigned missing=0;
    // Unit/class filtering has already passed. As requested, leave gear restrictions
    // to the game: restore all known item IDs directly, preserving every other field.
    for(int i=0;i<pm_mapping.count;++i) {
        unsigned char *target=pm_live_roster+(size_t)pm_mapping.target[i]*trooper_size;
        const PM_Member *member=&p->members[pm_mapping.source[i]];
        for(int g=0;g<PM_EQUIPMENT;++g) {
            const char *item=member->gear[g];
            if(*item && (!*object_library || !get_equipment(*object_library,item))) {++missing;continue;}
            unsigned char *destination=target+inventory_offset+(size_t)pm_inventory_slots[g]*string_size;
            memset(destination,0,string_size);strcpy((char *)destination+string_offset,item);
            uint32_t hash=pm_hash(item);memcpy(destination+hash_offset,&hash,sizeof hash);
        }
    }
    pm_roster_save();
    void *screen=pm_pointer(*pm_game_gui,pm_gui_screen);
    void *inventory=loader->GuiFind(screen,"#inventory_panel");
    pm_update_inventory(pm_live_selected,inventory);
    pm_update_stats(pm_live_selected,pm_live_squad,false,screen);
    snprintf(pm_status,sizeof pm_status,"Applied to %d trooper%s. %u missing item%s kept unchanged.",pm_mapping.count,pm_mapping.count==1?"":"s",missing,missing==1?"":"s");
    loader->Log("Preset '%s' applied to %d troopers; %u missing items retained.",p->name,pm_mapping.count,missing);
}
static void pm_capture_input(bool active) {
    // This window is modal over the gear screen. Global ImGui WantCapture flags
    // describe the previous frame and are shared with other plugins' windows.
    bool capture=active;
    if(capture!=pm_captured && loader->CaptureGameInput(capture?1:0)==DK2ML_OK)pm_captured=capture;
}
static int pm_gui_input_pre(DK2ML_Regs *regs,void *user) {
    (void)user;
    if((pm_open||pm_captured) && layout_active && pm_gui_manager && *pm_gui_manager==(void *)(uintptr_t)DK2ML_Arg(regs,0)) {
        // GUIManager::UpdateInput returns whether input was consumed. Protect text
        // entry (including spaces/Escape) and clicks from reaching the gear screen.
        regs->rax=1;return DK2ML_SKIP_ORIGINAL;
    }
    return DK2ML_CALL_ORIGINAL;
}
static void pm_frame(const DK2ML_Event *event,void *user) {
    (void)event;(void)user;
    if(!pm_open) {pm_capture_input(false);return;}
    if(!pm_get_context()) {loader->Log("Preset window closed: gear context is no longer available.");pm_open=false;pm_capture_input(false);return;}
    pm_capture_input(true);
    if(pm_load_due())pm_reload();
    if(pm_begin("Loadout presets##r5k_gmo",&pm_open,pm_window_flags)) {
        if(pm_reload_pending){
            pm_text("Loading preset library...");
            if(pm_button("Close"))pm_open=false;
        } else {
        char unit_name[PM_TEXT]={0};pm_read_static(pm_live_squad,pm_squad_name,unit_name,32);
        pm_text("Unit: %s | Current section: %d troopers",unit_name,pm_context.sections[pm_context.selected_section].count);
        const char *tabs[]={"", "Trooper", "Section", "Unit"};
        for(int k=1;k<=3;++k) {
            if(k>1)pm_same_line(0,-1);
            char label[48];snprintf(label,sizeof label,pm_kind==k?"[%s]":"%s",tabs[k]);
            if(pm_button(label)) {pm_kind=k;pm_selected=-1;pm_selected_file[0]=0;pm_name[0]=0;}
        }
        pm_same_line(0,-1);if(pm_button("Refresh"))pm_reload();
        if(pm_selected>=0 && (pm_library[pm_selected].preset.kind!=pm_kind ||
           !pm_plan(&pm_library[pm_selected].preset,&pm_context,pm_scope,&pm_mapping)))pm_selected=-1;
        const PM_Vec2 list_size={680,250},row_size={0,0};
        int visible=0;
        if(pm_begin_child("Preset library",pm_hash("r5k_gmo_presets"),&list_size,true,0)) {
            for(int i=0;i<pm_library_count;++i) {
                const PM_Preset *p=&pm_library[i].preset;
                if(p->kind!=pm_kind || !pm_plan(p,&pm_context,pm_scope,&pm_mapping))continue;
                ++visible;char label[256];snprintf(label,sizeof label,"%s (%d trooper%s)##preset%d",p->name,p->member_count,p->member_count==1?"":"s",i);
                if(pm_selectable(label,pm_selected==i,0,&row_size)) {
                    pm_selected=i;wcscpy(pm_selected_file,pm_library[i].file);strcpy(pm_name,p->name);
                }
            }
            if(!visible)pm_text("No compatible %s presets saved yet.",pm_kind==1?"trooper":pm_kind==2?"section":"unit");
        }
        pm_end_child();
        const PM_Vec2 field_size={680,0};pm_input("Name","Give this loadout a name",pm_name,sizeof pm_name,&field_size,0,NULL,NULL);
        if(pm_button(pm_kind==1?"Save current trooper":pm_kind==2?"Save current section":"Save current unit"))pm_save_current();
        if(pm_selected>=0) {
            pm_same_line(0,-1);
            if(pm_button("Rename selected")) {
                pm_trim_name();
                if(pm_text_valid(pm_name,sizeof pm_name,false)) {
                    pm_work=pm_library[pm_selected].preset;strcpy(pm_work.name,pm_name);
                    if(pm_write_preset(pm_library[pm_selected].file,&pm_work)) {pm_reload();strcpy(pm_status,"Preset renamed.");}
                    else strcpy(pm_status,"Could not rename the preset.");
                } else strcpy(pm_status,"Enter a name without quotes or line breaks.");
            }
            pm_same_line(0,-1);if(pm_button("Delete selected"))pm_delete_selected();
            if(pm_selected>=0 && pm_kind==PM_TROOPER) {
                const char *scopes[]={"Apply to current trooper","Apply to current section","Apply to matching troopers in unit"};
                for(int s=0;s<3;++s) {
                    if(s)pm_same_line(0,-1);
                    if(pm_button(scopes[s])) {pm_scope=s;pm_apply();}
                }
            } else if(pm_selected>=0 && pm_button(pm_kind==PM_SECTION?"Apply to current section":"Apply to whole unit"))pm_apply();
        }
        if(pm_status[0])pm_text("%s",pm_status);
        if(pm_button("Close"))pm_open=false;
        }
    }
    // Release at the next frame, so the click closing this window cannot also
    // activate the underlying gear screen during the current input update.
    pm_end();
}
static void pm_open_window(void *item,float x,float y,void *user) {
    (void)item;(void)x;(void)y;(void)user;
    if(!pm_get_context())return;
    pm_open=!pm_open;
    if(pm_open) {pm_status[0]=0;pm_capture_input(true);pm_request_library();}
}
static void pm_layout_loaded(void) {
    pm_open=false;pm_reload_pending=false;pm_capture_input(false);
    if(!layout_active)return;
    void *screen=loader->GuiFind(NULL,"Customization_Screen");
    void *button=loader->GuiFind(screen,"#r5k_preset_button");
    if(!button || loader->GuiSetCallback(button,pm_click_event,pm_open_window,NULL)!=DK2ML_OK)
        loader->Log("Could not bind the loadout preset button.");
}
static bool init_presets(void) {
    pm_game_gui=loader->ResolveSymbol("g_pGameGUI");
    pm_gui_manager=loader->ResolveSymbol("g_pGUIManager");
    void *input_update=loader->ResolveSymbol("GUIManager::UpdateInput");
    pm_get_squad=(const void *(*)(const void *))loader->ResolveSymbol("Roster::GetSquadForTrooper");
    pm_roster_save=(void (*)(void))loader->ResolveSymbol("Roster::Save");
    pm_update_inventory=(void (*)(void *,void *))loader->ResolveSymbol("Customize_UpdateInventory");
    pm_update_stats=(void (*)(void *,const void *,bool,void *))loader->ResolveSymbol("Customize_UpdateEquipmentRelatedStats");
    pm_begin=(bool (*)(const char *,bool *,int))loader->ResolveSymbol("ImGui::Begin");
    pm_end=(void (*)(void))loader->ResolveSymbol("ImGui::End");
    pm_text=(void (*)(const char *,...))loader->ResolveSymbol("ImGui::Text");
    pm_same_line=(void (*)(float,float))loader->ResolveSymbol("ImGui::SameLine");
    pm_button_ex=(bool (*)(const char *,const PM_Vec2 *,int))loader->ResolveSymbol("ImGui::ButtonEx");
    pm_input=(bool (*)(const char *,const char *,char *,int,const PM_Vec2 *,int,void *,void *))loader->ResolveSymbol("ImGui::InputTextEx");
    pm_selectable=(bool (*)(const char *,bool,int,const PM_Vec2 *))loader->ResolveSymbol("ImGui::Selectable");
    pm_begin_child=(bool (*)(const char *,unsigned,const PM_Vec2 *,bool,int))loader->ResolveSymbol("ImGui::BeginChildEx");
    pm_end_child=(void (*)(void))loader->ResolveSymbol("ImGui::EndChild");
    pm_gui_selected=loader->GetFieldOffset("GameGUI","m_pCustomizeSelection");
    pm_gui_screen=loader->GetFieldOffset("GameGUI","m_pCustomizationScreen");
    pm_squad_roster=loader->GetFieldOffset("Roster::Squad","roster");
    pm_squad_count=loader->GetFieldOffset("Roster::Squad","numInRoster");
    pm_squad_unit=loader->GetFieldOffset("Roster::Squad","unit");
    pm_squad_name=loader->GetFieldOffset("Roster::Squad","name");
    pm_class_offset=loader->GetFieldOffset("Roster::Trooper","className");
    if(!pm_game_gui || !pm_gui_manager || !input_update || !pm_get_squad || !pm_roster_save || !pm_update_inventory || !pm_update_stats || !pm_begin || !pm_end ||
       !pm_text || !pm_same_line || !pm_button_ex || !pm_input || !pm_selectable || !pm_begin_child || !pm_end_child ||
       pm_gui_selected<0 || pm_gui_screen<0 || pm_squad_roster<0 || pm_squad_count<0 || pm_squad_unit<0 || pm_squad_name<0 ||
       pm_class_offset<0 ||
       (unsigned)string_offset+PM_TEXT>(unsigned)hash_offset)return false;
    for(int g=0;g<PM_EQUIPMENT;++g) {
        int64_t value;
        if(loader->GetEnumValue("Inventory::eType",pm_slot_enums[g],&value)!=DK2ML_OK || value<0 ||
           (uint64_t)inventory_offset+((uint64_t)value+1)*string_size>trooper_size)return false;
        pm_inventory_slots[g]=(int)value;
    }
    const char *flags[]={"ImGuiWindowFlags_AlwaysAutoResize","ImGuiWindowFlags_NoSavedSettings","ImGuiWindowFlags_NoCollapse"};
    for(unsigned i=0;i<sizeof flags/sizeof flags[0];++i) {
        int64_t value;if(loader->GetEnumValue("ImGuiWindowFlags_",flags[i],&value)!=DK2ML_OK)return false;
        pm_window_flags|=(int)value;
    }
    int64_t click;
    if(loader->GetEnumValue("GUI::Item::eItemEventType","EVENT_CLICK",&click)!=DK2ML_OK)return false;
    pm_click_event=(int)click;
    if(loader->CreateSafeHook(input_update,pm_gui_input_pre,NULL,NULL)!=DK2ML_OK || loader->EnableHook(input_update)!=DK2ML_OK)return false;
    return loader->Subscribe(DK2ML_EVENT_FRAME,pm_frame,NULL)==DK2ML_OK;
}
#endif
