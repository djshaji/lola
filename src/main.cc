/*  Scaffold to test LV2 Library
 *
 *  This file is part of the LV2 Loader project.
 *
 *  Copyright (C) 2024  DJ Shaji
 *  MIT License

*/

#include <iostream>
#include <dlfcn.h>
#include "lola.h"

int main(int argc, char **argv) {
    Plugin plugin("dyson_compress-swh.lv2/plugin.json", 0, 44100);
    return 0;
}