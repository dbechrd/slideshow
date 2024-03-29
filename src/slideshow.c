#include <stdio.h>
#include <math.h>
#include "raylib/raylib.h"

Font font16;
Font font24;
Font font36;

#define MAX_ROWS 8
#define MAX_SLIDES 64

typedef enum {
    Row_Empty,
    Row_Text,
    Row_Image
} RowType;

typedef struct {
    Font font;
    const char *text;
} RowText;

typedef struct {
    Texture texture;
} RowImage;

typedef struct {
    float percent;  // 0 = fixed pixels, >0 = percent, <0 = dynamic fill
    Vector2 pixels;
    Vector2 actual;
} RowSize;

typedef struct {
    RowType type;
    RowSize size;
    union {
        RowText text;
        RowImage image;
    };
} Row;

typedef struct {
    int rowCount;
    Row rows[MAX_ROWS];
} Slide;

Slide slides[MAX_SLIDES];

int slideCount;
int slide;

Row *PushRow(Slide *slide, RowType type)
{
    if (slide->rowCount >= MAX_ROWS) {
        return 0;
    }

    Row *row = &slide->rows[slide->rowCount++];
    row->type = type;
    return row;
}

Row *PushRowEmpty(Slide *slide, float pctHeight)
{
    Row *row = PushRow(slide, Row_Empty);
    if (!row) {
        return 0;
    }

    if (pctHeight) {
        row->size.percent = pctHeight;
    }
    return row;
}

Row *PushRowText(Slide *slide, Font font, const char *text, float pctHeight)
{
    Row *row = PushRow(slide, Row_Text);
    if (!row) {
        return 0;
    }

    row->size.pixels = MeasureTextEx(font, text, (float)font.baseSize, 1.0f);

    char *c = text;
    while (c && *c) {
        if (*c == '\n') row->size.pixels.y += 6;
        c++;
    }

    if (pctHeight) {
        row->size.percent = pctHeight;
    }

    row->text.font = font;
    row->text.text = text;
    return row;
}

Row *PushRowImage(Slide *slide, Texture texture, float pctHeight)
{
    Row *row = PushRow(slide, Row_Image);
    if (!row) {
        return 0;
    }

    row->size.pixels = (Vector2){ (float)texture.width, (float)texture.height };
    if (pctHeight) {
        row->size.percent = pctHeight;
    }

    row->image.texture = texture;
    return row;
}

Slide *MakeSlide(void)
{
    if (slideCount >= MAX_SLIDES) {
        return 0;
    }

    Slide *slide = &slides[slideCount++];
    return slide;
}

Slide *MakeTextSlide(const char *title, const char *subtitle)
{
    Slide *slide = MakeSlide();
    if (!slide) {
        return 0;
    }

    PushRowEmpty(slide, 0.35f);
    PushRowText(slide, font36, title, 0.1f);
    if (subtitle) {
        PushRowText(slide, font24, subtitle, 0.1f);
    }
    PushRowEmpty(slide, 0.45f);
    return slide;
}

Slide *MakeImageSlide(const char *title, Texture texture, const char *subtitle)
{
    Slide *slide = MakeSlide();
    if (!slide) {
        return 0;
    }

    PushRowText(slide, font36, title, 0.1f);
    PushRowImage(slide, texture, 0.7f);
    if (subtitle) {
        PushRowText(slide, font24, subtitle, 0.2f);
    }
    return slide;
}

