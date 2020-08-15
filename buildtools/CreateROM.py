from glife import *
import golly as g
import os
import time
pattern_fp = None
if ("outfilename" in os.environ):
    pattern_fp = os.environ["outfilename"]
    g.open(os.environ["outfilename"])

if pattern_fp:
    fpath = pattern_fp
else:
    fpath = g.getpath()

dirpath = os.path.dirname(fpath)
basename = os.path.basename(fpath)
timestr = time.strftime("%Y%m%d-%H%M%S")
backuppath = os.path.join(dirpath, "backups", basename[:-3] + "_" + timestr + ".mc")

g.show(backuppath)
g.save(backuppath,"mc")

bitOff = pattern('4.DBD$5.B$4.2B$5.D$D.B2.F2.3D$3BD2FD4B$D4.D2.3D$5.B$4.DBD$4.DBD$4.DBD!')
bitOn = pattern('4.DBD$5.B.B$4.2B.2B$5.D.2D$D.B2.F.D2.D$3BD2FDF.2B$D4.D2.B.D$5.B2.B$4.DBD$4.DBD$4.DBD!')

bitOffRedLin = pattern('4.DBD$5.B$4.2B$5.D$D.B2.F2.3D$3BD2FD4B$D4.D2.3D$5.B$4.DBD$4.DBD$3F.DBD.3F!')
bitOnRedLine = pattern('4.DBD$5.B.B$4.2B.2B$5.D.2D$D.B2.F.D2.D$3BD2FDF.2B$D4.D2.B.D$5.B2.B$4.DBD$4.DBD$3F.DBD.3F!')

bitOffHorzLine = pattern('4.DBD$.F3.B$.F2.2B$5.D$D.B2.F2.3D$3BD2FD4B$D4.D2.3D$5.B$.F2.DBD$.F2.DBD$4.DBD!')
bitOnHorzLine = pattern('4.DBD$.F3.B.B$.F2.2B.2B$5.D.2D$D.B2.F.D2.D$3BD2FDF.2B$D4.D2.B.D$5.B2.B$.F2.DBD$.F2.DBD$4.DBD!')

"""
x = 11, y = 11, rule = Varlife
4.DBD$.F3.B.B$.F2.2B.2B$5.D.2D$D.B2.F.D2.D$3BD2FDF.2B$D4.D2.B.D$5.B2.B$.F2.DBD$.F2.DBD$4.DBD!
"""

# startX = -1610
# startY = 706
startX = 3199
startY = 738
# g.select([startX-3200, startY, 3201, 700])
# g.clear(0)

for layer in range(0, g.numlayers()):
    if g.getname(layer) == "ROM-3ins":
        g.setlayer(layer)

        g.dellayer()
        break

#g.addlayer()
#g.new('ROM-3ins')
#g.setrule('Varlife')


#code = g.getclipstr()
#open("c:\\tmp\\glife.log","a").write(code + "\n\n")

code = open("lang/out.asm","r").read()
#code = open("lang/bj-randomer.asm").read()
# code = """0. MLZ -1 A205 205;
# 1. XOR A192 56835 192;
# 2. ADD A3 15 4;"""
#
###### RIGHT HERE ERIK
# code = """0. MLZ -1 255 120
# 1. XOR A70 0 A70
# 2. XOR A71 0 A71 """

if code[-1:] == "\n":
    code = code[:-1]


opcodes = {'MNZ': '0000',
           'MLZ': '0001',
           'ADD': '0010',
           'SUB': '0011',
           'AND': '0100',
           'OR' : '0101',
           'XOR': '0110',
           'ANT': '0111',
           'SL' : '1000',
           'SRL': '1001',
           'SRA': '1010',
           'SEN': '1011',
           'REC': '1100',
           'RND': '1101'}

modes = {'A': '01',
         'B': '10',
         'C': '11'}

# x = 0
# y = 0
#length = len(code.split("\n"))
length = 0
for line in code.split("\n"):
    line = line.strip()
    if line.startswith(";"):
        continue
    if line.strip() == "":
        continue
    if line.strip() == "\n":
        continue
    length += 1

x = 1
open("c:\\tmp\\glife.log","a").write(str(length) + " " + str(x) )
y = 737 #16*11

startCodeX = startX - ((length + 1)*11) + 2
startCodeY = startY - 1

g.select([-1517, startY, 4717, y * 11])
g.cut()

#Iterate through the instructions, backwards
linecnt = 0
for line in code.split('\n')[::-1]:
  line = line.strip()
  if line.startswith(str(linecnt)) or line.find(".") > -1:
      instruction = line.split(';')[0].split('.')[-1].split()
  else:
      if line.startswith(";"):
          continue
      instruction = line.strip().split(';')[0].split()

  linecnt += 1
  g.show(line)
  bincode = []
  y = 0
  #Remove starting numbering and comments


  #Parse each argument
  for argument in instruction[:0:-1]:
    if argument[0] in modes:
      bincode.append('{}{:016b}'.format(modes[argument[0]], 65535 & int(argument[1:], 0)))
    else:
      bincode.append('00{:016b}'.format(65535 & int(argument, 0)))
  bincode.append(opcodes[instruction[0]]) #Add opcode at end

  #Insert beginning clock generation line
  #blank.put(startCodeX+11*x, startCodeY+ y*11)
  bitOn.put(startCodeX+11*x, startCodeY+ y*11)

  y += 1
  #Paste ROM bits
  # 00 0000000000000100 00 0000000000000001 00 1111111111111111 0001

  for bitcnt, bit in enumerate(''.join(bincode)):
    #blank.put(startCodeX + 11 * x, startCodeY + y * 11)
    if bit == '0':
        if (bitcnt % 18 == 0 and bitcnt > 0):
            bitOffRedLin.put(startCodeX+11*x, startCodeY+11*y)
        else:
            if linecnt % 8 == 0 and linecnt > 0:
                bitOffHorzLine.put(startCodeX + 11 * x, startCodeY + 11 * y)
            else:
                bitOff.put(startCodeX + 11 * x, startCodeY + 11 * y)
    else:
        if (bitcnt % 18 == 0 and bitcnt > 0):
            bitOnRedLine.put(startCodeX+11*x, startCodeY+11*y)
        else:
            if linecnt % 8 == 0 and linecnt > 0:
                bitOnHorzLine.put(startCodeX + 11 * x, startCodeY + 11 * y)
            else:
                bitOn.put(startCodeX+11*x, startCodeY+11*y)


    y += 1
  x += 1

g.select([startX-(((len(code.split("\n")))*11) + 1) , startY, (((len(code.split("\n")))*11) + 2), y*11])
#g.copy()

g.save(fpath,"mc")
g.show("Saved new version to  " + fpath)
# 0. MLZ -1 3 3;
# 1. MLZ -1 7 6; preloadCallStack
#
