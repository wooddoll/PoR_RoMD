// Finds indirect calls through vtable offsets 0x1c/0x20/0x24 (cFont GetCharHeight/Width/Rect)
// and classifies each argument (sext8/zext8 = single byte read -> 1-byte char assumption).
//@category Analysis
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import java.util.*;
import java.io.*;

public class FindFontMetricCalls extends GhidraScript {

    private String cls(Varnode v, int d, Set<PcodeOp> seen) {
        if (v == null || d > 10) return "?";
        if (v.isConstant()) return "c" + Long.toHexString(v.getOffset());
        PcodeOp op = v.getDef();
        if (op == null) return "in" + v.getSize();
        if (!seen.add(op)) return "loop";
        int oc = op.getOpcode();
        switch (oc) {
            case PcodeOp.INT_SEXT:
            case PcodeOp.INT_ZEXT: {
                Varnode in = op.getInput(0);
                String k = (oc == PcodeOp.INT_SEXT) ? "sext" : "zext";
                if (in.getSize() == 1) {
                    PcodeOp d2 = in.getDef();
                    String src = d2 == null ? "in" : d2.getMnemonic();
                    return k + "8(" + src + ")";
                }
                return k + in.getSize() * 8 + "(" + cls(in, d + 1, seen) + ")";
            }
            case PcodeOp.COPY:
            case PcodeOp.CAST:
                return cls(op.getInput(0), d + 1, seen);
            case PcodeOp.MULTIEQUAL: {
                TreeSet<String> s = new TreeSet<>();
                for (int i = 0; i < op.getNumInputs(); i++) s.add(cls(op.getInput(i), d + 1, seen));
                return "phi{" + String.join("|", s) + "}";
            }
            case PcodeOp.LOAD:
                return "load" + v.getSize();
            default:
                return op.getMnemonic() + v.getSize();
        }
    }

    private long vtOffset(Varnode target) {
        PcodeOp ld = target.getDef();
        while (ld != null && (ld.getOpcode() == PcodeOp.COPY || ld.getOpcode() == PcodeOp.CAST))
            ld = ld.getInput(0).getDef();
        if (ld == null || ld.getOpcode() != PcodeOp.LOAD) return -1;
        Varnode addr = ld.getInput(1);
        PcodeOp a = addr.getDef();
        while (a != null && (a.getOpcode() == PcodeOp.COPY || a.getOpcode() == PcodeOp.CAST))
            a = a.getInput(0).getDef();
        if (a == null) return -1;
        int oc = a.getOpcode();
        if (oc == PcodeOp.INT_ADD || oc == PcodeOp.PTRSUB) {
            if (a.getInput(1).isConstant()) return a.getInput(1).getOffset();
            if (a.getInput(0).isConstant()) return a.getInput(0).getOffset();
        } else if (oc == PcodeOp.PTRADD) {
            if (a.getInput(1).isConstant() && a.getInput(2).isConstant())
                return a.getInput(1).getOffset() * a.getInput(2).getOffset();
        }
        return -1;
    }

    private boolean prefilter(Function f) {
        Listing l = currentProgram.getListing();
        boolean disp = false, icall = false;
        for (Instruction ins : l.getInstructions(f.getBody(), true)) {
            String s = ins.toString().toLowerCase();
            if (s.contains("+ 0x1c]") || s.contains("+ 0x20]") || s.contains("+ 0x24]")) disp = true;
            if (ins.getMnemonicString().equalsIgnoreCase("CALL") && ins.getOperandReferences(0).length == 0) icall = true;
            if (disp && icall) return true;
        }
        return false;
    }

    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        String outPath = a.length > 0 ? a[0] : (System.getProperty("user.home") + "/ghidra_scripts/fontcalls_" + currentProgram.getName() + ".txt");
        long lo = a.length > 2 ? Long.parseLong(a[1], 16) : 0, hi = a.length > 2 ? Long.parseLong(a[2], 16) : Long.MAX_VALUE;
        DecompInterface di = new DecompInterface();
        di.toggleCCode(false);
        di.openProgram(currentProgram);
        PrintWriter pw = new PrintWriter(new OutputStreamWriter(new FileOutputStream(outPath), "UTF-8"));
        int nf = 0, nc = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (monitor.isCancelled()) break;
            long e = f.getEntryPoint().getOffset();
            if (e < lo || e > hi || f.isThunk() || f.isExternal()) continue;
            if (!prefilter(f)) continue;
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            HighFunction h = r.getHighFunction();
            if (h == null) { pw.println("FAIL\t" + f.getEntryPoint() + "\t" + f.getName()); continue; }
            nf++;
            Iterator<PcodeOpAST> it = h.getPcodeOps();
            while (it.hasNext()) {
                PcodeOpAST op = it.next();
                if (op.getOpcode() != PcodeOp.CALLIND) continue;
                long off = vtOffset(op.getInput(0));
                if (off != 0x1c && off != 0x20 && off != 0x24) continue;
                StringBuilder sb = new StringBuilder();
                for (int i = 1; i < op.getNumInputs(); i++) {
                    if (i > 1) sb.append(", ");
                    sb.append(cls(op.getInput(i), 0, new HashSet<>()));
                }
                String name = off == 0x1c ? "H" : off == 0x20 ? "W" : "R";
                pw.println(f.getEntryPoint() + "\t" + f.getName() + "\t" + op.getSeqnum().getTarget() + "\t" + name + "\t" + sb);
                nc++;
            }
        }
        pw.close();
        di.dispose();
        println("functions decompiled=" + nf + " calls=" + nc + " -> " + outPath);
    }
}
