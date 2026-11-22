#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "gear_management.c"

static unsigned char source[384], target[384];
static unsigned char unlock_entries[16], unlock_list[32];
static unsigned free_item, locked_item = 1;
static void *library = (void *)1;
static unsigned checks;
static void quiet_log(const char *fmt, ...) { (void)fmt; }
static const void *lookup(const void *lib, const char *name) {
    assert(lib == library);
    ++checks;
    if (!strcmp(name, "Missing")) return NULL;
    return !strcmp(name, "Locked") ? &locked_item : &free_item;
}
static bool allowed(const void *eq, const void *trooper) {
    assert(eq && trooper == target);
    return strcmp((const char *)slot_at(source, slots[0]), "WrongUnit") != 0;
}
static void put(unsigned char *trooper, int slot, const char *name, uint32_t hash) {
    unsigned char *p = trooper + inventory_offset + (size_t)slot * string_size;
    memset(p, 0, string_size);
    assert(strlen(name) < (size_t)hash_offset);
    strcpy((char *)p, name);
    memcpy(p + hash_offset, &hash, sizeof hash);
}
static void setup(void) {
    static const DK2ML_API api = {.Log=quiet_log};
    loader = &api;
    object_library = &library;
    get_equipment = lookup;
    can_use = allowed;
    inventory_offset=48; string_offset=0; hash_offset=12; string_size=16;
    unlock_cost_offset=0; list_data_offset=8; list_count_offset=16;
    paste_depth=0; checks=0; layout_active=true;
    memset(source, 0xa5, sizeof source);
    memset(target, 0x5a, sizeof target);
    memset(unlock_list, 0, sizeof unlock_list);
    for (unsigned i=0; i<sizeof slots/sizeof slots[0]; ++i) {
        slots[i]=(int)i;
        put(source, slots[i], "Allowed", 100+i);
        put(target, slots[i], "Old", 200+i);
    }
}
static void assert_inventory_matches(void) {
    for (unsigned i=0; i<sizeof slots/sizeof slots[0]; ++i)
        assert(!memcmp(slot_at(source, slots[i]), slot_at(target, slots[i]), string_size));
    for (int i=0; i<inventory_offset; ++i) assert(target[i]==0x5a);
    for (size_t i=inventory_offset+11*string_size; i<sizeof target; ++i) assert(target[i]==0x5a);
}
int main(void) {
    setup();
    assert(supplement_loadout(unlock_list, source, target)==0);
    assert_inventory_matches(); // both extras, final cosmetic, hashes; identity/stats preserved
    assert(checks==11);
    assert(supplement_loadout(unlock_list, source, target)==0 && checks==11);
    put(source, slots[0], "", 0); put(source, slots[1], "", 0);
    assert(supplement_loadout(unlock_list, source, target)==0);
    assert_inventory_matches(); // explicitly empty slots clear previously equipped items

    setup(); put(source, slots[0], "WrongUnit", 101);
    assert(supplement_loadout(unlock_list, source, target)==11);
    assert(!strcmp((const char *)slot_at(target, slots[0]), "Old"));
    setup(); put(source, slots[1], "Missing", 102);
    assert(supplement_loadout(unlock_list, source, target)==1);
    assert(!strcmp((const char *)slot_at(target, slots[1]), "Old"));

    setup(); put(source, slots[1], "Locked", 12345);
    assert(supplement_loadout(unlock_list, source, target)==1);
    assert(!strcmp((const char *)slot_at(target, slots[1]), "Old"));
    memset(unlock_entries, 0, sizeof unlock_entries);
    uint32_t hash=12345; memcpy(unlock_entries+hash_offset, &hash, sizeof hash);
    const void *entries=unlock_entries; int count=1;
    memcpy(unlock_list+list_data_offset, &entries, sizeof entries);
    memcpy(unlock_list+list_count_offset, &count, sizeof count);
    assert(supplement_loadout(unlock_list, source, target)==0);
    assert_inventory_matches();

    setup();
    DK2ML_Regs regs={.rcx=(uintptr_t)unlock_list, .rdx=(uintptr_t)source, .r8=(uintptr_t)target, .rax=1};
    copy_pre(&regs, NULL); copy_post(&regs, NULL);
    assert(checks==0); // unrelated helper calls have no side effects
    paste_pre(&regs, NULL); copy_pre(&regs, NULL);
    regs.rcx=regs.rdx=regs.r8=0; // original may clobber every volatile argument
    copy_post(&regs, NULL); paste_post(&regs, NULL);
    assert_inventory_matches(); assert(paste_depth==0 && regs.rax==1);
    regs.rax=0; copy_post(&regs, NULL); assert(regs.rax==0);
    put(source, slots[1], "Missing", 999); regs.rax=1;
    copy_post(&regs, NULL); assert(regs.rax==0);
    setup(); layout_active=false;
    regs=(DK2ML_Regs){.rcx=(uintptr_t)unlock_list, .rdx=(uintptr_t)source, .r8=(uintptr_t)target};
    paste_pre(&regs,NULL);
    DK2ML_Regs copy_regs=regs;
    copy_pre(&copy_regs,NULL); copy_post(&copy_regs,NULL);
    paste_post(&regs,NULL);
    assert(checks==0 && paste_depth==0);
    puts("Extended paste checks passed: full/empty extra slots, all cosmetics, hashes, compatibility, unlocks, hook scope, saved arguments, partial failures, and unchanged non-equipment memory.");
    return 0;
}
