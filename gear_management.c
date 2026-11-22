#include "third_party/dk2ml/dk2ml.h"
#include <stdbool.h>

DK2ML_PLUGIN_MANIFEST(3, "Gear Management Overhaul", "0.5.9", "Rake5000", "", 112);
static const DK2ML_API *loader;
static void **object_library;
static const void *(*get_equipment)(const void *, const char *);
static bool (*can_use)(const void *, const void *);
static int inventory_offset, string_offset, hash_offset, unlock_cost_offset;
static int list_data_offset, list_count_offset;
static unsigned string_size, trooper_size;
static unsigned paste_depth;
static bool layout_active;
static const char *slot_names[] = {"SUPPORT_GEAR2", "SUPPORT_GEAR3", "CUSTOM1", "CUSTOM2", "CUSTOM3", "CUSTOM4",
                                  "CUSTOM5", "CUSTOM6", "CUSTOM7", "CUSTOM8", "CUSTOM9"};
static int slots[sizeof slot_names / sizeof slot_names[0]];

static const unsigned char *slot_at(const void *trooper, int slot) {
    return (const unsigned char *)trooper + inventory_offset + (size_t)slot * string_size;
}

static bool unlocked(const void *equipment, const void *unlocks, const unsigned char *item) {
    unsigned cost;
    memcpy(&cost, (const unsigned char *)equipment + unlock_cost_offset, sizeof cost);
    if (!cost) return true;
    if (!unlocks) return false;
    int count;
    const unsigned char *entries;
    memcpy(&count, (const unsigned char *)unlocks + list_count_offset, sizeof count);
    memcpy(&entries, (const unsigned char *)unlocks + list_data_offset, sizeof entries);
    if (count < 0 || count > 100000 || (count && !entries)) return false;
    for (int i = 0; i < count; ++i) {
        if (!memcmp(entries + (size_t)i * string_size + hash_offset, item + hash_offset, sizeof(uint32_t))) return true;
    }
    return false;
}

/* StaticString<48> owns no allocation: copy both the text and its cached hash.
 * Called inside the game's paste operation, before its Save and inventory refresh. */
static unsigned supplement_loadout(const void *unlocks, const void *source, void *target) {
    if (!source || !target || source == target) return 0;
    unsigned rejected = 0;
    for (unsigned i = 0; i < sizeof slots / sizeof slots[0]; ++i) {
        const unsigned char *from = slot_at(source, slots[i]);
        unsigned char *to = (unsigned char *)target + inventory_offset + (size_t)slots[i] * string_size;
        if (!memcmp(from, to, string_size)) continue;
        const char *name = (const char *)from + string_offset;
        if (*name) {
            const void *equipment = *object_library ? get_equipment(*object_library, name) : NULL;
            if (!equipment || !can_use(equipment, target) || !unlocked(equipment, unlocks, from)) {
                loader->Log("Paste retained %s: item '%s' is missing, unavailable, or incompatible with the target.", slot_names[i], name);
                ++rejected;
                continue;
            }
        }
        memcpy(to, from, string_size);
    }
    return rejected;
}

static int paste_pre(DK2ML_Regs *regs, void *user) {
    (void)user;
    regs->scratch[0] = layout_active ? 1 : 0;
    if (regs->scratch[0]) ++paste_depth;
    return DK2ML_CALL_ORIGINAL;
}

static void paste_post(DK2ML_Regs *regs, void *user) {
    (void)user;
    if (regs->scratch[0] && paste_depth) --paste_depth;
}

static int copy_pre(DK2ML_Regs *regs, void *user) {
    (void)user;
    regs->scratch[0] = paste_depth ? DK2ML_Arg(regs, 0) : 0;
    regs->scratch[1] = paste_depth ? DK2ML_Arg(regs, 1) : 0;
    regs->scratch[2] = paste_depth ? DK2ML_Arg(regs, 2) : 0;
    return DK2ML_CALL_ORIGINAL;
}

static void copy_post(DK2ML_Regs *regs, void *user) {
    (void)user;
    if (!regs->scratch[1] || !regs->scratch[2]) return;
    unsigned rejected = supplement_loadout((const void *)(uintptr_t)regs->scratch[0],
                                          (const void *)(uintptr_t)regs->scratch[1],
                                          (void *)(uintptr_t)regs->scratch[2]);
    // Preserve an existing partial-failure result; let the game's normal warning run.
    if (rejected) regs->rax &= ~UINT64_C(0xff);
    loader->Log("Extended paste completed: both extra support slots and all nine cosmetics checked; %u additional rejected slot(s).", rejected);
}

