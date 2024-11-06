#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"

#define MAKE_ID( s )        ( (unsigned int)( ( s[0] << 24 ) | ( s[1] << 16 ) | ( s[2] << 8 ) | ( s[3] ) ) )
#define IFF_FORM            MAKE_ID ( "FORM" )
#define IFF_FRAM            MAKE_ID("FRAM")
#define IFF_NIMG            MAKE_ID("NIMG")

typedef struct iff iff_t;
struct iff {
    iff_t *next;
    unsigned int type;
    unsigned int length;
    void *data;
    unsigned int reserved;
};

struct iff_nimg_frame
{
    unsigned char  left;
    unsigned char  top;
    unsigned char  width;
    unsigned char  height;
    unsigned short delay;
    unsigned char  compr;
    unsigned char  reserved;         // SBZ
    unsigned char  data[];
};

unsigned int f_get_w(FILE *fh) {
    unsigned int ret;

    if ( !fh )
        return 1;

    ret  = fgetc ( fh ) << 24;
    ret |= fgetc ( fh ) << 16;
    ret |= fgetc ( fh ) << 8;
    ret |= fgetc ( fh );

    return ret;
}

struct iff *iff_open(FILE *fh) {
    struct iff *iffh = NULL, *n = NULL, *p = NULL;
    unsigned int x = 0;
    unsigned int tlen = 0;

    x = f_get_w ( fh );

    if ( x == IFF_FORM ) {         // is this an IFF file
        iffh = (struct iff *)calloc ( 1, sizeof ( struct iff ) );

        if ( iffh ) {
            tlen = f_get_w ( fh ) - 4;                           // total len (tlen-4 is length of all chunks)
            iffh->type = f_get_w ( fh );                         // IFF file type

            // read all chunks
            for ( p = iffh; tlen > 0; ) {
                n = (struct iff *)malloc ( sizeof ( struct iff ) );
                n->next = NULL;
                n->type = f_get_w ( fh );
                n->length = f_get_w ( fh );

                if ( n->length > 0 ) {
                    n->data = malloc ( n->length );
                    fread ( n->data, 1, n->length, fh );
                } else {
                    n->data = NULL;
                }

                tlen -= 8 + n->length;

                if ( ( tlen > 0 ) && ( n->length & 3 ) ) {                                 // skip padding
                    for ( x = 0; x < 4 - ( n->length & 3 ); x++ ) {
                        fgetc ( fh );
                    }

                    tlen -= 4 - ( n->length & 3 );
                }

                n->reserved = 0;
                p->next = n;
                p = n;
            }
        }
    }

    return iffh;
}

struct iff *iff_find(struct iff *prev, unsigned int type) {
    if ( prev ) {
        prev = prev->next;
    }

    while ( prev && ( prev->type != type ) ) {
        prev = prev->next;
    }

    return prev;
}

/* byterun1 decompress algorithm by [Yak] */
int byterun1_decompress ( register unsigned char *outbuf, register unsigned char *inbuf, int size ) {
    int in = 0, out = 0;

    while(in < size) {
        if ( inbuf[in] <= 127 ) {
            memcpy ( outbuf + out, inbuf + in + 1, inbuf[in] + 1 );
            out += inbuf[in] + 1;
            in += inbuf[in] + 2;
        } else if ( inbuf[in] != 128 ) {
            memset ( outbuf + out, inbuf[in + 1], 257 - inbuf[in] );
            out += 257 - inbuf[in];
            in += 2;
        } else {
            in++;                  /* 128 = NOP */
        }
    }

    return out;
}

int renderToBmp(int *screen, struct iff *img, int twidth, int theight) {
    struct iff_nimg_frame *frame = (struct iff_nimg_frame *)( img->data );
    unsigned char *bmp = malloc(((frame->height+7) / 8) * frame->width );

    if ( bmp ) {
        if ( frame->compr == 1 ) {
            byterun1_decompress ( bmp, frame->data, img->length - sizeof ( struct iff_nimg_frame ) );
        } else {
            memcpy ( bmp, frame->data, img->length - sizeof ( struct iff_nimg_frame ) );
        }
    }

    for ( int row = 0; row < ( ( frame->height + 7 ) / 8 ); row++ ) {
        for ( int col = 0; col < frame->width; col++ ) {
            for ( int bit = 0; bit < 8; bit++ ) {
                if ( row * 8 + bit >= frame->height ) {
                    break;
                }
                screen[(frame->top+row*8 + bit) * twidth + frame->left + col] = ( bmp[row * frame->width + col] & ( 1 << bit ) ) ? 1 : 0;
            }
        }
    }
    free(bmp);
    return 0;
}

int main(void) {
    FILE *iffFile = fopen("8002.ni", "rb");
    struct iff *iffh = iff_open(iffFile);
    struct iff *img = iff_find(iffh, IFF_FRAM);
    struct iff *firstFrame = img;
    struct iff_nimg_frame *frame = (struct iff_nimg_frame *)( img->data );
    int resWidth = frame->width;
    int resHeight = frame->height;

    int *screen = (int*)malloc(resWidth*resHeight*sizeof(int));

    renderToBmp(screen, img, resWidth, resHeight);

    const int screenWidth = frame->width * 4;
    const int screenHeight = frame->height * 4;

    InitWindow(screenWidth, screenHeight, "NI viewer");
    SetTargetFPS(60);

    int ticks = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        for(int n = 0; n < screenHeight; n+=4) {
            for(int m = 0; m < screenWidth; m+=4) {
                if(screen[(n/4)*frame->width + (m/4)]) {
                    DrawRectangle(m, n, 4, 4, BLACK);
                }
            }
        }
        if(ticks > 10) {
            img = iff_find(img, IFF_FRAM);
            if(img) {
                renderToBmp(screen, img, resWidth, resHeight);
            } else {
                img = firstFrame;
                renderToBmp(screen, img, resWidth, resHeight);
            }
            ticks = 0;
        }
        EndDrawing();
        ticks += 1;
    }
    free(screen);
    CloseWindow();
    return 0;
}
