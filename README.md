## Dye leather armor

This repository contains a program that finds all craftable colours of leather armor given leather armor with a specific base colour or undyed leather armor (in Minecraft Java Edition). The craftable colours are found by recursively testing all possible crafting recipes for newly found colours (brute-force).

A tool that can generate a crafting recipe for any craftable colour of leather armor can be found at https://anrar4.github.io/DyeLeatherArmor/

To calculate the combinations for yourself or to calculate combinations given a specific otherwise unobtainable base colour `calculateCombinations.c` can be used.
The programm uses POSIX-Threads, so it can't be run on Windows.
The programm can be compiled using `gcc -o calculateCombinations calculateCombinations.c -lpthread`, the programm can be run using `./calculateCombinations <Number of threads to use>`.

`colourOptions_depth_8.log` contains a list of all possible and unique dye combinations using up to 8 dyes. Each dye is represented using 5 bits in the uint64_t represented by each row. (0 = no colour(end of dye list), 1 = black, 16 = white etc((<1.13 colour subids) + 1)). The file is used as input for `calculateCombinations.c`.

The files produced by the programm(e. g. `colourMap_Layer_0_Depth_8_1-12.log`) each represent all new possible colours for a specific crafting step.
Each line represents a newly found colour. A line(e. g. `1908001#-1#1`) consists of `<the newly found colour>#<the base colour that was used for the crafting step(-1 representing an undyed piece of armor)>#<the dye list as an integer>`.
