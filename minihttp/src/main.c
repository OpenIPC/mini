#include <stdio.h>
#include <stdlib.h>

#include "hidemo.h"

void Usage(char *sPrgNm) {
    printf("Usage : %s <path to sensor config ini>\n", sPrgNm);
    printf("   ex: %s ./configs/imx222_1080p_line.ini\n", sPrgNm);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { Usage(argv[0]); return EXIT_FAILURE; }
    const char* config_path = argv[1];
    return run_sdk(config_path);
}
