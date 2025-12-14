#include "include/resizehelper.h"
#include <QHeaderView>
#include <QScrollBar>

namespace ResizeHelper {

QSize resizeTableToContents(QTableWidget* table, int maxWidth, int maxHeight)
{
    if (!table) return QSize(350, 200);

    table->resizeColumnsToContents();
    table->resizeRowsToContents();

    int totalWidth = 0;
    int totalHeight = 0;

    for (int col = 0; col < table->columnCount(); ++col)
        totalWidth += table->columnWidth(col);
    for (int row = 0; row < table->rowCount(); ++row)
        totalHeight += table->rowHeight(row);

    // Add header and frame
    totalWidth += table->verticalHeader()->isVisible() ? table->verticalHeader()->width() : 0;
    totalHeight += table->horizontalHeader()->isVisible() ? table->horizontalHeader()->height() : 0;
    totalWidth += 4; // frame
    totalHeight += 4;

    // Add scrollbar size if needed
    if (totalWidth > maxWidth) totalHeight += table->horizontalScrollBar()->sizeHint().height();
    if (totalHeight > maxHeight) totalWidth += table->verticalScrollBar()->sizeHint().width();

    // Clamp to max
    totalWidth = std::min(totalWidth, maxWidth);
    totalHeight = std::min(totalHeight, maxHeight);

    table->setMinimumSize(totalWidth, totalHeight);
    table->setMaximumSize(maxWidth, maxHeight);

    return QSize(totalWidth, totalHeight);
}

}