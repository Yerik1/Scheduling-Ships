#include "components.h"
#include "raylib.h"

void DrawTextBox(TextBox *box)
{
    Color color = box->is_active ? YELLOW : LIGHTGRAY;
    DrawRectangleLinesEx(box->bounds, 2, color);
    DrawText(box->label, box->bounds.x, box->bounds.y - 25, 20, color);
    DrawText(box->buffer, box->bounds.x + 10, box->bounds.y + 10, 20, RAYWHITE);

    // Cursor parpadeante
    if (box->is_active && (int)(GetTime() * 2) % 2)
    {
        int width = MeasureText(box->buffer, 20);
        DrawLine(box->bounds.x + 12 + width, box->bounds.y + 8,
                 box->bounds.x + 12 + width, box->bounds.y + 32, YELLOW);
    }
}

void UpdateTextBox(TextBox *box)
{
    int key = GetCharPressed();
    // Captura de números
    while (key > 0)
    {
        if ((key >= '0') && (key <= '9') && (box->cursor_pos < 15))
        {
            box->buffer[box->cursor_pos] = (char)key;
            box->buffer[box->cursor_pos + 1] = '\0';
            box->cursor_pos++;
        }
        key = GetCharPressed();
    }
    // Borrado
    if (IsKeyPressed(KEY_BACKSPACE) && box->cursor_pos > 0)
    {
        box->cursor_pos--;
        box->buffer[box->cursor_pos] = '\0';
    }
}

void DrawDropdown(Dropdown *dd, bool isFocused)
{
    // 1. Dibujar el cuadro base (el que siempre se ve)
    Color mainColor = isFocused ? YELLOW : LIGHTGRAY;
    if (dd->isExpanded)
        mainColor = SKYBLUE;

    DrawRectangleLinesEx(dd->bounds, 2, mainColor);
    DrawText(dd->label, dd->bounds.x, dd->bounds.y - 22, 18, mainColor);
    DrawText(dd->options[dd->selectedIndex], dd->bounds.x + 10, dd->bounds.y + 10, 18, RAYWHITE);

    // Icono indicador (v o ^)
    DrawText(dd->isExpanded ? "^" : "v", dd->bounds.x + dd->bounds.width - 25, dd->bounds.y + 10, 18, mainColor);

    // 2. Dibujar la lista desplegable
    if (dd->isExpanded)
    {
        for (int i = 0; i < dd->optionsCount; i++)
        {
            Rectangle itemRec = {dd->bounds.x, dd->bounds.y + dd->bounds.height + (i * dd->bounds.height),
                                 dd->bounds.width, dd->bounds.height};

            // Fondo de la opción (resaltar la seleccionada)
            Color bgColor = (dd->selectedIndex == i) ? DARKBLUE : BLACK;
            DrawRectangleRec(itemRec, bgColor);
            DrawRectangleLinesEx(itemRec, 1, DARKGRAY);

            // Texto de la opción
            DrawText(dd->options[i], itemRec.x + 10, itemRec.y + 10, 18, RAYWHITE);
        }
    }
}

void UpdateDropdown(Dropdown *dd, bool isFocused)
{
    Vector2 mouse = GetMousePosition();
    bool mouseOverMain = CheckCollisionPointRec(mouse, dd->bounds);

    // Open on click
    if (mouseOverMain && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        dd->isExpanded = !dd->isExpanded;
    }

    if (dd->isExpanded)
    {
        // Detect click on option selection
        for (int i = 0; i < dd->optionsCount; i++)
        {
            Rectangle itemRec = {dd->bounds.x, dd->bounds.y + dd->bounds.height + (i * dd->bounds.height),
                                 dd->bounds.width, dd->bounds.height};

            if (CheckCollisionPointRec(mouse, itemRec))
            {
                // Highlight option
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    dd->selectedIndex = i;
                    dd->isExpanded = false;
                }
            }
        }

        // Close on click outside
        if (!mouseOverMain && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mouse.y < dd->bounds.y + dd->bounds.height)
        {
            dd->isExpanded = false;
        }
    }
}
