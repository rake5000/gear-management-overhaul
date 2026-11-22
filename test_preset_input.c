#undef NDEBUG
#include <assert.h>
#include "gear_management.c"
static unsigned capture_calls;static int capture_value;static bool capture_fail;
static DK2ML_Status capture_mock(int active){capture_calls++;capture_value=active;return capture_fail?DK2ML_ERROR:DK2ML_OK;}
int main(void){
 DK2ML_API mock={0};mock.CaptureGameInput=capture_mock;loader=&mock;
 int manager_object=0;void *manager=&manager_object;pm_gui_manager=&manager;
 DK2ML_Regs regs={0};regs.rcx=(uintptr_t)manager;layout_active=true;
 // Immediate protection on opening, without an ImGui hover/capture flag.
 pm_open=true;pm_capture_input(true);assert(pm_captured&&capture_calls==1&&capture_value==1);
 assert(pm_gui_input_pre(&regs,NULL)==DK2ML_SKIP_ORIGINAL&&regs.rax==1);
 pm_capture_input(true);assert(capture_calls==1);
 // Unrelated GUI manager calls are never swallowed.
 regs.rcx=0;assert(pm_gui_input_pre(&regs,NULL)==DK2ML_CALL_ORIGINAL);regs.rcx=(uintptr_t)manager;
 // A close click remains protected through that input update, then releases.
 pm_open=false;assert(pm_gui_input_pre(&regs,NULL)==DK2ML_SKIP_ORIGINAL);
 pm_frame(NULL,NULL);assert(!pm_captured&&capture_calls==2&&capture_value==0);
 assert(pm_gui_input_pre(&regs,NULL)==DK2ML_CALL_ORIGINAL);
 // Failed capture is retried; cached state must not claim it succeeded.
 capture_fail=true;pm_capture_input(true);assert(!pm_captured);
 capture_fail=false;pm_capture_input(true);assert(pm_captured);pm_capture_input(false);
 // First opening can draw before disk IO. Reopening a cached library does not
 // enqueue another scan; explicit Refresh still calls pm_reload directly.
 pm_library_loaded=false;pm_request_library();assert(pm_reload_pending&&pm_reload_wait==1);
 assert(!pm_load_due());assert(pm_load_due());assert(!pm_load_due());
 pm_library_loaded=true;pm_request_library();assert(!pm_reload_pending&&!pm_load_due());
 puts("PASS: modal input without hover flags, manager scoping, close-click drain, capture retry, deferred first load and cached reopen.");
 return 0;
}
