#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "raylib.h"

typedef struct
{
    Rectangle bounds;
    char buffer[16];
    int cursor_pos;
    bool is_active;
    char label[32];
} TextBox;

typedef struct
{
    Rectangle bounds;
    const char **options;
    int optionsCount;
    int selectedIndex;
    bool isExpanded;
    char label[32];
} Dropdown;

void DrawTextBox(TextBox *box);
void UpdateTextBox(TextBox *box);

void DrawDropdown(Dropdown *dd, bool isFocused);
void UpdateDropdown(Dropdown *dd, bool isFocused);

#endif
