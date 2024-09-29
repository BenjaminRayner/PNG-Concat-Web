#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "crc.h"
#include "zutil.h"

typedef struct {
    unsigned int length;    /* length of data in the chunk, host byte order */
    unsigned char type[4];  /* chunk type */
    unsigned char *p_data;  /* pointer to location where the actual data are */
    unsigned int crc;       /* CRC field  */
} chunk_p;

typedef struct {
    unsigned int length;          /* length of data in the chunk, host byte order */
    unsigned char type[4];        /* chunk type */
    unsigned int crc;             /* CRC field  */

    unsigned int width;           /* width in pixels, big endian   */
    unsigned int height;          /* height in pixels, big endian  */
    unsigned char bit_depth;      /* num of bits per sample or per palette index.
                                   * valid values are: 1, 2, 4, 8, 16 */
    unsigned char color_type;     /* =0: Grayscale; =2: Truecolor; =3 Indexed-color
                                   * =4: Greyscale with alpha; =6: Truecolor with alpha */
    unsigned char compression;    /* only method 0 is defined for now */
    unsigned char filter;         /* only method 0 is defined for now */
    unsigned char interlace;      /* =0: no interlace; =1: Adam7 interlace */
} data_IHDR_p;

typedef struct {
    unsigned long long signature;
    data_IHDR_p* p_IHDR;
    chunk_p* p_IDAT; /* only handles one IDAT chunk */
    chunk_p* p_IEND;
} simple_PNG_p;

void getSignature(const unsigned char* png_buffer, unsigned long long* signature);
void getIHDR(const unsigned char* png_buffer, data_IHDR_p* IHDR);
void getIDAT(unsigned char* png_buffer, chunk_p* IDAT, long png_size);
void getIEND(const unsigned char* png_buffer, chunk_p* IEND, long png_size);
int catpng(char PNGS[50][24]);

int catpng(char PNGS[50][24]) {

    int pngCount = 50;   /* Amount of split pngs in directory */

    simple_PNG_p* PNG_Array[pngCount];

    for (int i=0; i < pngCount; ++i) {

        FILE* png = NULL;
        unsigned char* png_buffer = NULL;
        long png_size = 0;

        /*Open png for reading only*/
        png = fopen(PNGS[i], "r");

        PNG_Array[i] = malloc(sizeof(simple_PNG_p));
        PNG_Array[i]->p_IHDR = malloc(sizeof(data_IHDR_p));
        PNG_Array[i]->p_IDAT = malloc(sizeof(chunk_p));
        PNG_Array[i]->p_IEND = malloc(sizeof(chunk_p));

        /*Set file position indicator to end of file, get size,
        * then set indicator back to start of file*/
        fseek(png, 0, SEEK_END);
        png_size = ftell(png);
        fseek(png, 0, SEEK_SET);

        /*Allocate enough memory to accommodate size of png*/
        png_buffer = malloc(png_size);

        /*Store all png_size bytes of png into buffer*/
        fread(png_buffer, 1, png_size, png);

        /* Get PNG data */
        getSignature(png_buffer, &PNG_Array[i]->signature);
        getIHDR(png_buffer, PNG_Array[i]->p_IHDR);
        getIDAT(png_buffer, PNG_Array[i]->p_IDAT, png_size);
        getIEND(png_buffer, PNG_Array[i]->p_IEND, png_size);

        fclose(png);
    }

    unsigned int totalInfSize = 0;
    unsigned int totalPNGHeight = 0;
    for (int i=0; i < pngCount; ++i) {
        totalInfSize += PNG_Array[i]->p_IHDR->height * (PNG_Array[i]->p_IHDR->width * 4 + 1);
        totalPNGHeight += PNG_Array[i]->p_IHDR->height;
    }

/* --------------------------------------------------------------------------------------------------------- */

    unsigned int offset = 0;
    unsigned long len_def = 0;
    unsigned long len_inf = 0;
    unsigned char gp_buf_inf[totalInfSize];
    unsigned char gp_buf_def[1000000];

    /*Store uncompressed data for all PNGs in gp_buf_inf*/
    for (int i=0; i < pngCount; ++i) {
        mem_inf(&gp_buf_inf[offset], &len_inf, PNG_Array[i]->p_IDAT->p_data, PNG_Array[i]->p_IDAT->length-1);
        offset += PNG_Array[i]->p_IHDR->height * (PNG_Array[i]->p_IHDR->width * 4 + 1);
    }

    /*Compress into gp_buf_def*/
    mem_def(gp_buf_def, &len_def, gp_buf_inf, len_inf, Z_DEFAULT_COMPRESSION);
    unsigned char png[44+len_def+13];
    memcpy(&png[41], gp_buf_def, len_def);

    /*Fill in IDAT data*/
    for (int i=0; i < 41; ++i) {
        png[i] = PNG_Array[0]->p_IDAT->p_data[-41+i];
    }

    /*Inserting width*/
    png[16] = PNG_Array[0]->p_IHDR->width >> 24;
    png[17] = PNG_Array[0]->p_IHDR->width >> 16;
    png[18] = PNG_Array[0]->p_IHDR->width >> 8;
    png[19] = PNG_Array[0]->p_IHDR->width;


    /*Inserting height*/
    png[20] = totalPNGHeight >> 24;
    png[21] = totalPNGHeight >> 16;
    png[22] = totalPNGHeight >> 8;
    png[23] = totalPNGHeight;

    /*Inserting IDAT length*/
    png[33] = len_def >> 24;
    png[34] = len_def >> 16;
    png[35] = len_def >> 8;
    png[36] = len_def;

    /*Inserting IEND*/
    png[44+len_def+5] = 0x49;
    png[44+len_def+6] = 0x45;
    png[44+len_def+7] = 0x4e;
    png[44+len_def+8] = 0x44;
    png[44+len_def+9] = 0xae;
    png[44+len_def+10] = 0x42;
    png[44+len_def+11] = 0x60;
    png[44+len_def+12] = 0x82;
    png[44+len_def+1] = 0;
    png[44+len_def+2] = 0;
    png[44+len_def+3] = 0;
    png[44+len_def+4] = 0;

    /*IHDR crc*/
    unsigned int IHDR_crc = crc(&png[12], 17);
    png[29] = IHDR_crc >> 24;
    png[30] = IHDR_crc >> 16;
    png[31] = IHDR_crc >> 8;
    png[32] = IHDR_crc;

    /*IDAT crc*/
    unsigned int IDAT_crc = crc(&png[37], len_def+4);
    png[len_def+41] = IDAT_crc >> 24;
    png[len_def+42] = IDAT_crc >> 16;
    png[len_def+43] = IDAT_crc >> 8;
    png[len_def+44] = IDAT_crc;

    FILE* catpng;
    catpng = fopen("all.png", "w");
    fwrite(&png, 1, sizeof(png), catpng);
    fclose(catpng);

/* -------------------------------------------------------------------------------------------------------- */
    /* Deallocate memory */
    for (int i=0; i < pngCount; ++i) {
        free(&PNG_Array[i]->p_IDAT->p_data[-41]);
        free(PNG_Array[i]->p_IDAT);
        free(PNG_Array[i]->p_IHDR);
        free(PNG_Array[i]->p_IEND);
    }
    for (int i=0; i < pngCount; ++i) {
        free(PNG_Array[i]);
    }

    return 0;
}

