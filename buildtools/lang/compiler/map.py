

# exit at 27, 1
# secret flag exit at 29,1
map = """
1111111111111111111111111111111
1000000000001000000100000000101
1001010101001001000000000000001
1001111111111001111111111111111
1001000001000000000010001000001
1001000001011111001000100010001
1001001001110001111111111110001
1000001000000000000010000000011
1111111111111000000010000010001
1000000000000111111011111110001
1000000011100000000000001001001
1001111100010000000001000001001
1000001010111111111111111111001
1001000100100000000010000001001
1001000000000001000000001000001
1111111111111111111111111111111"""
# Y 0 to 16 (0 starts at top)
# X 0 through 30
# change map[10] = 45057 or xormap[10] = 60097
# change map[28] = 32779 or xormap[28] = 56011
# key1 = 10, 14
# key2 = 21, 8
# key3 = 9, 5
xor_val1 = 23232
xor_val2 = 39499

proof_map = []
for pm in map.split("\n"):
    proof_map.append(list(pm))
#proof_map = proof_map[::-1]  # reverse it

key1 = (10 << 8) | 11
print(key1, "xord=", key1 ^ xor_val1, (key1 & 0xff00) >> 8, key1 & 0xff)
key2 = (10 << 8) | 9
print(key2, "xord=", key2 ^ xor_val2, (key2 & 0xff00) >> 8, key2 & 0xff)
key3 = (10 << 8) | 2
print(key3, "xord=", key3 ^ xor_val1, (key3 & 0xff00) >> 8, key3 & 0xff)

# key1 = (1 << 8) | 13
# print(key1, "xord=", key1 ^ xor_val1, (key1 & 0xff00) >> 8, key1 & 0xff)
# key2 = (1 << 8) | 12
# print(key2, "xord=", key2 ^ xor_val2, (key2 & 0xff00) >> 8, key2 & 0xff)
# key3 = (3 << 8) | 12
# print(key3, "xord=", key3 ^ xor_val1, (key3 & 0xff00) >> 8, key3 & 0xff)

#proof_map[16-10][(key1 & 0xff00) >> 8] = "*"

#proof_map[Y][X]
assert(proof_map[(key1 & 0xff)+1][(key1 & 0xff00) >> 8] != 1)
proof_map[(key1 & 0xff)+1][(key1 & 0xff00) >> 8] = "*"
assert(proof_map[(key2 & 0xff)+1][(key2 & 0xff00) >> 8] != 1)
proof_map[(key2 & 0xff)+1][(key2 & 0xff00) >> 8] = "*"
assert(proof_map[(key3 & 0xff)+1][(key3 & 0xff00) >> 8] != 1)
proof_map[(key3 & 0xff)+1][(key3 & 0xff00) >> 8] = "*"
proof_map[12+1][7] = "S"
assert(proof_map[12+1][7] != 1)
proof_map[1+1][29] = "E"
assert(proof_map[1+1][29] != 1)
proof_map[14+1][1] = "A"
assert(proof_map[13+1][1] != 1)
#proof_map = proof_map[::-1]  # reverse it back

for row in proof_map:
    print("".join(row))

map_list = map.split("\n")
map_list = map_list[1:]
#print(map_list)
print("")

addresses = []
xord_addresses = []
total = 0
xord_total = 0
keys = [key1, key2, key3]
for col in range (0, len(keys) + len(map_list[0])):
    if col < 3:
        num = int(keys[col])
    else:
        bit_str_value = ""
        for row in map_list:
            bit_str_value += row[col-3]
        num = int(bit_str_value[::-1], 2)
    addresses.append(str(num))
    #total += num % 32767
    #total = total % 32767
    total += num & 255
    num = num ^ 23232
    # if col & 1 == 0:
    #     num = num ^ 23232
    # else:
    #     num = num ^ 39499
    xord_addresses.append(str(num))
    #xord_total += num % 32767
    xord_total += num & 255
    #assert xord_total < 0xFFFF
    #xord_total = xord_total % 32767
    #assert xord_total <= 0x7FFF

print(f"ORIGINAL [{len(addresses)}] = [{','.join(addresses)}]")
print(f"TOTAL={total}")
print(f"int map[{len(addresses)}] = [{','.join(xord_addresses)}]")
print(f"XOR TOTAL = {xord_total}")

#poi = [int(addresses[10]), int(addresses[20]), int(addresses[28]), key1, key2, key3]

# total = 0
# xord_total = 0
# addresses = []
# xord_addresses = []
# for i in range(0, len(poi)):
#     num = poi[i]
#     addresses.append(str(num))
#     total += num & 255
#     num = num ^ 23232
#     # if i & 1 == 0:
#     #     num = num ^ 23232
#     # else:
#     #     num = num ^ 39499
#     xord_total += num & 255
#     xord_addresses.append(str(num))
#
# print(f"ORIGINAL [{len(addresses)}] = [{','.join(addresses)}]")
# print(f"TOTAL={total}")
# print(f"int map[{len(xord_addresses)}] = [{','.join(xord_addresses)}]")
# print(f"XOR TOTAL={xord_total}")
