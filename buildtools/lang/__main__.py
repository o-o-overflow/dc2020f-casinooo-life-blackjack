from compiler.compiler import Compiler
import sys
import re
import os
import subprocess

class MyCompiler():
    def __init__(self, filename):
        outCPUfn = ""
        for ln in open(filename).read().split("\n"):
            if ln.startswith("#outfilename"):
                tmp = ln.split(" ")[1:]
                outCPUfn = " ".join(tmp)
        self.file_interpreter = Compiler(filename).low_level_file_interpreter
        compiled = "\n".join(self.file_interpreter.compiled)
        if compiled[-1] == "\n":
            compiled = compiled[0:-1]

        #compiled.replace("32221","")
        #compiled = re.sub(r'32221 [0-9]{1,3}', '32221 50', compiled)
        # this will also trigger read output for X,Y in addr[71]
        compiled = re.sub(r'-1 9999 ([0-9]{1,3});', r'-1 A71 \1; Writing start', compiled)

        #compiled = re.sub(r'9999 ([0-9]{1,3}) ([0-9]{1,3});', r'A71 \1 \2; Reading first 4 bits of A71 into action', compiled)
        # OR 9998 A43 44; -->
        #compiled = re.sub(r'OR 9998 (A[0-9]{1,3}) [0-9]{1,3};', r'MLZ -1 \1 71; Writing X,Y into bottom 12 bits of A71 and trashing old A71', compiled)
        #MLZ -1 9997 44;
        #compiled = re.sub(r'9997 ([0-9]{1,3});', r'A71 \1; Reading A71 for output to teams ', compiled)
        #AND 9996 4095 44;;   --> #AND 9996 4095 71;
        #compiled = re.sub(r'9996 65520 [0-9]{1,3};', r'A71 65520 71; Clearing action but leaving XY in controller A71', compiled)
        cnt = 3
        final_out = ""
        linecnt = 0
        total_lines = len(compiled.split("\n"))

        for ln in compiled.split("\n"):
            linecnt += 1

            if re.match(r'(^[1]?[0-9]|2[0-5])\. MLZ -1 0 [1-4]?[0-9]{1,3};.*', ln):
                tmp_line = re.sub(r'([1-4]?[0-9]{1,2}\. MLZ -1) 0 ([1-4]?[0-9]{1,3});.*', r'\1 A\2 \2', ln)
                #tmp_line = ln
                final_out += f"{tmp_line} \n"
            # 23. SUB 333 333 22;
            # 24. SUB 444 444 24;
            elif re.match(r'.*SUB 333 333.*', ln):
                tmp_line = re.sub(r'([1-4]?[0-9]{1,2}\. )SUB 333 333 ([1-4]?[0-9]{1,3});.*', r'\1XOR A\2 A24 \2;', ln)
                final_out+= f"{tmp_line} \n"
            elif re.match(r'(^[1]?[0-9]|2[0-5])\. SUB 444 444 24', ln):
                tmp_line = re.sub(r'([1-4]?[0-9]{1,2}\. )SUB 444 444 ([1-4]?[0-9]{1,3});.*', rf'\1MNZ A22 {total_lines-4} 0;', ln)
                final_out += f"{tmp_line} \n"
            else:
                final_out += f"{ln} \n"


        compiled = final_out

        open("out.asm","w").write(compiled)
        print(compiled)

        if len(outCPUfn) > 1 and len(sys.argv)>= 3:
            env = os.environ
            gollyfp = sys.argv[2]
            env["outfilename"] = outCPUfn
            subprocess.Popen([gollyfp, "../CreateROM.py"], env=env)

    def get_ram(self, variables):
        rtn = []
        for variable in variables:
            var = self.file_interpreter.global_store["main_"+variable]
            extra = None
            if var.is_array:
                extra = self.ram[slice(var.offset, var.offset+var.size)]
            else:
                extra = self.ram[var.offset]
            rtn.append(extra)
        return rtn
        #return [self.ram[self.file_interpreter.global_store["main_"+variable].offset] for variable in variables]


if __name__ == '__main__':
    c = MyCompiler(sys.argv[1])