void RowDraw(Row *row, float y)
{
    Vector2 pos = { 0, y };
    switch (row->type) {
        case Row_Text: {
            pos.y += floorf((row->size.actual.y - row->size.pixels.y) / 2.0f);

            char buf[2048] = { 0 };
            const char *c = row->text.text;
            const char *lineStart = c;
            while (c && *c) {
                if (*c == '\n') {
                    const char *text = TextFormat("%.*s", c - lineStart, lineStart);
                    Vector2 size = MeasureTextEx(row->text.font, text, row->text.font.baseSize, 1.0f);
                    pos.x = floorf(GetRenderWidth() / 2.0f - size.x / 2.0f);
                    DrawTextEx(row->text.font, text, pos, (float)row->text.font.baseSize, 1.0f, WHITE);
                    pos.y += row->text.font.baseSize;
                    lineStart = c + 1;
                }
                c++;
            }

            if (c > lineStart) {
                const char *text = TextFormat("%.*s", c - lineStart, lineStart);
                Vector2 size = MeasureTextEx(row->text.font, text, row->text.font.baseSize, 1.0f);
                pos.x = floorf(GetRenderWidth() / 2.0f - size.x / 2.0f);
                DrawTextEx(row->text.font, text, pos, (float)row->text.font.baseSize, 1.0f, WHITE);
                pos.y += row->text.font.baseSize;
                lineStart = c + 1;
            }

            break;
        }
        case Row_Image: {
            float aspect = row->image.texture.width / (float)row->image.texture.height;
            Vector2 destSize = { 0 };
            if (row->size.actual.x >= row->size.pixels.x && row->size.actual.y >= row->size.pixels.y) {
                destSize.x = row->size.pixels.x;
                destSize.y = row->size.pixels.y;
            } else {
                float overflowX = row->size.pixels.x - row->size.actual.x;
                float overflowY = row->size.pixels.y - row->size.actual.y;
                if (overflowX > overflowY) {
                    destSize.x = floorf(row->size.actual.x);
                    destSize.y = floorf(row->size.actual.x / aspect);
                } else {
                    destSize.x = floorf(aspect * row->size.actual.y);
                    destSize.y = floorf(row->size.actual.y);
                }
            }
            pos.x = floorf(GetRenderWidth() / 2.0f - destSize.x / 2.0f);
            if (destSize.y < row->size.actual.y) {
                pos.y += floorf((row->size.actual.y - destSize.y) / 2.0f);
            }
            Rectangle src = { 0, 0, (float)row->image.texture.width, (float)row->image.texture.height };
            Rectangle dst = { pos.x, pos.y, destSize.x, destSize.y };
            DrawTexturePro(row->image.texture, src, dst, (Vector2){ 0, 0 }, 0, WHITE);
            break;
        }
    }
}

void SlideDraw(Slide *slide, float *y, float height)
{
    // Count dynamic rows (to divide dynamic height)
    float leftoverHeight = height;
    float leftoverPct = 1.0f;
    int dynamicRows = 0;
    for (int i = 0; i < slide->rowCount; i++) {
        Row *row = &slide->rows[i];
        if (row->size.percent) {
            dynamicRows++;
            if (row->size.percent > 0) {
                leftoverPct -= row->size.percent;
            }
        } else {
            leftoverHeight -= row->size.pixels.y;
        }
    }

    // Update dynamic rows to have appropriate height
    float dynamicHeight = leftoverHeight / dynamicRows;
    for (int i = 0; i < slide->rowCount; i++) {
        Row *row = &slide->rows[i];
        row->size.actual = row->size.pixels;
        if (row->size.actual.x > (float)GetRenderWidth()) {
            row->size.actual.x = (float)GetRenderWidth();
        }
        if (row->size.percent > 0) {
            row->size.actual.y = floorf(leftoverHeight * row->size.percent);
        } else if (row->size.percent < 0) {
            row->size.actual.y = floorf(dynamicHeight);
        }
    }

    for (int i = 0; i < slide->rowCount; i++) {
        Row *row = &slide->rows[i];
        RowDraw(row, *y);
        *y += row->size.actual.y;
    }
}

