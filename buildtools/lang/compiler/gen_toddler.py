import random
# coding: utf-8
def is_prime(num):
    for i in range(2, num):
        if num % i == 0:
            return False
    return True 
    
cnts={}
tvars=[]
start = 2
prime_value=61463
random.seed(1000)

d = "OOO{in_this_life___youre_on_your_own}"
genn_code = []
declar_code = [f"    int UNKWN = {prime_value}"]
for indx, c in enumerate(d):
    origc = c
    c = c.replace("{","Y").replace("}","Z")

    if c in cnts:
        cnts[c] += 1
    else:
        cnts[c] = 1
   
    varname=f"{c}{cnts[c]}"
    start += 1
    while not is_prime(start):
        start += 1
    cur_prime = start
    varvalue = ord(origc)
    pos = indx + 1
    if (indx % 2) == 0:
        corrector = random.randint(500, 31000)
        messer = (varvalue - pos + corrector) ^ prime_value

        tester = (prime_value ^ messer) - corrector + pos
        #print(f"\ttester={tester}   corrector={corrector}   messer={messer}")
        assert tester == ord(origc)
        declar_code.append(f"    int {varname} = {corrector - pos}")
        genn_code.append(f"    tmp = UNKWN ^ {messer}\n    {varname} = tmp - {varname} ")

    elif (indx % 2) == 1:
        corrector = random.randint(500, 31000)
        messer = (varvalue - pos + corrector) ^ prime_value

        tester = (prime_value ^ messer) - corrector + pos
        #print(f"\ttester={tester}   corrector={corrector}   messer={messer}")
        assert tester == ord(origc)
        declar_code.append(f"    int {varname} = {corrector - pos}")
        genn_code.append(f"    tmp = UNKWN ^ {messer}\n    {varname} = tmp - {varname}")




outstr= """#include stdint

sub main
"""

declar_code.append(f"    int tmp = 0")
for dc in declar_code:

    outstr += dc + "\n"
#random.shuffle(genn_code)
for g in genn_code:
    outstr += g + "\n"

open("toddler.qpy","w").write(outstr)

 
