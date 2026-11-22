#ifndef R5K_PORTRAIT_PICKER_H
#define R5K_PORTRAIT_PICKER_H
#define PG_CELLS 24
#define PG_LIMIT 8192
static void *pg_panel,*pg_target,*pg_cells[PG_CELLS],*pg_images[PG_CELLS],*pg_markers[PG_CELLS];
static void **pg_texture_manager;
static unsigned (*pg_load_texture)(void *,const char *,bool);
static void (*pg_set_texture)(void *,unsigned);
static void (*pg_update_portrait)(void *,void *);
static void (*pg_deploy_update)(void *);
static int pg_selection_offset,pg_selection_roster;
static int pg_list_offset,pg_data_offset,pg_count_offset,pg_tex_offset,pg_unit_offset,pg_class_offset,pg_gender_offset;
static int pg_id_offset,pg_id_tex,pg_id_hash,pg_id_gender,pg_render_offset;
static unsigned pg_stride;
static const unsigned char *pg_data;
static int pg_choices[PG_LIMIT],pg_count,pg_page;
static bool pg_active;
static bool pg_eligible(uint32_t unit,uint32_t class_id,uint32_t wanted_unit,uint32_t wanted_class) {
    return unit==wanted_unit && (!class_id || class_id==wanted_class);
}
static void pg_close(void) {
    if(pg_panel && loader->GuiIsShown(pg_panel))loader->GuiShow(pg_panel,0);
    pg_active=false;pg_target=NULL;
}
static const unsigned char *pg_choice(int visible) {
    int index=pg_page*PG_CELLS+visible;
    return visible>=0 && visible<PG_CELLS && index>=0 && index<pg_count ? pg_data+(size_t)pg_choices[index]*pg_stride : NULL;
}
static void pg_render(void) {
    int pages=(pg_count+PG_CELLS-1)/PG_CELLS;if(pages<1)pages=1;
    if(pg_page>=pages)pg_page=pages-1;if(pg_page<0)pg_page=0;
    const char *current=(const char *)pg_target+pg_id_offset+pg_id_tex;
    for(int i=0;i<PG_CELLS;++i) {
        const unsigned char *portrait=pg_choice(i);
        loader->GuiShow(pg_cells[i],portrait?1:0);
        if(!portrait)continue;
        const char *texture=(const char *)portrait+pg_tex_offset;
        unsigned id=pg_load_texture(*pg_texture_manager,texture,true);
        pg_set_texture((unsigned char *)pg_images[i]+pg_render_offset,id);
        loader->GuiShow(pg_markers[i],!strcmp(texture,current));
    }
    char label[100];snprintf(label,sizeof label,"Page %d / %d  |  %d available portraits",pg_page+1,pages,pg_count);
    loader->GuiSetText(loader->GuiFind(pg_panel,"#r5k_portrait_page"),label);
}
static bool pg_collect(void) {
    if(!object_library || !*object_library)return false;
    const unsigned char *list=(const unsigned char *)*object_library+pg_list_offset;
    int count;memcpy(&count,list+pg_count_offset,sizeof count);memcpy(&pg_data,list+pg_data_offset,sizeof pg_data);
    if(count<0 || count>500000 || (count && !pg_data))return false;
    pg_count=0;pg_page=0;
    uint32_t wanted_unit=pm_hash(pm_context.unit),wanted_class=pm_hash(pm_context.classes[pm_context.selected]);
    const char *current=(const char *)pg_target+pg_id_offset+pg_id_tex;
    for(int i=0;i<count && pg_count<PG_LIMIT;++i) {
        const unsigned char *portrait=pg_data+(size_t)i*pg_stride;
        uint32_t unit,class_id;memcpy(&unit,portrait+pg_unit_offset,sizeof unit);memcpy(&class_id,portrait+pg_class_offset,sizeof class_id);
        const char *texture=(const char *)portrait+pg_tex_offset;
        if(!pg_eligible(unit,class_id,wanted_unit,wanted_class) || !memchr(texture,0,128) || !texture[0])continue;
        bool duplicate=false;
        for(int n=0;n<pg_count;++n)if(!strcmp(texture,(const char *)pg_data+(size_t)pg_choices[n]*pg_stride+pg_tex_offset)) {duplicate=true;break;}
        if(duplicate)continue;
        if(!strcmp(current,texture))pg_page=pg_count/PG_CELLS;
        pg_choices[pg_count++]=i;
    }
    return true;
}
static void pg_open_picker(void *item,float x,float y,void *user) {
    (void)item;(void)x;(void)y;(void)user;
    if(!pg_panel) {loader->Log("Portrait click received, but picker controls are not bound.");return;}
    if(!pm_get_context()) {loader->Log("Portrait click received, but no active customization trooper was found.");return;}
    if(!pg_texture_manager || !*pg_texture_manager) {loader->Log("Portrait click received, but the texture manager is unavailable.");return;}
    pm_open=false;pm_capture_input(false);pg_target=pm_live_selected;
    if(!pg_collect()) {loader->Log("Portrait click received, but the portrait catalogue is unavailable.");pg_target=NULL;return;}
    pg_render();loader->GuiShow(pg_panel,1);pg_active=true;
    loader->Log("Portrait picker opened: %d matching portraits for unit %s.",pg_count,pm_context.unit);
}
static void pg_choose(void *item,float x,float y,void *user) {
    (void)item;(void)x;(void)y;
    if(!pg_active || !pm_get_context() || pm_live_selected!=pg_target) {pg_close();return;}
    const unsigned char *portrait=pg_choice((int)(intptr_t)user);
    if(!portrait)return;
    const char *texture=(const char *)portrait+pg_tex_offset;
    unsigned char *identity=(unsigned char *)pg_target+pg_id_offset;
    memset(identity+pg_id_tex,0,128);strcpy((char *)identity+pg_id_tex,texture);
    uint32_t hash=pm_hash(texture);memcpy(identity+pg_id_hash,&hash,sizeof hash);
    int gender;memcpy(&gender,portrait+pg_gender_offset,sizeof gender);memcpy(identity+pg_id_gender,&gender,sizeof gender);
    pm_roster_save();
    void *screen=pm_pointer(*pm_game_gui,pm_gui_screen);
    pg_update_portrait(pg_target,loader->GuiFind(screen,"#player_profile_panel"));
    // Match the native portrait-arrow refresh: temporarily identify the changed
    // roster member to Deploy_Update, then preserve the prior deploy selection.
    if(pg_deploy_update) {
        unsigned char *selection=(unsigned char *)*pm_game_gui+pg_selection_offset+pg_selection_roster;
        void *previous;memcpy(&previous,selection,sizeof previous);
        memcpy(selection,&pg_target,sizeof pg_target);
        pg_deploy_update(*pm_game_gui);
        memcpy(selection,&previous,sizeof previous);
    }
    loader->Log("Portrait picker selected %s.",texture);pg_close();
}
static void pg_turn_page(void *item,float x,float y,void *user) {
    (void)item;(void)x;(void)y;
    if(!pg_active)return;
    int pages=(pg_count+PG_CELLS-1)/PG_CELLS;if(pages<1)pages=1;
    pg_page=(pg_page+(int)(intptr_t)user+pages)%pages;pg_render();
}
static void pg_frame(const DK2ML_Event *event,void *user) {
    (void)event;(void)user;
    if(pg_active && (!loader->GuiIsShown(pg_panel) || !pm_get_context() || pm_live_selected!=pg_target))pg_close();
}
static void pg_layout_loaded(void) {
    pg_active=false;pg_target=NULL;pg_panel=NULL;
    if(!layout_active)return;
    pg_panel=loader->GuiFind(NULL,"#r5k_portrait_picker");
    void *screen=loader->GuiFind(NULL,"Customization_Screen");
    void *portrait=loader->GuiFind(screen,"#r5k_portrait_open");
    if(!pg_panel || !portrait || loader->GuiSetCallback(portrait,pm_click_event,pg_open_picker,NULL)!=DK2ML_OK)goto missing;
    for(int i=0;i<PG_CELLS;++i) {
        char name[64];snprintf(name,sizeof name,"#r5k_portrait_cell_%d",i);pg_cells[i]=loader->GuiFind(pg_panel,name);
        snprintf(name,sizeof name,"#r5k_portrait_image_%d",i);pg_images[i]=loader->GuiFind(pg_cells[i],name);
        snprintf(name,sizeof name,"#r5k_portrait_current_%d",i);pg_markers[i]=loader->GuiFind(pg_cells[i],name);
        if(!pg_cells[i] || !pg_images[i] || !pg_markers[i] || loader->GuiSetCallback(pg_cells[i],pm_click_event,pg_choose,(void *)(intptr_t)i)!=DK2ML_OK)goto missing;
    }
    if(loader->GuiSetCallback(loader->GuiFind(pg_panel,"#r5k_portrait_prev"),pm_click_event,pg_turn_page,(void *)(intptr_t)-1)!=DK2ML_OK ||
       loader->GuiSetCallback(loader->GuiFind(pg_panel,"#r5k_portrait_next"),pm_click_event,pg_turn_page,(void *)(intptr_t)1)!=DK2ML_OK)goto missing;
    loader->Log("Portrait picker button and all 24 thumbnail controls bound.");
    return;
missing:
    pg_panel=NULL;loader->Log("Portrait picker controls were not available; original portrait arrows remain usable.");
}
static bool init_portraits(void) {
    pg_texture_manager=loader->ResolveSymbol("g_textureManager");
    pg_load_texture=(unsigned (*)(void *,const char *,bool))loader->ResolveSymbol("TextureManagerImpl::Load");
    pg_set_texture=(void (*)(void *,unsigned))loader->ResolveSymbol("RenderObject2D::SetTexture");
    pg_update_portrait=(void (*)(void *,void *))loader->ResolveSymbol("Customize_UpdatePortrait");
    pg_deploy_update=(void (*)(void *))loader->ResolveSymbol("GameGUI::Deploy_Update");
    pg_selection_offset=loader->GetFieldOffset("GameGUI","m_deploySelection");
    pg_selection_roster=loader->GetFieldOffset("GameGUI::sDeploySlot","roster");
    pg_list_offset=loader->GetFieldOffset("ObjectLibrary","m_humanPortraits");
    pg_data_offset=loader->GetFieldOffset("List<sHumanPortrait>","m_list");
    pg_count_offset=loader->GetFieldOffset("List<sHumanPortrait>","m_elements");
    pg_tex_offset=loader->GetFieldOffset("sHumanPortrait","tex");
    pg_unit_offset=loader->GetFieldOffset("sHumanPortrait","unit");
    pg_class_offset=loader->GetFieldOffset("sHumanPortrait","className");
    pg_gender_offset=loader->GetFieldOffset("sHumanPortrait","gender");
    pg_id_offset=loader->GetFieldOffset("Roster::Trooper","id");
    pg_id_tex=loader->GetFieldOffset("sHumanId","tex");
    pg_id_hash=loader->GetFieldOffset("sHumanId","texHash");
    pg_id_gender=loader->GetFieldOffset("sHumanId","gender");
    pg_render_offset=loader->GetFieldOffset("GUI::StaticImage","m_renderObject");
    pg_stride=loader->GetTypeSize("sHumanPortrait");
    if(!pg_deploy_update || pg_selection_offset<0 || pg_selection_roster<0 || !pg_texture_manager || !pg_load_texture || !pg_set_texture || !pg_update_portrait || pg_list_offset<0 || pg_data_offset<0 ||
       pg_count_offset<0 || pg_tex_offset<0 || pg_unit_offset<0 || pg_class_offset<0 || pg_gender_offset<0 || pg_id_offset<0 ||
       pg_id_tex<0 || pg_id_hash<0 || pg_id_gender<0 || pg_render_offset<0 || !pg_stride ||
       (unsigned)pg_tex_offset+128>pg_stride || (unsigned)pg_id_offset+pg_id_gender+sizeof(int)>trooper_size)return false;
    return loader->Subscribe(DK2ML_EVENT_FRAME,pg_frame,NULL)==DK2ML_OK;
}
#endif
