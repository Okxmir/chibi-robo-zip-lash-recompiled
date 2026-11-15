"""Prepare ARM function discovery from the pinned Clockwork recompiler."""

import argparse
import shutil
from pathlib import Path

from prepare_runtime import replace


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    shutil.copytree(args.upstream, args.output, dirs_exist_ok=True)
    path = args.output / "discover.rs"
    text = replace(path.read_text(encoding="utf-8"),
        "        Op::Udf | Op::Bkpt(_) => Flow::Stop,",
        "        Op::Udf | Op::Bkpt(_) if !conditional => Flow::Stop,")
    text = replace(text, "                    Op::BranchReg { rm, .. } => self.resolve(&e.f, site, rm, 0),",
        """                    // Thumb BX PC is an interworking veneer for the following ARM code.
                    Op::BranchReg { rm: PC, link: false } if insn.thumb =>
                        Some(Value::Const((site + 4) & !3)),
                    Op::BranchReg { rm, .. } => self.resolve(&e.f, site, rm, 0),""")
    text = replace(text, "        d.add_entry(image.text.addr, false);",
        """        // Seed known entries before tentative pointer discovery can reject them.
        // Zip Lash EUR invokes this memory callback through BLX r12 at 0x00166aa0.
        d.add_entry(0x0012_7ec0, false);
        // A save-open exception unwinds through this SDK cleanup landing pad.
        d.add_entry(0x0045_79d4, false);
        d.add_entry(0x0045_77a4, false);
        d.add_entry(0x0030_1d38, true); // _Unwind_Resume entry invoked by the cleanup
        d.add_entry(0x002f_dbac, true); // its Thumb-to-ARM context capture veneer
        d.add_entry(image.text.addr, false);""")
    path.write_text(text, encoding="utf-8")

    path = args.output / "emit.rs"
    text = path.read_text(encoding="utf-8")
    text = replace(text, '            Exit::Goto(t) if f.insns.contains_key(&t) => format!("goto {};", label(t)),',
        """            Exit::Goto(t) if f.insns.contains_key(&t) => {
                if t <= site {
                    format!("recomp_poll_guest(ctx); goto {};", label(t))
                } else {
                    format!("goto {};", label(t))
                }
            },""")
    text = replace(text, '        writeln!(out, "    RECOMP_ENTER(ctx, 0x{:08x}u);", f.entry).unwrap();',
        '        writeln!(out, "    RECOMP_ENTER(ctx, 0x{:08x}u); recomp_poll_guest(ctx);", f.entry).unwrap();')
    text = replace(text, "            // guest threads only switch at SVCs, so exclusive pairs can't be interrupted\n", "")
    text = replace(text, """                match size {
                    MemSize::Dual => writeln!(o, "ctx->r[{rt}] = rd32({a}); ctx->r[{}] = rd32({a} + 4);", rt + 1).unwrap(),""",
        """                writeln!(o, "uint32_t exclusive_address = {a};").unwrap();
                let kind = *size as u32 + 1;
                writeln!(o, "ctx->exclusive_address = exclusive_address; ctx->exclusive_kind = {kind};").unwrap();
                let a = "exclusive_address";
                match size {
                    MemSize::Dual => writeln!(o, "ctx->r[{rt}] = rd32({a}); ctx->r[{}] = rd32({a} + 4);", rt + 1).unwrap(),""")
    text = replace(text, """                let a = rd(*rn);
                match size {
                    MemSize::Dual => writeln!(o, "wr32({a}, {}); wr32({a} + 4, {});", rd(*rt), rd(rt + 1)).unwrap(),""",
        """                let a = rd(*rn);
                let kind = *size as u32 + 1;
                writeln!(o, "if (ctx->exclusive_kind == {kind} && ctx->exclusive_address == {a}) {{").unwrap();
                match size {
                    MemSize::Dual => writeln!(o, "wr32({a}, {}); wr32({a} + 4, {});", rd(*rt), rd(rt + 1)).unwrap(),""")
    text = replace(text, """                writeln!(o, "ctx->r[{d}] = 0;").unwrap();
            }
            Op::Clrex | Op::Pld | Op::Hint(_) => {}""",
        """                writeln!(o, "ctx->r[{d}] = 0; }} else {{ ctx->r[{d}] = 1; }}").unwrap();
                writeln!(o, "ctx->exclusive_kind = 0;").unwrap();
            }
            Op::Clrex => writeln!(o, "ctx->exclusive_kind = 0;").unwrap(),
            Op::Pld | Op::Hint(_) => {}""")
    text = replace(text, 'format!("{}(ctx); return;", fn_name(t))',
                   'format!("ctx->pending_tail = {}; return;", fn_name(t))')
    text = replace(text, 'format!("recomp_call_indirect(ctx, 0x{t:08x}u); return;")',
                   'format!("recomp_tail_indirect(ctx, 0x{t:08x}u); return;")')
    text = replace(text, 'format!("recomp_call_indirect(ctx, {v}); return;")',
                   'format!("recomp_tail_indirect(ctx, {v}); return;")')
    text = replace(text, "            if let Some(words) = self.variants.get(&insn.addr) {",
        """            if matches!(insn.addr, 0x0013_b48c | 0x0013_b02c | 0x0012_b0f0 | 0x0012_62d0 | 0x0012_63a8) {
                let return_pc = insn.addr + 4;
                body = format!(concat!(
                    "RecompUnwindFrame unwind_frame;\\n",
                    "unwind_frame.previous = ctx->unwind_frame;\\n",
                    "unwind_frame.sp = ctx->r[13];\\n",
                    "unwind_frame.return_pc = 0x{:08x}u;\\n",
                    "ctx->unwind_frame = &unwind_frame;\\n",
                    "if (setjmp(unwind_frame.resume)) {{\\n",
                    "    ctx->unwind_frame = unwind_frame.previous;\\n",
                    "    return;\\n",
                    "}}\\n",
                    "{}",
                    "ctx->unwind_frame = unwind_frame.previous;\\n"
                ), return_pc, body);
            }
            if let Some(words) = self.variants.get(&insn.addr) {""")
    text = replace(text, '            if regs & (1 << PC) != 0 {\n                let e = if rn == SP',
        '''            if regs & (1 << PC) != 0 {
                // The SDK unwinder installs a saved register context with POP {LR, PC}.
                if insn.addr == 0x0016_bec4 {
                    writeln!(o, "recomp_restore_guest_context(ctx, rd32(a + 4)); return;").unwrap();
                    return;
                }
                let e = if rn == SP''')
    text = replace(text, '            } else if rt == PC {\n                let e = match resolved {',
        '''            } else if rt == PC {
                // The Thumb SDK's ARM context veneer restores PC separately after restoring LR.
                if insn.addr == 0x002f_db6c {
                    writeln!(o, "recomp_restore_guest_context(ctx, rd32({ea})); return;").unwrap();
                    return;
                }
                let e = match resolved {''')
    # BL, statically resolved BLX, and calls with a manually assigned return address
    if text.count('writeln!(o, "{}(ctx);",') != 3:
        raise ValueError("Unexpected direct-call emission sites")
    text = text.replace('writeln!(o, "{}(ctx);",', 'writeln!(o, "{}(ctx); recomp_follow_tail(ctx);",')
    path.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
