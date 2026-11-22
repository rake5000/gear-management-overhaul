#ifndef R5K_PRESET_CORE_H
#define R5K_PRESET_CORE_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define PM_MEMBERS 32
#define PM_EQUIPMENT 25
#define PM_TEXT 48
#define PM_NAME 128
#define PM_LIBRARY 128
enum { PM_TROOPER=1, PM_SECTION=2, PM_UNIT=3 };
static const char *pm_slot_keys[PM_EQUIPMENT] = {
    "PrimaryWeapon", "PrimaryWeaponScope", "PrimaryWeaponAmmo", "PrimaryWeaponMuzzle",
    "SecondaryWeapon", "SecondaryWeaponScope", "SecondaryWeaponAmmo", "SecondaryWeaponMuzzle",
    "Helmet", "HelmetNVG", "Armor", "UtilityPouch1", "UtilityPouch2", "SupportGear1", "SupportGear2", "SupportGear3",
    "Custom1", "Custom2", "Custom3", "Custom4", "Custom5", "Custom6", "Custom7", "Custom8", "Custom9"};
static const char *pm_slot_enums[PM_EQUIPMENT] = {
    "PRIMARY_WEAPON", "PRIMARY_WEAPON_SCOPE", "PRIMARY_WEAPON_AMMO", "PRIMARY_WEAPON_MUZZLE",
    "SECONDARY_WEAPON", "SECONDARY_WEAPON_SCOPE", "SECONDARY_WEAPON_AMMO", "SECONDARY_WEAPON_MUZZLE",
    "HELMET", "HELMET_NVG", "ARMOR", "UTILITY_POUCH1", "UTILITY_POUCH2", "SUPPORT_GEAR1", "SUPPORT_GEAR2", "SUPPORT_GEAR3",
    "CUSTOM1", "CUSTOM2", "CUSTOM3", "CUSTOM4", "CUSTOM5", "CUSTOM6", "CUSTOM7", "CUSTOM8", "CUSTOM9"};
