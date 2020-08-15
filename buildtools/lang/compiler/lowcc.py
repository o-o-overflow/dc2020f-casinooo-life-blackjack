import unittest
import sys
import re
sys.path.insert(1,'..')

from compiler import Compiler
from interpreter.interpreter import Interpreter

class MyCompiler():
    def __init__(self, filename):
        self.file_interpreter = Compiler(filename).low_level_file_interpreter
        compiled = "\n".join(self.file_interpreter.compiled)
        if compiled[-1] == "\n":
            compiled = compiled[0:-1]
        compiled.replace("32221","")
        compiled = re.sub(r'32221 [0-9]{1,3}', '32221 50', compiled)
        # this will also trigger read output for X,Y in addr[71]
        compiled = re.sub(r'-1 9999 [0-9]{1,3};', r'-1 1 71; Writing start',A71
                          compiled)
        compiled = re.sub(r'9999 ([0-9]{1,3}) ([0-9]{1,3});', r'A71 \1 \2; Reading first 4 bits of A71 into action', compiled)
        # OR 9998 A43 44; -->
        compiled = re.sub(r'OR 9998 (A[0-9]{1,3}) [0-9]{1,3};', r'MLZ -1 \1 71; Writing X,Y into bottom 12 bits of A71 and trashing old A71', compiled)
        #MLZ -1 9997 44;
        compiled = re.sub(r'9997 ([0-9]{1,3});', r'A71 \1; Reading A71 for output to teams ', compiled)
        #AND 9996 4095 44;;   --> #AND 9996 4095 71;
        #compiled = re.sub(r'9996 65520 [0-9]{1,3};', r'A71 65520 71; Clearing action but leaving XY in controller A71', compiled)
        cnt = 3
        final_out = ""
        for linecnt, ln in enumerate(compiled.split("\n")):
            if linecnt >= 3 and linecnt <= 36:
                # holds space for bootloaded array
                final_out += str(linecnt) + ". MLZ 0 " + str(cnt) + " " + str(cnt) + "; NOP \n"
                cnt+= 1
            else:
                #cnt += 1
                #ln = re.sub(r'^[0-9]{1,3}.', str(cnt) + "." , ln)
                final_out += ln + "\n"

        compiled = final_out

        open("out.asm","w").write(compiled)
        print(compiled)
        # interpreter = Interpreter("")
        # interpreter.__init__(compiled)
        # interpreter.run()
        # self.ram = interpreter.ram

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

    # def test_assign(self):
    #     self.run_prg("tests/test_assign.txt")
    #
    #     self.assertEqual(self.get_ram("a"), [7])
    #
    # def test_assign_multi(self):
    #     self.run_prg("tests/test_assign_multi.txt")
    #     self.assertEqual(self.get_ram("abc"), [7,3,1337])
    #
    # def test_if(self):
    #     self.run_prg("tests/test_if.txt")
    #     self.assertEqual(self.get_ram("a"), [5])
    #
    # def test_if_multi(self):
    #     self.run_prg("tests/test_if_multi.txt")
    #     self.assertEqual(self.get_ram("ab"), [5,3])
    #
    # def test_stdint(self):
    #     self.run_prg("tests/test_stdint.txt")
    #     self.assertEqual(self.get_ram("abcdefghij"), [31,65162,32204,1,0,0,1,65504,1,0])
    #
    # def test_stdint_complex(self):
    #     self.run_prg("tests/test_stdint_complex.txt")
    #     self.assertEqual(self.get_ram("ab"), [5472,151])
    #
    # def test_factorial(self):
    #     self.run_prg("tests/test_factorial.txt")
    #     self.assertEqual(self.get_ram("a"), [120])
    #
    # def test_prime(self):
    #     self.run_prg("tests/test_prime.txt")
    #     self.assertEqual(self.get_ram(["cur_prime"]), [73])
    #
    # def test_recursion(self):
    #     self.run_prg("tests/test_recursion.txt")
    #     self.assertEqual(self.get_ram("a"), [120])
    #
    # def test_complex(self):
    #     self.run_prg("tests/test_complex.txt")
    #     self.assertEqual(self.get_ram("caefb"), [2,[6,3,1213],1213,5,[6,3,9]])

if __name__ == '__main__':
    c = MyCompiler(sys.argv[1])


