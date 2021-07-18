## Dye leather armor

This repository contains programs for finding crafting recipes for specific colors of leather armor in Minecraft.

A tool to get the crafting recipe for any craftable color of leather armor can be found at https://anrar4.github.io/DyeLeatherArmor/

To calculate the combinations for yourself or to calculate combinations given a specific otherwise unobtainable base color `calculateCombinations.c` can be used.
The programm used POSIX-Threads, so it can only be run on linux.
The programm can be compiled using `gcc -o calculateCombinations calculateCombinations.c -lpthread`.

`colorOptions_depth_8.log` contains a list of all possible and unique dye combinations using up to 8 dyes. Each dye is represented using 5 bits in the uint64_t represented by each row. (0 = no color(end of dye list), 1 = black, 15 = white etc(< 1.13 color subids)). The file is used as input for `calculateCombinations.c`.

The files produced by the programm(e. g. `colorMap_Layer_0_Depth_8_1-12.log`) each represent all new possible colors for a specific crafting step.
Each line represents a newly found color. A line(e. g. `1908001#-1#1`) consists of `<the newly found color>#<the base color that was used for the crafting step(-1 representing an undyed piece of armor)>#<the dye list as an integer>`.
