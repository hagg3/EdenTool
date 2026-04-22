STANDARD USAGE: This will convert FILE.eden to Minecraft and output region files to /ConvertedWorld/

./eden_tool_v2 <path_to_eden_file>

TERRAIN GENERATOR
Usage: ./eden_tool_v2 generate <width> <depth> <seed> <output_file.eden> [baseHeight] [waterAmnt(1-5)]

Example:

./eden_tool_v2 generate 512 512 12345 terrain.eden
./eden_tool_v2 generate 256 256 12345 test_water3.eden 30 3

<width> <depth> = horizontal and vertical measurements of world size in blocks
<seed> = seed for the generator
<output_file.eden> = output file name. Generates uncompressed .eden file
[baseHeight] = lower values give you less underground and more above ground space
[waterAmnt(1-5)] = 1 for 10% land and more water, 5 for 100% land and no water.

REVERSE (EXPERIMENTAL):
Will attempt to convert basic features (terrain) from Minecraft region files to a .eden file.
./eden_tool_v2 mc2eden

./eden_tool_v2 mc2eden <path_to_input_region_directory> Output.eden
