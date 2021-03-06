#define __STDC_FORMAT_MACROS

#include "panda/panda_addr.h"

extern "C" {


#include "rr_log.h"
#include "qemu-common.h"
#include "cpu.h"
#include "panda_plugin.h"
#include "pandalog.h"
#include "panda_common.h"

#include "../pri/pri_types.h"
#include "../pri/pri_ext.h"
#include "../pri/pri.h"
//#include "../dwarfp/dwarfp_ext.h"
#include "panda_plugin_plugin.h"

    bool init_plugin(void *);
    void uninit_plugin(void *);

    int get_loglevel() ;
    void set_loglevel(int new_loglevel);

    //void on_line_change(CPUState *env, target_ulong pc, const char *file_Name, const char *funct_name, unsigned long long lno);
}
struct args {
    CPUState *env;
    const char *src_filename;
    uint64_t src_linenum;
};
#if defined(TARGET_I386) && !defined(TARGET_X86_64)
//void pfun(VarType var_ty, const char *var_nm, LocType loc_t, target_ulong loc, void *in_args){
void pfun(void *var_ty_void, const char *var_nm, LocType loc_t, target_ulong loc, void *in_args){
    //const char *var_ty = (const char *) var_ty_void;
    // restore args
    struct args *args = (struct args *) in_args;
    CPUState *pfun_env = args->env;
    //const char *src_filename = args->src_filename;
    //uint64_t src_linenum = args->src_linenum;

    target_ulong guest_dword;
    switch (loc_t){
        case LocReg:
            printf("VAR REG: %s in Reg %d", var_nm, loc);
            printf("    => 0x%x\n", pfun_env->regs[loc]);
            break;
        case LocMem:
            printf("VAR MEM: %s @ 0x%x", var_nm, loc);
            panda_virtual_memory_rw(pfun_env, loc, (uint8_t *)&guest_dword, sizeof(guest_dword), 0);
            printf("    => 0x%x\n", guest_dword);
            break;
        case LocConst:
            //printf("VAR CONST: %s %s as 0x%x\n", var_ty, var_nm, loc);
            break;
        case LocErr:
            printf("VAR does not have a location we could determine. Most likely because the var is split among multiple locations\n");
            break;
    }
}
void on_line_change(CPUState *env, target_ulong pc, const char *file_Name, const char *funct_name, unsigned long long lno){
    //struct args args = {env, file_Name, lno};
    //printf("[%s] %s(), ln: %4lld, pc @ 0x%x\n",file_Name, funct_name,lno,pc);
    //pri_funct_livevar_iter(env, pc, (liveVarCB) pfun, (void *) &args);
}
void on_fn_start(CPUState *env, target_ulong pc, const char *file_Name, const char *funct_name, unsigned long long lno){
    struct args args = {env, file_Name, lno};
    printf("fn-start: %s() [%s], ln: %4lld, pc @ 0x%x\n",funct_name,file_Name,lno,pc);
    pri_funct_livevar_iter(env, pc, (liveVarCB) pfun, (void *) &args);
}


int virt_mem_helper(CPUState *env, target_ulong pc, target_ulong addr, bool isRead) {
    SrcInfo info;
    // if NOT in source code, just return
    int rc = pri_get_pc_source_info(env, pc, &info);
    // We are not in dwarf info
    if (rc == -1){
        return 0;
    }
    // We are in the first byte of a .plt function
    if (rc == 1) {
        return 0;
    }
    printf("==%s %ld==\n", info.filename, info.line_number);
    struct args args = {env, NULL, 0};
    pri_funct_livevar_iter(env, pc, (liveVarCB) pfun, (void *) &args);
    char *symbol_name = pri_get_vma_symbol(env, pc, addr);
    if (!symbol_name){
        // symbol was not found for particular addr
        if (isRead) {
            printf ("Virt mem read at 0x%x - (NONE)\n", addr);
        }
        else {
            printf ("Virt mem write at 0x%x - (NONE)\n", addr);
        }
        return 0;
    }
    else {
        if (isRead) {
            printf ("Virt mem read at 0x%x - \"%s\"\n", addr, symbol_name);
        }
        else {
            printf ("Virt mem write at 0x%x - \"%s\"\n", addr, symbol_name);
        }
    }
    return 0;
}

int virt_mem_read(CPUState *env, target_ulong pc, target_ulong addr, target_ulong size, void *buf) {
    return virt_mem_helper(env, pc, addr, true);

}

int virt_mem_write(CPUState *env, target_ulong pc, target_ulong addr, target_ulong size, void *buf) {
    return virt_mem_helper(env, pc, addr, false);
}
#endif

bool init_plugin(void *self) {

#if defined(TARGET_I386) && !defined(TARGET_X86_64)
    printf("Initializing plugin pri_simple\n");
    //panda_arg_list *args = panda_get_args("pri_taint");
    panda_require("pri");
    assert(init_pri_api());
    //panda_require("dwarfp");
    //assert(init_dwarfp_api());

    PPP_REG_CB("pri", on_before_line_change, on_line_change);
    //PPP_REG_CB("pri", on_fn_start, on_fn_start);
    {
        panda_cb pcb;
        pcb.virt_mem_write = virt_mem_write;
        panda_register_callback(self,PANDA_CB_VIRT_MEM_WRITE,pcb);
        pcb.virt_mem_read = virt_mem_read;
        panda_register_callback(self,PANDA_CB_VIRT_MEM_READ,pcb);
    }
#endif
    return true;
}



void uninit_plugin(void *self) {
}