static bool init_copying(void) {
    void *copy = loader->ResolveSymbol("CopyTrooperLoadoutTo");
    void *paste = loader->ResolveSymbol("GameGUI::Customize_OnPasteLoadout");
    object_library = loader->ResolveSymbol("g_pObjectLibrary");
    get_equipment = (const void *(*)(const void *, const char *))loader->ResolveSymbol("?GetEquipment@ObjectLibrary@@QEBAPEBVEquipment_Template@@PEBD@Z");
    can_use = (bool (*)(const void *, const void *))loader->ResolveSymbol("CanEquipmentBeUsedBy");
    inventory_offset = loader->GetFieldOffset("Roster::Trooper", "inventory");
    string_offset = loader->GetFieldOffset("StaticString<48>", "m_szString");
    hash_offset = loader->GetFieldOffset("StaticString<48>", "m_hash");
    unlock_cost_offset = loader->GetFieldOffset("Equipment_Template", "unlockCostBH");
    list_data_offset = loader->GetFieldOffset("List<StaticString<48> >", "m_list");
    list_count_offset = loader->GetFieldOffset("List<StaticString<48> >", "m_elements");
    string_size = loader->GetTypeSize("StaticString<48>");
    trooper_size = loader->GetTypeSize("Roster::Trooper");
    int64_t count;
    if (!copy || !paste || !object_library || !get_equipment || !can_use || inventory_offset < 0 ||
        string_offset < 0 || hash_offset < 0 || unlock_cost_offset < 0 || list_data_offset < 0 || list_count_offset < 0 ||
        !string_size || !trooper_size || (unsigned)hash_offset + sizeof(uint32_t) > string_size ||
        loader->GetEnumValue("Inventory::eType", "NUM_VALUES", &count) != DK2ML_OK || count <= 0 ||
        (uint64_t)inventory_offset + (uint64_t)count * string_size > trooper_size) return false;
    for (unsigned i = 0; i < sizeof slots / sizeof slots[0]; ++i) {
        int64_t slot;
        if (loader->GetEnumValue("Inventory::eType", slot_names[i], &slot) != DK2ML_OK || slot < 0 || slot >= count) return false;
        slots[i] = (int)slot;
    }
    if (loader->CreateSafeHook(paste, paste_pre, paste_post, NULL) != DK2ML_OK ||
        loader->CreateSafeHook(copy, copy_pre, copy_post, NULL) != DK2ML_OK ||
        loader->EnableHook(paste) != DK2ML_OK || loader->EnableHook(copy) != DK2ML_OK) return false;
    return true;
}

#include "presets_runtime.h"
#include "portrait_picker.h"

static void check_layout(const DK2ML_Event *event, void *user) {
    (void)event;
    (void)user;
    layout_active = false;
    pm_layout_loaded();
    pg_layout_loaded();
    void *screen = loader->GuiFind(NULL, "Customization_Screen");
    if (!screen || !loader->GuiFind(screen, "#r5k_gmo_v01_layout")) {
        loader->Log("Gear Management Overhaul layout is inactive; extended paste is idle.");
        return;
    }
    const char *slots[] = {"Helmet", "Armor", "Custom1", "Custom2", "Custom3", "Custom4", "Custom5", "Custom6",
                           "Custom7", "Custom8", "Custom9", "SupportGear1", "SupportGear2", "SupportGear3"};
    void *inventory = loader->GuiFind(screen, "#inventory_panel");
    for (unsigned i = 0; i < sizeof slots / sizeof slots[0]; ++i) {
        if (!loader->GuiFind(inventory, slots[i])) {
            loader->Log("Combined layout is missing slot %s.", slots[i]);
            return;
        }
    }
    layout_active = true;
    pm_layout_loaded();
    pg_layout_loaded();
    loader->Log("Combined gear layout verified. Target: 3840 x 2160. Extended loadout paste active.");
}

DK2ML_EXPORT int DK2ML_PluginInit(const DK2ML_API *api, const DK2ML_PluginInfo *info) {
    (void)info;
    loader = api;
    if (!init_copying()) {
        api->Log("Extended paste initialization failed; no guessing of game layouts or addresses.");
        return 1;
    }
    if (!init_presets()) {
        api->Log("Preset manager initialization failed.");
        return 2;
    }
    if (!init_portraits()) {
        api->Log("Portrait picker initialization failed.");
        return 3;
    }
    if (api->Subscribe(DK2ML_EVENT_GUI_LOADED, check_layout, NULL) != DK2ML_OK) return 1;
    api->Log("Gear Management Overhaul v0.5.9 loaded. Modal preset input and cached library; portrait picker and extended paste retained.");
    return 0;
}
