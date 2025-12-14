// Button color definitions for WinInfoApp and future projects

// Normal green (used for OK in About dialogs)
constexpr COLORREF COLOR_BUTTON_GREEN = RGB(39, 174, 96);

// Olive green (used for Yes in Quit dialogs)
constexpr COLORREF COLOR_BUTTON_OLIVE = RGB(128, 128, 0);

// Dark blue (used for No, Cancel, Close buttons)
constexpr COLORREF COLOR_BUTTON_BLUE = RGB(52, 73, 94);

// Example usage:
// SetBkColor(hdc, COLOR_BUTTON_GREEN);
// SetBkColor(hdc, COLOR_BUTTON_OLIVE);
// SetBkColor(hdc, COLOR_BUTTON_BLUE);

// Button type enum for reference
enum ButtonColorType {
    BTN_GREEN, // OK (About)
    BTN_OLIVE, // Yes (Quit)
    BTN_BLUE   // No, Cancel, Close
};