typedef struct { char class_id[PM_TEXT]; int count; } PM_Section;
typedef struct { char class_id[PM_TEXT]; char gear[PM_EQUIPMENT][PM_TEXT]; } PM_Member;
typedef struct {
    int kind, section_count, member_count;
    char name[PM_NAME], unit[PM_TEXT];
    PM_Section sections[PM_MEMBERS];
    PM_Member members[PM_MEMBERS];
} PM_Preset;
typedef struct {
    char unit[PM_TEXT], classes[PM_MEMBERS][PM_TEXT];
    int count, selected, section_count, selected_section;
    PM_Section sections[PM_MEMBERS];
    int indices[PM_MEMBERS][PM_MEMBERS];
} PM_Context;
typedef struct { int count, target[PM_MEMBERS], source[PM_MEMBERS]; } PM_Plan;
static bool pm_text_valid(const char *s, size_t capacity, bool allow_empty) {
    const char *end=memchr(s,0,capacity);
    if(!end || (!allow_empty && end==s)) return false;
    for(const char *p=s; p<end; ++p) if((unsigned char)*p<32 || *p=='"') return false;
    return true;
}
static const char *pm_family(const char *unit, const char *class_id) {
    if(!strcmp(unit,"FOG") && (!strcmp(class_id,"bv_operator_1") || !strcmp(class_id,"bv_operator_2") ||
       !strcmp(class_id,"bv_operator_3") || !strcmp(class_id,"bv_operator_4"))) return "r5k_blue_vector_operator";
    return class_id;
}
static bool pm_same_role(const char *unit, const char *a, const char *b) {
    return !strcmp(pm_family(unit,a),pm_family(unit,b));
}
static bool pm_context_build(PM_Context *c) {
    if(c->count<1 || c->count>PM_MEMBERS || c->selected<0 || c->selected>=c->count ||
       !pm_text_valid(c->unit,sizeof c->unit,false)) return false;
    c->section_count=0; c->selected_section=-1;
    memset(c->sections,0,sizeof c->sections);
    for(int i=0;i<c->count;++i) {
        if(!pm_text_valid(c->classes[i],PM_TEXT,false)) return false;
        int s=0;
        while(s<c->section_count && strcmp(c->sections[s].class_id,c->classes[i])) ++s;
        if(s==c->section_count) {
            strcpy(c->sections[s].class_id,c->classes[i]); ++c->section_count;
        }
        c->indices[s][c->sections[s].count++]=i;
        if(i==c->selected) c->selected_section=s;
    }
    return c->selected_section>=0;
}
static bool pm_valid(const PM_Preset *p) {
    if(p->kind<PM_TROOPER || p->kind>PM_UNIT || p->member_count<1 || p->member_count>PM_MEMBERS ||
       p->section_count<1 || p->section_count>PM_MEMBERS || !pm_text_valid(p->name,PM_NAME,false) ||
       !pm_text_valid(p->unit,PM_TEXT,false)) return false;
    if((p->kind==PM_TROOPER && (p->member_count!=1 || p->section_count!=1)) ||
       (p->kind==PM_SECTION && p->section_count!=1)) return false;
    int index=0;
    for(int s=0;s<p->section_count;++s) {
        const PM_Section *section=&p->sections[s];
        if(!pm_text_valid(section->class_id,PM_TEXT,false) || section->count<1 || section->count>PM_MEMBERS ||
           index+section->count>p->member_count) return false;
        for(int i=0;i<section->count;++i,++index) {
            if(!pm_text_valid(p->members[index].class_id,PM_TEXT,false) || strcmp(section->class_id,p->members[index].class_id)) return false;
            for(int g=0;g<PM_EQUIPMENT;++g) if(!pm_text_valid(p->members[index].gear[g],PM_TEXT,true)) return false;
        }
    }
    return index==p->member_count;
}
static bool pm_plan(const PM_Preset *p, const PM_Context *c, int scope, PM_Plan *plan) {
    memset(plan,0,sizeof *plan);
    if(!pm_valid(p) || c->count<1 || c->count>PM_MEMBERS || c->selected<0 || c->selected>=c->count ||
       scope<0 || scope>2 || strcmp(p->unit,c->unit)) return false;
    int section=c->selected_section;
    if(section<0 || section>=c->section_count) return false;
    if(p->kind==PM_TROOPER) {
        if(!pm_same_role(c->unit,p->members[0].class_id,c->classes[c->selected])) return false;
        for(int i=0;i<c->count;++i) {
            bool included=scope==0 ? i==c->selected : scope==1 ? !strcmp(c->classes[i],c->classes[c->selected]) :
                          pm_same_role(c->unit,c->classes[i],p->members[0].class_id);
            if(included) {plan->target[plan->count]=i; plan->source[plan->count++]=0;}
        }
    } else if(p->kind==PM_SECTION) {
        if(p->sections[0].count!=c->sections[section].count ||
           !pm_same_role(c->unit,p->sections[0].class_id,c->sections[section].class_id)) return false;
        for(int i=0;i<p->member_count;++i) {plan->target[i]=c->indices[section][i]; plan->source[i]=i;}
        plan->count=p->member_count;
    } else {
        if(p->section_count!=c->section_count) return false;
        int source=0;
        for(int s=0;s<p->section_count;++s) {
            if(p->sections[s].count!=c->sections[s].count ||
               !pm_same_role(c->unit,p->sections[s].class_id,c->sections[s].class_id)) return false;
            for(int i=0;i<p->sections[s].count;++i) {
                plan->target[plan->count]=c->indices[s][i]; plan->source[plan->count++]=source++;
            }
        }
    }
    return plan->count>0;
}
static uint32_t pm_hash(const char *s) {
    if(!*s) return 0;
    uint32_t hash=2166136261u;
    while(*s) {hash^=(unsigned char)*s++; hash*=16777619u;}
    return hash;
}
#endif
