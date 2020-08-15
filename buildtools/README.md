# Build Tools

This folder includes some of the tools I used to build the challenge. 

The `wire-delays.mc` pattern includes some of the common signal delays that were used to build the original CPU and the additional hardware portions.

The `createROM.py` runs inside golly and uses either the `outfilename` environment variable or `out.asm` file as input to build the ROM.

In the lang folder, is modified code that was borrowed from the Quest for Tetris team (https://github.com/QuestForTetris/QFT).
The modifications to the code include:
 - variable shifting that shifts the location of the temp variables (explored as a performance improvement).
 - support for random, send, and read commands that incorporate the related instructions for the challenge.
 - added a compiler operator for the qpy's that targets a specific pattern file using `#outfilename` at the top of the script.
 - breaking the > < operators (most likely through my modifications), as a result, the qpy's should only use  >= and <= instead. 

NOTE: code using random, send, and read will not work in the JavaScript QFTASM interpreter. 

Good luck
trickE
 
 
 



