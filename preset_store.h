#ifndef R5K_PRESET_STORE_H
#define R5K_PRESET_STORE_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "preset_core.h"
static bool pm_wide(const char *src,wchar_t *dst,int count) {
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src,-1,dst,count)>0;
}
static bool pm_utf8(const wchar_t *src,char *dst,int count) {
    return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,src,-1,dst,count,NULL,NULL)>0;
}
static bool pm_read_text(const wchar_t *file,const wchar_t *section,const wchar_t *key,char *dst,int capacity,bool empty) {
    wchar_t buffer[512];
    DWORD n=GetPrivateProfileStringW(section,key,L"\x1",buffer,512,file);
    if(n>=511 || buffer[0]==1 || !pm_utf8(buffer,dst,capacity)) return false;
    return pm_text_valid(dst,(size_t)capacity,empty);
}
static bool pm_read_preset(const wchar_t *file,PM_Preset *p) {
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if(!GetFileAttributesExW(file,GetFileExInfoStandard,&attr) || attr.nFileSizeHigh || attr.nFileSizeLow>256*1024) return false;
    memset(p,0,sizeof *p);
    if(GetPrivateProfileIntW(L"preset",L"format",0,file)!=1) return false;
    p->kind=(int)GetPrivateProfileIntW(L"preset",L"kind",0,file);
    p->section_count=(int)GetPrivateProfileIntW(L"preset",L"sections",0,file);
    p->member_count=(int)GetPrivateProfileIntW(L"preset",L"members",0,file);
    if(p->section_count<1 || p->section_count>PM_MEMBERS || p->member_count<1 || p->member_count>PM_MEMBERS ||
       !pm_read_text(file,L"preset",L"name",p->name,PM_NAME,false) ||
       !pm_read_text(file,L"preset",L"unit",p->unit,PM_TEXT,false)) return false;
    for(int s=0;s<p->section_count;++s) {
        wchar_t section[32]; swprintf(section,32,L"section%d",s);
        p->sections[s].count=(int)GetPrivateProfileIntW(section,L"count",0,file);
        if(!pm_read_text(file,section,L"class",p->sections[s].class_id,PM_TEXT,false)) return false;
    }
    for(int m=0;m<p->member_count;++m) {
        wchar_t section[32]; swprintf(section,32,L"member%d",m);
        if(!pm_read_text(file,section,L"class",p->members[m].class_id,PM_TEXT,false)) return false;
        for(int g=0;g<PM_EQUIPMENT;++g) {
            wchar_t key[64]; if(!pm_wide(pm_slot_keys[g],key,64) ||
                !pm_read_text(file,section,key,p->members[m].gear[g],PM_TEXT,true)) return false;
        }
    }
    return pm_valid(p);
}
static bool pm_write_preset(const wchar_t *file,const PM_Preset *p) {
    if(!pm_valid(p)) return false;
    const size_t capacity=128*1024;
    char *text=malloc(capacity); wchar_t *wide=malloc(capacity*sizeof(wchar_t));
    if(!text || !wide) {free(text);free(wide);return false;}
    size_t used=(size_t)snprintf(text,capacity,"[preset]\r\nformat=1\r\nkind=%d\r\nname=%s\r\nunit=%s\r\nsections=%d\r\nmembers=%d\r\n",p->kind,p->name,p->unit,p->section_count,p->member_count);
    for(int s=0;s<p->section_count;++s) used+=(size_t)snprintf(text+used,capacity-used,"\r\n[section%d]\r\nclass=%s\r\ncount=%d\r\n",s,p->sections[s].class_id,p->sections[s].count);
    for(int m=0;m<p->member_count;++m) {
        used+=(size_t)snprintf(text+used,capacity-used,"\r\n[member%d]\r\nclass=%s\r\n",m,p->members[m].class_id);
        for(int g=0;g<PM_EQUIPMENT;++g) used+=(size_t)snprintf(text+used,capacity-used,"%s=%s\r\n",pm_slot_keys[g],p->members[m].gear[g]);
    }
    bool ok=false;
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,wide,(int)capacity);
    wchar_t temporary[MAX_PATH];
    if(count>0 && wcslen(file)+5<MAX_PATH) {
        swprintf(temporary,MAX_PATH,L"%ls.tmp",file);
        HANDLE handle=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(handle!=INVALID_HANDLE_VALUE) {
            WORD bom=0xfeff; DWORD written; DWORD bytes=(DWORD)(count-1)*sizeof(wchar_t);
            ok=WriteFile(handle,&bom,sizeof bom,&written,NULL) && written==sizeof bom &&
               WriteFile(handle,wide,bytes,&written,NULL) && written==bytes && FlushFileBuffers(handle);
            CloseHandle(handle);
            if(ok) {
                WritePrivateProfileStringW(NULL,NULL,NULL,file);
                ok=MoveFileExW(temporary,file,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
                WritePrivateProfileStringW(NULL,NULL,NULL,file);
            }
            if(!ok) DeleteFileW(temporary);
        }
    }
    free(text); free(wide); return ok;
}
#endif