int main(int argc, char *argv[])
{
    InitWindow(800, 600, "Slideshow");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowState(FLAG_VSYNC_HINT);

    font16 = LoadFontEx("KarminaBold.otf", 16, 0, 0);
    font24 = LoadFontEx("KarminaBold.otf", 24, 0, 0);
    font36 = LoadFontEx("KarminaBold.otf", 36, 0, 0);

    Texture babyTex = LoadTexture("baby.png");
    Texture schoolTex = LoadTexture("school.png");
    Texture graduateTex = LoadTexture("graduate.png");
    Texture animTex = LoadTexture("anim.png");

    MakeTextSlide("Owl's Story", "Master of the WingDings (TM)");
    MakeImageSlide("Jan 1, 2003", babyTex, "Owl's Birthday");
    MakeImageSlide("Aug 28, 2008", schoolTex, "Owl's first day of school");
    MakeImageSlide("May 15, 2025", graduateTex, "Owl graduates college");
    MakeImageSlide("Animation Editor", animTex,
        "Allows you to split a spritesheet into frames,\n"
        "edit frame properties, and create and preview animations.\n"
        "\n"
        "This has the added benefit of being able to play the animations\n"
        "back at full speed, or frame-by-frame, allowing the artist to\n"
        "quickly sanity check their work without leaving the editor.\n"
    );
    MakeTextSlide("The End.", 0);

    bool hoveringBox = false;

    const float barSize = 16;
    const float iconMargin = 4;

    while (!WindowShouldClose()) {
        const double now = GetTime();
        const Vector2 mouse = GetMousePosition();

        const int boxBarY = GetRenderHeight() - barSize;

        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) ||
            (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse.y > barSize && mouse.y < boxBarY) ||
            GetMouseWheelMove() < 0)
        {
            if (slide < slideCount - 1) {
                slide++;
            }
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) ||
            (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && mouse.y > barSize && mouse.y < boxBarY) ||
            GetMouseWheelMove() > 0)
        {
            if (slide) {
                slide--;
            }
        }
        if (IsKeyPressed(KEY_HOME)) {
            slide = 0;
        }
        if (IsKeyPressed(KEY_END)) {
            slide = slideCount - 1;
        }

        ClearBackground(BLACK);
        BeginDrawing();

        // Header
        DrawRectangle(0, 0, GetRenderWidth(), barSize, ColorBrightness(DARKGRAY, -0.5f));
        DrawTextEx(font16, TextFormat("%d of %d", slide + 1, slideCount), (Vector2){ 4, 0 }, (float)font16.baseSize, 1.0f, WHITE);

        // Slide
        float slideY = font16.baseSize + 8.0f;
        SlideDraw(&slides[slide], &slideY, boxBarY - slideY);

        // Footer
        DrawRectangle(0, (int)GetRenderHeight() - barSize, GetRenderWidth(), barSize, ColorBrightness(DARKGRAY, -0.5f));

        hoveringBox = false;
        Vector2 boxPos = { 0, (float)GetRenderHeight() - barSize };

        for (int i = 0; i < slideCount; i++) {
            Rectangle rec = { boxPos.x, boxPos.y, barSize, barSize };
            Color color = i == slide ? BLUE : BLANK;
            if (CheckCollisionPointRec(mouse, rec)) {
                hoveringBox = true;
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    slide = i;
                } else {
                    color = SKYBLUE;
                }
            }
            DrawRectangleRec(rec, color);

            RowType rowType = 0;
            for (int r = 0; r < slides[r].rowCount; r++) {
                Row *row = &slides[i].rows[r];
                if (row->type > rowType) {
                    rowType = row->type;
                }
            }

            switch (rowType) {
                case Row_Text: {
                    rec.x += iconMargin;
                    rec.y += iconMargin;
                    rec.width -= iconMargin * 2;
                    rec.height -= iconMargin * 2;
                    DrawRectangleRec(rec, LIGHTGRAY);
                    break;
                }
                case Row_Image: {
                    Vector2 v1 = { rec.x + iconMargin            , rec.y + rec.height - iconMargin };  // bottom left
                    Vector2 v2 = { rec.x + rec.width - iconMargin, rec.y + rec.height - iconMargin };  // bottom right
                    Vector2 v3 = { rec.x + rec.width / 2         , rec.y + iconMargin              };  // top middle
                    DrawTriangle(v1, v2, v3, PURPLE);
                    break;
                }
            }

            boxPos.x += barSize;
        }

        EndDrawing();
    }

    UnloadFont(font16);
    UnloadFont(font24);
    UnloadFont(font36);
    UnloadTexture(babyTex);
    UnloadTexture(schoolTex);
    UnloadTexture(graduateTex);
    UnloadTexture(animTex);
    CloseWindow();
    return 0;
}