void getSignature(const unsigned char* png_buffer, unsigned long long* signature) {
    unsigned long long signature_temp1 = (png_buffer[0] << 24) + (png_buffer[1] << 16) + (png_buffer[2] << 8) + png_buffer[3];
    unsigned long long signature_temp2 = (png_buffer[4] << 24) + (png_buffer[5] << 16) + (png_buffer[6] << 8) + png_buffer[7];
    *signature = (signature_temp1 << 32) + signature_temp2;
}

void getIHDR(const unsigned char* png_buffer, data_IHDR_p* IHDR) {
    IHDR->length = (png_buffer[8] << 24) + (png_buffer[9] << 16) + (png_buffer[10] << 8) + png_buffer[11];
    IHDR->type[0] = png_buffer[12]; IHDR->type[1] = png_buffer[13]; IHDR->type[2] = png_buffer[14]; IHDR->type[3] = png_buffer[15];
    IHDR->width = (png_buffer[16] << 24) + (png_buffer[17] << 16) + (png_buffer[18] << 8) + png_buffer[19];
    IHDR->height = (png_buffer[20] << 24) + (png_buffer[21] << 16) + (png_buffer[22] << 8) + png_buffer[23];
    IHDR->bit_depth = png_buffer[24];
    IHDR->color_type = png_buffer[25];
    IHDR->compression = png_buffer[26];
    IHDR->filter = png_buffer[27];
    IHDR->interlace = png_buffer[28];
    IHDR->crc = (png_buffer[29] << 24) + (png_buffer[30] << 16) + (png_buffer[31] << 8) + png_buffer[32];
}

void getIDAT(unsigned char* png_buffer, chunk_p* IDAT, long png_size) {
    IDAT->length = (png_buffer[33] << 24) + (png_buffer[34] << 16) + (png_buffer[35] << 8) + png_buffer[36];
    IDAT->type[0] = png_buffer[37]; IDAT->type[1] = png_buffer[38]; IDAT->type[2] = png_buffer[39]; IDAT->type[3] = png_buffer[40];
    IDAT->p_data = &png_buffer[41];
    IDAT->crc = (png_buffer[png_size-16] << 24) + (png_buffer[png_size-15] << 16) + (png_buffer[png_size-14] << 8) + png_buffer[png_size-13];
}

void getIEND(const unsigned char* png_buffer, chunk_p* IEND, long png_size) {
    IEND->length = (png_buffer[png_size-12] << 24) + (png_buffer[png_size-11] << 16) + (png_buffer[png_size-10] << 8) + png_buffer[png_size-9];
    IEND->type[0] = png_buffer[png_size-8]; IEND->type[1] = png_buffer[png_size-7]; IEND->type[2] = png_buffer[png_size-6]; IEND->type[3] = png_buffer[png_size-5];
    IEND->p_data = NULL;
    IEND->crc = (png_buffer[png_size-4] << 24) + (png_buffer[png_size-3] << 16) + (png_buffer[png_size-2] << 8) + png_buffer[png_size-1];
}
