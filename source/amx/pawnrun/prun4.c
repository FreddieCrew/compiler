/*  Command-line shell for the "Small" Abstract Machine.
 *
 *  Copyright (c) ITB CompuPhase, 2001-2005
 *
 *  This file may be freely used. No warranties of any kind.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amx.h"

#define MAX_OUTPUT_LENGTH 128

size_t srun_program_size(const char *filename)
{
    FILE *fp;
    AMX_HEADER hdr;

    if ((fp = fopen(filename, "rb")) == NULL)
        return 0;

    fread(&hdr, sizeof hdr, 1, fp);
    fclose(fp);

    amx_Align16(&hdr.magic);
    amx_Align32((unsigned long *)&hdr.stp);
    return (hdr.magic == AMX_MAGIC) ? (size_t)hdr.stp : 0;
}

int srun_load_program(AMX *amx, const char *filename, void *memblock)
{
    FILE *fp;
    AMX_HEADER hdr;

    if ((fp = fopen(filename, "rb")) == NULL)
        return AMX_ERR_NOTFOUND;

    fread(&hdr, sizeof hdr, 1, fp);
    amx_Align32((unsigned long *)&hdr.size);
    rewind(fp);
    fread(memblock, 1, (size_t)hdr.size, fp);
    fclose(fp);

    memset(amx, 0, sizeof *amx);
    return amx_Init(amx, memblock);
}

void error_exit(AMX *amx, int errorcode)
{
    printf("Run time error %d: \"%s\" on line %ld\n",
           errorcode, amx_StrError(errorcode),
           (amx != NULL) ? amx->curline : 0);
    exit(1);
}

void print_usage(const char *program)
{
    printf("Usage: %s <filename>\n", program);
    exit(1);
}

int main(int argc, char *argv[])
{
    size_t memsize;
    void *program;
    AMX amx;
    int index, err;
    cell amx_addr, *phys_addr;
    char output[MAX_OUTPUT_LENGTH];

    if (argc != 4)
        print_usage(argv[0]);

    const char *filename = argv[1];

    if ((memsize = srun_program_size(filename)) == 0)
        print_usage(argv[0]);

    program = malloc(memsize);
    if (program == NULL)
        error_exit(NULL, AMX_ERR_MEMORY);

    err = srun_load_program(&amx, filename, program);
    if (err)
        error_exit(&amx, err);

    amx_ConsoleInit(amx);
    err = amx_CoreInit(amx);
    if (err)
        error_exit(&amx, err);

    const char *function_name = argv[2];
    const char *argument = argv[3];

    err = amx_FindPublic(&amx, function_name, &index);
    if (err)
        error_exit(&amx, err);

    err = amx_Allot(&amx, strlen(argument) + 1, &amx_addr, &phys_addr);
    if (err)
        error_exit(&amx, err);

    amx_SetString(phys_addr, argument, 0);

    err = amx_Exec(&amx, NULL, index, 1, amx_addr);
    if (err)
        error_exit(&amx, err);

    amx_GetString(output, phys_addr);
    amx_Release(&amx, amx_addr);
    printf("%s returns %s\n", filename, output);

    amx_ConsoleCleanup(&amx);
    amx_CoreCleanup(&amx);
    amx_Cleanup(&amx);
    free(program);
    return 0;
}

