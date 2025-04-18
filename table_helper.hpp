// TableHelper.hpp
#ifndef TABLE_HELPER_HPP
#define TABLE_HELPER_HPP

#include "qtheaders.hpp"
#include <functional>
#include "global.hpp"

namespace DiveComputer {

class TableHelper {
public:
    // Configure basic table properties
    static void configureTable(QTableWidget* table, 
                              QAbstractItemView::SelectionBehavior behavior = QAbstractItemView::SelectRows,
                              QAbstractItemView::SelectionMode mode = QAbstractItemView::SingleSelection) {
        if (!table) return;
        
        table->setSelectionBehavior(behavior);
        table->setSelectionMode(mode);
        table->setAlternatingRowColors(true);
        table->verticalHeader()->setVisible(false);
        
        // Common edit triggers
        table->setEditTriggers(QAbstractItemView::DoubleClicked | 
                              QAbstractItemView::SelectedClicked | 
                              QAbstractItemView::EditKeyPressed);
    }
    
    // Set table headers and column properties
    static void setHeaders(QTableWidget* table, const QStringList& headers) {
        if (!table || headers.isEmpty()) return;
        
        table->setColumnCount(headers.size());
        table->setHorizontalHeaderLabels(headers);
    }
    
    // Overload for null pointers (when no signal/slot handling is needed)
    static void safeUpdate(QTableWidget* table, std::nullptr_t, std::nullptr_t, std::function<void()> updateFunc) {
        if (!table || !updateFunc) return;
        
        // Just execute the update function
        table->clearContents();
        updateFunc();
    }
    
    // Safe table update with signal blocking
    template<typename T, typename Func>
    static void safeUpdate(QTableWidget* table, T* instance, Func cellChangedHandler, std::function<void()> updateFunc) {
        if (!table || !updateFunc) return;
        
        // Only disconnect if a valid handler is provided
        if (instance && cellChangedHandler) {
            QObject::disconnect(table, &QTableWidget::cellChanged, instance, cellChangedHandler);
        }
        
        // Clear and reset the table
        table->clearContents();
        
        // Execute the update function
        updateFunc();
        
        // Only reconnect if a valid handler is provided
        if (instance && cellChangedHandler) {
            QObject::connect(table, &QTableWidget::cellChanged, instance, cellChangedHandler);
        }
    }
    
    // Create non-editable cell with center alignment
    static QTableWidgetItem* createReadOnlyCell(const QString& text) {
        QTableWidgetItem* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable); // Make non-editable
        return item;
    }
    
    // Create numeric cell with proper formatting
    static QTableWidgetItem* createNumericCell(double value, int precision, bool isEditable) {
        QTableWidgetItem *item = new QTableWidgetItem();
        
        // Format the number with the specified precision
        QString formattedValue = QString::number(value, 'f', precision);
        item->setText(formattedValue);
        
        // Set alignment to center
        item->setTextAlignment(Qt::AlignCenter);
        
        // Make read-only if not editable
        if (!isEditable) {
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        } else {
            // Apply editable style using our helper function
            applyEditableCellStyle(item);
        }
        
        return item;
    }

    // Highlight cell based on condition
    static void highlightCell(QTableWidgetItem* item, bool condition, 
                            const QColor& color = QColor(255, 200, 200)) {
        if (!item) return;
        
        if (condition) {
            item->setBackground(QBrush(color));
        } else {
            item->setBackground(QBrush());
        }
    }
};

} // namespace DiveComputer

#endif // TABLE_HELPER_HPP