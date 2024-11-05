#include <stdio.h>
#include <stdlib.h>
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

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;

    FILE *iffFile = fopen("8014.ni", "rb");
    struct iff *iffh = iff_open(iffFile);
    struct iff *img = iff_find(iffh, IFF_FRAM);

    InitWindow(screenWidth, screenHeight, "NI viewer");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("NI viewer", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
