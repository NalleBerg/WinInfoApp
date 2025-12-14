#pragma once
#include <QTableWidget>
#include <QWidget>

namespace ResizeHelper {
    // Resizes the table to fit its contents, up to maxWidth/maxHeight.
    // Returns the recommended size for the parent widget/dialog.
    QSize resizeTableToContents(QTableWidget* table, int maxWidth = 1300, int maxHeight = 900);
}