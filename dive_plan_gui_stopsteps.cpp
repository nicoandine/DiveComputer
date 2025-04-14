#include "dive_plan_gui.hpp"
#include "main_gui.hpp"

namespace DiveComputer {

void DivePlanWindow::setupStopStepsTable() {
    // Log performance
    QElapsedTimer timer;
    timer.start();

    // Set up column headers
    QStringList headers;
    headers << "Depth\n(m)" << "Time\n(min)" << "";
    
    // Configure table
    TableHelper::configureTable(stopStepsTable, QAbstractItemView::SelectItems);
    TableHelper::setHeaders(stopStepsTable, headers);
    
    // Set column widths
    stopStepsTable->setColumnWidth(STOP_COL_DEPTH, 60);
    stopStepsTable->setColumnWidth(STOP_COL_TIME,  60);
    stopStepsTable->setColumnWidth(STOP_COL_DELETE, 45);
    
    // Connect cell change signal
    connect(stopStepsTable, &QTableWidget::cellChanged, 
        this, &DivePlanWindow::stopStepCellChanged);

    // Monitor performance
    printf("DivePlanWindow::setupStopStepsTable() took %lld ms\n", timer.elapsed());

    // Refresh the stop steps table
    refreshStopStepsTable();
}

void DivePlanWindow::refreshStopStepsTable() {
    // Log performance
    QElapsedTimer timer;
    timer.start();

    // Use the TableHelper for safe update
    TableHelper::safeUpdate(stopStepsTable, this, &DivePlanWindow::stopStepCellChanged, [this]() {
        stopStepsTable->setRowCount(m_divePlan->m_stopSteps.nbOfStopSteps());
        
        // Add stop steps to table
        for (int i = 0; i < m_divePlan->m_stopSteps.nbOfStopSteps(); ++i) {
            // Depth item
            stopStepsTable->setItem(i, STOP_COL_DEPTH, 
                TableHelper::createNumericCell(m_divePlan->m_stopSteps.m_stopSteps[i].m_depth, 1, true));
            
            // Time item
            stopStepsTable->setItem(i, STOP_COL_TIME, 
                TableHelper::createNumericCell(m_divePlan->m_stopSteps.m_stopSteps[i].m_time, 1, true));
            
            // Delete button (except for the last row if there's only one)
            if (m_divePlan->m_stopSteps.nbOfStopSteps() > 1 || i < m_divePlan->m_stopSteps.nbOfStopSteps() - 1) {
                stopStepsTable->setCellWidget(i, STOP_COL_DELETE, createDeleteButtonWidget([this, i]() {
                    deleteStopStep(i);
                }).release());
            }
        }
    });

    // Allow UI to process events after the edit
    QApplication::processEvents();

    // Monitor performance
    printf("DivePlanWindow::refreshStopStepsTable() took %lld ms\n", timer.elapsed());
}

void DivePlanWindow::stopStepCellChanged(int row, int column) {
    // Only handle valid columns (depth and time)
    if (column == STOP_COL_DEPTH || column == STOP_COL_TIME) {
        // Get the current value from the table
        QTableWidgetItem* item = stopStepsTable->item(row, column);
        if (!item) return;
        
        bool ok;
        double value = item->text().toDouble(&ok);
        
        if (ok) {
            // Get current values
            double depth = m_divePlan->m_stopSteps.m_stopSteps[row].m_depth;
            double time = m_divePlan->m_stopSteps.m_stopSteps[row].m_time;
            
            // Update with new value
            if (column == STOP_COL_DEPTH) {
                depth = value;
            } else { // STOP_COL_TIME
                time = value;
            }
            
            // Update the stop step
            m_divePlan->m_stopSteps.editStopStep(row, depth, time);

            // Rebuild and refresh the dive plan
            rebuildDivePlan();
            refreshWindow();
        }
    }
}

void DivePlanWindow::addStopStep() {
    // Add a new stop step with similar values
    m_divePlan->m_stopSteps.addStopStep(0.0, 0.0);
    
    // Rebuild and refresh the dive plan
    rebuildDivePlan();
    refreshWindow();
}

void DivePlanWindow::deleteStopStep(int row) {
    // Ensure we maintain at least one stop step
    if (m_divePlan->m_stopSteps.nbOfStopSteps() > 1) {
        
        // Remove the specified stop step
        m_divePlan->m_stopSteps.removeStopStep(row);
        
        // Rebuild and refresh the dive plan
        rebuildDivePlan();
        refreshWindow();
    }
}


} // namespace DiveComputer
