#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage: %s input.bin output.dat start_addr_hex\n", argv[0]);
        printf("Example: %s app.bin app.dat 0x3F0000\n", argv[0]);
        return -1;
    }

    FILE *fin = fopen(argv[1], "rb");
    if (!fin)
    {
        perror("Open input bin failed");
        return -1;
    }

    FILE *fout = fopen(argv[2], "w");
    if (!fout)
    {
        perror("Open output dat failed");
        fclose(fin);
        return -1;
    }

    uint32_t start_addr = strtoul(argv[3], NULL, 16);

    /* CCS DAT address header */
    fprintf(fout, "@%06X\n", start_addr);

    uint8_t byte_lo, byte_hi;
    uint16_t word;
    int cnt = 0;

    while (1)
    {
        if (fread(&byte_lo, 1, 1, fin) != 1)
            break;
        if (fread(&byte_hi, 1, 1, fin) != 1)
            byte_hi = 0x00;   // bin 长度为奇数

        /* 小端：低字节在前 */
        word = ((uint16_t)byte_hi << 8) | byte_lo;

        fprintf(fout, "%04X\n", word);

        cnt++;
    }

    printf("Convert done, %d words written\n", cnt);

    fclose(fin);
    fclose(fout);
    return 0;
}
