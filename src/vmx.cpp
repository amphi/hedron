/*
 * Virtual Machine Extensions (VMX)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 *
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#include "cmdline.hpp"
#include "cpulocal.hpp"
#include "ept.hpp"
#include "hip.hpp"
#include "idt.hpp"
#include "math.hpp"
#include "msr.hpp"
#include "stdio.hpp"
#include "tss.hpp"
#include "vmx.hpp"
#include "vmx_preemption_timer.hpp"
#include "x86.hpp"

Vmcs::Vmcs (mword esp, mword bmp, mword cr3, Ept const &ept, unsigned cpu) : rev (basic().revision)
{
    make_current();

    uint64 eptp = ept.vmcs_eptp();
    uint32 pin = PIN_EXTINT | PIN_NMI | PIN_VIRT_NMI | PIN_PREEMPT_TIMER;
    uint32 exi = EXI_INTA | EXI_SAVE_PREEMPT_TIMER;
    uint32 ent = 0;

    write (PF_ERROR_MASK, 0);
    write (PF_ERROR_MATCH, 0);
    write (CR3_TARGET_COUNT, 0);

    write (VMCS_LINK_PTR,    ~0ul);
    write (VMCS_LINK_PTR_HI, ~0ul);

    write (VPID, Atomic::add(remote_ref_vpid_ctr(cpu), 1U));

    write (EPTP,    static_cast<mword>(eptp));
    write (EPTP_HI, static_cast<mword>(eptp >> 32));

    write (IO_BITMAP_A, bmp);
    write (IO_BITMAP_B, bmp + PAGE_SIZE);

    write (HOST_SEL_CS, SEL_KERN_CODE);
    write (HOST_SEL_SS, SEL_KERN_DATA);
    write (HOST_SEL_DS, SEL_KERN_DATA);
    write (HOST_SEL_ES, SEL_KERN_DATA);
    write (HOST_SEL_TR, SEL_TSS_RUN);

    write (HOST_PAT,  Msr::read (Msr::IA32_CR_PAT));
    write (HOST_EFER, Msr::read (Msr::IA32_EFER));
    exi |= EXI_SAVE_EFER | EXI_LOAD_EFER | EXI_HOST_64;
    exi |= EXI_SAVE_PAT  | EXI_LOAD_PAT;
    ent |= ENT_LOAD_EFER;
    ent |= ENT_LOAD_PAT;

    write (PIN_CONTROLS, (pin | ctrl_pin().set) & ctrl_pin().clr);
    write (EXI_CONTROLS, (exi | ctrl_exi().set) & ctrl_exi().clr);
    write (ENT_CONTROLS, (ent | ctrl_ent().set) & ctrl_ent().clr);

    write (HOST_CR3, cr3);
    write (HOST_CR0, get_cr0());
    write (HOST_CR4, get_cr4());

    write (HOST_BASE_GS,   reinterpret_cast<mword>(&Cpulocal::get_remote(cpu).self));
    write (HOST_BASE_TR,   reinterpret_cast<mword>(&Tss::remote(cpu)));
    write (HOST_BASE_GDTR, reinterpret_cast<mword>(&Cpulocal::get_remote(cpu).gdt));
    write (HOST_BASE_IDTR, reinterpret_cast<mword>(Idt::idt));

    write (HOST_SYSENTER_CS,  SEL_KERN_CODE);
    write (HOST_SYSENTER_ESP, reinterpret_cast<mword>(&Tss::remote(cpu).sp0));
    write (HOST_SYSENTER_EIP, reinterpret_cast<mword>(&entry_sysenter));

    write (HOST_RSP, esp);
    write (HOST_RIP, reinterpret_cast<mword>(&entry_vmx));

    vmx_timer::set (~0ull);
}

bool Vmcs::try_enable_vmx()
{
    auto feature_ctrl = Msr::read (Msr::IA32_FEATURE_CONTROL);

    // Some BIOSes don't enable VMX, but leave the lock bit unset. In this case,
    // we try to enable it ourselves.
    if (not (feature_ctrl & Msr::FEATURE_LOCKED)) {
            Msr::write (Msr::IA32_FEATURE_CONTROL, feature_ctrl | Msr::FEATURE_VMX_O_SMX | Msr::FEATURE_LOCKED);
    }

    return !!(Msr::read (Msr::IA32_FEATURE_CONTROL) & Msr::FEATURE_VMX_O_SMX);
}

void Vmcs::init()
{
    if (not Cpu::feature (Cpu::FEAT_VMX) or not try_enable_vmx()) {
        Hip::clr_feature (Hip::FEAT_VMX);
        return;
    }

    fix_cr0_set() =  Msr::read (Msr::IA32_VMX_CR0_FIXED0);
    fix_cr0_clr() = ~Msr::read (Msr::IA32_VMX_CR0_FIXED1);
    fix_cr0_mon() = 0;

    fix_cr4_set() =  Msr::read (Msr::IA32_VMX_CR4_FIXED0);
    fix_cr4_clr() = ~Msr::read (Msr::IA32_VMX_CR4_FIXED1);
    fix_cr4_mon() = 0;

    basic().val       = Msr::read (Msr::IA32_VMX_BASIC);
    ctrl_exi().val    = Msr::read (basic().ctrl ? Msr::IA32_VMX_TRUE_EXIT  : Msr::IA32_VMX_CTRL_EXIT);
    ctrl_ent().val    = Msr::read (basic().ctrl ? Msr::IA32_VMX_TRUE_ENTRY : Msr::IA32_VMX_CTRL_ENTRY);
    ctrl_pin().val    = Msr::read (basic().ctrl ? Msr::IA32_VMX_TRUE_PIN   : Msr::IA32_VMX_CTRL_PIN);
    ctrl_cpu()[0].val = Msr::read (basic().ctrl ? Msr::IA32_VMX_TRUE_CPU0  : Msr::IA32_VMX_CTRL_CPU0);

    if (has_secondary()) {
        ctrl_cpu()[1].val = Msr::read (Msr::IA32_VMX_CTRL_CPU1);
    }

    // Until we have a way to test on such machines, we require all those
    // features to be present:
    // - Extended Page Tables (EPT)
    // - Unrestricted Guest (URG)
    // - Guest PAT
    // - MSR Bitmap
    // - VMX preemption timer
    if (not has_ept() or
        not has_urg() or
        not has_guest_pat() or
        not has_msr_bmp() or
        not has_vmx_preemption_timer()) {

        Hip::clr_feature (Hip::FEAT_VMX);
        return;
    }

    vmx_timer::init();

    ept_vpid().val = Msr::read (Msr::IA32_VMX_EPT_VPID);

    // Bit n in this mask means that n can be a leaf level.
    mword const leaf_bit_mask {1U /* 4K */ | (ept_vpid().super << 1)};
    auto  const leaf_levels {static_cast<Ept::level_t>(bit_scan_reverse (leaf_bit_mask) + 1)};
    Ept::set_supported_leaf_levels (leaf_levels);

    fix_cr0_set() &= ~(Cpu::CR0_PG | Cpu::CR0_PE);

    fix_cr0_clr() |= Cpu::CR0_CD | Cpu::CR0_NW;

    ctrl_cpu()[0].set |= CPU_HLT | CPU_IO | CPU_SECONDARY;
    ctrl_cpu()[1].set |= CPU_VPID | CPU_URG;

    if (not ept_vpid().invept) {
        Hip::clr_feature (Hip::FEAT_VMX);
        return;
    }

    if (Cmdline::novpid or not ept_vpid().invvpid) {
        ctrl_cpu()[1].clr &= ~CPU_VPID;
    }

    if (has_secondary()) {
        Hip::set_secondary_vmx_caps(ctrl_cpu()[1].val);
    }

    set_cr0 ((get_cr0() & ~fix_cr0_clr()) | fix_cr0_set());
    set_cr4 ((get_cr4() & ~fix_cr4_clr()) | fix_cr4_set());

    // We pass through this code at boot time and after each resume. Re-use the
    // root VMCS by allocating it only once.
    static Vmcs *root = new Vmcs;
    root->vmxon();

    trace (TRACE_VMX, "VMCS:%#010lx REV:%#x EPT:%d URG:%d VNMI:%d VPID:%d", Buddy::ptr_to_phys (root), basic().revision, has_ept(), has_urg(), has_vnmi(), has_vpid());
}
